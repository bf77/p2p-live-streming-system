#include "../common/p2p.h"



void Receive(){

  //Loop
  int i;

  //Initialized
  for( i = 0; i < BLOCKINFO_BYTES ; i++ ){

    BLOCKINFO_START[i] = 0x0;
    BLOCKINFO_END[i] = 0xff;
    
  }
  
  while( 1 ){

    u8 packet[MAX_PACKET_SIZE];
    
    printf("Reading...\n");

    //receive
    read( SD , packet , MAX_PACKET_SIZE );
    
    //init ether header
    struct ether_header *ethhdr;
    ethhdr = (struct ether_header *)packet;

    
    //ARP
    if( ethhdr->ether_type == ntohs( ETH_P_ARP ) ){

      printf("Receiving ARP packet\n");

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

      iphdr = (struct iphdr *)( (u8 *)ethhdr + sizeof( struct ether_header ) );
      udphdr = (struct udphdr *)( (u8 *)iphdr + sizeof( struct iphdr ) );
      datahdr = (struct datahdr *)( (u8 *)udphdr + sizeof( struct udphdr ) );
      data = (u8 *)( (u8 *)datahdr + sizeof( struct datahdr ) );

      //Addressed to myself  
      if( iphdr->daddr == SRC_IP ){
	
	///printf("Received a IP packet\n");
	
	
	if( ntohs(udphdr->dest) == SRC_PORT ){
	  
	  //--Setup--	  
	  if( datahdr->protocol == SETUP_PROTO ){
	    
	    printf("Receiving a Setup packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);
	    NodeSetup( data , data_bytes , iphdr->saddr , udphdr->source );
	    
	  }//--ID--
	  else if( datahdr->protocol == ID_PROTO ){

	    //Only node
	    printf("Receiving a ID packet\n");
	    NodeID( data );
	    
	  }//--Initial Status--
	  else if( datahdr->protocol == INITIAL_STATUS_PROTO ){
	    
	    printf("Receiving a Initial Status packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);
	    
	    NodeInitialStatus( data , data_bytes );

	    //For experimentation 
	    //IS_BEGIN_STREAMING = 1;
	    
	  }//--Advertise--
	  else if( datahdr->protocol == ADVERTISE_PROTO ){

	    printf("Receiving a Advertise packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);
	    NodeAdvertise( data , data_bytes );

	  }//--Block--
	  else if( datahdr->protocol == BLOCK_PROTO ){

	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("Receiving a Block packet\n");
	    printf("The data bytes received is %d\n",data_bytes);
	    
	    AssembleBlock( data , data_bytes );
	    	    
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

	    printf("Receiving a Request List packet\n");	    
	    NodeReqList( data );

	  }//--Responce List--
	  else if( datahdr->protocol == RES_LIST_PROTO ){

	    printf("Receiving a Responce List packet\n");
	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("The data bytes received is %d\n",data_bytes);

	    NodeResList( data , data_bytes );

	  }//--Accept--
	  else if( datahdr->protocol == ACCEPT_PROTO ){

	    printf("Receiving a Accept packet\n");
	    NodeAccept( data );

	  }//--Unaccept--
	  else if( datahdr->protocol == UNACCEPT_PROTO ){

	    printf("Receiving a Unaccept packet\n");
	    NodeUnaccept( data );

	  }//--KeepAlive--
	  else if( datahdr->protocol == KEEPALIVE_PROTO ){

	    printf("Receiving a KeepAlive packet\n");
	    NodeKeepAlive( data );

	  }//--KeepAliveRes--
	  else if( datahdr->protocol == KEEPALIVERES_PROTO ){

	    printf("Receiving a KeepAliveRes packet\n");
	    NodeKeepAliveRes( data );

	  }//--Unknown
	  else{
	    
	    printf("Receiving a Unknown packet\n");	   
	    Dump( data , htons(udphdr->len) - UDP_HDR_BYTES );
	    
	  }

	}//Port end
	
      }//IP src end
      
    }//IP end

    //--Process SendBlock--
    if( IS_BEGIN_PLAYING ){
     
      SendMulStream();

    }//end SendCacheBlock

    
  }//while end
  
}

// GatewayIP = argv[1] , FileName = argv[2] 
int main( int argc , char *argv[] ){

  //Declarations
  
  //infomation for interface
  struct IFinfo if_info[MAX_IF_NUM];
  int    if_num;

  //loop variables
  int i;

  //thread
  //pthread_t thread;

  //--Arguments--
  if( argc != 5 ){
    printf("Usage: program < gateway-ip-address > <originsource-global-ip-address> <src-port> < file-name >\n");
    exit(0);
  }

  //--Open webm--
  
  strcpy( FILE_NAME , argv[4] );

  //Create WebM
  CreateWebM();
  OpenWebM( FOP_RDWR );

  //--Interface--
  
  //Get infomation related to interface  
  GetIFInfo( if_info , &if_num );

  //Host
  InitHostInfo( &if_info[HOST_INTERFACE_INDEX] );

  
  //--Socket--

  //[arg1 , arg2] = [option,interface name]
  RawSocketBuilderWithBind( ETH_P_ALL , HOST_INFO->if_name );

  
  //--Service--
  SRC_PORT = atoi(argv[3]);
  DaemonServiceForUDP( SRC_PORT , "service_test" );

  //Assign Source IP and MAC
  RenewSrcIPandMAC( HOST_INFO->addr_str , HOST_INFO->mac );
  
  //--Create Receive thread--
  pthread_create( &READ_THREAD , NULL, ReadThread , NULL );
  pthread_create( &TIME_THREAD , NULL, TimeThread , NULL );

  
  //--Gateway--
  GetGatewayIPandMAC(argv[1]);

  //Assign Source IP and MAC
  RenewDstIPandMAC( argv[2] , OS_PORT , GATEWAY_MAC , PARENT_ID );
  
  //--Send Packet--
  
  printf("\n----Initialized----\n\n");

  printf("\n----Start Live Streaming ----\n\n");

  //--Init NODES_STATUS--
  for( i=0 ; i<MAX_NODE_NUM ; i++ ){
    InitNodeStatus( NODES_STATUS[i] );
  }

  //Host Status(Origin Source)
  inet_pton( AF_INET , argv[2] , (u8 *)&(NODES_STATUS[PARENT_ID].ipaddr) );
  NODES_STATUS[PARENT_ID].ipaddr = ntohl(NODES_STATUS[PARENT_ID].ipaddr);
  NODES_STATUS[PARENT_ID].port = OS_PORT;
  NODES_STATUS[PARENT_ID].id = 0;
  NODES_STATUS[PARENT_ID].layer = 0;
  NODES_STATUS[MY_ID].p_ipaddr = ntohl(HOST_INFO->addr.l);

  NODES_LIST = NULL;
  EXCEPT_LIST = NULL;
  NODES_LIST = ListPush( NODES_LIST , NODES_STATUS[PARENT_ID] );
  LATEST_BLOCK_SEQ = 0;

  //Block
  InitChildBlocks();

  //Inform myself of host
  UpdateTimestamp(&SETUP_TIMESTAMP);
  SendSetup();
  
  while(1){
    //printf("---main loop---\n");
  }

  
  /* Destruction */
  pthread_join( READ_THREAD , NULL );
  pthread_join( TIME_THREAD , NULL );
  close(SD);  

  return 0;
  
}

