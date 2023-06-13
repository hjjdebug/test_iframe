#include "IFrame.h"

int main(int argc, char *argv[])
{
	if(argc==1)
	{
		printf("Usage:%s <file>\n",argv[0]);
		return 1;
	}
    int num_pes=0;
	int video_pid;            
	FILE *fp;   	
	fp = fopen(argv[1],"rb");
    if(fp==NULL)
		printf("File open error!");
	video_pid=Getpid(argv[1]);
	if(video_pid==0)
	{    
		printf("获取video_pid失败！\n");
	    exit(0);
	}

	printf("Video_pid=%d\n",video_pid);
 
	ts_dump(argv[1],"test1.ts",video_pid);
  
	Ts_Pes("test1.ts","test1.pes",video_pid);
   
	num_pes=pes_es("test1.pes","test1.es");
  
	es_iframe( "test1.es", "test1.i" );
   
	es_pes("test1.es", "test2.pes",num_pes);
  
	pes_ts("test2.pes", "test2.ts",video_pid);
   
	fclose(fp);	
	return 0;
}
