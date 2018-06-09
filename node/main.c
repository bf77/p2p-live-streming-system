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

      printf("Receiving IP packet\n");
      
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
	
	printf("Received a IP packet\n");
	
	
	if( ntohs(udphdr->dest) == SRC_PORT ){
	  
	  //--Setup--	  
	  if( datahdr->protocol == SETUP_PROTO ){
	    
	    printf("Reciving a Setup packet\n");
	    NodeSetup( iphdr->saddr );
	    
	  }//--Block--
	  else if( datahdr->protocol == BLOCK_PROTO ){

	    const int data_bytes = htons(udphdr->len) - UDP_HDR_BYTES - DATA_HDR_BYTES;
	    printf("Reciving a Block packet\n");
	    printf("The data bytes received is %d\n",data_bytes);
	    
	    CheckBeginStreaming( (u8 *)data );
	    SendMulStream( (u8 *)datahdr , data_bytes + DATA_HDR_BYTES );
	    AssembleBlock( data , data_bytes );
	    
	  }//--Connect--
	  else if( datahdr->protocol == CONNECT_PROTO ){

	    NodeConnect( ipaddr->saddr );

	  }//--Depart--
	  else if( datahdr->protocol == DEPART_PROTO ){

	    NodeDepart( data );

	  }//--Unknown--
	  else if( datahdr->protocol == UNACCEPT_PROTO ){

	    NodeUnaccept();

	  }else{
	    
	    printf("Reciving a Unknown packet\n");	   
	    Dump( data , htons(udphdr->len) - UDP_HDR_BYTES );
	    
	  }

	}//Port end
	
      }//IP src end
      
    }//IP end
    
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
  pthread_t thread;

  
  //--Arguments--
  if( argc != 4 ){
    printf("Usage: program < gateway-ip-address > <host-ip-address> < file-name >\n");
    exit(0);
  }

  
  //--Open webm--
  
  strcpy( FILE_NAME , argv[3] );

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
  DaemonServiceForUDP( SRC_PORT , "service_test" );

  //Assign Source IP and MAC
  RenewSrcIPandMAC( HOST_INFO->addr_str , HOST_INFO->mac );
  
  //--Create Receive thread--
  pthread_create( &thread , NULL, ReadThread , NULL );

  
  //--Gateway--
  GetGatewayIPandMAC(argv[1]);

  //Assign Source IP and MAC
  RenewDstIPandMAC( argv[2] , GATEWAY_MAC , PARENT_ID );
  
  //--Send Packet--
  
  printf("\n----Initialized----\n\n");

  printf("\n----Start Live Streaming ----\n\n");

  //Inform myself of host 
  SendSetup();
  
  while(1){
    sleep(3);
    printf("---main loop---\n");
  }

  
  /* Destruction */
  pthread_join( thread , NULL );
  close(SD);  

  return 0;
  
}

