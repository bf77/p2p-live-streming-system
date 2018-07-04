/*
*----------------------------------------------------------------------------
* #include
*----------------------------------------------------------------------------
*/


#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <netpacket/packet.h>
#include <linux/if_ether.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <pthread.h>
#include <netdb.h>
#include "wrapper.h"
#include "webm_parser.h"

/*
*----------------------------------------------------------------------------
* #define
*----------------------------------------------------------------------------
*/
#define OS_PORT 12345

#define SRC_UNICAST    0
#define DST_UNICAST    1
#define SRC_MULTICAST  2
#define DST_MULTICAST  3


/* Ethernet 10/100Mbps.  */
#define ARPHRD_ETHER 1

//IP
#define IP_LENGTH 4
#define MAX_IP_STR_LENGTH 16
#define MAX_IF_NUM 10

//MTU + EthernetHeader(14bytes)
#define MAX_PACKET_SIZE 1514

//Interface
#define MAX_IF_STR_LENGTH    12
#define HOST_INTERFACE_INDEX 1

//Header
#define DATA_HDR_BYTES  1
#define UDP_HDR_BYTES   8
#define IP_HDR_BYTES    20
#define ETHER_HDR_BYTES 14

//Protocol
#define SETUP_PROTO 0
#define BLOCK_PROTO 1
#define INITIAL_STATUS_PROTO 2
#define ADVERTISE_PROTO 3
#define CONNECT_PROTO 4
#define DEPART_PROTO 5
#define DETACH_PROTO 6
#define ACCEPT_PROTO 7
#define UNACCEPT_PROTO 8
#define REQ_LIST_PROTO 9
#define RES_LIST_PROTO 10
#define ID_PROTO 11
#define KEEPALIVE_PROTO 12
#define KEEPALIVERES_PROTO 13

//Node
#define MAX_NODE_NUM 4
//#define MAX_CHILD_NUM 2 <- wrapper.h
#define CHILD_ID_1 0
#define CHILD_ID_2 1
//#define CHILD_ID_3 2
#define PARENT_ID 2
#define TMP_ID 3
#define MY_ID 3 //For NODES_STATUS
#define NODE_STATUS_BYTES 30
#define MAX_LIST_NUM 50
#define ORIGIN_ID 0

//Cache
#define DEFAULT_CACHE 200
#define BUFFERING_MBPS 0.01 //Mbps

//Time
#define KEEPALIVE_SEC 5
#define KEEPALIVE_MSEC 0
#define LISTUPDATE_SEC 5
#define LISTUPDATE_MSEC 0
#define DEPART_SEC 5
#define DEPART_MSEC 0
#define PLAYED_SEC 1
#define PLAYED_MSEC 0

/*
*----------------------------------------------------------------------------
* #structure
*----------------------------------------------------------------------------
*/


//for l4 checksum
struct PseudoIP_header{

  u32 src_addr;
  u32 dst_addr;
  u8 zeroes;
  u8 protocol;
  u16 protocol_len;//header of protocol + datalen
  
};


//ether_header -> iphdr -> l4hdr(tcphdr,udphdr) -> datahdr
struct datahdr{

  u8 protocol;
   
};

union ip_addr{

  u32 l;
  u8 array[4];

};

struct IFinfo{

  u8  if_name[MAX_IF_STR_LENGTH];
  u8  mac[ETH_ALEN];
  u8  addr_str[MAX_IP_STR_LENGTH];
  union ip_addr addr;
  u8 padding[10];

};


struct arphdr{

  struct ether_header ethhdr;

  u16 hw_type;
  u16 pro_type;
  u8  hw_length;
  u8  pro_length;
  u16 operation;

  u8 src_mac[ETH_ALEN];
  u8 src_ip[IP_LENGTH];
  u8 dst_mac[ETH_ALEN];
  u8 dst_ip[IP_LENGTH];

};

//No padding
struct NodeStatus{
  
  //BlockInfo 18bytes
  u32 pos;//The position form the cluster
  u32 seq;//The seq from the cluster
  u16 bytes;//Bytes of the block
  u16 total_piece_bytes;//The total bytes of the simple block
  u16 cluster_seq;//The seq of the clusters
  u8 total_pieces;//Total pieces that consists of the simple block
  u8 piece_seq;//The seq of pieces
  u8 status;//piece or block or simple block 
  u8 layer;//attribute
  
  //12bytes
  u16 id;//Limit 65565 Node
  u32 ipaddr;
  u32 p_ipaddr;
  u16 port;

  //2bytes
  u16 padding16;

  //16bytes
  struct timeval timestamp;

};

//No padding
struct NodeList{

  struct NodeStatus status;
  struct NodeList *next;

};

struct ExceptList{

  u16 id;
  struct ExceptList *next;

};

/*
*----------------------------------------------------------------------------
* #Declaration
*----------------------------------------------------------------------------
*/


//Information about current packet
u8 SRC_MAC[ETH_ALEN];
u8 DST_MAC[MAX_NODE_NUM][ETH_ALEN];
u32 SRC_IP;
u32 DST_IP[MAX_NODE_NUM];
u16 SRC_PORT;
u16 DST_PORT[MAX_NODE_NUM];

//Setup
extern int IS_SETUP;

//Constant values
extern u8 BROADCAST_MAC[ETH_ALEN];
extern u8 ZEROS_MAC[ETH_ALEN];


//The bytes of data in packet
extern u16 MAX_DATA_BYTES;
//u16 DATA_BYTES;


//Auto detected HOST interface infomation
struct IFinfo *HOST_INFO;


//Gateway
union ip_addr GATEWAY_IP;
u8 GATEWAY_MAC[ETH_ALEN];
u8 GATEWAY_IP_STR[MAX_IP_STR_LENGTH];
extern int IS_GATEWAY_MAC_GET;


