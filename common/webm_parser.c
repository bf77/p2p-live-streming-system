/*
*----------------------------------------------------------------------------
* #include
*----------------------------------------------------------------------------
*/

#include "webm_parser.h"

/*
*----------------------------------------------------------------------------
* #Declaration
*----------------------------------------------------------------------------
*/


/* Initialization */

size_t CURRENT_READ_BYTES = 0;
__off64_t TOTAL_FILE_BYTES = 0;


u8 FOOTER[ CLUSTER_HEADER_BYTES ] = {
  0x1f,0x43,0xb6,0x75,
  0x01,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff
};

int FD = -1;

int CURRENT_BLOCK_NUM = 0;

u64 TOTAL_BLOCK_NUM = 0;

int IS_FIRST_CLUSTER = 1;

u64 NODE_TIMECODE_OFFSSET = 0;

/*
*----------------------------------------------------------------------------
* #function()
*----------------------------------------------------------------------------
*/


u32 GetEBMLID( const u8 *buf , u16 *size_bytes_ptr ){
 
  int i;
  for( i = 7 ; i >= 0 ; i-- ){    

    u8 nthbit = (buf[0] >> i) & 0x01;
    //if most significant bit (MSB)
    if( nthbit == 0x01 )
      break;
    
  }

  // the size bytes of bytes of chank
  int id_bytes = 8 - i ;
  *size_bytes_ptr = id_bytes;

  //return value
  u64 ebml_id = buf[0];
  for( i = 1 ; i < id_bytes ; i++ ){

    //processing for carry by 1byte
    ebml_id <<= 8 ;    
    ebml_id += buf[i];
    
  }
  
  return ebml_id;

}

u64 GetDataBytes( const u8 *buf , u16 *size_bytes_ptr ){

  int i;
  for( i = 7 ; i >= 0 ; i-- ){    

    u8 nthbit = (buf[0] >> i) & 0x01;
    //if most significant bit (MSB)
    if( nthbit == 0x01 )
      break;
    
  }

  // the size bytes of bytes of chank
  int data_bytes_length = 8 - i;
  *size_bytes_ptr = data_bytes_length;

  u_char byte_value_through_msb;
  //discard upper 2byte
  byte_value_through_msb = buf[0] & ( 0xff >> data_bytes_length );
 
  u64 data_bytes = byte_value_through_msb;
  for( i = 0 ; i < ( data_bytes_length - 1 ) ; i++ ){
    
    //processing for carry by 1byte
    data_bytes <<= 8 ;    
    data_bytes += buf[i+1];
    
  }

  return data_bytes;

}

struct Chank GetChank( const u8 *buf ){

  struct Chank current_chank;
  u16 size_bytes = 0;
  u64 current_position = 0;

  //get EBML element
  current_chank.id = GetEBMLID( &buf[0] , &size_bytes );
  
  //store id bytes
  current_chank.id_bytes = size_bytes;
  
  //move
  current_position += size_bytes;
  //DEBUG_PRINTF("emove %dbytes\n",size_bytes);

  //get data size
  current_chank.data_bytes = GetDataBytes( &buf[current_position] , &size_bytes );
  //DEBUG_PRINTF("Databytes: %llx\n",current_chank.data_bytes);
  
  //move
  current_position += size_bytes;
  //DEBUG_PRINTF("move %dbytes\n",size_bytes);
  
  // ---move to the end of data---
  
  //ignore limited value or not most deepest ID 
  if( current_chank.data_bytes == 0xffffffffffffff ){

    current_chank.data_bytes = 0;
    //8bytes (0x01 0xff 0xff 0xff 0xff 0xff 0xff 0xff)
    current_chank.chank_bytes = current_position;
    //printf("%#8x\n",&buf[current_position]);
    current_chank.data_header_position = current_position;
    
  }else{

    current_position += current_chank.data_bytes;
    current_chank.chank_bytes = current_position;
    current_chank.data_header_position = current_position - current_chank.data_bytes;
  }
     
  //DEBUG_PRINTF("chank_bytes:%lld\n\n",current_chank.chank_bytes);

  return current_chank;

}

void WriteClusterHeaderInWebM( u8 *file_buf ){

  memcpy( file_buf , FOOTER , CLUSTER_HEADER_BYTES );

}

void UpdateClusterDataBytesToWebM( const struct Cluster cluster ){

  u64 current_position = cluster.info.id_bytes;
  u64 data_bytes = cluster.info.data_bytes;
  u8 update_data_buf[7];

  //Loop
  int i = 0;

  //skip 0x01
  current_position += 1;
  
  int count_shift;
  //update the total data size  
  for( count_shift = 6*8 ; 0 <= count_shift ; count_shift -= 8 ){
    
    update_data_buf[i++] = ( ( data_bytes >> count_shift ) & 0xff );

  }

  //--Writing--
  UpdateOffsetWebM( &CURRENT_FPOS_W );
  __off64_t offset = CURRENT_FPOS_W.__pos;
  SeekABSWebM( cluster.pos + current_position , &CURRENT_FPOS_W );
  ExportWebM( update_data_buf , 7 );
  SeekABSWebM( offset , &CURRENT_FPOS_W );

}

void AttachHeader( u8 *file_buf , const u8 *recv_data ){

  //attached Header
  memcpy( file_buf , recv_data , WEBM_HEADER.total_bytes );

}

void AttachFooter( u8 *file_buf , const struct Cluster cluster ){

  u64 offset = cluster.info.chank_bytes;
  
  //attached Footer 
  memcpy( &file_buf[offset] , FOOTER , CLUSTER_HEADER_BYTES );

}

