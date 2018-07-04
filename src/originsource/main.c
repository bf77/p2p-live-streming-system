#include "../common/p2p.h"


void Receive(){


  while( 1 ){

    u8 packet[MAX_PACKET_SIZE];
    int current_block_num = 0;
    
    //printf("Reading...\n");

    //receive
    read( SD , packet , MAX_PACKET_SIZE );
    
    //init ether header
    struct ether_header *ethhdr;
    ethhdr = (struct ether_header *)packet;

    
    //ARP
    if( ethhdr->ether_type == ntohs( ETH_P_ARP ) ){

      printf("ARP receiving\n");

      //Declaration
      struct arphdr *arp;
      
      //allocate address
      arp = (struct arphdr *)packet;
      
      //for ARP Reply ( destination is  only host )
      if( memcmp( arp->ethhdr.ether_dhost , BROADCAST_MAC , ETH_ALEN ) != 0 && !IS_GATEWAY_MAC_GET ){
	
	memcpy( GATEWAY_MAC , arp->ethhdr.ether_shost , ETH_ALEN );
	IS_GATEWAY_MAC_GET = 1;
	printf("GATEWAY MAC:");
	Dump( arp->ethhdr.ether_shost , ETH_ALEN );
	
      }
      
    }//IP
    else if( ethhdr->ether_type == ntohs( ETH_P_IP )){

      //printf("Receiving IP packet\n");
      
      //--allocate each adderss--
      
      struct iphdr *iphdr;
      struct udphdr *udphdr;
      struct datahdr *datahdr;
      u8 *data;

      iphdr = (struct iphdr *)( (u8 *)ethhdr + ETHER_HDR_BYTES );
      udphdr = (struct udphdr *)( (u8 *)iphdr + IP_HDR_BYTES );
      datahdr = (struct datahdr *)( (u8 *)udphdr + UDP_HDR_BYTES );
      data = (u8 *)( (u8 *)datahdr + DATA_HDR_BYTES );

      //Addressed to myself  
      if( iphdr->daddr == SRC_IP ){
	
	//printf("Received a IP packets\n");

	//printf("Received by Port %u\n", ntohs(udphdr->dest) );
	
	if( ntohs(udphdr->dest) == OS_PORT ){
	  
	  //--Setup--	  
	  if( datahdr->protocol == SETUP_PROTO ){
	    
	    printf("Receiving a Setup packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);
	   
	    //Only host
	    if( data_bytes == 4 ){
	      SendID( iphdr->saddr , udphdr->source );
	    }

	    NodeSetup( data , data_bytes ,iphdr->saddr , udphdr->source );
	    
	  }//--Initial Status--
	  else if( datahdr->protocol == INITIAL_STATUS_PROTO ){
	    
	    printf("Receiving a Initial Status packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);
	    NodeInitialStatus( data , data_bytes );

	  }//--Advertise--
	  else if( datahdr->protocol == ADVERTISE_PROTO ){

	    printf("Receiving a Advertise packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);
	    NodeAdvertise( data , data_bytes );

	  }//--Block--
	  else if( datahdr->protocol == BLOCK_PROTO ){

	    printf("Receiving a Block packet\n");
	    //AssembleBlock();	    	    

	  }//--Connect--
	  else if( datahdr->protocol == CONNECT_PROTO ){

	    printf("Receiving a Connect packet\n");
	    NodeConnect( data );

	  }//--Depart--
	  else if( datahdr->protocol == DEPART_PROTO ){

	    printf("Receiving a Depart packet\n");
	    NodeDepart( data );

	  }//--Have Departed--
	  else if( datahdr->protocol == DETACH_PROTO ){

	    printf("Receiving a Detach packet\n");
	    NodeDetach( data );

	  }//--Request List--
	  else if( datahdr->protocol == REQ_LIST_PROTO ){

	    printf("Receiving a Reqest List packet\n");
	    NodeReqList( data );

	  }//--Responce List--
	  else if( datahdr->protocol == RES_LIST_PROTO ){

	    printf("Receiving a Responce List packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);

	    //NodeResList( data , data_bytes );

	  }//--Accept--
	  else if( datahdr->protocol == ACCEPT_PROTO ){

	    printf("Receiving a Accept packet\n");
	    //NodeAccept( data );

	  }//--Unaccept--
	  else if( datahdr->protocol == UNACCEPT_PROTO ){

	    printf("Receiving a Unaccept packet\n");
	    //NodeUnaccept( data );

	  }//--KeepAlive--
	  else if( datahdr->protocol == KEEPALIVE_PROTO ){

	    printf("Receiving a KeepAlive packet\n");
	    NodeKeepAlive( data );

	  }//--KeepAliveRes--
	  else if( datahdr->protocol == KEEPALIVERES_PROTO ){

	    printf("Receiving a KeepAliveRes packet\n");
	    NodeKeepAliveRes( data );
	  }//--Unknown--
	  else{
	    
	    printf("Receiving a Unknown packet\n");	   
	    Dump( data , htons(udphdr->len) - UDP_HDR_BYTES );
	    
	  }
	  
	}//Port end
	
      }//IP src end
      
    }//IP end
    

  }//while end

}

// GatewayIP = argv[1] , FileName = argv[2] 
int main( int argc , char *argv[] ){
  
  
  //--Declarations--  
    
  //infomation for interface
  struct IFinfo if_info[MAX_IF_NUM];
  int    if_num;

  //loop variables
  int i;
  
  /*for test*/
  
  //printf("Sizeof:%ld",sizeof(struct NodeList));

  //Init NODES_STATUS
  for( i=0 ; i<MAX_NODE_NUM ; i++ ){
    InitNodeStatus( NODES_STATUS[i] );
  }
  
  //Only host
  TOTAL_BUFFER = DEFAULT_CACHE;
  LATEST_BLOCK_SEQ = 0;
  //printf("size:%ld",sizeof(struct NodeStatus));
  //exit(1);
  
  /*for test*/

  //--Arguments--
  if(argc != 3){
    printf("Usage: program <gateway-ip-address> <file name>\n");
    exit(0);
  }

  //Signal
  SetSignal(SIGINT);
  
  //--Open webm--
  
  strcpy( FILE_NAME , argv[2] );
  //InitOpenWebM();
  OpenWebM( FOP_RD );


  //--Interface--
  
  //Get infomation related to interface  
  GetIFInfo( if_info , &if_num );

  //Host
  InitHostInfo( &if_info[HOST_INTERFACE_INDEX] );
  
  //Host Status(Origin Source)
  NODES_STATUS[MY_ID].ipaddr = ntohl(HOST_INFO->addr.l);
  NODES_STATUS[MY_ID].p_ipaddr = ntohl(HOST_INFO->addr.l);
  NODES_STATUS[MY_ID].port = OS_PORT;
  NODES_STATUS[MY_ID].id = 0;
  NODES_STATUS[MY_ID].layer = 0;
  IS_SETUP = 0;

  //Init value
  NODES_LIST = NULL;
  EXCEPT_LIST = NULL;
  NODES_LIST = ListPush( NODES_LIST , NODES_STATUS[MY_ID] );
  
    
  //--Socket--

  //[arg1 , arg2] = [option,interface name]
  RawSocketBuilderWithBind( ETH_P_ALL , HOST_INFO->if_name );

  
  //--Service--
  SRC_PORT = OS_PORT;
  DaemonServiceForUDP( OS_PORT , "service_test" );

  //Assign IP and MAC
  RenewSrcIPandMAC( HOST_INFO->addr_str , HOST_INFO->mac );
  
  //--Create Receive thread--
  pthread_create( &READ_THREAD , NULL, ReadThread , NULL );

  //--Create Time thread--
  pthread_create( &TIME_THREAD , NULL, TimeThread , NULL );

  
  //--Gateway--
  GetGatewayIPandMAC(argv[1]);

  //--Send Packet--

  printf("\n----Initialized----\n\n");

  printf("\n----Start Live Streaming ----\n\n");

  UpdateWebMHeaderFromWebM();

  //Seek
  SeekABSWebM( WEBM_HEADER.total_bytes , &CURRENT_FPOS_R );

  //Init Clusters
  InitCluster();
  
  //Init Block
  InitBlock();
  InitChildBlocks();
  FIRST_CLUSTER_SEQ = 0;

  //--Send Clusters--
  
  while( UpdateClusterFromWebM( CURRENT_CLUSTER ) != WEBM_END ){

    //--Send Cluster--

    while( ReadBlock( CURRENT_BLOCK ) != CLUSTER_END ){

      //printf("Send Block ...done\n\n");

      LATEST_BLOCK_SEQ = CURRENT_BLOCK->info.seq;

      //NanoSleep(0,100000);
      //PrintBlock(CURRENT_BLOCK->info);
      // printf("LATEST:%u\n",LATEST_BLOCK_SEQ);
      
      SendMulStream();
      
      //To renew Block
      SeekForNextBlock( CURRENT_BLOCK );
      ShiftToPreBlock( CURRENT_BLOCK );
      
    }
    
    LATEST_BLOCK_SEQ = CURRENT_BLOCK->info.seq;
    //printf("LATEST:%u\n",LATEST_BLOCK_SEQ);

    //Without allocating heap memory
    SeekForNextBlock( CURRENT_BLOCK );
    RenewBlock( CURRENT_BLOCK );

    //To print current Cluster
    PrintCluster( CURRENT_CLUSTER );

    //Without allocating heap memory
    ShiftToPreCluster( CURRENT_CLUSTER );
          
  }
  
  int is_streaming;
  while( is_streaming ){

    is_streaming = SendMulStream();

  }

  printf("End");

  return 0;
  
}

