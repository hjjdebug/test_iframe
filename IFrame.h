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
int getVideoPid(char *file);
/* ����TS */
int ts_dump_video( char *srcfile, char *tsfile, int pid );
/* ��ȡPES */
int ts2pes( char *tsfile, char *pesfile, int pid );
/*��ȡES*/
int pes2es( char *pesfile, char *esfile );
/*��ȡi֡*/
int es2iframe(char *esfile, char *ifile );
/*ES�����pes*/
int es2pes(char *src, char *des,int num_pes);
/*���TS*/
int pes2ts(char *tsfile, char *pesfile,int pid);