void AttachNullToCluster( u8 *file_buf , const struct Cluster cluster ){

  //adding null char
  u8 offset = cluster.info.chank_bytes;
  file_buf[ offset ] = '\0';

}

void InitCluster(){

  CURRENT_CLUSTER = (struct Cluster *)malloc(sizeof(struct Cluster));
  CURRENT_CLUSTER->prev = (struct Cluster *)malloc(sizeof(struct Cluster));

  //Initiatlize Cluster
  bzero( &CURRENT_CLUSTER->info , sizeof(struct Chank) );
  bzero( &CURRENT_CLUSTER->prev->info , sizeof(struct Chank) );

  //Initialized value
  CURRENT_CLUSTER->prev->option = 0xffffffff;

  CURRENT_CLUSTER->prev->prev = NULL;
  CURRENT_CLUSTER->prev->next = CURRENT_CLUSTER;
  CURRENT_CLUSTER->next = NULL;
  
}

void ShiftToPreCluster( struct Cluster *cluster ){

  //Copy to prev_cluster->info
  memcpy( &cluster->prev->info , &cluster->info , sizeof(struct Chank) );
  cluster->prev->pos    = cluster->pos;
  cluster->prev->seq    = cluster->seq;
  cluster->prev->option = cluster->option;
  
  //Initialized
  bzero( &cluster->info , sizeof(struct Chank) );
  cluster->pos    = 0;
  cluster->seq    = 0;
  cluster->option = 0;

}

void RenewClusterForRead( struct Cluster *cluster , const u8 *buf ){

  if( cluster == NULL ){

    printf("Cluster is NULL\n");
    exit(1);
    
  }
  
  //Assign Cluster
  cluster->info = GetChank(buf);
  cluster->pos = CURRENT_FPOS_R.__pos;
  cluster->option = 0;

  if( cluster->prev->option == 0xffffffff )
    cluster->seq = 0;
  else
    cluster->seq = cluster->prev->seq + 1;
  
}

void RenewClusterForWrite( struct Cluster *cluster ){

  cluster->pos = CURRENT_FPOS_W.__pos;
  cluster->seq = CURRENT_BLOCK->info.cluster_seq;

  cluster->info.id = CLUSTER_ID;
  cluster->info.id_bytes = CLUSTER_ID_BYTES;
  cluster->info.data_header_position = CLUSTER_HEADER_BYTES;
  cluster->info.data_bytes = 0;
  cluster->info.chank_bytes = CLUSTER_HEADER_BYTES;

  //Write initialized Cluster's values in WebM files.
  ExportWebM( FOOTER , CLUSTER_HEADER_BYTES );

}

void InitBlock(){

  //Allocation
  CURRENT_BLOCK = (struct Block *)malloc(sizeof(struct Block));
  CURRENT_BLOCK->prev = (struct Block *)malloc(sizeof(struct Block));
  
  //Initialized BlockInfo
  bzero( &CURRENT_BLOCK->info , BLOCKINFO_BYTES );
  bzero( &CURRENT_BLOCK->prev->info , BLOCKINFO_BYTES );

  //Initialized value
  CURRENT_BLOCK->prev->info.status = STATUS_INIT;
  CURRENT_BLOCK->prev->info.cluster_seq = 0;
  CURRENT_BLOCK->info.pos = CLUSTER_HEADER_BYTES;

  CURRENT_BLOCK->prev->prev = NULL;
  CURRENT_BLOCK->prev->next = CURRENT_BLOCK;
  CURRENT_BLOCK->next = NULL;

}

void InitChildBlocks(){

  int i;
  for( i=0 ; i<MAX_CHILD_NUM ; i++ ){

    //Allocation
    CURRENT_CHILD_BLOCK[i] = (struct Block *)malloc(sizeof(struct Block));
    CURRENT_CHILD_BLOCK[i]->prev = (struct Block *)malloc(sizeof(struct Block));

  }

}

void ShiftToPreBlock( struct Block *block ){
  
  //Copy to prev_block->info
  memcpy( &block->prev->info , &block->info , BLOCKINFO_BYTES );
  
  //Initialized
  bzero( &block->info , BLOCKINFO_BYTES );

}

//Withou alloating heap memory
void RenewBlock( struct Block *block ){

  if( block == NULL ){

    printf("Block is NULL by RenewBlock()\n");
    exit(1);
    
  }
  
  u16 cluster_seq = block->info.cluster_seq;

  //Initialized BlockInfo
  bzero( &block->info , BLOCKINFO_BYTES );
  bzero( &block->prev->info , BLOCKINFO_BYTES );

  //Initialized value
  block->prev->info.status = STATUS_INIT;
  block->prev->info.cluster_seq = cluster_seq + 1;
  block->prev->info.pos = CLUSTER_HEADER_BYTES;
  //block->info.pos = CLUSTER_HEADER_BYTES;

  block->prev->prev = NULL;
  block->prev->next = block;
  block->next = NULL;

}

void RenewBlockInfos(){

  int i;
  for( i = 0 ; i < MAX_BLOCKS_NUM ; i++ )
    bzero( &CURRENT_BLOCKINFO[i] , BLOCKINFO_BYTES );

}

void CopyBytesToBlockInfo( struct BlockInfo *info , const u8 *buf ){

  //block  
  memcpy( (u8 *)info , buf , BLOCKINFO_BYTES );
  
  //BlockInfo ByteOrder changed
  info->bytes = ntohs(info->bytes);
  info->pos = ntohl(info->pos);
  info->seq = ntohl(info->seq);
  info->total_piece_bytes = ntohs(info->total_piece_bytes);
  info->cluster_seq = ntohs(info->cluster_seq);

}

