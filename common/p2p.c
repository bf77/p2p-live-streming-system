/*
*----------------------------------------------------------------------------
* #include
*----------------------------------------------------------------------------
*/


#include "p2p.h"

/*
*----------------------------------------------------------------------------
* #Declaration
*----------------------------------------------------------------------------
*/

//Packet
u16 MAX_DATA_BYTES = DATA_HDR_BYTES + BLOCKINFO_BYTES + MAX_BLOCK_BYTES;

//Constant values
u8 BROADCAST_MAC[ETH_ALEN] = {0xff,0xff,0xff,0xff,0xff,0xff};
u8 ZEROS_MAC[ETH_ALEN] = {0x0,0x0,0x0,0x0,0x0,0x0};

//Gateway
int IS_GATEWAY_MAC_GET = 0;
int IS_SETUP = 1;

//Node
int CURRENT_CHILD_NUM = 0;
u8 CONNECTED_NODES = 0;
u8 IS_BEGIN_STREAMING = 0;
u8 IS_BEGIN_PLAYING = 0;
u8 IS_RETRY = 0;

//Cache
u32 CACHE_TLV = (u32)(DEFAULT_CACHE * 0.8);
long long int CURRENT_CACHE = 0;
u64 TOTAL_BUFFER = 0;
u64 PLAYED_BUFFER = 0;
u64 BUFFER_DT_SEC = (u64)(BUFFERING_MBPS*(1000)*(1000)/(8*MAX_BLOCK_BYTES));

//ID
u16 CURRENT_NODE_ID = 0;

/*
*----------------------------------------------------------------------------
* #function()
*----------------------------------------------------------------------------
*/



void RawSocketBuilderWithBind( const int option , const u8 *if_name ){

  
  if( ( SD = socket( PF_PACKET , SOCK_RAW , htons( option ) ) )  < 0 ){
    
    perror("socket");
    exit(1);
    
  }

  
  struct sockaddr_ll sa_ll;
  bzero( &sa_ll , sizeof( struct sockaddr_ll ));
  sa_ll.sll_family = PF_PACKET;
  sa_ll.sll_protocol = htons( option );
  sa_ll.sll_ifindex =  if_nametoindex( if_name );

  //bind the specified interface
  if( bind( SD , (struct sockaddr *)&sa_ll , sizeof(sa_ll) ) < 0 ){
    
    perror("bind");
    exit(1);
    
  }

}

void DaemonServiceForUDP( const int port , const u8 *service_name ){

  int sd;
  
  if( ( sd = socket( AF_INET , SOCK_DGRAM , 0 ) )  < 0 ){
    
    perror("socket");
    exit(1);
    
  }

  struct sockaddr_in sa_in;
  bzero( &sa_in , sizeof( struct sockaddr_in ));
  sa_in.sin_family = AF_INET;
  sa_in.sin_port = htons( port );
  sa_in.sin_addr.s_addr =  INADDR_ANY;

  //bind the specified interface
  if( bind( sd , (struct sockaddr *)&sa_in , sizeof(sa_in) ) < 0 ){
    
    perror("bind");
    exit(1);
    
  }

}

u32 CalSumEach16bit( u16 *buf , int size ){

  u32 sum = 0x0;
  
  //plus by 16bit
  while( size > 1 ){

    //printf("%#02x\n",*buf);
    sum += *buf;
    buf++;
    size -= 2;
    
  }

  //if extra 1byte
  if( size == 1 ){

    //printf("%#02x\n",*buf);
    sum += *( (u8 *)buf );
    size = 0;
    
  }
  
  return sum;
  
}

u16 CalIPChecksum( u16 *buf , int size ){

  u16 checksum;
  u32 sum;

  sum = CalSumEach16bit( buf , size );
  
  //for the carry , added to the rest of the value
  sum = ( sum >> 16 ) + ( sum & 0xffff );
  
  //once again
  sum = ( sum >> 16 ) + ( sum & 0xffff );

  //flip every bits
  checksum = ~( sum & 0xffff );
  
  return checksum;

}

u16 CalL4Checksum( u16 *buf , int size , u16 *pseudo_iphdr ){

  u16 checksum;
  u32 sum;
  
  //firstly, calculate the sum of each 16bit within the pseudo IP header
  sum = CalSumEach16bit( pseudo_iphdr , sizeof( struct PseudoIP_header ) );

  //next, calculate the sum of each 16bit within L4 protocol header and data
  sum += CalSumEach16bit( buf , size );
  
  //for the carry , added to the rest of the value
  sum = ( sum >> 16 ) + ( sum & 0xffff );
  
  //once again
  sum = ( sum >> 16 ) + ( sum & 0xffff );

  //flip every bits
  checksum = ~( sum & 0xffff );
  
  return checksum;

}

void PacketGenerate( u8 *packet , const u8 *send_data , const u16 data_bytes , int dst_id ){

  //l2
  struct ether_header *ethhdr;
  //l3
  struct iphdr *iphdr;
  //l4
  struct udphdr *udphdr;

  //data pointer
  u8 *data_ptr;

  
  /* ---allocate each adderss--- */
  
  ethhdr = (struct ether_header *)packet;
  iphdr = (struct iphdr *)( (u8 *)ethhdr + sizeof( struct ether_header ) );
  udphdr = (struct udphdr *)( (u8 *)iphdr + sizeof( struct iphdr ) );
  data_ptr = (u8 *)( (u8 *)udphdr + sizeof( struct udphdr ) );
  
  /* ---l2 ethernet ---*/
  
  memcpy( ethhdr->ether_shost , SRC_MAC, ETH_ALEN );
  memcpy( ethhdr->ether_dhost , &DST_MAC[dst_id], ETH_ALEN );

  printf("DST_IP:");
  Dump((u8 *)&DST_IP[dst_id],IP_LENGTH);
  printf("DST_PORT:%u\n",DST_PORT[dst_id]);

  //set ether type
  ethhdr->ether_type=htons( ETHERTYPE_IP );
  
  
  /* ---l3 ip  ---*/
  
  //4bit ipv4 or ipv6
  iphdr->version = 4;

  //4bit header length = 4octets * n 
  iphdr->ihl = 20/4;

  //8bit [0:2] priority 1~8 ([0:5] DSCP) , [6:7] Reserve 
  iphdr->tos = 0;
  
  //16bit total length of packet ( l3 ip header + l4 header + data length )
  iphdr->tot_len=htons( sizeof(struct udphdr) + sizeof(struct iphdr) + data_bytes );

  //16bit identification any value
  iphdr->id = htons( 34567 );
  
  //16 bit field = [0:2] flags + [3:15] offset = 0x0
  iphdr->frag_off = htons(0);

  //8bit time to live
  iphdr->ttl = 0xff;

  //8bit protocol number
  iphdr->protocol = IPPROTO_UDP;
  //iphdr->protocol = 0xfe;
  
  //16bit checksum  Can't calculate at this point
  iphdr->check = 0x0;

  //32bit src addrss
  //iphdr->saddr = htonl(SRC_IP);
  iphdr->saddr = SRC_IP;
  //inet_aton( saddr , (struct in_addr *)&iphdr->saddr );
  
  //32bit dst adderss
  iphdr->daddr = DST_IP[dst_id];
  //inet_aton( daddr , (struct in_addr *)&iphdr->daddr );  

  //--Options start--

  //--Options unimplemented--

  //--Options end--
  
  //calculate checksum (do not htons , used the value computed in network byte order)
  iphdr->check = CalIPChecksum( (u16 *)iphdr , sizeof( struct iphdr ) );

  //printf("%2x",iphdr->check);
  
  
  /* ---l4 udp ---*/
  
  //16bit src port
  udphdr->source = htons( SRC_PORT );
  
  //16bit dest port
  udphdr->dest = htons( DST_PORT[dst_id] );

  //16bit data length
  udphdr->len = htons( sizeof(struct udphdr) + data_bytes );

  /* make pseudo IP header */

  //init Pseudo IP header
  struct PseudoIP_header *pseudo_iphdr;
  pseudo_iphdr = (struct PseudoIP_header *)malloc( sizeof(struct PseudoIP_header) );
  bzero( pseudo_iphdr , sizeof( struct PseudoIP_header ) );
  
  pseudo_iphdr->src_addr = (iphdr->saddr);
  pseudo_iphdr->dst_addr = (iphdr->daddr);
  //8bit
  pseudo_iphdr->protocol = (iphdr->protocol);
  
  //16bit
  pseudo_iphdr->protocol_len = htons( sizeof(struct udphdr) + data_bytes );
  
  /* Store data in advance for checksum */
  memcpy( data_ptr , send_data , data_bytes );
  
  
  //16bit checksum (do not htons , used the value computed in network byte order)
  udphdr->check = CalL4Checksum( (u16 *)udphdr , ( sizeof( struct udphdr) + data_bytes ) , (u16 *)pseudo_iphdr );

  free( pseudo_iphdr );
  
}

