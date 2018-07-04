#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOCAL_ADDR "192.168.20.11"
#define GATEWAY_ADDR "192.168.20.1"
#define SSDP_MUL_ADDR "239.255.255.250"
#define SSDP_PORT 1900
#define GET_REQUEST 1
#define SSDP_REQUEST 2
#define SOAP_REQUEST 3
#define MAX_MSG 10000
#define WIP "urn:schemas-upnp-org:service:WANIPConnection:1"
#define WPPP "urn:schemas-upnp-org:service:WANPPPConnection:1"
#define WROOT "upnp:rootdevice"

/*

  Struct sockaddr_in {
      u_char  sin_len;    
      u_char  sin_family; 
      u_short sin_port;   
      struct  in_addr sin_addr;
      char    sin_zero[8];      //ignore
   };

*/

/*   
//need to link ip to output ip and if for multicast
if( setsockopt( sd , 
		  IPPROTO_IP, 
		  IP_MULTICAST_IF,
		  &(saddr.sin_addr),
		  sizeof(saddr.sin_addr) ) < 0 ){
    
    perror("setsockopt");
    exit(1);
    
}*/


typedef struct HttpHeader{

  struct sockaddr_in *daddr; //destination addr
  char *path;                //request path
  int opt;                   //option
  
}HttpHeader;


int UDP_Init( struct sockaddr_in saddr ){

  int sd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  
  // -*bind*- ip and port locked
  if(bind(sd, (struct sockaddr *)&saddr , sizeof(saddr) ) < 0 ){
    perror("bind");
    exit(1);
  }

  return sd;
    
}

void UDP_Send( int sd, struct sockaddr_in daddr, char *msg ){


  if( sendto( sd, msg, MAX_MSG, 0, (struct sockaddr *)&daddr,sizeof(daddr) ) < 0 ){
    
    perror("udp send");
    close(sd);
    exit(1);
    
  }

  //display massage
  fprintf( stdout , "Send msg:\n %s\n" , msg );

}

char *UDP_Receive( int sd, struct sockaddr_in saddr ,char *msg ){

  //embedded by 0
  bzero(msg,MAX_MSG);
  int addr_size = sizeof( saddr );
  
  //receive
  if( recvfrom( sd , msg, MAX_MSG , 0 , (struct sockaddr *)&saddr , &addr_size ) < 0 ){
    
    perror("recvfrom");
    exit(1);
  }

  //display massage
  fprintf( stdout , "Receive msg: %s\n" , msg );

  return msg;

}

int TCP_Init( struct sockaddr_in saddr , struct sockaddr_in daddr ){

  //tcp connection
  int sd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  
  //need to connect for tcp
  connect(sd, (struct sockaddr *)&daddr,sizeof(daddr));

  return sd;
  
}

void TCP_Send( int sd, char *msg ){
  
  if( send( sd, msg, MAX_MSG , 0 ) < 0 ){

    perror("tcp send");
    close(sd);
    exit(1);
    
  }

  //display massage
  fprintf( stdout , "Send msg:\n %s\n" , msg );

}

char *TCP_Receive( int sd, char *msg ){

  //embedded by 0
  bzero(msg,MAX_MSG);
    
  //receive
  if( recv( sd , msg , MAX_MSG , MSG_WAITALL ) < 0 ){
    
    perror("recv");
    exit(1);
  }

  //display massage
  fprintf( stdout , "Receive msg:\n%s\n" , msg );

  return msg;

}

 /*
     ---handling ip function---    

    inet_addr // str to binary
    inet_aton(inet_aton(const char *cp, struct in_addr *inp) //recommend

 */

struct sockaddr_in *AddrInit( struct sockaddr_in *addr , int opt ,char *ip,int port){

  //embedded by 0
  bzero( addr , sizeof(struct sockaddr_in) );
  
  if( opt == 0b00 ){//unicast destination address

    //set ip
    inet_aton( ip , &(addr->sin_addr) );

    //set port
    addr->sin_port = htons( port );
    
    //IP --addoress family---
    addr->sin_family = AF_INET;

  }
  else if( opt == 0b10){//unicast source address

    
    //set ip
    inet_aton( ip , &(addr->sin_addr) );
    //set port
    addr->sin_port = htons( port );  
    //addoress family
    addr->sin_family = AF_INET;
    

  }
  else if( opt == 0b01 ){//multicast destination address

    //set ip
    inet_aton( ip , &(addr->sin_addr) );
    //set port
    addr->sin_port = htons( port );
    //address family
    addr->sin_family = AF_INET;

  }
  else if( opt == 0b11 ){//multicast source address

    //set ip
    inet_aton( ip , &(addr->sin_addr) );
    //set port
    addr->sin_port = htons( port );
    //address family
    addr->sin_family = AF_INET;
    
  }
  else{
    
    printf("exception error");
    exit(1);
    
  }//end