void CopyBlockInfoToBytes( u8 *buf , struct BlockInfo info ){
  
  //BlockInfo ByteOrder changed
  info.bytes = htons(info.bytes);
  info.pos = htonl(info.pos);
  info.seq = htonl(info.seq);
  info.total_piece_bytes = htons(info.total_piece_bytes);
  info.cluster_seq = htons(info.cluster_seq);

  //block  
  memcpy( buf , (u8 *)&info , BLOCKINFO_BYTES );

}

int CheckClusterSeq(){

  int status = 0;

  if(CURRENT_BLOCK->prev->info.cluster_seq != CURRENT_BLOCK->info.cluster_seq)
    status = 1;

  return status;

}

void UpdateClusterToWebM( struct Cluster *cluster , const struct BlockInfo *infos , const int count ){

  //init
  u64 total_data_bytes = 0;

  //loop
  int i;

  
  for( i=0 ; i < count ; i++ )
    total_data_bytes += infos[i].bytes;

    
  cluster->info.data_bytes += total_data_bytes;
  cluster->info.chank_bytes += total_data_bytes;
  
  UpdateClusterDataBytesToWebM( *cluster );
  
}

void InsertBlockToCluster( const u8 *recv_data , const u16 bytes ){
  
  if( CheckClusterSeq() > 0 ){
   
    UpdateClusterToWebM( CURRENT_CLUSTER , CURRENT_BLOCKINFO , CURRENT_BLOCK_NUM );
    //PrintCluster( CURRENT_CLUSTER );
    ShiftToPreCluster( CURRENT_CLUSTER );
    RenewClusterForWrite( CURRENT_CLUSTER );

    //Check BlockInfos

    //ReInitialization
    CURRENT_BLOCK_NUM = 0;

  }
    
  ExportWebM( &recv_data[BLOCKINFO_BYTES] , bytes );
      
}

int GetChankByBytes( struct Block *block , const u8 *buf , const u16 req ){

  u16 current_simpleblock_num = 0;
  u64 current_total_bytes = 0;
  u64 current_position = 0;
  
  int status = 0;  

  //Dump( buf , 4 );
  
  while( 1 ){

    //get current chank
    const struct Chank current_chank = GetChank(&buf[current_position]);
    
    DEBUG_PRINTF("current position :%"PRIu64"\n",current_position);
    DEBUG_PRINTF("Bytes:NextSimpleBlock %"PRIu64"\n",current_chank.chank_bytes);

    if( current_chank.id == CLUSTER_ID ){

      DEBUG_PRINTF("ID:Cluster %08x\n",current_chank.id);
      //printf("ID:Cluster\n");
      status = CLUSTER_END; 
      break;
      
    }else if( current_chank.id == CUE_ID ){
    
      //printf("ID:CUE_ID\n");
      status = CLUSTER_END; 
      break;

    }
     
    //Check over req_bytes without updating current position and total_bytes
    if( req < ( current_total_bytes + current_chank.chank_bytes ) )
      break;
    
    if( current_chank.id == TIMECODE_ID ){
      
      DEBUG_PRINTF("ID:Timecode %08x\n",current_chank.id);
      status = ONLY_TIMECODE;
      //printf("ID:Timecode\n");
      
    }else if( current_chank.id == SIMPLEBLOCK_ID ){
      
      DEBUG_PRINTF("ID:SimpleBlock %08x\n",current_chank.id);
      DEBUG_PRINTF("Bytes:SimpleBlock %"PRIu64"\n",current_chank.chank_bytes);
      //printf("ID:SimpleBlock\n");

      status = 1;
      current_simpleblock_num += 1;
      
    }else{

      DEBUG_PRINTF("ID:Unknown\n");
      //printf("ID:Unknown\n");
      break;
      status = -1;

    }
    
    current_position += current_chank.chank_bytes;
    current_total_bytes +=  current_chank.chank_bytes;
    
    DEBUG_PRINTF("current position :%"PRIu64"\n",current_position);
    DEBUG_PRINTF("chank bytes :%"PRIu64"\n",current_chank.chank_bytes);
    DEBUG_PRINTF("The Total SimpleBlock :%"PRIu16"\n",current_simpleblock_num);
    
    
  }//end while
  
  
  //To update
  block->info.bytes += current_total_bytes;

  
  return status;

}


void AssignBlockInfoByPiece( struct Block *block , const u8 *buf , const u16 req ,const int flag ){

  
  //--Piece--
  DEBUG_PRINTF("--Status Piece--\n");
  block->info.status = STATUS_PIECE;
 
  if( flag == 1 ){
  
    //--The First Piece--
  
    //get the information of next chank
    struct Chank next_chank = GetChank(buf);
      
    //update the information for next Block
    //                                 the bytes of the next chank +   the remains
    block->info.total_pieces = (int)(ceil( (next_chank.chank_bytes + block->info.bytes)*1.0 / MAX_BLOCK_BYTES));
    block->info.bytes += req;
    block->info.piece_seq = 1;
    block->info.total_piece_bytes += next_chank.chank_bytes;
    //block->info.total_simpleblock = 0;
  
  }else if( flag == 2 ){

    //--The Other Pieces--

    block->info.bytes = req;
    //block->info.piece_info = block->prev->info.piece_info + 1;
    block->info.total_pieces = block->prev->info.total_pieces;
    block->info.piece_seq = block->prev->info.piece_seq + 1;
    block->info.total_piece_bytes = block->prev->info.total_piece_bytes;	
    //block->info.total_simpleblock = 0;
    
  }else if( flag == 3 ){

    //--The Last Piece--

    //to calcurate bytes of the last piece
    u8 prev_piece_seq = block->prev->info.piece_seq;
    block->info.bytes = block->prev->info.total_piece_bytes - ( req * ( prev_piece_seq ) );
    //block->info.piece_info = block->prev->info.piece_info + 1;
    block->info.total_pieces = block->prev->info.total_pieces;
    block->info.piece_seq = block->prev->info.piece_seq + 1;
    block->info.total_piece_bytes = block->prev->info.total_piece_bytes;
    //block->info.total_simpleblock = 0;
  
  }
      
}