void GetIFInfo( struct IFinfo *if_info , int *if_num ){

  struct ifreq ifs_req[MAX_IF_NUM];
  struct ifconf if_conf;
  
  int sd;
  int get_if_num;
  int i;

  printf("---------------Interface List ------------------\n\n");
  
  //any socket ok ( communicate kernel by using fd , AF_INET is required)
  sd = socket(AF_INET, SOCK_DGRAM, 0);

  // store upper limited size 
  if_conf.ifc_len = sizeof( ifs_req );

  // address to receive from kernel
  if_conf.ifc_req = ifs_req;

  //get interface config
  ioctl( sd, SIOCGIFCONF, &if_conf );

  //the interface-num receieved from kernel
  get_if_num = ( if_conf.ifc_len / sizeof( struct ifreq ) );
  *if_num = get_if_num;
  
  
  //show interface infomation (MACaddr and interface name)
  for ( i = 0 ; i < get_if_num ; i++ ) {
    
    //show interface name
    u8 *if_name;
    if_name = ifs_req[i].ifr_name;
    strncpy( if_info[i].if_name , if_name , strlen(if_name) + 1 );
        
    printf("InterfaceName:%s\n", if_name );

    //get mac addr
    if( ioctl( sd , SIOCGIFHWADDR , &ifs_req[i] ) < 0 )
      perror( "HWADDR:");

    //store mac addr
    u8 *mac_addr;
    mac_addr = ifs_req[i].ifr_hwaddr.sa_data;
    memcpy( if_info[i].mac , mac_addr , 6 );

    //show mac addr
    printf("MACaddr:");
    Dump( mac_addr , ETH_ALEN );
    
    //get ip addr
    if( ioctl( sd , SIOCGIFADDR , &ifs_req[i] ) < 0 )
      perror( "IPADDR:");
    
    //cut 2byte for sockaddr_in
    u8 *ip_addr;    
    ip_addr = ifs_req[i].ifr_addr.sa_data;

    
    ip_addr+=2;

    //store ip addr
    memcpy( (u8 *)&(if_info[i].addr) , ip_addr , IP_LENGTH );
    DEBUG_PRINTF("0x%"PRIu32"x",if_info[i].addr.l);

    //store ip address in String type
    u8 ip_addr_str[MAX_IP_STR_LENGTH];
    inet_ntop( AF_INET , ip_addr ,ip_addr_str , MAX_IP_STR_LENGTH );
    strncpy( if_info[i].addr_str , ip_addr_str , strlen(ip_addr_str) + 1 );

    //show ip address in String type
    printf("IP Address:");
    Dump( ip_addr , IP_LENGTH );

  }

  close(sd);

  printf("------------------------------------------------\n\n");

}

int ARPRequest( const u8 *src_mac , const u8 *src_ip , const u8 *dst_mac , const u8 *dst_ip ){  

  struct arphdr *arp;

  /* init */
  arp = (struct arphdr *)malloc( sizeof(struct arphdr) );
  bzero( arp , sizeof(struct arphdr) );

  /* arp packet */

  //ether header
  memcpy( arp->ethhdr.ether_shost , src_mac , ETH_ALEN );
  memcpy( arp->ethhdr.ether_dhost , BROADCAST_MAC , ETH_ALEN );
  arp->ethhdr.ether_type = htons( ETHERTYPE_ARP );

  //hardware type
  arp->hw_type = htons( ARPHRD_ETHER );
  //inet protocol type
  arp->pro_type = htons( ETH_P_IP );
  //mac address length
  arp->hw_length = ETH_ALEN;
  //protocol length
  arp->pro_length = 0x4;
  //[Request]=[0x0001] , [Reply]=[0x0002]  
  arp->operation = htons(0x0001);

  //dst and src mac address
  memcpy( arp->src_mac , src_mac , ETH_ALEN );
  memcpy( arp->dst_mac , ZEROS_MAC , ETH_ALEN );

  //dst and src ip address
  memcpy( arp->src_ip , src_ip , IP_LENGTH );
  memcpy( arp->dst_ip , dst_ip , IP_LENGTH );

  //send packet
  if( write( SD , arp , sizeof(struct arphdr) ) < 0 ){
    perror( "write:" );
  };
  
  free(arp);

}

void GetGatewayIPandMAC( const char *gw ){

  //get gateway ip
  memcpy( GATEWAY_IP_STR , (u8 *)gw , strlen(gw) + 1 );

  bzero( &GATEWAY_IP , IP_LENGTH );

  //get gateway-ip in u8 * type
  inet_pton( AF_INET , GATEWAY_IP_STR , (u8 *)&GATEWAY_IP );
  
  //get router mac by ARP Request
  memcpy( SRC_MAC , HOST_INFO->mac , ETH_ALEN );
  ARPRequest( SRC_MAC , HOST_INFO->addr.array , GATEWAY_MAC , GATEWAY_IP.array );

  
  //sleep for gateway MAC
  while( !IS_GATEWAY_MAC_GET ){

    sleep(1);
    printf("Waiting for gateway MAC address\n");
    
  }
  
  printf("Get gateway MAC address\n");

}

void RenewDstIPandMAC( const char *ip_str , const u16 port , const u8 *dst_mac , const int dst_id ){

  printf("--------Setting Dst ... --------\n");

  //Assign MAC and IP
  inet_pton( AF_INET , ip_str , (u8 *)&DST_IP[dst_id] );
  memcpy( &DST_MAC[dst_id] ,  dst_mac , ETH_ALEN );
  DST_PORT[dst_id] = port;

  printf("DST IP:");
  Dump( (u8 *)&DST_IP[dst_id] , IP_LENGTH );
  printf("DST Port:%u\n",DST_PORT[dst_id]);
  printf("DST MAC:");
  Dump( (u8 *)&DST_MAC[dst_id] , ETH_ALEN );

  printf("------------- Done -------------\n");
  
}

void RenewSrcIPandMAC( const char *ip_str , const u8 *src_mac ){

  //Assign MAC and IP
  inet_pton( AF_INET , ip_str , (u8 *)&SRC_IP );
  memcpy( SRC_MAC ,  src_mac , ETH_ALEN );

  printf("--------Setting SRC ... --------\n");

  printf("SRC IP:");
  Dump( (u8 *)&SRC_IP , IP_LENGTH );
  printf("SRC MAC:");
  Dump( SRC_MAC , ETH_ALEN );

  printf("------------- Done -------------\n");
  
}

void InitHostInfo( struct IFinfo *info ){

  HOST_INFO = info;

  //Assign MAC address
  memcpy( SRC_MAC , HOST_INFO->mac , ETH_ALEN );
  
  //Show host interface name
  printf("HOST interface name:%s\n", HOST_INFO->if_name );
  
  //Show Host MAC
  printf("HOST MAC:");
  Dump( HOST_INFO->mac , ETH_ALEN );

  //Show Host IP address
  printf("HOST IP u32:%08x\n",HOST_INFO->addr.l);
  printf("HOST IP array:");
  Dump( HOST_INFO->addr.array , IP_LENGTH );

}

void SendPacket( const u8 *data , const u16 data_bytes , const int dst_id ){

  u8 packet[MAX_PACKET_SIZE];
  
  u16 packet_bytes;
  
  //Initialize
  bzero( packet, MAX_PACKET_SIZE );

  //Assign packet
  PacketGenerate( packet , data , data_bytes , dst_id );

  //Calculate packet size
  packet_bytes = sizeof( struct ether_header ) + sizeof( struct iphdr ) + sizeof( struct udphdr ) + data_bytes;  
  
  //send
  write( SD , packet , packet_bytes );
  NanoSleep(0,500000);
  
}

void SendWebMHeader( const int dst_id ){

  //For send
  u8 data[MAX_DATA_BYTES];

  //For read
  u16 data_header_bytes = BLOCKINFO_BYTES + DATA_HDR_BYTES;
  u16 data_bytes;

  struct datahdr *datahdr;
  
  //initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for block
  datahdr->protocol = BLOCK_PROTO;

  //input data webm_header
  memcpy( &data[data_header_bytes] , WEBM_HEADER.data , WEBM_HEADER.total_bytes );
 
  //get data length
  data_bytes = WEBM_HEADER.total_bytes + data_header_bytes;

  SendPacket( data , data_bytes , dst_id );
  
}

