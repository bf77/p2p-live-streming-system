/*
*----------------------------------------------------------------------------
* #define (in advance)
*----------------------------------------------------------------------------
*/

#define __STDC_FORMAT_MACROS

/*
*----------------------------------------------------------------------------
* #include
*----------------------------------------------------------------------------
*/


#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <time.h>
#include <signal.h>

/*
*----------------------------------------------------------------------------
* #typedef
*----------------------------------------------------------------------------
*/


//Bytes related
typedef uint64_t u64;
typedef uint32_t u32;
typedef unsigned short u16;
typedef unsigned char  u8;


/*
*----------------------------------------------------------------------------
* #define
*----------------------------------------------------------------------------
*/

#ifdef _DEBUG_
#define DEBUG_PRINTF printf
#define DEBUG_DUMP   Dump
#else
#define DEBUG_PRINTF  1 ? (void)0 : printf
#define DEBUG_DUMP  1 ? (void)0 : Dump
#endif

//Time
#define INTARVAL_SEC 3
#define INTARVAL_MSEC 5000

//Node
#define MAX_CHILD_NUM 2

/*
*----------------------------------------------------------------------------
* #Declaration
*----------------------------------------------------------------------------
*/

extern int SIG_INT_COUNT;

//Timesamp
struct timeval TIMESTAMP;
struct timeval START_TIMESTAMP;
struct timeval TIMEPROCESS_INTARVAL;

extern int DETACH_NUM;
extern int DEPART_NUM;
extern int INC;

/*
*----------------------------------------------------------------------------
* #function()
*----------------------------------------------------------------------------
*/

//for debug
void Dump( const u8 *buf , const int bytes );
void DebugTest();
void NanoSleep( time_t  s , long n );
void GetTimestamp( struct timeval *timestamp );
void UpdateTimestamp( struct timeval *timestamp );
void DiffCurrentTime( struct timeval timestamp , struct timeval *diff_timestamp );
int IsExceedTime( struct timeval exceed_time , struct timeval tv );
void PrintTime( struct timeval timestamp );