void AssignBlockInfoBySB( struct BlockInfo *info ){

  //--Simple Block--

  DEBUG_PRINTF("--Status Simple Block--\n");

  //To update the information for Block except block->info.bytes
  info->status = STATUS_SIMPLEBLOCK;
  info->total_piece_bytes = 0x0;
  info->total_pieces = 0x0;
  info->piece_seq = 0x0;
  
}

int GetNextBlockByBytes( struct Block *block , const u16 req , const u8 *block_head_ptr ){

  
  u64 current_total_bytes = 0;
  u16 current_simpleblock_num = 0;

  //For piece
  const int first_flag = 1;
  const int other_flag = 2;
  const int last_flag  = 3;

  //loop
  int i;

  u8 prev_status;
  
  //return
  int ret = 0;

  //To update block's Cluster sequences
  block->info.cluster_seq = block->prev->info.cluster_seq;
  

  //The first block in the Cluster 
  if( block->prev->info.status == STATUS_INIT ){

    block->info.seq = 1;
    block->info.pos = CLUSTER_HEADER_BYTES;
    
    DEBUG_PRINTF("The status of Block is STATUS INIT\n");

    //To request the total bytes and the number of simple-block for chanks
    ret = GetChankByBytes( block , block_head_ptr , req );
    
    if(  ret == 0 ){

      //--Piece--
      AssignBlockInfoByPiece( block , block_head_ptr , req ,first_flag );
      
    }else if( ret == ONLY_TIMECODE ){

      //Bytes for Timecode      
      struct Chank timecode = GetChank( block_head_ptr );
      block->info.total_piece_bytes = timecode.chank_bytes;
      block->info.bytes = timecode.chank_bytes;

      //printf("time_code:%u req:%u\n",block->info.bytes,req);
      //Dump( block_head_ptr , block->info.bytes );

      //--Piece--
      AssignBlockInfoByPiece( block , &block_head_ptr[block->info.bytes] , req - block->info.bytes ,first_flag );

    }else{

      //--Simple Block--

      //To update the information for Block except block->info.bytes and total_sb
      AssignBlockInfoBySB( &block->info );

    }

    
  } 
  else if( block->prev->info.status != STATUS_INIT ){

    DEBUG_PRINTF("The status of Block is not STATUS INIT\n");

    //To update the block sequence
    block->info.seq = block->prev->info.seq + 1;
    
    //To update the block position
    block->info.pos = block->prev->info.pos + block->prev->info.bytes;

    //Iniitalization
    prev_status = block->prev->info.status;

    
    if( prev_status == STATUS_PIECE ){
      
      //--Piece--
      
      u8 prev_total_piece = block->prev->info.total_pieces;
      u8 prev_seq    = block->prev->info.piece_seq;

      int is_current_piece_last = ( prev_total_piece == ( prev_seq + 1 ) );

      int is_prev_piece_last    = ( prev_total_piece == prev_seq );

      //to see if the current piece is the last piece
      if( is_current_piece_last ){

	//--The block is the last piece--
	DEBUG_PRINTF("The current block is the last piece\n");

	AssignBlockInfoByPiece( block , block_head_ptr , req , last_flag );	
	DEBUG_PRINTF("block info bytes:%u",block->info.bytes );
	
	//--to see if current block is Block or Piece--
	
	//To request the total bytes and the number of simple-block for chanks
	//[block_head_ptr] to move by the last piece
	ret = GetChankByBytes( block , &block_head_ptr[block->info.bytes] , (req - block->info.bytes) );

	//The chank is the only Cluster or the Cluster in including S.B.
 	if( ret > 0 ){
	  
	  //--Block--
	  
	  //-- prev     next --
	  //-- Piece    Block --

	  DEBUG_PRINTF("Chenge Piece to Block\n");
	  block->info.status = STATUS_BLOCK;

	  //to update bytes of the next block
	  block->info.bytes += current_total_bytes;

	}

	
      }else if( is_prev_piece_last ){
	
	//-- The previous Block is the last piece--
	DEBUG_PRINTF("The previous block is the last piece\n");
	
   	//To request the total bytes and the number of simple-block for chanks
	ret = GetChankByBytes( block , block_head_ptr , req );

	//The chank is S.B.s , or the next chank is Cluster 
 	if( ret > 0 ){
	  
	  //--Block--
	  
	  //-- prev     next --
	  //-- Piece    S.B. --

	  AssignBlockInfoBySB( &block->info );
	  

	}else{
	  
	  // -- prev     next --
	  // -- Piece    New Piece --
	
	  AssignBlockInfoByPiece( block , block_head_ptr , req , first_flag );

	}

      }else{

	//--The other pieces--
	DEBUG_PRINTF("The currnt block is the other piece\n");
	
	//-- prev     next --
	//-- Piece    Piece --

	AssignBlockInfoByPiece( block , block_head_ptr , req , other_flag );
	
      }
      
      
    }else if( prev_status != STATUS_PIECE ){
      
      //--Block and SimpleBlock--
      
      //To request the total bytes and the number of simple-block for chanks
      ret = GetChankByBytes( block , block_head_ptr , req );
		     
      if( ret > 0 ){
	
	//--Simple Block--	
	AssignBlockInfoBySB( &block->info );
	
      }else{
	
	//--Piece--
	AssignBlockInfoByPiece( block , block_head_ptr , req , first_flag );
	
      }
      
    }
    
  }//end if status


  return ret;
  
}

