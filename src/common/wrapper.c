/*
*----------------------------------------------------------------------------
* #include
*----------------------------------------------------------------------------
*/


#include "wrapper.h"

int INC = 0;
int SIG_INT_COUNT = 0;

int DETACH_NUM = 0;
int DEPART_NUM = 0;

/*
*----------------------------------------------------------------------------
* #function()
*----------------------------------------------------------------------------
*/


void Dump(const  u8 *buf , const int bytes ){

  int i;
  for( i=0 ; i<bytes ; i++ ){
    printf("0x%02x ",buf[i]);
  }
  printf("\n");
  
}

void DebugTest(){

  printf("test:%d\n\n",INC);
  INC++;

}

void NanoSleep( time_t  s , long n ){

  struct timespec ts;
  
  ts.tv_sec = s;

  ts.tv_nsec = n;

  nanosleep( &ts , NULL );
}

void GetTimestamp( struct timeval *timestamp ){

  gettimeofday(timestamp,NULL);
 
}

void UpdateTimestamp( struct timeval *timestamp ){

  GetTimestamp( timestamp );

  //Sec
  timestamp->tv_sec = timestamp->tv_sec - START_TIMESTAMP.tv_sec;

  //Mircro Sec
  timestamp->tv_usec = timestamp->tv_usec - START_TIMESTAMP.tv_usec;

  if( timestamp->tv_usec < 0 ){

    timestamp->tv_sec -= 1;
    timestamp->tv_usec += 100000;

  }
  
}

void DiffCurrentTime( struct timeval timestamp , struct timeval *diff_timestamp ){

  struct timeval current_timestamp;
  
  UpdateTimestamp( &current_timestamp );

  diff_timestamp->tv_sec = current_timestamp.tv_sec - timestamp.tv_sec;
  diff_timestamp->tv_usec = current_timestamp.tv_usec - timestamp.tv_usec;

  //printf("Diff TIme \n");
  //PrintTime( *diff_timestamp );

}

int IsExceedTime( struct timeval exceed_time , struct timeval tv ){

  int ret = 0;

  if( exceed_time.tv_sec > tv.tv_sec ){

    ret = 1;

  }else if( exceed_time.tv_sec == tv.tv_sec && exceed_time.tv_usec > tv.tv_usec ){

    ret = 1;

  }

  return ret;

}

void PrintTime( struct timeval timestamp ){

  printf("\nCurrent Sec: %ld",timestamp.tv_sec );
  printf(" ms: %ld\n",timestamp.tv_usec );

}
