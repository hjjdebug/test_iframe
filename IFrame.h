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

/*���pid*/
int Getpid(char *file);
/* ����TS */
int ts_dump( char *srcfile, char *tsfile, int pid );
/* ��ȡPES */
int Ts_Pes( char *tsfile, char *pesfile, int pid );
/*��ȡES*/
int pes_es( char *pesfile, char *esfile );
/*��ȡi֡*/
int es_iframe(char *esfile, char *ifile );
/*ES�����pes*/
int es_pes(char *src, char *des,int num_pes);
/*���TS*/
int pes_ts(char *tsfile, char *pesfile,int pid);