int AnalyzeChankToWebMHeader( const u8 *buf ){

  //current
  u32 current_id = 0;
  u64 current_position = 0;
  u64 current_bytes = 0;
  struct Chank current_chank = {};

  //function status
  int status = 0;

  
  while( buf[current_position] != '\0' ){

    //get chank_info
    current_chank = GetChank(&buf[current_position]);
    current_id = current_chank.id;
        
    if( current_id == EBML_ID ){

      DEBUG_PRINTF("ID:EBML 0x%08x\n",current_id);

    }else if( current_id == SEGMENT_ID ){

      printf("ID:Segment 0x%08x\n",current_id);

      PrintChank(current_chank);

      if( current_chank.data_bytes != 0 ){

	//To update the variables
	current_position += current_chank.data_header_position;
	current_bytes += current_chank.data_header_position;
	Dump(&buf[current_position],12);
	continue;
	
      }
      
           
    }else if( current_id == SEEKHEAD_ID ){

    printf("ID:SeekHead 0x%08x\n",current_id);

    //}//
    }else if( current_id == VOID_ID ){
      
      DEBUG_PRINTF("ID:Void 0x%08x\n",current_id);
     	
    }else if( current_id == INFO_ID ){

      DEBUG_PRINTF("ID:Info 0x%08x\n",current_id);
                
    }else if( current_id == TRACKS_ID ){

      DEBUG_PRINTF("ID:Tracks 0x%08x\n",current_id);
                
    }else if( current_id == CLUSTER_ID ){

      DEBUG_PRINTF("ID:CLUSTER 0x%08x\n",current_id);
      status = 1;
      break;
      
    }else{
      
      DEBUG_PRINTF("ID:Unknown 0x%08x\n",current_id);
      status = -2;
      break;
      
    }

    
    if( TOTAL_FILE_BYTES  < current_chank.chank_bytes ){

      status = -1;
      break;
    }
    
    //To update the variables
    current_position += current_chank.chank_bytes;
    current_bytes += current_chank.chank_bytes; 
    DEBUG_PRINTF("webm header current:%"PRIu64"\n\n",current_position);
    
  }


  if( status == 1 ){

    WEBM_HEADER.total_bytes = current_bytes;
    printf("File :%ld bytes\n",TOTAL_FILE_BYTES);
    printf("WebM header size:%"PRIu16"\n",WEBM_HEADER.total_bytes);

  }
  else if( status == -1 )
    printf("TOTAL_FILE_BYTES is not enough ...\n");
  else if( status == -2 )
    printf("Unknown ebml ID found ...\n");
  else
    printf("Could not complete in AnalyzeChankToWebMHeader()...\n");

  
  return status;
  
}

void InitOpenWebM(){
  
  FD = open( FILE_NAME , O_RDWR );

  //To use Read and Write mode
  if( FD < 0 ){
    
    printf("Could not open file ( open() )...\n");
    exit(1);
    
  }
  
}

void CreateWebM(){

  FD = open( FILE_NAME , O_CREAT );
  
  //To use Read and Write mode
  if( FD < 0 ){
    
    printf("Could not open file ( CreateWebM() )...\n");
    exit(1);
    
  }

  close(FD);
  
}

void OpenWebM( const char *option ){

  FD = open( FILE_NAME , O_RDWR );
  
  //To use Read and Write mode
  if( FD < 0 ){
    
    printf("Could not open file ( open() )...\n");
    exit(1);
    
  }
  
  FP = fdopen( FD , option );

  if( FP == NULL ){
    
    printf( "Could not open %s...\n",FILE_NAME);
    exit(1);

  }

  DEBUG_PRINTF("Open %s...\n", FILE_NAME );
  DEBUG_PRINTF("Open address:%p...\n", FP );

}

void ImportWebM( u8 *file_buf , const size_t bytes ){

  size_t count;

  __off64_t dbytes = TOTAL_FILE_BYTES - CURRENT_FPOS_R.__pos;
  //printf("dbytes:%ld\n",dbytes);
  
  
  if( bytes < dbytes )
    count = fread( file_buf , sizeof(u8) , bytes , FP );
  else
    count = fread( file_buf , sizeof(u8) , dbytes , FP );

  //Calculate read bytes
  CURRENT_READ_BYTES = count * sizeof(u8);
  
  //printf("Read %ld bytes... by ImportWebM()\n",CURRENT_READ_BYTES);

}

void SearchWebM( u8 *file_buf , const size_t offset , const __off64_t read_bytes ){

  __off64_t dbytes = TOTAL_FILE_BYTES - read_bytes;
    
  if( offset < dbytes ){
    fread( file_buf , sizeof(u8) , offset , FP );
  }
  else{
    fread( file_buf , sizeof(u8) , dbytes , FP );
    printf("There is not enough file space.\n");
  }
  
}