int ReadBlock( struct Block *block ){
  
  //For send
  u8 data[MAX_DATA_BYTES];

  //GetNextBlockByBytes()
  int status = 0;

  ImportWebM( data , MAX_BLOCK_BYTES );
  
  //get the next block
  status = GetNextBlockByBytes( block , MAX_BLOCK_BYTES , data );
  DEBUG_PRINTF("GetNextBlockByBytes() Status:%d\n",status);

  TOTAL_BLOCK_NUM++;

  return status;
  
}

int SendCacheBlock( struct Block *block , const int dst_id ){

  //printf("\n------SendCacheBlock()------\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  //GetNextBlockByBytes()
  int status = 0;

  //For read
  u16 data_header_bytes = BLOCKINFO_BYTES + DATA_HDR_BYTES;
  u16 data_bytes;

  struct datahdr *datahdr;

  //initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for block
  datahdr->protocol = BLOCK_PROTO;

  //Read from webm file
  __off64_t offset = SeekABSForNextBlock( block );
  SearchWebM( &data[data_header_bytes] , MAX_BLOCK_BYTES , offset );
    
  //get the next block
  status = GetNextBlockByBytes( block , MAX_BLOCK_BYTES , &data[data_header_bytes] );
  DEBUG_PRINTF("GetNextBlockByBytes() Status:%d\n",status);

  //Show BlockInfo
  //PrintBlock( block->info );
  
  //Assign block -> data
  CopyBlockInfoToBytes( &data[DATA_HDR_BYTES] , block->info );

  //Calculate the bytes of data
  data_bytes = block->info.bytes + data_header_bytes;
  
  SendPacket( data , data_bytes , dst_id );
  
  NanoSleep(0,500000);

  //printf("\n------SendCacheBlock() End------\n");

  return status;
  
}

void InitNodeStatus( struct NodeStatus node_status ){

  bzero( (u8 *)&node_status , NODE_STATUS_BYTES );

}

int IsListContains( const u16 id ){

  int ret = 0;
  struct NodeList *current_node_list = NODES_LIST;

  while( current_node_list!=NULL ){

    //Found
    if( current_node_list->status.id == id ){
      ret = 1;
      printf("ID %u Already have been contained...\n",id);
      break;
    }

    current_node_list = current_node_list->next;

  }

  return ret;

}

int ListSize( struct NodeList *node_list ){

  int size = 0;
  struct NodeList *current_node_list = node_list;

  while( current_node_list!=NULL ){

    size++;
    current_node_list = current_node_list->next;

  }

  return size;

}

struct NodeList *ListPush( struct NodeList *node_list, struct NodeStatus node_status ){

  struct NodeList *current_node_list = node_list;

  if( current_node_list == NULL ){

    current_node_list = (struct NodeList *)malloc(sizeof(struct NodeList));
    memcpy( (u8 *)&current_node_list->status , (u8 *)&node_status , NODE_STATUS_BYTES );

    current_node_list->next = NULL;

    printf("Push Node Status\n");
    PrintNodeStatus( current_node_list->status );

    return current_node_list;

  }
  
  while( current_node_list->next != NULL ){

    current_node_list = current_node_list->next;

  }

  //Storing
  current_node_list->next = (struct NodeList *)malloc(sizeof(struct NodeList));
  memcpy( (u8 *)&current_node_list->next->status , (u8 *)&node_status , NODE_STATUS_BYTES );
  current_node_list->next->next = NULL;

  printf("Push Node Status\n");
  PrintNodeStatus( current_node_list->next->status );

  return node_list;

}

struct NodeList *ListUpdate( struct NodeList *node_list, struct NodeStatus node_status ){

  struct NodeList *current_node_list = node_list;
  int update_id = node_status.id;

  while( current_node_list != NULL ){

    if( update_id == current_node_list->status.id ){

      //Copy
      memcpy( (u8 *)&current_node_list->status , (u8 *)&node_status , NODE_STATUS_BYTES );
      printf("Update Node Status\n");
      PrintNodeStatus( current_node_list->status );

    }
    
    current_node_list = current_node_list->next;

  }

  return node_list;

}

struct NodeList *GetListIndex( struct NodeList *node_list, int index ){

  struct NodeList *current_node_list = node_list;
  int count = 0;

  while( current_node_list != NULL ){

    if( index == count ){
      
      break;
      
    }
    count++;
    current_node_list = current_node_list->next;

  }

  return current_node_list;

}

u8 GetCloestLayer( struct NodeList *node_list ){

  u8 layer = NODES_STATUS[MY_ID].layer;
  u8 cloest_layer = 0;
  int diff_layers = 0;

  struct NodeList *current_node_list = node_list;

  while( current_node_list!=NULL ){

    diff_layers = current_node_list->status.layer - layer;
    current_node_list = current_node_list->next;

  }
  
  cloest_layer = layer + diff_layers; 

  return cloest_layer;

}

struct NodeList *GetPriorList( struct NodeList *node_list ){

  struct NodeList *current_node_list = node_list;
  u8 cloest_layer = GetCloestLayer( node_list );

  while( current_node_list!=NULL ){

    PrintNodeStatus( current_node_list->status );

    if( IsExceptListContains( EXCEPT_LIST , current_node_list->status.id ) ){

      current_node_list = current_node_list->next;
      continue;

    }

    if( current_node_list->status.layer == cloest_layer ){
      break;
    }

    current_node_list = current_node_list->next;

  }

  return current_node_list;
  
}

struct NodeList *GetList( struct NodeList *node_list , u16 id ){

  struct NodeList *current_node_list = node_list;

  while( current_node_list != NULL ){

    if( current_node_list->status.id == id ){

      break;

    }

  }

  return current_node_list;

}

void FreeExceptList( struct ExceptList *except_list ){

  free( except_list );
  except_list = NULL;
  
}

int IsExceptListContains( struct ExceptList *except_list , u16 id ){

  int ret = 0;
  struct ExceptList *current_except_list = except_list;
  
  while( current_except_list!=NULL ){

    if( current_except_list->id == id ){

      ret = 1;
      printf("Found Except ID %u\n",id);
      break;

    }
     
    current_except_list = current_except_list->next;

  }

  return ret;

}

struct ExceptList *ExceptListPush( struct ExceptList *except_list, u16 id ){

  struct ExceptList *current_except_list = except_list;
  struct ExceptList *ret_except_list;

  if( current_except_list == NULL ){

    current_except_list = (struct ExceptList *)malloc(sizeof(struct ExceptList));
    current_except_list->id = NODES_STATUS[MY_ID].id;
    printf("Push Inital Except ID %u\n",current_except_list->id);
    current_except_list->next = NULL;
    
    ret_except_list = current_except_list;

  }else{

    ret_except_list = except_list;

  }
  
  while( current_except_list->next != NULL ){

    current_except_list = current_except_list->next;

  }

  //Storing
  current_except_list->next = (struct ExceptList *)malloc(sizeof(struct ExceptList));
  current_except_list->next->id = id;
  current_except_list->next->next = NULL;

  printf("Push Except ID %u\n",current_except_list->next->id);

  return ret_except_list;

}

void PrintList( struct NodeList *node_list ){

  printf("------Print List------\n");

  struct NodeList *current_node_list = node_list;

  while( current_node_list != NULL ){

    PrintNodeStatus( current_node_list->status );
    current_node_list = current_node_list->next;

  }

  printf("------Print List End------\n");

}

void PrintExceptList( struct ExceptList *except_list ){

  printf("------Print ExceptList------\n");

  struct ExceptList *current_except_list = except_list;

  while( current_except_list != NULL ){

    printf("ID %u\n",current_except_list->id);
    current_except_list = current_except_list->next;

  }

  printf("------Print ExceptList End------\n");

}

int GenerateRndID( const int init_val , const int range ){
  
  int rnd_id = -1;
  int rnd_val;
  int child_count = 0;

  //Prevent 0 division
  if( range != 0 ){

    srand((unsigned)time(NULL));
    rnd_val  = (rand() % range ) + init_val;  
    printf("rnd_val %d\n",rnd_val);
    
  }else{

    rnd_val = init_val;

  }

  //rnd_val  change to ID
  if( rnd_val == 0 ){

    rnd_id = MY_ID;

  }else{

    int i;
    for( i=0 ; i<MAX_CHILD_NUM ; i++ ){

      if( (CONNECTED_NODES >> i) & 0x01 ){
	child_count++;
	
      }
      
      if( child_count == rnd_val ){	
	rnd_id = i;
	break;
      }

    }//end for

  }

  return rnd_id;

}

