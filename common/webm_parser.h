/*
*----------------------------------------------------------------------------
* #include
*----------------------------------------------------------------------------
*/

//htonl , hstons , ntohl , ntohs
#include <arpa/inet.h>
#include "wrapper.h"

/*
*----------------------------------------------------------------------------
* #define
*----------------------------------------------------------------------------
*/

#define EBML_ID         0x1a45dfa3
#define VOID_ID         0xec
#define SEGMENT_ID      0x18538067
#define SEEKHEAD_ID     0x114d9b74
#define TRACKS_ID       0x1654ae6b
#define INFO_ID         0x1549a966
#define CLUSTER_ID      0x1f43b675
#define SIMPLEBLOCK_ID  0xa3
#define TIMECODE_ID     0xe7
#define CUE_ID          0x1c53bb6b

#define CLUSTER_ID_BYTES     4
#define CLUSTER_HEADER_BYTES 12

#ifdef _MAIN_
#define MAIN_PRINTF printf
#else
#define MAIN_PRINTF 1 ? (void)0 : printf
#endif

#ifdef _SIMPLEBLOCK_
#define S_PRINTF printf
#else
#define S_PRINTF 1 ? (void)0 : printf
#endif

#ifdef _CLUSTER_
#define C_PRINTF printf
#else
#define C_PRINTF 1 ? (void)0 : printf
#endif

//Only piece
#define STATUS_PIECE       1
//Only simple block
#define STATUS_SIMPLEBLOCK 2
//Piece and simple block
#define STATUS_BLOCK       3
//Unknown status
#define STATUS_UNKNOWN     4
//Initalized status
#define STATUS_INIT        5

//File
#define FIRST_FILE_SIZE    2500000

//BlockInfo
#define BLOCKINFO_BYTES    18

//Clusters
#define MAX_CLUSTER_NUM    20

//Block
#define MAX_BLOCK_BYTES    1000
#define FIRST_BLOCK_SEQ 1

//Blocks
#define MAX_BLOCKS_NUM     1

//For String
#define DEFAULT_CHAR_NUM   30

//GetNextBlockByBytes
#define CLUSTER_END        2
#define ONLY_TIMECODE      3

//WebM End
#define WEBM_END           2

//File Options
#define FOP_WRRD           "wb+"
#define FOP_RDWR           "rb+"
#define FOP_RD             "rb"

/*
*----------------------------------------------------------------------------
* #structure
*----------------------------------------------------------------------------
*/


struct WebMHeader{

  u16 total_bytes;
  u8 *data;

};

struct Chank{
 
  u32 id;
  u16 id_bytes;
  u16 data_header_position;
  u64 data_bytes;
  u64 chank_bytes;
  
};

struct Cluster{

  struct Chank info;
  __off64_t pos;
  u32 seq;
  u32 option;

  struct Cluster *prev;
  struct Cluster *next;

};

struct BlockInfo{

  u32 pos;//The position form the cluster
  u32 seq;//The seq from the cluster
  u16 bytes;//Bytes of the block
  u16 total_piece_bytes;//The total bytes of the simple block
  u16 cluster_seq;//The seq of the clusters
  u8 total_pieces;//Total pieces that consists of the simple block
  u8 piece_seq;//The seq of pieces
  u8 status;//piece or block or simple block 
  u8 layer;//layer HOST == 0

  u16 padding;

};

/*
 *=====================================================*
 * ----------------- BlockInfo Packet------------------*
 *=====================================================*
 *-----------------------------------------------------*
 |                      position                       |
 *-----------------------------------------------------*
 |                         seq                         |
 *-----------------------------------------------------*
 |          bytes           |     total piece bytes    | 
 *-----------------------------------------------------*
 |        clsuter_seq       |total_pieces|  piece_seq  | 
 *-----------------------------------------------------*
 |    status    |                padding               |
 *-----------------------------------------------------*
*/


//if no piece , piece_bytes and total_piece is 0
struct Block{

  struct BlockInfo info;
  struct Block *prev;
  struct Block *next;

};

/*
*----------------------------------------------------------------------------
* #Declaration
*----------------------------------------------------------------------------
*/


//File related
char FILE_NAME[DEFAULT_CHAR_NUM];
extern int FD;
FILE *FP;
FILE *FILE_PTR_WRITE;
fpos_t CURRENT_FPOS_W;
fpos_t CURRENT_FPOS_R;
extern __off64_t CURRENT_OFFSET;
extern size_t CURRENT_READ_BYTES;