__off64_t UpdateOffsetWebM( fpos_t *fpos ){

  fgetpos( FP , fpos );
  //printf("\n--Current offset:%ld(0x%lx)--\n\n",fpos->__pos,fpos->__pos);

  return fpos->__pos;
  
}

void SeekABSWebM( __off64_t offset , fpos_t *fpos ){

  //To update position
  UpdateOffsetWebM( fpos );
  fpos->__pos = offset;

  //To set 
  fsetpos( FP , fpos );
  //CURRENT_OFFSET += pos;
  
  //printf("--Seek to %ld position--\n",fpos->__pos);

}

void SeekRLTWebM( __off64_t offset , fpos_t *fpos ){
  
  //To update position
  UpdateOffsetWebM( fpos );
  fpos->__pos += offset;
  
  //To set 
  fsetpos( FP , fpos );

  //printf("--Seek to %ld position--\n",fpos->__pos);
  
}

void SeekForNextBlock( struct Block *block ){

  //Set FP to the head  of next block
  __off64_t offset = block->info.bytes - CURRENT_READ_BYTES;
  SeekRLTWebM( offset , &CURRENT_FPOS_R );
  
}

__off64_t CalOffset( struct BlockInfo info ){

  u32 cluster_seq = info.cluster_seq;
  u32 count = cluster_seq - FIRST_CLUSTER_SEQ;
  __off64_t offset = WEBM_HEADER.total_bytes;
  
  int i;

  fpos_t fpos;

  //Cluster
  for( i=0 ; i<count ; i++ ){

    u8 file_buf[CLUSTER_HEADER_BYTES];
    SeekABSWebM( offset , &fpos );
    SearchWebM( file_buf , CLUSTER_HEADER_BYTES , offset );            
       
    struct Chank chank = GetChank( file_buf );
    //printf("Cluster %u\n",(i+FIRST_CLUSTER_SEQ));
    //PrintChank( chank );
    offset += chank.chank_bytes;

  }

  //Block
  offset += info.pos + info.bytes;

  //printf("Offset :%lld info.pos%"PRIu32"\n",offset,info.pos);

  return offset;

}

__off64_t SeekABSForNextBlock( struct Block *block ){

  //Set FP to the head  of next block
  __off64_t offset = CalOffset( block->prev->info );
  SeekABSWebM( offset , &CURRENT_FPOS_R );

  return offset;

}

void ExportWebM( const u8 *file_buf , const size_t bytes ){

  size_t count;
  size_t written_bytes;
  
  count = fwrite( file_buf , sizeof(u8) , bytes , FP );

  //Calculate bytes written
  written_bytes = count * sizeof(u8);

  if( written_bytes == bytes ){
    
    //printf("Writing is successful.\n");

  }
  else{
    
    printf("Illegally Written\n");
    exit(1);
    
  }
  
}

int CheckWebMSize( const int sleep_sec , const long sleep_nano_sec ){
  
  //File related
  __off64_t file_size;
  struct stat stat_buf;

  //Function status
  int status = 0;

  //For repetition
  NanoSleep( sleep_sec , sleep_nano_sec );

  if( fstat(FD, &stat_buf) < 0 ){
    
    printf("File size error...\n ( fstat() )");
    exit(1);

  }

  //Get file bytes
  file_size = stat_buf.st_size;
  //printf( "Get file bytes: %ld\n", file_size );

  
  if( TOTAL_FILE_BYTES < file_size ){
    
    status = 1;
    TOTAL_FILE_BYTES = file_size;
    //printf( "-----File size:%ld-----\n", TOTAL_FILE_BYTES );
   
  }else{
    
    //printf("----- No change in file size-----\n"); 
    
  }

  return status;

}

void CloseWebM(){

  fclose(FP);
  close(FD);

}

void UpdateWebM( fpos_t *fpos , const char *option ){
  
  UpdateOffsetWebM( fpos );

  __off64_t offset = fpos->__pos;

  CloseWebM();
  OpenWebM( option );

  SeekABSWebM( offset , fpos );

}

void WriteWebMHeader( const u8 *buf , const u16 bytes ){

  ExportWebM( buf, bytes );
  WEBM_HEADER.total_bytes = bytes;
  WEBM_HEADER.data = (u8 *)malloc(bytes);
  memcpy( WEBM_HEADER.data , buf , bytes );
  
}

void UpdateWebMHeaderFromWebM(){

  while(1){

    if( CheckWebMSize(0,100000) > 0 ){

      //Read
      if( 1024 < TOTAL_FILE_BYTES ){

	u8 file_buf[1024];
	bzero( file_buf , 1024 );
	ImportWebM( file_buf , 1024 );
	//Analyze to WEBM_HEADER
	if( AnalyzeChankToWebMHeader( file_buf ) > 0 ){

	  SeekABSWebM( 0 , &CURRENT_FPOS_R );
	  WEBM_HEADER.data = (u8 *)malloc(WEBM_HEADER.total_bytes);
	  memcpy( WEBM_HEADER.data , file_buf , WEBM_HEADER.total_bytes );
	  break;

	}

      }else{

	u8 file_buf[TOTAL_FILE_BYTES];
	bzero( file_buf , TOTAL_FILE_BYTES );
	ImportWebM( file_buf , TOTAL_FILE_BYTES );
	
	//Analyze to WEBM_HEADER
	if( AnalyzeChankToWebMHeader( file_buf ) > 0 ){
	  
	  SeekABSWebM( 0 , &CURRENT_FPOS_R );
	  memcpy( WEBM_HEADER.data , file_buf , WEBM_HEADER.total_bytes );
	  break;

	}

      }
            
    }//CheckWebmSize()
    
  }//while
  
}

