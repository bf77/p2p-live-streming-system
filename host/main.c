#include "../common/p2p.h"


void Receive(){


  while( 1 ){

    u8 packet[MAX_PACKET_SIZE];
    int current_block_num = 0;
    
    printf("Reading...\n");

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

      printf("Receiving IP packet\n");
      
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
	
	printf("Received a IP packets\n");

	printf("Recived by Port %u\n", ntohs(udphdr->dest) );
	
	if( ntohs(udphdr->dest) == SRC_PORT ){
	  
	  //--Setup--	  
	  if( datahdr->protocol == SETUP_PROTO ){
	    
	    printf("Reciving a Setup packet\n");
	    NodeSetup( iphdr->saddr );
	    
	  }//--Block--
	  else if( datahdr->protocol == BLOCK_PROTO ){

	    printf("Reciving a Block packet\n");
	    //AssembleBlock();	    	    

	  }//--Connect--
	  else if( datahdr->protocol == CONNECT_PROTO ){

	    NodeConnect( ipaddr->saddr );

	  }//--Depart--
	  else if( datahdr->protocol == DEPART_PROTO ){

	    NodeDepart( data );

	  }//--Unaccept--
	  else if( datahdr->protocol == UNACCEPT_PROTO ){

	    NodeUnaccept();

	   
	  }//--Unknown--
	  else{

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
  

  //--Declarations--  
    
  //infomation for interface
  struct IFinfo if_info[MAX_IF_NUM];
  int    if_num;

  //loop variables
  int i;
  
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

  
  //--Socket--

  //[arg1 , arg2] = [option,interface name]
  RawSocketBuilderWithBind( ETH_P_ALL , HOST_INFO->if_name );

  
  //--Service--
  DaemonServiceForUDP( SRC_PORT , "service_test" );

  //Assign IP and MAC
  RenewSrcIPandMAC( HOST_INFO->addr_str , HOST_INFO->mac );
  
  //--Create Receive thread--
  pthread_create( &READ_THREAD , NULL, ReadThread , NULL );

  
  //--Gateway--
  GetGatewayIPandMAC(argv[1]);

  //--Send Packet--

  printf("\n----Initialized----\n\n");

  printf("\n----Start Live Streaming ----\n\n");

  UpdateWebMHeaderFromWebM();

  while(1){

    sleep(1);
    if( CURRENT_CHILD_NUM == 2 )break;
    printf("Waiting for responces from node\n\n");
    
  }

  /*
  while(1){

    sleep(1);
    printf("ok\n");

  }
  */
  //--Send WebMHeader
  //SendWebMHeader();

  //Seek
  SeekABSWebM( WEBM_HEADER.total_bytes , &CURRENT_FPOS_R );

  //Init Clusters
  InitCluster();
  
  //Init Block
  InitBlock();

  //--Send Clusters--
  
  while( UpdateClusterFromWebM( CURRENT_CLUSTER ) != WEBM_END ){

    //--Send Cluster--

    while( SendBlock( CURRENT_BLOCK ) != CLUSTER_END ){

	  printf("Send Block ...done\n\n");
	  
	  NanoSleep(0,100000);
	  
	  
	  //To renew Block
	  SeekForNextBlock( CURRENT_BLOCK );
	  ShiftToPreBlock( CURRENT_BLOCK );

    }
    
    //Without allocating heap memory
    SeekForNextBlock( CURRENT_BLOCK );
    RenewBlock( CURRENT_BLOCK );

    //To print current Cluster
    PrintCluster( CURRENT_CLUSTER );

    //    if( CURRENT_CLUSTER->seq == 11 )
    //exit(1);
    
    //Without allocating heap memory
    ShiftToPreCluster( CURRENT_CLUSTER );
          
  }
  
  //  SendEndStatus();
  /*
  printf("The number of total blocks : %d\n",TOTAL_BLOCK_NUM);
  while( 1 ){
    
    sleep(1);
    printf("Live streaming end...\n");
    
  }
   

  //--Destruction--
  pthread_join( thread , NULL );
  close(SD);  

  */

  return 0;
  
}