//Socket descriptor
int SD;

//thread
pthread_t READ_THREAD;
pthread_t TIME_THREAD;

//WebM FILE SIZE
extern long TOTAL_FILE_BYTES;

//Node
extern int CURRENT_CHILD_NUM;
extern u8 CONNECTED_NODES;
extern u8 IS_BEGIN_STREAMING;
extern u8 IS_BEGIN_PLAYING;
struct NodeStatus NODES_STATUS[MAX_NODE_NUM];
struct NodeList *NODES_LIST;
struct ExceptList *EXCEPT_LIST;
extern u8 IS_RETRY;

//Cache
//Cache is the number of Block IDs.
extern u32 CACHE_TLV;
extern long long int CURRENT_CACHE;
extern u64 TOTAL_BUFFER;
extern u64 PLAYED_BUFFER;
extern u64 BUFFER_DT_SEC;

//ID
extern u16 CURRENT_NODE_ID;

//Time
struct timeval SETUP_TIMESTAMP;
struct timeval KEEPALIVE_TLV_INTARVAL;
struct timeval LISTUPDATE_TLV_INTARVAL;
struct timeval LISTUPDATE_TIMESTAMP;
struct timeval DEPART_TIMESTAMP;
struct timeval DEPART_INTARVAL;
struct timeval RETRY_TIMESTAMP;
struct timeval PLAYED_TIMESTAMP;
struct timeval PLAYED_INTARVAL;

/*
*----------------------------------------------------------------------------
* #function()
*----------------------------------------------------------------------------
*/


//Socket
void RawSocketBuilderWithBind( const int option , const u8 *if_name );

//Service
void DaemonServiceForUDP( const int port , const u8 *service_name );

//Checksum
u32 CalSumEach16bit( u16 *buf , int size );
u16 CalIPChecksum( u16 *buf , int size );
u16 CalL4Checksum( u16 *buf , int size , u16 *pseudo_iphdr );

//Packet
void PacketGenerate( u8 *packet , const u8 *send_data , const u16 data_bytes , const int dst_id );

//Interface related
void GetIFInfo( struct IFinfo *if_info , int *if_num );

//ARP
int ARPRequest( const u8 *src_mac , const u8 *src_ip , const u8 *dst_mac , const u8 *dst_ip );

//Gateway
void GetGatewayIPandMAC( const char *gw );

//Host
void InitHostInfo( struct IFinfo *info );

//For sending
void RenewSrcIPandMAC( const char *ip_str , const u8 *src_mac );
void RenewDstIPandMAC( const char *ip_str , const u16 port , const u8 *dst_mac , int dst_id );

//Random
int GenerateRndID( const int init_val , const int range );

//Send
u32 GtoPIP( struct NodeStatus nd_status );
void SendPacket( const u8 *data , const u16 data_bytes , const int dst_id );
void SendSetup();
void SendWebMHeader( const int dst_id );
int ReadBlock( struct Block *block );
void SendEndStatus( const int dst_id );
void SendID( const u32 ipaddr , const u16 port );
void SendInitialStatus( const int dst_id );
void SendAdvertise( const int dst_id , const int ad_id );
void SendConnect( const int dst_id );
void SendDepart();
void SendDetach( const int dst_id , const u16 cluster_seq );
void SendAccept( const int dst_id );
void SendUnaccept( const int dst_id );
void SendReqList( const int dst_id );
void SendResList( const int dst_id );
void SendKeepAlive( const int dst_id );
void SendKeepAliveRes( const int dst_id );
int SendCacheBlock( struct Block *block , const int dst_id );
int SendMulStream();

//Child
int IsDuplicatedChild( u16 id );
int CheckChildNum();

//List
int IsListContains( const u16 id );
int ListSize( struct NodeList *node_list );
u8 GetCloestLayer( struct NodeList *node_list );
struct NodeList *GetPriorList( struct NodeList *node_list );
struct NodeList *GetList( struct NodeList *node_list , u16 id );
struct NodeList *GetListIndex( struct NodeList *node_list , int index );
struct NodeList *ListPush( struct NodeList *node_list, struct NodeStatus node_status );
struct NodeList *ListUpdate( struct NodeList *node_list, struct NodeStatus node_status );
int IsExceptListContains( struct ExceptList *except_list , u16 id );
struct ExceptList *ExceptListPush( struct ExceptList *except_list, u16 id );
void FreeExceptList( struct ExceptList *except_list );
void PrintList( struct NodeList *node_list );
void PrintExceptList( struct ExceptList *except_list );

//Node
void NodeSetup( u8 *data , const u16 data_bytes , const u32 ipaddr , const u16 port );
void NodeID( u8 *data );
void NodeInitialStatus( u8 *data , const u16 data_bytes );
void NodeAdvertise( u8 *data , const u16 data_bytes );
void NodeConnect( u8 *data );
void NodeDepart( u8 *data );
void NodeDetach( u8 *data );
void NodeAccept( u8 *data );
void NodeUnaccept();
void NodeReqList( u8 *data );
void NodeResList( u8 *data , const u16 data_bytes );
void NodeKeepAlive( u8 *data );
void NodeKeepAliveRes( u8 *data );
void NodeRetry();
void PrintNodeStatus( const struct NodeStatus node_status);

//NodeStatus
void InitNodeStatus( struct NodeStatus node_status );
void CopyBytesToNodeStatus( struct NodeStatus *node_status , u8 *data );
void CopyNodeStatusToBytes( u8 *data , struct NodeStatus node_status );

//Signal
void SetSignal( int signum );
void SignalHandler( int signum );

//Thread
extern void Receive();
void *ReadThread( void *args );
void *TimeThread( void *args );
void CalPlayedBuffer();
void TimeProcess();