int UpdateClusterFromWebM( struct Cluster *cluster ){

  u64 chank_bytes = 0;
  u32 ebml_id = 0;
  int status = 0;
  u8 file_buf[CLUSTER_HEADER_BYTES];

  //Firstly, call by zero
  CheckWebMSize(0,0);

  
  //WebM is not enough buffers
  if(  CLUSTER_HEADER_BYTES  > (TOTAL_FILE_BYTES - CURRENT_FPOS_R.__pos) )
    while( CheckWebMSize(0,100000) <= 0 );

  while(1){
    
    //Try to check buffers of WebM
    ImportWebM( file_buf , CLUSTER_HEADER_BYTES );
    //Dump( file_buf , CLUSTER_HEADER_BYTES );

    struct Chank chank = GetChank( file_buf );
    NanoSleep(0,1000*1000*100);
    
    
    if( chank.id == CLUSTER_ID ){
      
      if( memcmp( file_buf , FOOTER ,  CLUSTER_HEADER_BYTES ) != 0 ){
	
	printf("Updated Cluster Header\n");
	break;
	
      }

    }else if( chank.id == CUE_ID ){
      
      printf("CUE ID...\n");
      
    }
    
    //Streaming While waiting
    SendMulStream();
    
    CheckWebMSize(0,100000);    
    SeekRLTWebM( 0 - CLUSTER_HEADER_BYTES , &CURRENT_FPOS_R );
    UpdateWebM( &CURRENT_FPOS_R , FOP_RD );
    
  }

  
  
  //To update Cluster
  RenewClusterForRead( cluster , file_buf );
  ebml_id = cluster->info.id;
  chank_bytes = cluster->info.chank_bytes;
  
  
  //Check Cluster in data
  while(1){
    
    if( ebml_id == CLUSTER_ID ){
      
      //Check bytes of Cluster
      if( (TOTAL_FILE_BYTES - CURRENT_FPOS_R.__pos) < chank_bytes ){

	//Streaming While waiting
	SendMulStream();
	
	printf("Buffers in Cluster is not enough...Try to update buffers in Cluster\n");
	CheckWebMSize(0,100000);
	continue;
	
      }else{

	printf("Update Cluster\n");
	status = 1;
	break;
      }
      
    }else{
      
      printf("Unexpected ebml ID:%"PRIx64"...",ebml_id);
      status = -1;
      exit(1);
      
    }
      
  }

  return status;
    
}

int IsOldBlockSeq( struct BlockInfo info ){

  u32 current_cluster_seq = CURRENT_CLUSTER->prev->seq;
  u32 current_seq = LATEST_BLOCK_SEQ;
  u16 cluster_seq = info.cluster_seq;
  u32 seq = info.seq;
  u16 bytes = info.bytes;

  //Cluster
  if( cluster_seq < current_cluster_seq ){
    return 1;
  }
  else{

    //Case:The same cluster
    if( cluster_seq == current_cluster_seq ){

      if( seq <= current_seq ){		
	return 1;	
      }else{
	return 0;
      }

    }else{
      return 0;
    }

  } 
}

void ChangeTimecode( u8 *data ){

  struct Chank timecode = GetChank(data);
  //PrintChank( timecode );

  if( timecode.id == TIMECODE_ID ){

    int i;
    u64 timestamp = 0;
    for( i=0 ; i<timecode.data_bytes ; i++ ){
      
      timestamp <<= 8;
      timestamp += data[timecode.data_header_position + i];

    }

    printf("Timestamp %"PRIx64"\n",timestamp);

    int k;
    if( !IS_GET_TIMECODE_OFFSET ){

      TIMECODE_OFFSET = timestamp;
      IS_GET_TIMECODE_OFFSET = 1;
     
    }

    u64 modified_timestamp = timestamp + NODE_TIMECODE_OFFSSET - TIMECODE_OFFSET;

    printf("Modified Timestamp %"PRIx64"\n",modified_timestamp);

    for( i=0 ; i<timecode.data_bytes ; i++ ){
      
      u64 shift_val = modified_timestamp >> (8*(timecode.data_bytes-i-1));
      data[timecode.data_header_position + i] = shift_val & 0xff;

      //printf("%02x ",(shift_val&0xff));

    }

    printf("\n");

  }else{

    printf("Unexpected Error in ChangeTimecode()\n");
    exit(1);

  }

}