//
__off64_t TOTAL_FILE_BYTES;
struct WebMHeader WEBM_HEADER;
extern u8 FOOTER[CLUSTER_HEADER_BYTES];

//Cluster related
//struct Cluster *CLUSTER[MAX_CLUSTER_NUM];
struct Cluster *CURRENT_CLUSTER;
extern int IS_FIRST_CLUSTER;
u32 FIRST_CLUSTER_SEQ;

//Block related
struct Block *CURRENT_BLOCK;
extern int CURRENT_BLOCK_NUM;
extern u64 TOTAL_BLOCK_NUM;
struct Block *CURRENT_CHILD_BLOCK[MAX_CHILD_NUM];
u32 LATEST_BLOCK_SEQ;
u64 TIMECODE_OFFSET;
u64 NODE_TIMECODE_OFFSET;
int IS_GET_TIMECODE_OFFSET;

//BlockInfo related
struct BlockInfo CURRENT_BLOCKINFO[MAX_BLOCKS_NUM];
u8 BLOCKINFO_START[BLOCKINFO_BYTES];
u8 BLOCKINFO_END[BLOCKINFO_BYTES];

/*
*----------------------------------------------------------------------------
* #function()
*----------------------------------------------------------------------------
*/


//Chank related
u32 GetEBMLID( const u8 *buf , u16 *size_bytes_ptr );
u64 GetDataBytes( const u8 *buf , u16 *size_bytes_ptr );
struct Chank GetChank( const u8 *buf );
int GetChankByBytes( struct Block *block , const u8 *buf , const u16 req );
void PrintChank( const struct Chank chank );

// WebM related
void WriteWebMHeader( const u8 *buf , const u16 bytes );
void UpdateWebMHeaderFromWebM();
void AttachHeader( u8 *file_buf , const u8 *recv_data );
void AttachFooter( u8 *file_buf , const struct Cluster cluster );
void AttachNullToCluster( u8 *file_buf , const struct Cluster cluster );

//Block related
void InitBlock();
void ShiftToPreBlock( struct Block *block );
void RenewBlock( struct Block *block );
void RenewBlockInfos();
int GetNextBlockByBytes( struct Block *block , const u16 req , const u8 *file_buf );
void PrintBlock( const struct BlockInfo info );
void CopyBytesToBlockInfo( struct BlockInfo *info , const u8 *buf );
void CopyBlockInfoToBytes( u8 *buf , struct BlockInfo info );
void AssignBlockInfoByPiece( struct Block *block , const u8 *buf , const u16 req ,const int flag );
void AssignBlockInfoBySB( struct BlockInfo *info );
void AssembleBlock( u8 *data , const u16 bytes );
int IsOldBlockSeq( struct BlockInfo info );
void ChangeTimecode( u8 *data );

//Block and Cluster related
void InsertBlockToCluster( const u8 *recv_data , const u16 bytes );

//Cluster related
void InitCluster();
void ShiftToPreCluster( struct Cluster *cluster );
void RenewClusterForRead( struct Cluster *cluster , const u8 *buf );
void RenewClusterForWrite( struct Cluster *cluster );
void WriteClusterHeaderInWebM( u8 *file_buf );
void UpdateClusterToWebM( struct Cluster *cluster , const struct BlockInfo *info , const int count );
int UpdateClusterFromWebM( struct Cluster *cluster );
void UpdateClusterDataBytesToWebM( const struct Cluster cluster );
int CheckClusterSeq();
void PrintCluster( struct Cluster *cluster );

//Analyze Chank
int AnalyzeChankToWebMHeader( const u8 *buf );

//File
__off64_t UpdateOffsetWebM( fpos_t *fpos );
void SeekABSWebM( __off64_t offset , fpos_t *fpos );
void SeekRLTWebM( __off64_t offset , fpos_t *fpos );
void SeekForNextBlock( struct Block *block );
void SearchWebM( u8 *file_buf , const size_t offset , const __off64_t read_bytes );
__off64_t CalOffset( struct BlockInfo info );
__off64_t SeekABSForNextBlock( struct Block *block );
int CheckWebMSize( const int sleep_sec , const long sleep_nano_sec );
void InitOpenWebM();
void CreateWebM();
void OpenWebM( const char *option );
void ImportWebM( u8 *file_buf , const size_t bytes );
void ExportWebM( const u8 *file_buf , const size_t bytes );
void CloseWebM();
void UpdateWebM( fpos_t *fpos , const char *option );