  return addr;

}

char *SSDP_Request( char *msg, HttpHeader hhdr ){

  //inititialize
  bzero(msg,MAX_MSG);

  //strings
  char **http_req;
  //lines
  int line = 5;
  //tmp
  int i  =  0;

  /* ---CarriageReturn(0x0a) and LineFeed(0x0d)--- */
  char *rn = "\r\n";
  
  //secure memory for http protocols
  http_req = malloc(sizeof(char *)*line);
  
  for( i = 0; i < line ; i++ )
    http_req[i] = (char *)malloc(sizeof(char)*128);

  i = 0;
  //ssdp request
  sprintf( http_req[i++] , "M-SEARCH * HTTP/1.1" );

  /* ---join multicast group--- */
  //http_req[i] = malloc(100);
  sprintf( http_req[i++] , "HOST: %s:%d" , inet_ntoa(hhdr.daddr->sin_addr) , ntohs(hhdr.daddr->sin_port) );
  
  //search router
  sprintf( http_req[i++] , "MAN: \"ssdp:discover\"" );
  
  //resposce time every second
  sprintf( http_req[i++] , "MX: 3" );

  //statement for gettting location


  if( hhdr.opt == 1)
    sprintf(http_req[i],"ST: %s",WIP);
  else if( hhdr.opt == 2 )
    sprintf(http_req[i],"ST: %s",WPPP);
  else if( hhdr.opt == 3 )
    sprintf(http_req[i],"ST: %s",WROOT);
    
  //combine strings
  for( i = 0 ; i < line ; i++ ){
    msg = strcat( msg , http_req[i] );
    msg = strcat( msg , rn );
  }

  msg = strcat( msg , rn );

  //release memory
  for( i = 0 ; i < line ; i++ )
    free(http_req[i]);
  free(http_req);  
  
}

void Get_Request( char *msg, HttpHeader hhdr ){

  //embedded by 0
  bzero(msg,MAX_MSG);
  
  //strings
  char **http_req;
  //lines
  int line = 3;
  //tmp
  int i  =  0;

  /* ---CarriageReturn(0x0a) and LineFeed(0x0d)--- */
  char *rn = "\r\n";
  
  http_req = malloc(sizeof(char *)*line);
  for( i = 0 ; i < line ; i++ )
    http_req[i] = (char *)malloc(sizeof(char)*128);

  i = 0;
  //get request
  sprintf( http_req[i++] , "GET %s HTTP/1.1" , hhdr.path );
  //host
  sprintf( http_req[i++] , "HOST: %s:%d" , inet_ntoa(hhdr.daddr->sin_addr) , ntohs(hhdr.daddr->sin_port) );

  //contents length
  sprintf( http_req[i++] , "CONNECTION: Close" );
  
  //combine strings
  for( i = 0 ; i < line ; i++ ){
    msg = strcat( msg , http_req[i] );
    msg = strcat( msg , rn );
  }
  //end rn
  msg = strcat( msg , rn );

  //release memory
  for( i = 0 ; i < line ; i++)
    free(http_req[i]);
  free(http_req);

}

void SOAP_Request( char *msg , HttpHeader hhdr , char *wstr ){

  //inititialize
  bzero( msg , MAX_MSG );

  //content-length
  int clen  = 301;
  
  //strings
  char **http_req;
  //lines
  int line = 6;
  //tmp
  int i;

  /* ---CarriageReturn(0x0a) and LineFeed(0x0d)--- */
  char *rn = "\r\n";
  
  http_req = malloc(sizeof(char *)*line);
  for( i = 0 ; i < line ; i++ )
    http_req[i] = (char *)malloc(sizeof(char)*128);

  i = 0;
  //get request
  sprintf( http_req[i++] , "POST %s HTTP/1.1" , hhdr.path );

  //host
  sprintf( http_req[i++] , "HOST: %s:%d" , inet_ntoa(hhdr.daddr->sin_addr), ntohs(hhdr.daddr->sin_port) );

  //contents length
  sprintf( http_req[i++] , "CONTENT-LENGTH: %d" , clen );

  
  sprintf( http_req[i++] , "CONTENT-TYPE: text/xml; charset=\"utf-8\"" );

  //contents length
  sprintf( http_req[i++] , "CONNECTION: Close" );


  if( hhdr.opt == 1 ){

    sprintf( http_req[i++] , "SOAPACTION: %s#GetGenericPortMappingEntry",wstr);
    
  }
  else if( hhdr.opt == 2 ){

    sprintf( http_req[i++] , "SOAPACTION: %s#GetExternalIPAddress",wstr);
    
  }

  //combine strings
  for( i = 0 ; i < line ; i++ ){
    msg = strcat( msg , http_req[i] );
    msg = strcat( msg , rn );
  }
  //end need 1 line space
  msg = strcat( msg , rn );

  //secure memory for xml
  char *xml;
  xml = (char *)malloc(MAX_MSG);
  
  //xml
  if( hhdr.opt == 1){

    sprintf( xml , "<?xml version=\"1.0\"?><SOAP-ENV:Envelope xmlns:SOAP-ENV:=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><SOAP-ENV:Body><m:GetGenericPortMappingEntry xmlns:m=\"%s\"><NewPortMappingIndex>0</NewPortMappingIndex></m:GetGenericPortMappingEntry></SOAP-ENV:Body></SOAP-ENV:Envelope>" , wstr );
    
    msg = strcat( msg , xml );
    
  }
  else if( hhdr.opt == 2 ){

    sprintf( xml , "<?xml version=\"1.0\"?><s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><u:GetExternalIPAddress xmlns:u=\"%s\"></u:GetExternalIPAddress></s:Body></s:Envelope>" , wstr);

    msg = strcat( msg , xml );
    
  }
  
  //release memory
  for( i = 0 ; i < line ; i++)
    free(http_req[i]);
  
  //release memory
  free(http_req);
  free(xml);
  
}