void AssembleBlock( u8 *data , const u16 bytes ){

  const u8 *block_data_ptr = &data[BLOCKINFO_BYTES];
  u16 block_data_bytes = bytes - BLOCKINFO_BYTES;

  //loop
  int i;  

  //Block start
  if( memcmp(  data , BLOCKINFO_START , BLOCKINFO_BYTES ) == 0 ){
    
    printf("Recivied WebM Header...and Writing\n\n");
    WriteWebMHeader( block_data_ptr , block_data_bytes );
    printf("Done\n");

    UpdateWebM( &CURRENT_FPOS_W , FOP_RDWR );
    RenewClusterForWrite( CURRENT_CLUSTER );

    //Total
    TOTAL_FILE_BYTES += block_data_bytes;
        
  }//Block end
  else if( memcmp( data , BLOCKINFO_END , BLOCKINFO_BYTES ) == 0 ){

    UpdateClusterToWebM( CURRENT_CLUSTER , CURRENT_BLOCKINFO , CURRENT_BLOCK_NUM );
    //PrintCluster( CURRENT_CLUSTER );
    ExportWebM( FOOTER , CLUSTER_HEADER_BYTES );

    //Check BlockInfos
    
    //Total
    TOTAL_FILE_BYTES += block_data_bytes;

    printf("The Total Number to be detached :%d\n",DETACH_NUM);
    printf("The Total Number to be departed :%d\n",DEPART_NUM);
    printf("Total Blcok Nunber:%lld\n",TOTAL_BLOCK_NUM);
    printf("Total File Bytes:%lld\n",TOTAL_FILE_BYTES);
    CloseWebM();
    
    

    exit(1);

  }//Block
  else{
    
    //--Block--

    struct BlockInfo tmp_info;
    CopyBytesToBlockInfo( &tmp_info , data );

    //Ignore old blocks
    if( IsOldBlockSeq( tmp_info ) ){
      
      return;
      
    }
    
    //First Cluster Flag
    if( IS_FIRST_CLUSTER ){

      FIRST_CLUSTER_SEQ = tmp_info.cluster_seq;
      IS_FIRST_CLUSTER = 0;
      
    }

    
    if( CURRENT_BLOCK_NUM == 0 ){
      
      printf("First Block \n");
            
    }else if( CURRENT_BLOCK_NUM == MAX_BLOCKS_NUM  ){

      UpdateClusterToWebM( CURRENT_CLUSTER , CURRENT_BLOCKINFO , MAX_BLOCKS_NUM );
      //PrintCluster( CURRENT_CLUSTER );
      UpdateWebM( &CURRENT_FPOS_W , FOP_RDWR );

      //Reinitialization
      CURRENT_BLOCK_NUM = 0;

      //Check all seq
      RenewBlockInfos();

    }
    
    //Copy
    CopyBytesToBlockInfo( &CURRENT_BLOCK->info , data );

    //Change Timecode( Only Fisrst Block )
    if( CURRENT_BLOCK->info.seq == FIRST_BLOCK_SEQ ){
      ChangeTimecode( &data[ BLOCKINFO_BYTES ] );
    }
    
    //Update Seq
    LATEST_BLOCK_SEQ = CURRENT_BLOCK->info.seq;
    //printf("LATEST BLOCK SEQ:%"PRIu32"\n",LATEST_BLOCK_SEQ);

    InsertBlockToCluster( data , CURRENT_BLOCK->info.bytes );
    CopyBytesToBlockInfo( &CURRENT_BLOCKINFO[CURRENT_BLOCK_NUM] , data );

    //Total
    TOTAL_FILE_BYTES += block_data_bytes;
    TOTAL_BLOCK_NUM++;
    //printf("The number of total blocks : %d\n",TOTAL_BLOCK_NUM);
    
    ShiftToPreBlock( CURRENT_BLOCK );    
    CURRENT_BLOCK_NUM++;
    

  }

  

}

void PrintChank( const struct Chank chank ){

  printf("\n------Chank------\n");

  printf("ID:0x%"PRIx32"\n",chank.id);
  printf("ID Bytes:%u\n",chank.id_bytes);
  printf("Data header pos:%u(0x%04x)\n",chank.data_header_position,chank.data_header_position);
  printf("Data Bytes:%"PRIu64"",chank.data_bytes);
  printf("(0x%"PRIx64")\n",chank.data_bytes);
  printf("Chank Bytes:%"PRIu64"",chank.chank_bytes);
  printf("(0x%"PRIx64")\n",chank.chank_bytes);
  
  printf("-------End-------\n\n");
  
}

void PrintCluster( struct Cluster *cluster ){

  printf("\n-----Cluster-----\n");

  printf("Position:%ld(0x%lx)\n",cluster->pos,cluster->pos);
  printf("Sequence:%"PRIu32"",cluster->seq);
  printf("(0x%"PRIx32")\n",cluster->seq);
  printf("Option:%"PRIx32"\n",cluster->option);
  
  //Cluster Informaition
  PrintChank( cluster->info );
 
  printf("-------End-------\n\n");
  
}

void PrintBlock( const struct BlockInfo info ){

  printf("-----Block-----\n");
  printf("Bytes:%u(0x%04x)\n",info.bytes,info.bytes);
  printf("Position:%"PRIu32"",info.pos);
  printf("(0x%"PRIx32")\n",info.pos);
  printf("Sequence:%"PRIu32"",info.seq);
  printf("(0x%"PRIx32")\n",info.seq);
  
  //status
  if( info.status == STATUS_PIECE ){
    printf("Status:Piece\n");
  }
  else if( info.status == STATUS_SIMPLEBLOCK ){
    printf("Status:S.B.\n");
  }
  else if( info.status == STATUS_BLOCK ){
    printf("Status:Block\n");
  }else if( info.status == STATUS_INIT ){
    printf("Status:Init Value\n");
  }else if( info.status == 0 ){
    printf("Status:Init Value\n");
  }else{
    printf("Status:Unknown %x\n",info.status);
    //exit(1);
  }
  
  //printf("Piece Info:%02x\n",info.piece_info);
  //u8 tmp; 
  //tmp = info.total_pieces;
  printf("Total Pieces%x\n",info.total_pieces);
  //tmp = info.piece_info & 0xf;
  printf("Piece Sequence%x\n",info.piece_seq);
  printf("Total piece bytes:%"PRIu32"",info.total_piece_bytes);
  printf("(0x%"PRIx32")\n",info.total_piece_bytes);
  printf("Cluster Sequence:%"PRIu32"",info.cluster_seq);
  printf("(0x%"PRIx32")\n",info.cluster_seq);
  printf("Layer:%d\n",info.layer);

  printf("------End------\n\n");

}