//%-- Setup --%
void SendSetup(){

  printf("\n------SendSetup()------\n");
  
  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = SETUP_PROTO;

  //Parent is not Host
  if( NODES_STATUS[MY_ID].id > 0 ){

    u16 id = htons(NODES_STATUS[MY_ID].id); 
    memcpy( &data[data_header_bytes] , (u8 *)&id , 2 );
    data_bytes = data_header_bytes + 2;

  }//Parent is Host
  else{

    data_bytes = data_header_bytes;  

  }

  SendPacket( data , data_bytes , PARENT_ID );
  printf("\n------SendSetup() end------\n");

}

int CheckChildNum(){

  int child_id = -1;
  int i;

   //Check the number of children
  if( CURRENT_CHILD_NUM < MAX_CHILD_NUM ){

    for( i=0 ; i<MAX_CHILD_NUM ; i++ ){
      
      //The child have Not connected the my node  
      if( !( (CONNECTED_NODES >> i) & 0x01 ) ){

	child_id = i;
	break;
	
      }
      
    }//for end MAX_CHILD_NUM

    printf("The child ID is %d\n",child_id);

  }else{
    
    printf("The number of child nodes is full...\n");

  }

  return child_id;

}

void NodeSetup( u8 *data , const u16 data_bytes , const u32 ipaddr , u16 port ){

  printf("\n------NodeSetup()------\n\n");

  u16 node_port = ntohs(port);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr) );
  printf("Node IP:%s(0x%"PRIx32")\n",node_ip_str,ntohl(ipaddr));

  //child + my node
  int rnd_child_id = GenerateRndID( 0 , CURRENT_CHILD_NUM + 1 );
  //int rnd_child_id = 0;

  if( rnd_child_id != MY_ID ){

    printf("Advertising...\n");
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
    SendAdvertise( TMP_ID , rnd_child_id );
    
    return;

  }

  printf("Child ID %d\n",rnd_child_id);

  //Check Cache
  if( CURRENT_CACHE < DEFAULT_CACHE ){
    
    //Process of List and advertise
    printf("The cache is not sufficient...\n");
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
    //Try to connect me
    SendAdvertise( TMP_ID , MY_ID );
    return;

  } 

  int child_id = CheckChildNum();

  //To join is successful
  if( child_id != -1 ){
      
    //Update  information to connect the child
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , child_id );

    //--Update Nodes Status and Renew Current child Block--
    CURRENT_CHILD_BLOCK[child_id]->info.cluster_seq = CURRENT_CLUSTER->prev->seq;
    RenewBlock( CURRENT_CHILD_BLOCK[child_id] );
    CURRENT_CHILD_BLOCK[child_id]->prev->info.cluster_seq -= 1;
    
    //Copy the previous block
    memcpy( (u8 *)&(NODES_STATUS[child_id]), (u8 *)&(CURRENT_CHILD_BLOCK[child_id]->prev->info) , BLOCKINFO_BYTES );    
    NODES_STATUS[child_id].layer = NODES_STATUS[MY_ID].layer + 1;
    NODES_STATUS[child_id].ipaddr = ntohl(ipaddr);
    NODES_STATUS[child_id].port = node_port;

    //Host ID
    if( NODES_STATUS[MY_ID].id == 0 ){
      NODES_STATUS[child_id].id = CURRENT_NODE_ID;
    }//Node ID
    else{
      
      u16 id = 0;
      memcpy( (u8 *)&id , data , 2 );
      NODES_STATUS[child_id].id = ntohs(id);

    }

    SendInitialStatus( child_id );
    SendWebMHeader( child_id );

    //Time
    UpdateTimestamp(&NODES_STATUS[child_id].timestamp);

    //Child Process
    CONNECTED_NODES += (0x01 << child_id);
    CURRENT_CHILD_NUM += 1;
    
    return;
    
  }else{
    
    //Process of List and advertise
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );

    //Re-draw except for my node
    rnd_child_id = GenerateRndID( 1 , CURRENT_CHILD_NUM );
    SendAdvertise( TMP_ID , rnd_child_id );
    return;

  }

  printf("\n------NodeSetup() end------\n\n");
  
}
//%-- Setup End --%

//%-- ID --%
void SendID( const u32 ipaddr , const u16 port ){

  printf("\n------SendID()------\n\n");

  CURRENT_NODE_ID++;

  u16 node_port = ntohs(port);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr) );
  printf("Node 1 IP:%s(0x%"PRIx32")\n",node_ip_str,ntohl(ipaddr));

  RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );

  //For send
  u8 data[MAX_DATA_BYTES];
  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = ID_PROTO;

  //Storing data
  u16 id = htons(CURRENT_NODE_ID);
  memcpy( &data[data_header_bytes] , (u8 *)&id , sizeof(u16) );
  
  //Total data bytes
  data_bytes = data_header_bytes + sizeof(u16);

  SendPacket( data , data_bytes , TMP_ID );
  printf("\n------SendID() end------\n\n");

}

void NodeID( u8 *data ){

  printf("\n------NodeID()------\n\n");
  
  memcpy( (u8 *)&(NODES_STATUS[MY_ID].id) , data , sizeof(u16) );
  NODES_STATUS[MY_ID].id = ntohs(NODES_STATUS[MY_ID].id);
  printf("NODE MY_ID id%d",NODES_STATUS[MY_ID].id);

  printf("\n------NodeID() end------\n\n");

}
//%-- ID End --%

//%-- Initial Status --%
void SendInitialStatus( const int dst_id ){

  printf("\n------SendInitialStatus()------\n\n");
  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = INITIAL_STATUS_PROTO;
  
  int status_num;
  int current_status_num = 0;

  if( (CONNECTED_NODES >> PARENT_ID) & 0x01 ){
    
    //Including my parent 
    status_num = CURRENT_CHILD_NUM + 1 + 1;

  }else{
    
    status_num = CURRENT_CHILD_NUM + 1;

  }

  //New child status
  printf("New Child Node Status\n");
  CopyNodeStatusToBytes( &data[data_header_bytes+NODE_STATUS_BYTES*current_status_num] , NODES_STATUS[dst_id] );
  current_status_num++;
  PrintNodeStatus( NODES_STATUS[dst_id] );

  //My status
  printf("My Status\n");
  CopyNodeStatusToBytes( &data[data_header_bytes+NODE_STATUS_BYTES*current_status_num] , NODES_STATUS[MY_ID] );
  current_status_num++;
  PrintNodeStatus( NODES_STATUS[MY_ID] );

  int i;
  //Except for my status
  for( i=0 ; i<(MAX_NODE_NUM-1) ; i++ ){
    
    //Except for new child
    if( i == dst_id )
      continue;

    if( (CONNECTED_NODES >> i) & 0x01 ){

      CopyNodeStatusToBytes( &data[data_header_bytes+NODE_STATUS_BYTES*current_status_num] , NODES_STATUS[i] );
      current_status_num++;

    }

  }

  data_bytes = data_header_bytes + (NODE_STATUS_BYTES * status_num);
  SendPacket( data , data_bytes , dst_id );
  printf("\n------SendInitialStatus() end------\n\n");

}

void NodeInitialStatus( u8 *data , const u16 data_bytes ){

  printf("\n------NodeInitialStatus()------\n\n");

  //    0     |      1      |      2     | ... |    X
  //my_status->parent_status->node_status->...->node_status

  int status_num = data_bytes / NODE_STATUS_BYTES;
  int list_num = status_num - 2;
  int i;

  //My status (Not overwrite NODES_STATUS[MY_ID].id)
  CopyBytesToNodeStatus( (&NODES_STATUS[MY_ID]) , data );
  
  //--Iniitializing

  printf("Initializing the first cluster...\n");
  InitCluster();
  printf("Done\n");    

  printf("Initializing the current block...\n");
  InitBlock();
  printf("Done\n");    
  
  //Cluster Seq Check
  CURRENT_BLOCK->info.cluster_seq = NODES_STATUS[MY_ID].cluster_seq;
  CURRENT_BLOCK->prev->info.cluster_seq = NODES_STATUS[MY_ID].cluster_seq;
  
  IS_GET_TIMECODE_OFFSET = 0;

  printf("My Node Status\n");
  PrintNodeStatus( NODES_STATUS[MY_ID] );

  //Parent status
  CopyBytesToNodeStatus( &(NODES_STATUS[PARENT_ID]) , &data[ NODE_STATUS_BYTES ] );
  printf("Parent Status\n");
  PrintNodeStatus( NODES_STATUS[PARENT_ID] );

  //Connect to the parent
  CONNECTED_NODES += ( 0x01 << PARENT_ID );

  //Init NODES_LIST
  struct NodeStatus current_node_status;
  InitNodeStatus( current_node_status );

  //Storing List 
  for( i=0 ; i<list_num ; i++ ){

    CopyBytesToNodeStatus( &current_node_status , &data[ NODE_STATUS_BYTES * (2+i) ] );
    
    //Check NODES_LIST contains
    if( IsListContains( current_node_status.id ) ){
      continue;
    }

    //Storing
    printf("List status\n");
    NODES_LIST = ListPush( NODES_LIST , current_node_status );
        
  }
  
  PrintList( NODES_LIST );

  //List Update
  UpdateTimestamp( &LISTUPDATE_TIMESTAMP );
  UpdateTimestamp( &DEPART_TIMESTAMP );
  UpdateTimestamp(&NODES_STATUS[PARENT_ID].timestamp);
  IS_SETUP = 0;

  printf("\n------NodeInitialStatus() end------\n\n");

}
//%-- Initial Status End --%

