#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>
#include <stdio.h>
//#include <conio.h>
//#include <Winsock2.h>
//#include <ws2tcpip.h>
//#include <io.h>

/*获得pid*/
int Getpid(char *file);
/* 分离TS */
int ts_dump( char *srcfile, char *tsfile, int pid );
/* 提取PES */
int Ts_Pes( char *tsfile, char *pesfile, int pid );
/*提取ES*/
int pes_es( char *pesfile, char *esfile );
/*提取i帧*/
int es_iframe(char *esfile, char *ifile );
/*ES打包成pes*/
int es_pes(char *src, char *des,int num_pes);
/*打包TS*/
int pes_ts(char *tsfile, char *pesfile,int pid);