void HttpRequest( char *msg , HttpHeader hhdr , int proto , char *wstr ){
  
  if( proto == SSDP_REQUEST ){

    SSDP_Request( msg , hhdr );
      
  }
  else if( proto == GET_REQUEST ){
    
    Get_Request( msg , hhdr );
    
  }
  else if( proto == SOAP_REQUEST ){

    SOAP_Request( msg , hhdr , wstr );

  }
  else{

    printf("http request exception error\n");
    exit(1);
    
  }
  
}

void HhdrInit(HttpHeader *hhdr,struct sockaddr_in *addr, char *path, int opt){

  //embedded by 0
  bzero(hhdr,sizeof(HttpHeader));
  
  hhdr->daddr = addr;
  hhdr->path = path;
  hhdr->opt = opt;
  
}

  /*  ---make socket---
  
    AF_INET     //address family ipv4
    SOCK_DGRAM  //udp support data gram(connection less,no credibility,and so on)  
    SOCK_STREAM //tcp support

    IPPROTO_UDP //udp protocol
    IPPROTO_TCP //tcp protocol

  */

/* ---addr initialinze--- 
     
     about SocketInit functions third argument
     
     the first bit
     0 : unicast
     1 : multicast

     the second bit
     0 : destination addr
     1 : source addr     

*/


int main(){
 
  //socket descriptor d is destination , s is source
  int udp_sd,tcp_sd;
  
  //send and recive message
  char msg[MAX_MSG];

  //destaddress information
  struct sockaddr_in saddr,daddr;

  //HttpHeader initialized
  HttpHeader hhdr;
  
  /* ---descovery router by multicast---*/

  //multicast address for destination
  AddrInit( &daddr , 0b01 , SSDP_MUL_ADDR , SSDP_PORT );
  //unicast address for source
  AddrInit( &saddr , 0b10 , LOCAL_ADDR , 44444 );

  //HttpHeader Initialized
  HhdrInit( &hhdr , &daddr , "" , 1 );
  
  //SSDP request (router search)
  HttpRequest( msg , hhdr , SSDP_REQUEST , "" );

  //init
  udp_sd = UDP_Init( saddr );

  //send massage
  UDP_Send( udp_sd , daddr , msg );

  UDP_Receive( udp_sd , saddr , msg );

  close(udp_sd);
  
  /* ---get xml by unicast---*/
 
  //get request for tcp
  AddrInit( &daddr , 0b00 , GATEWAY_ADDR , 50345 );
  AddrInit( &saddr , 0b10 , LOCAL_ADDR , 51515 );

  //HttpHeader initialized
  HhdrInit( &hhdr , &daddr , "/gatedesc.xml" , 0 );

  //GET request
  HttpRequest( msg , hhdr , GET_REQUEST , "" );

  //initialized
  tcp_sd = TCP_Init( saddr , daddr );
  
  //send by tcp
  TCP_Send( tcp_sd , msg );
  
  //receive
  TCP_Receive( tcp_sd , msg );

  /* ---get infomation--- */
  
  //HttpHeader initialized
  HhdrInit( &hhdr , &daddr , "/upnp/control/WANIPConn1" , 2 );

  //SOAPACTION REQUEST
  HttpRequest( msg , hhdr , SOAP_REQUEST , WIP );
  
  //close socket
  close(tcp_sd);

  //initialized
  tcp_sd = TCP_Init( saddr , daddr );
  
  //send by tcp
  TCP_Send( tcp_sd , msg );
  
  //receive
  TCP_Receive( tcp_sd , msg );

  close(tcp_sd);

  return 0;
  
}