//%-- Advertise --%
void SendAdvertise( const int dst_id , const int ad_id ){

  printf("\n------SendAdvertise()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = ADVERTISE_PROTO;

  //                     ch/ad        m y
  int status_num = CURRENT_CHILD_NUM + 1;
  int current_status_num = 0;

  //Advertise child status
  CopyNodeStatusToBytes( &data[data_header_bytes+NODE_STATUS_BYTES*current_status_num] , NODES_STATUS[ad_id] );
  current_status_num++;
  PrintNodeStatus( NODES_STATUS[ad_id] );

  //Try to connect to me
  if( ad_id != MY_ID ){

    //My status
    CopyNodeStatusToBytes( &data[data_header_bytes+NODE_STATUS_BYTES*current_status_num] , NODES_STATUS[MY_ID] );
    current_status_num++;
    PrintNodeStatus( NODES_STATUS[MY_ID] );
    
  }

  int i;
  //Except for my status
  for( i=0 ; i<(MAX_NODE_NUM-1) ; i++ ){
    
    //Except for new child
    if( i == ad_id )
      continue;

    if( (CONNECTED_NODES >> i) & 0x01 ){

      CopyNodeStatusToBytes( &data[data_header_bytes+NODE_STATUS_BYTES*current_status_num] , NODES_STATUS[i] );
      current_status_num++;

    }

  }

  data_bytes = data_header_bytes + (NODE_STATUS_BYTES * status_num);
  SendPacket( data , data_bytes , dst_id );

  printf("\n------SendAdvertise() end------\n\n");

}

void NodeAdvertise( u8 *data , const u16 data_bytes ){

  printf("\n------NodeAdvertise()------\n\n");

  //    0     |      1      |     2     | ... |    X
  //ad_status->node_status->node_status->...->node_status

  int status_num = data_bytes / NODE_STATUS_BYTES;
  int i;

  struct NodeStatus ad_node_status;
  InitNodeStatus( ad_node_status );
  CopyBytesToNodeStatus( &ad_node_status , data );
  printf("Advertise Node Status\n");
  PrintNodeStatus( ad_node_status );

  //Init NODES_LIST
  struct NodeStatus current_node_status;
  InitNodeStatus( current_node_status );

  //Storing List 
  for( i=1 ; i<status_num ; i++ ){

    CopyBytesToNodeStatus( &current_node_status , &data[ NODE_STATUS_BYTES * i ] );
    
    //Check NODES_LIST contains
    if( IsListContains( current_node_status.id ) ){
      continue;
    }

    //Storing
    printf("List status\n");
    NODES_LIST = ListPush( NODES_LIST , current_node_status );

  }//end for
  
  //Retry Setup
  u16 node_port = ad_node_status.port;
  u32 ipaddr_n = htonl(ad_node_status.ipaddr);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr_n) );
  RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , PARENT_ID );

  UpdateTimestamp(&SETUP_TIMESTAMP);
  SendSetup();
  
  printf("\n------NodeAdvertise() end------\n\n");
  
}
//%-- Advertise End--%

int IsDuplicatedChild( u16 id ){
  
  int ret = 0;
  int i;

  for( i=0 ; i<MAX_CHILD_NUM ; i++ ){
    
    if( (CONNECTED_NODES >> i) & 0x01 ){
      
      if( id == NODES_STATUS[id].id )
	ret = 1;
      
    }

  }

  return ret;
  
}

//%-- Connect --%
void SendConnect( const int dst_id ){

  printf("\n------SendConnect()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = CONNECT_PROTO;

  //--Storing My Status--
  //Copy the previous block to the My status
  CopyNodeStatusToBytes( &data[data_header_bytes] , NODES_STATUS[MY_ID] );

  data_bytes = data_header_bytes + NODE_STATUS_BYTES;   
  SendPacket( data , data_bytes , TMP_ID );

  printf("\n------SendConnect() End------\n\n");

}

void NodeConnect( u8 *data ){

  printf("\n------NodeConnect()------\n\n");

  struct NodeStatus node_status;
  InitNodeStatus( node_status );

  //Storing node status
  CopyBytesToNodeStatus( &node_status , data );

  u16 node_port = node_status.port;
  u32 ipaddr_n = htonl(node_status.ipaddr);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr_n) );
  
  printf("Connecting IP:%s Node Status\n",node_ip_str);
  PrintNodeStatus( node_status );

  //Chech Cache
  if( CURRENT_CACHE < DEFAULT_CACHE ){
    
    //Process of List and advertise
    printf("The cache is not sufficient...\n");
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
    SendUnaccept( TMP_ID );

    return;

  } 

  //Check the number of children
  int child_id = CheckChildNum();
      
  if( child_id != -1 ){
    
    //Check Duplicate
    if( IsDuplicatedChild( node_status.id ) )
      return;

    //Storing Child Node Status
    CopyBytesToNodeStatus( &(NODES_STATUS[child_id]) , data );    

    //Dst Config
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , child_id );

    //Copy to the previous block 
    CURRENT_CHILD_BLOCK[child_id]->info.cluster_seq = NODES_STATUS[child_id].cluster_seq;
    RenewBlock( CURRENT_CHILD_BLOCK[child_id] );
    //Copy the previous block
    memcpy( (u8 *)&(NODES_STATUS[child_id]), (u8 *)&(CURRENT_CHILD_BLOCK[child_id]->prev->info) , BLOCKINFO_BYTES );
    NODES_STATUS[child_id].layer = NODES_STATUS[MY_ID].layer + 1;
    
    SendAccept( child_id );
    
    //Child Process
    CONNECTED_NODES += (0x01 << child_id);
    CURRENT_CHILD_NUM += 1;


    return;

  }else{
       
    RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
    SendUnaccept( TMP_ID );
    return;
    
  }

  printf("\n------NodeConnect() End------\n\n");

}
//%-- Connect End --%

//%-- Depart --%
void SendDepart(){

  printf("\n------SendDeprt()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];
  
  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;
  
  //Protocol for setting
  datahdr->protocol = DEPART_PROTO;
  
  //Storing my status
  CopyNodeStatusToBytes( &data[data_header_bytes] , NODES_STATUS[MY_ID] );

  data_bytes = data_header_bytes + NODE_STATUS_BYTES;  
  SendPacket( data , data_bytes , PARENT_ID );

  DEPART_NUM++;
  printf("\n------SendDepart() end------\n\n");

}

void NodeDepart( u8 *data ){

  printf("\n------NodeDepart()------\n\n");

  //status
  struct NodeStatus depart_node_status;
  CopyBytesToNodeStatus( &depart_node_status , data );
  printf("Depart Node Status\n");
  PrintNodeStatus( depart_node_status );

  //id
  u16 depart_id = depart_node_status.id;
  printf("Depart ID %u\n",depart_id);

  u16 node_port = depart_node_status.port;
  u32 ipaddr_n = htonl(depart_node_status.ipaddr);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr_n) );
  u16 cluster_seq;
  
  int i;
  for( i=0 ; i<MAX_CHILD_NUM ; i++ ){

    if( NODES_STATUS[i].id == depart_id ){

      //Process child and node
      CURRENT_CHILD_NUM--;
      CONNECTED_NODES -= (0x01 << i);

      //Latest Cluster Sequence
      cluster_seq = htons(CURRENT_CHILD_BLOCK[i]->prev->info.cluster_seq);

      //Init
      InitNodeStatus( NODES_STATUS[i] );

      printf("Node IP %s have been departed...\n",node_ip_str);

    }

  }

  RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
  SendDetach( TMP_ID , cluster_seq );
  printf("\n------NodeDepart() end------\n\n");

}
//%-- Depart End--%

//%-- Detach --%

void SendDetach( const int dst_id , const u16 cluster_seq ){

  printf("\n------SendDetach()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = DETACH_PROTO;

  memcpy( &data[data_header_bytes] , (u8 *)&cluster_seq , 2 );

  data_bytes = data_header_bytes + 2;
  SendPacket( data , data_bytes , dst_id  );
  DETACH_NUM++;

  printf("\n------SendDetach() end------\n\n");
  

}

void NodeDetach( u8 *data ){

  printf("\n------NodeDetach()------\n\n");

  printf("Have departed from Parent Node\n");
  printf("Former Parent Node Status\n");

  PrintNodeStatus( NODES_STATUS[PARENT_ID] );

  u16 cluster_seq = 0;
  memcpy( (u8 *)&cluster_seq , data , 2 );
  cluster_seq = ntohs(cluster_seq);

  //Initialized(Stream and Connect)
  IS_BEGIN_STREAMING = 0;
  CONNECTED_NODES -= 0x01 << PARENT_ID;
  UpdateTimestamp( &DEPART_TIMESTAMP );
  InitNodeStatus( NODES_STATUS[PARENT_ID] );
  //NODES_STATUS[MY_ID].cluster_seq = cluster_seq;

  struct NodeList *prior_node = GetPriorList(NODES_LIST);

  //List not found
  if( prior_node == NULL ){
    
    prior_node = GetList( NODES_LIST , ORIGIN_ID );
    printf("Prior Node Status\n");
    PrintNodeStatus( prior_node->status );

  }//List found
  else{

    printf("Prior Node Status\n");
    PrintNodeStatus( prior_node->status );

  }

  u16 node_port = prior_node->status.port;
  u32 ipaddr_n = htonl(prior_node->status.ipaddr);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr_n) );

  //Built Retry Flag
  IS_RETRY = 1;
  UpdateTimestamp( &RETRY_TIMESTAMP );
  NODES_STATUS[PARENT_ID].id = prior_node->status.id;

  RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
  SendConnect( TMP_ID );

  printf("\n------NodeDetach() end------\n\n");

}
//%-- Detach End --%

//%-- Accept --%
void SendAccept( const int dst_id ){

  printf("\n------SendAccept()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = ACCEPT_PROTO;

  //Storing My Status
  CopyNodeStatusToBytes( &data[data_header_bytes] , NODES_STATUS[MY_ID] );
  PrintNodeStatus( NODES_STATUS[MY_ID] );

  int i;
  for( i=0 ; i<8 ; i++ ){
    data[ data_header_bytes + NODE_STATUS_BYTES + i ] = (TIMECODE_OFFSET >> 8*i) & 0xff;
  }

  data_bytes = data_header_bytes + NODE_STATUS_BYTES + 8;  
  SendPacket( data , data_bytes , dst_id  );

  printf("\n------SendAccept() End------\n\n");

}

void NodeAccept( u8 *data ){

  printf("\n------NodeAccept()------\n\n");

  //Retry
  IS_RETRY = 0;

  //UpdateTimestamp
  UpdateTimestamp( &(NODES_STATUS[PARENT_ID].timestamp) );
  

  CopyBytesToNodeStatus( &(NODES_STATUS[PARENT_ID]) , data );
  printf("New Parent Status\n");
  PrintNodeStatus( NODES_STATUS[PARENT_ID] );

  int i;
  for( i=0 ; i<8 ; i++ ){
    NODE_TIMECODE_OFFSET += (data[ NODE_STATUS_BYTES + i ]) << 8*i;
  }

  printf("Node Timecode offset"PRIx64"\n",NODE_TIMECODE_OFFSET);

  //layer
  NODES_STATUS[MY_ID].layer = NODES_STATUS[PARENT_ID].layer + 1;
  if( !( (CONNECTED_NODES >> PARENT_ID) & 0x01) ){
    CONNECTED_NODES += 0x01 << PARENT_ID;
  }
  

  //Initialzied
  free( EXCEPT_LIST );
  EXCEPT_LIST = NULL;

  printf("\n------NodeAccetpt() End------\n\n");

}

//%-- Accept End--%

//%-- Unaccept --%
void SendUnaccept( const int dst_id ){

  printf("\n------SendUnaccept()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = UNACCEPT_PROTO;

  u16 id = htons(NODES_STATUS[MY_ID].id);
  memcpy( &data[ data_header_bytes ] , (u8 *)&id , 2 );

  data_bytes = data_header_bytes + 2;   
  SendPacket( data , data_bytes , dst_id  );

  printf("\n------SendUnaccept() End------\n\n");

}

void NodeUnaccept( u8 *data ){

  printf("\n------NodeUnaccept()------\n\n");

  printf("Unaccepted ...\n");
  printf("Connect to the next cancdidate node from Node List\n");

  //Processing exception
  u16 except_id;
  memcpy( (u8 *)&except_id , data , 2 );
  except_id = ntohs( except_id );
  //Process Exception
  EXCEPT_LIST = ExceptListPush( EXCEPT_LIST , except_id );
  if( !IsExceptListContains( EXCEPT_LIST , except_id ) )
    PrintExceptList( EXCEPT_LIST );

  struct NodeList *prior_node = GetPriorList( NODES_LIST );

  //List not found
  if( prior_node == NULL ){
    
    prior_node = GetList( NODES_LIST , ORIGIN_ID );
    printf("Not found List\n");
    printf("Connecting to Origin Source\n");
    PrintNodeStatus( prior_node->status );

    //Initialzied
    free( EXCEPT_LIST );
    EXCEPT_LIST = NULL;

  }//List found
  else{

    printf("Prior Node Status\n");
    PrintNodeStatus( prior_node->status );

  }

  u16 node_port = prior_node->status.port;
  u32 ipaddr_n = htonl(prior_node->status.ipaddr);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr_n) );

  RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
  SendConnect( TMP_ID );

  printf("\n------NodeUnaccept() End------\n\n");

}
//%-- Unaccept End --%

//%-- KeepAlive --%
void SendKeepAlive( const int dst_id ){

  printf("\n------SendKeepAlive()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = KEEPALIVE_PROTO;

  //Storing
  u16 id = htons(NODES_STATUS[MY_ID].id);
  memcpy( &data[data_header_bytes] , (u8 *)&id , 2 );

  data_bytes = data_header_bytes + 2; 
  SendPacket( data , data_bytes , dst_id  );
  
  printf("\n------SendKeepAlive() End------\n\n");

}

void NodeKeepAlive( u8 *data ){
  
  printf("\n------NodeKeepAlive()------\n\n");

  int i;
  int dst_id = -1;
  
  //ID
  u16 id = 0;
  memcpy( (u8 *)&id , data , 2 );
  id = ntohs(id);

  for( i=0 ; i<MAX_NODE_NUM ; i++ ){

    if( (CONNECTED_NODES >> i) & 0x01 ){

      if( NODES_STATUS[i].id == id ){

	dst_id = i;

      }

    }

  }

  if( dst_id == -1 ){

    printf("Another thread has broken the connection. in NodeKeepAlive\n");
    return;
    //exit(1);

  }

  SendKeepAliveRes( dst_id );

  printf("\n------NodeKeepAlive() End------\n\n");

}

//%-- KeepAlive End --%

//%-- KeepAliveRes --%
void SendKeepAliveRes( const int dst_id ){

  printf("\n------SendKeepAliveRes()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = KEEPALIVERES_PROTO;

  //Storing
  u16 id = htons(NODES_STATUS[MY_ID].id);
  memcpy( &data[data_header_bytes] , (u8 *)&id , 2 );

  data_bytes = data_header_bytes + 2; 
  SendPacket( data , data_bytes , dst_id  );

  printf("\n------SendKeepAliveRes() End------\n\n");
  
}

void NodeKeepAliveRes( u8 *data ){

  printf("\n------NodeKeepAliveRes()------\n\n");

  int i;

  //ID
  u16 id = 0;
  memcpy( (u8 *)&id , data , 2 );
  id = ntohs(id);

  for( i=0 ; i<MAX_NODE_NUM ; i++ ){

    if( (CONNECTED_NODES >> i) & 0x01 ){

      if( NODES_STATUS[i].id == id ){

	//timestamp
	UpdateTimestamp(&NODES_STATUS[i].timestamp);

      }

    }

  }

  printf("\n------NodeKeepAliveRes() End------\n\n");

}

//%-- KeepAliveRes End --%

//%-- Request List --%
void SendReqList( const int dst_id ){

  printf("\n------SendReqList()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = REQ_LIST_PROTO;

  //Storing
  u16 id = htons(NODES_STATUS[MY_ID].id);
  memcpy( &data[data_header_bytes] , (u8 *)&id , 2 );

  data_bytes = data_header_bytes + 2; 
  SendPacket( data , data_bytes , dst_id  );

  printf("\n------SendReqList() End------\n\n");

}

void NodeReqList( u8 *data ){

  printf("\n------NodeReqList()------\n\n");

  int i;
  int dst_id = -1;
  
  //ID
  u16 id = 0;
  memcpy( (u8 *)&id , data , 2 );
  id = ntohs(id);

  for( i=0 ; i<MAX_NODE_NUM ; i++ ){

    if( (CONNECTED_NODES >> i) & 0x01 ){

      if( NODES_STATUS[i].id == id ){

	dst_id = i;

      }

    }

  }

  if( dst_id == -1 ){

    printf("Another thread has broken the connection. NodeReqList()\n");
    return;
    //exit(1);

  }

  SendResList( dst_id );
  printf("\n------NodeReqList() End------\n\n");

}
//%-- Request List End --%

//%-- Responce List --%
void SendResList( const int dst_id ){

  printf("\n------SendResList()------\n\n");

  //For send
  u8 data[MAX_DATA_BYTES];

  u16 data_header_bytes = DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = RES_LIST_PROTO;

  int list_num = ListSize( NODES_LIST );
  //Except for dst id
  int status_num = CURRENT_CHILD_NUM - 1 + list_num;
  int count = 0;
  int i;

  //Child
  for( i=0 ; i<MAX_CHILD_NUM ; i++ ){

    if( dst_id == i )
      continue;

    if( (CONNECTED_NODES >> i) & 0x01 ){
      
      CopyNodeStatusToBytes( &data[ data_header_bytes + (NODE_STATUS_BYTES*count) ] , NODES_STATUS[i] );
      count++;
      
    }

  }

  //List
  for( i=0 ; i<list_num ; i++ ){
  
    struct NodeList *current_node_list = GetListIndex( NODES_LIST , i );

    //Except for dst id
    if( current_node_list->status.id == NODES_STATUS[dst_id].id ){

      status_num--;
      continue;

    }

    CopyNodeStatusToBytes( &data[ data_header_bytes + NODE_STATUS_BYTES * count ] , current_node_list->status );
    count++;

  }

  data_bytes = data_header_bytes + NODE_STATUS_BYTES * status_num; 
  SendPacket( data , data_bytes , dst_id  );

  printf("\n------SendResList() End------\n\n");

}

void NodeResList( u8 *data , const u16 data_bytes ){

  printf("\n------NodeResList()------\n\n");

  struct NodeStatus current_node_status;
  int list_num = data_bytes / NODE_STATUS_BYTES;
  int i;

  for( i=0 ; i<list_num ; i++ ){

    CopyBytesToNodeStatus( &current_node_status , &data[ NODE_STATUS_BYTES * i ] );
    if( IsListContains( current_node_status.id ) ){

      NODES_LIST = ListUpdate( NODES_LIST , current_node_status );

    }else{

      NODES_LIST = ListPush( NODES_LIST , current_node_status );

    }

  }

  printf("\n------NodeResList() End------\n\n");

}
//%-- Responce List End --%

void NodeRetry(){

  printf("\n------NodeRetry()------\n\n");

  printf("Connect to the next cancdidate node from Node List\n");

  //Retry
  IS_RETRY = 1;
  UpdateTimestamp( &RETRY_TIMESTAMP );

  //Processing exception
  u16 except_id = NODES_STATUS[PARENT_ID].id;

  //Process Exception
  EXCEPT_LIST = ExceptListPush( EXCEPT_LIST , except_id );
  if( !IsExceptListContains( EXCEPT_LIST , except_id ) )
    PrintExceptList( EXCEPT_LIST );

  struct NodeList *prior_node = GetPriorList( NODES_LIST );

  //List not found
  if( prior_node == NULL ){
    
    prior_node = GetList( NODES_LIST , ORIGIN_ID );
    printf("Not found List\n");
    printf("Connecting to Origin Source\n");
    PrintNodeStatus( prior_node->status );
    
    //Initialzied
    free( EXCEPT_LIST );
    EXCEPT_LIST = NULL;

  }//List found
  else{

    printf("Prior Node Status\n");
    PrintNodeStatus( prior_node->status );

  }

  u16 node_port = prior_node->status.port;
  u32 ipaddr_n = htonl(prior_node->status.ipaddr);
  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr_n) );

  RenewDstIPandMAC( node_ip_str , node_port , GATEWAY_MAC , TMP_ID );
  SendConnect( TMP_ID );

  printf("\n------NodeRetry() End------\n\n");

}

void SendEndStatus( const int dst_id ){
  
  //For send
  u8 data[MAX_DATA_BYTES];
  
  //End Status
  u8 blockinfo_end[BLOCKINFO_BYTES];

  u16 data_header_bytes = BLOCKINFO_BYTES + DATA_HDR_BYTES;
  u16 data_bytes;
  
  struct datahdr *datahdr;
  
  //loop
  int i;

  for( i=0 ; i < BLOCKINFO_BYTES ; i++ )
    blockinfo_end[i] = 0xff;

  //Initialization
  bzero( data , MAX_DATA_BYTES );
  datahdr = (struct datahdr *)data;

  //Protocol for setting
  datahdr->protocol = BLOCK_PROTO;
  
  memcpy( &data[DATA_HDR_BYTES] , blockinfo_end , BLOCKINFO_BYTES );

  //Assign data length
  data_bytes = data_header_bytes;
  
  //Packet
  SendPacket( data , data_bytes , dst_id );

  printf("WEBM END\n\n");

}

void CopyNodeStatusToBytes( u8 *data , struct NodeStatus node_status ){

  CopyBlockInfoToBytes( data , *((struct BlockInfo *)&node_status) );

  //id
  u16 id = htons(node_status.id);
  memcpy( &data[BLOCKINFO_BYTES] , (u8 *)(&id) , sizeof(u16) );

  //ipaddr
  u32 ipaddr = htonl(node_status.ipaddr);
  memcpy( &data[BLOCKINFO_BYTES + 2] , (u8 *)(&ipaddr) , sizeof(u32) );
  
  u16 port = htons(node_status.port);
  memcpy( &data[BLOCKINFO_BYTES + 6] , (u8 *)(&port) , sizeof(u16) );

}

void CopyBytesToNodeStatus( struct NodeStatus *node_status , u8 *data ){

  CopyBytesToBlockInfo( (struct BlockInfo *)(node_status) , data );

  //id
  u16 id = 0;
  memcpy( (u8 *)(&id) , &data[BLOCKINFO_BYTES] , sizeof(u16) );
  node_status->id = ntohs(id);

  //ipaddr
  u32 ipaddr = 0;
  memcpy( (u8 *)(&ipaddr) , &data[BLOCKINFO_BYTES+2] , sizeof(u32) );
  node_status->ipaddr = ntohl(ipaddr);

  u16 port = 0;
  memcpy( (u8 *)(&port) , &data[BLOCKINFO_BYTES+6] , sizeof(u16) );
  node_status->port = ntohs(port);

}

void PrintNodeStatus( const struct NodeStatus node_status ){

  printf("------NodeStatus------\n\n");

  PrintBlock( *( (struct BlockInfo *)(&node_status) ) );

  printf("Node Status id: %u\n",node_status.id);
  printf("Node Status ip: %"PRIx32"\n",node_status.ipaddr);
  printf("Node Status port: %"PRIu16"\n",node_status.port);

  printf("------NodeStatus end------\n\n");

}

void SignalHandler( int signum ){

  SIG_INT_COUNT++;

  if ( SIG_INT_COUNT  <= 1 ) {

    printf("Have Detected the first internal interrupt.\n");
    printf("Start to send the last packet.\n\n");

    int i;
    for( i=0 ; i<CURRENT_CHILD_NUM ; i++ ){
      SendEndStatus(i);
    }

    printf("The number of total blocks : %d\n",TOTAL_BLOCK_NUM);
    printf("Live streaming end...\n");

  }
  else {

    /* Destruction */
    pthread_join( READ_THREAD , NULL );
    pthread_join( TIME_THREAD , NULL );
    close(SD);
 
    printf("The Total Number to be detached :%d\n",DETACH_NUM);
    printf("The Total Number to be departed :%d\n",DEPART_NUM);
   
    if( NODES_STATUS[MY_ID].id != ORIGIN_ID ){

      UpdateClusterToWebM( CURRENT_CLUSTER , CURRENT_BLOCKINFO , CURRENT_BLOCK_NUM );
      PrintCluster( CURRENT_CLUSTER );
      ExportWebM( FOOTER , CLUSTER_HEADER_BYTES );
      //Check BlockInfos
      
      printf("Total Blcok Nunber:%lld\n",TOTAL_BLOCK_NUM);
      printf("Total File Bytes:%lld\n",TOTAL_FILE_BYTES);
      CloseWebM();
      exit(1);
      
    }

    printf("This system have terminated\n");
    exit(1);

  }

  SetSignal( signum );

}

void SetSignal( int signum ){

  if (signal( signum , SignalHandler ) == SIG_ERR) {

    printf("Clould not set signal ...\n");
    exit(1);

  }

}

void CalPlayedBuffer(){

  if( NODES_STATUS[MY_ID].id == ORIGIN_ID )
    return;

  TOTAL_BUFFER = TOTAL_BLOCK_NUM;

  if( IS_BEGIN_PLAYING ){

    struct timeval dt;

    DiffCurrentTime( PLAYED_TIMESTAMP , &dt );
    if( IsExceedTime( dt , PLAYED_INTARVAL ) ){

      UpdateTimestamp( &PLAYED_TIMESTAMP );
      PLAYED_BUFFER += BUFFER_DT_SEC;
      
    }

  }

}

//int test;

int SendMulStream(){

  int ret=1;
  int count=0;

  int i;

  UpdateOffsetWebM( &CURRENT_FPOS_R );
  __off64_t offset = CURRENT_FPOS_R.__pos;

  for( i=0 ; i<MAX_CHILD_NUM ; i++ ){
    
    if( (CONNECTED_NODES >> i) & 0x01 ){

      if ( IsOldBlockSeq( CURRENT_CHILD_BLOCK[i]->prev->info ) ){	
	
	printf("CURRENT_CLUSTER_SEQ:%"PRIu32"\n",CURRENT_CLUSTER->prev->seq);
	int status = SendCacheBlock( CURRENT_CHILD_BLOCK[i] , i );
		
	//PrintBlock( CURRENT_CHILD_BLOCK[i]->info );

	if( status == CLUSTER_END ){
	  
	  RenewBlock( CURRENT_CHILD_BLOCK[i] );
	  //PrintBlock( CURRENT_CHILD_BLOCK[i]->prev->info );

	}else{
	
	  ShiftToPreBlock( CURRENT_CHILD_BLOCK[i] );

	}

      }else{

	count++;

      }
      
    }
    
  }

  if( count == CURRENT_CHILD_NUM ){

    ret = 0;
    
  }

  SeekABSWebM( offset , &CURRENT_FPOS_R );

  return ret;
  
}

void TimeProcess(){

  //Init
  TIMEPROCESS_INTARVAL.tv_sec = INTARVAL_SEC;
  TIMEPROCESS_INTARVAL.tv_usec = INTARVAL_MSEC;

  //KeepAlive
  KEEPALIVE_TLV_INTARVAL.tv_sec = KEEPALIVE_SEC;
  KEEPALIVE_TLV_INTARVAL.tv_usec = KEEPALIVE_MSEC;

  //ListUpate
  LISTUPDATE_TLV_INTARVAL.tv_sec = LISTUPDATE_SEC;
  LISTUPDATE_TLV_INTARVAL.tv_usec = LISTUPDATE_MSEC;
  
  //Depart
  DEPART_INTARVAL.tv_sec = DEPART_SEC;
  DEPART_INTARVAL.tv_usec = DEPART_MSEC;

  //Played
  PLAYED_INTARVAL.tv_sec = PLAYED_SEC;
  PLAYED_INTARVAL.tv_usec = PLAYED_MSEC;

  GetTimestamp( &START_TIMESTAMP );
  PrintTime( START_TIMESTAMP );

  while( 1 ){

    struct timeval diff_tv;
    DiffCurrentTime( TIMESTAMP , &diff_tv );

    if( IsExceedTime( diff_tv , TIMEPROCESS_INTARVAL ) ){

      UpdateTimestamp( &TIMESTAMP );
      PrintTime( TIMESTAMP );

      int child_id;
      for( child_id=0 ; child_id<MAX_NODE_NUM ; child_id++ ){

	if( (CONNECTED_NODES >> child_id) & 0x01 ){

	  SendKeepAlive( child_id );

	}

      }//for
      
    }//if

    //Setup
    if( IS_SETUP ){
      
      DiffCurrentTime( SETUP_TIMESTAMP , &diff_tv );
      if( IsExceedTime( diff_tv , KEEPALIVE_TLV_INTARVAL ) ){
	
	UpdateTimestamp(&SETUP_TIMESTAMP);
	SendSetup();
	
      }//exceed
      
    }//setup
    

    //KeepAlive
    int id;
    for( id=0 ; id<MAX_NODE_NUM ; id++ ){
      
      if( (CONNECTED_NODES >> id) & 0x01 ){

	if( NODES_STATUS[id].timestamp.tv_sec == 0 && 
	    NODES_STATUS[id].timestamp.tv_usec == 0 )
	  continue;
		
	DiffCurrentTime( NODES_STATUS[id].timestamp , &diff_tv );	
	if( IsExceedTime( diff_tv , KEEPALIVE_TLV_INTARVAL ) ){

	  //Process Depart
	  CONNECTED_NODES -= (0x01 << id);
	  CURRENT_CHILD_NUM--;
	  InitNodeStatus( NODES_STATUS[id] );
	  printf("Unfortunately Node ID %u have departed\n",NODES_STATUS[id].id);

	  //Retry
	  if( id == PARENT_ID ){
	    NodeRetry();
	  }

	}
	
      }

    }//end KeepAlive
    
    //List
    DiffCurrentTime(  LISTUPDATE_TIMESTAMP , &diff_tv );
    if( IsExceedTime( diff_tv , LISTUPDATE_TLV_INTARVAL ) ){

      //Node Only && Have parent
      if( (CONNECTED_NODES >> PARENT_ID) & 0x01 ){

	//Update timestamp
	UpdateTimestamp( &LISTUPDATE_TIMESTAMP );

	//Parent
	SendReqList( PARENT_ID );
	int list_num = ListSize( NODES_LIST );
	int i;

	//My List
	for( i=0 ; i<list_num ; i++ ){

	  u16 port = NODES_LIST->status.port;
	  u32 ipaddr = NODES_LIST->status.ipaddr;
	  char *node_ip_str = inet_ntoa( *((struct in_addr *)&ipaddr) );
	  RenewDstIPandMAC( node_ip_str , port , GATEWAY_MAC , TMP_ID );
	  SendReqList( TMP_ID );

	}

      }

    }//end List

    //Retry
    DiffCurrentTime(  RETRY_TIMESTAMP , &diff_tv );
    if( IS_RETRY && IsExceedTime( diff_tv , KEEPALIVE_TLV_INTARVAL ) ){

      NodeRetry();

    }

    
    //--Cache--

    //Calcurete PLAYED_BUFFER
    CalPlayedBuffer();

    //Calcurate cache
    CURRENT_CACHE = TOTAL_BUFFER - PLAYED_BUFFER;
    
    //Check Flag 
    if( CURRENT_CACHE == DEFAULT_CACHE ){

      IS_BEGIN_STREAMING = 1;
      IS_BEGIN_PLAYING = 1;
      UpdateTimestamp( &PLAYED_TIMESTAMP );

    }

    //Check Depart
    if( CURRENT_CACHE < CACHE_TLV && IS_BEGIN_STREAMING ){

      DiffCurrentTime(  DEPART_TIMESTAMP , &diff_tv );
      if( IsExceedTime( diff_tv , DEPART_INTARVAL ) ){

	UpdateTimestamp( &DEPART_TIMESTAMP );
	//Check Connect
	if( (CONNECTED_NODES >> PARENT_ID) & 0x01 ){
	  SendDepart();
	}

      }

    }
  
  }//while

}//end function


void *ReadThread( void *args ){

  printf( "Create ReadThread\n" );
  Receive();

  printf("ReadThread end\n");
  
  return NULL;
  
}

void *TimeThread( void *args ){

  printf( "Create TimeThread\n" );
  TimeProcess();

  printf("TimeThread end\n");
  
  return NULL;
  
}
