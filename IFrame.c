#include"IFrame.h"
unsigned char gbuf[10240*188];

int Getpid(char *file)
{	
	FILE *fp = fopen(file, "rb");  //打开文件
	if(NULL == fp )
	{
		return -1;
	}
	unsigned char buf[1024*188]={0};
	int buf_size = sizeof(buf);//大小，字节数
	unsigned char *p;
	int pid=0;
	unsigned char *payload,*pp=NULL;
	while(!feof(fp))
	{
		int read_size = fread(buf, 1, buf_size, fp ); //读取的元素个数
		p = buf;

		while(p < buf + read_size)
		{
			unsigned char adaptation_field_control = (p[3]>>4)&0x3; //判断是否有可调整字段
			payload = NULL;
			switch(adaptation_field_control) //这是针对mpeg ts的.
			{
				case 0:
				case 2:
					break;    //0为保留，2为只有调整无负载, 没有有效载荷
				case 1:
					payload = p + 4; //没有调整字段  
					pp = payload;
					break;
				case 3:
					payload = p + 5 + p[4];//净荷/
					pp = payload;
					break;
			}
			if(pp == payload) //有负载并且负载为00,00,01,e0(视频流ID:0xe0-0xef,音频流ID:0xc0-0xdf)
			{
				if( pp[0] == 0 && pp[1] == 0 && pp[2] == 0x01 && ( ( pp[3] & 0xe0 ) == 0xe0 ) )
				{
					pid = (p[1] & 0x1f) << 8 | p[2];

				}
			}
			p = p + 188;
		}
	}
	return pid;
}

//提取ts, 提取指定pid 的包 188字节到文件, ts 是188字节的流,提取了指定pid还是188字节的流
//这里的pid 是视频流的pid
int ts_dump(char *srcfile,char *tsfile,int pid)
{
	FILE *fpd,*fp;
	fp = fopen(srcfile,"rb");
	fpd = fopen(tsfile,"wb");
	if(fp==NULL || fpd == NULL)
	{
		return -1;
	}
	do{
		int rd_size = fread(gbuf, 1, sizeof(gbuf), fp);
		unsigned char *p = gbuf;

		while( p + 188 <= gbuf + rd_size )//寻找TS包的起始点，
		{
			if( p + 376 < gbuf + rd_size )//TS超过3个包长
			{
				if( *p == 0x47 && *(p+188) == 0x47 && *(p+376) == 0x47 )
					break;
			}
			else if( p + 188 < gbuf + rd_size )//TS为两个包长
			{
				if( *p == 0x47 && *(p+188) == 0x47 )
					break;
			}
			else if( *p == 0x47 )//ts为一个包长
				break;
			p++;
		}
		while( p < gbuf + rd_size)//将视频流数据提取出来，写到另一个新的文件中
		{
			unsigned short pkt_pid = ((p[1] & 0x1f)<<8) | p[2];
			if( pkt_pid == pid)
				fwrite(p,1,188,fpd);
			p += 188;
		}
	}	while(! feof( fp ));

	fclose(fp );
	fclose(fpd );
	printf("ts_dump() end\n");
	return 0;
}
//提取pes, 把该pid 下有效负载写到一个文件, 就是pes文件
//pes去掉了ts的头部及适配域信息, pcr,opcr就是适配域信息
int Ts_Pes(char *tsfile,char *pesfile,int pid)
{

	FILE *fp_d,*fp_s;
	fp_s = fopen( tsfile, "rb");
	fp_d = fopen( pesfile, "wb");
	if(fp_s == NULL || fp_d == NULL )
		return -1;
	
	int start = 0;
	int err_count = 0;
	unsigned char last_counter = 0xff;
	//初始化设置值大小
	while( ! feof( fp_s ))
	{
		int size = fread(gbuf, 1,sizeof(gbuf), fp_s);
		unsigned char *p = gbuf;

		do{
			unsigned short pkt_pid = (((p[1] & 0x1f)<<8) | p[2]); //获取pid
			unsigned char Adapcontrol = (p[3]>>4)&0x3 ;  //判断自适应区是否有可调整字段
			unsigned char counter = p[3]&0xf ; //提取连续计数器
			if( pkt_pid == pid)
			{
				unsigned char *payload = NULL;
				switch(Adapcontrol)
				{
					case 0: 
					case 2:
						break;  //0为保留，2为只有调整无负载
					case 1:
						payload = p + 4;  //无调整字段
						break;
					case 3:
						payload = p + 5 + p[4];  //pes包净荷
						break;
				}

				if((p[1] & 0x40)!= 0 )//取出有效荷载单元起始指示符，确定TS,pes数据开始，payload_unit_start_indicator
				{
					start = 1;
				}

				if(start && payload && fp_d)//往fpd写pes包数据
				{
					fwrite(payload, 1,p+188-payload, fp_d);
				}

				if( last_counter != 0xff && ((last_counter + 1)&0xf) != counter)//判断数据是否丢失
				{
					//printf("%ddata lost\n",err_count);
					err_count++;
				}
				last_counter = counter ; 
			}
			p += 188;
		}while(p<gbuf+size);
	}
	fclose( fp_s );
	fclose( fp_d );

	printf( "ts_pes() end\n" );
	return 0;
}
//pes 文件是由一系列00 00 01 e0 开始的二进制数据构成的文件
//es 是去掉了pes头部6字节并去掉了3+pes_header_data_Length个数据构成的文件
//pes->es
int pes_es( char *pesfile, char *esfile )
{
	FILE *fpd, *fp;
	fp = fopen( pesfile, "rb" );  
	fpd = fopen( esfile, "wb" );
	if( fp == NULL || fpd == NULL )
		return -1;

	int pes_num = 0;
	unsigned long remain_size = 0;
	unsigned char *p = gbuf;
	unsigned int pes_size;
	unsigned char PES_header_data_Length;

	while(1)
	{
REDO:
		if( gbuf + remain_size <= p )   // 定位P
		{
			p = gbuf;
			remain_size = 0;
		}
		else if( gbuf < p && p < gbuf + remain_size ) //调整p , 其实是不会发生的.
		{
			remain_size -= p - gbuf;
			memmove( gbuf, p, remain_size );
			p = gbuf;
		}

		if( !feof(fp) && remain_size < sizeof(gbuf) )
		{
			int rdsize = fread( gbuf+remain_size, 1, sizeof(gbuf)-remain_size, fp );
			remain_size += rdsize;
		}
		if( remain_size <= 0 )
			break;

		// 寻找PES-> HEADER: 0X000001E0 
		while( p[0] != 0 || p[1] != 0 || p[2] != 0x01 ||
				( ( p[3] & 0xe0 ) != 0xe0 ))
		{
			p++;
			if( gbuf + remain_size <= p )
				goto REDO;
		}
		// PES_packet_Length 
		pes_size = (p[4]<<8) | p[5];

		if( pes_size == 0 ) //等于0要自己计算长度
		{
			unsigned char *end = p + 6;
			while( end[0] != 0 || end[1] != 0 || end[2] != 0x01 ||
					( ( end[3] & 0xe0 ) != 0xe0 ))
			{
				if( gbuf + remain_size <= end )
				{
					if( feof(fp) )
						break;
					goto REDO;
				}
				end++;   //找到下一个pes的起始点
			}
			pes_size = end - p - 6;  //下一个pes 的起始点减去p再减6既是length
		}
		if( gbuf + remain_size < p + 6 + pes_size )
		{
			if( feof(fp) )
				break;
			continue; //当需要读数据时,继续读
		}
		p += 6;

		p++;
//		PES_extension_flag = (*p)&0x1;
		p++;
		PES_header_data_Length = *p;
		p++; // 到此加了3个bytes

		unsigned char *payload = p + PES_header_data_Length;//负载

		if( fpd )
		{
			fwrite( payload, 1, pes_size - 3 - PES_header_data_Length, fpd );//将负载ES写入文件

		}
		pes_num++;   //PES的数目+1
		p += pes_size - 3;

		payload = p;
		remain_size -= p - gbuf;
		memmove( gbuf, p, remain_size );
		p = gbuf;
	}

	fclose( fp );
	fclose( fpd );
	printf("pes个数为%d\n",pes_num);
	printf("pes_es_END\n");
	return pes_num;
} 
//es->I
//es 是由00 00 01 开始的二进制数据构成的文件,后面跟b3表示刷新
//es_buf[5]&0x38>>3表示类型,IPB帧
int es_iframe(char *esfile, char *ifile )
{
	// 打开ES文件 和 IFRAME文件 
	FILE *fd_es = fopen( esfile, "rb" );
	FILE *fd_i = fopen( ifile, "wb" );
	if( NULL == fd_es || NULL == fd_i )
	{
		printf( "error: open file error!\n" );
		return -1;
	}
	int num_video_sequence=0;
	int num_Group_of_picture=0;
	int num_I=0;
	int num_B=0;
	int num_P=0;

	unsigned char *p, *PI;
	int remain_size = 0;
	int total = 0;
	int flag_sequence = 0;
	int flag_iframe = 0;
	while( !feof( fd_es ) )
	{
		/* 读入ES文件，remain_size表示当前缓存中还有多少字节数据 */
		int read_size = fread( gbuf+remain_size, 1, sizeof(gbuf) - remain_size, fd_es );

		p = gbuf;
		PI = NULL; /* 指向Sequence 或 Picture 的开始位置 */

		while( p+6 < gbuf+remain_size+read_size )
		{
			/* 寻找前缀0x000001 */
			if( 0x00 == p[0] && 0x00 == p[1] && 0x01 == p[2] )
			{
				/* 以Sequence header与Picture header 为I帧 */
				if( 0xB3 == p[3] || 0x00 == p[3] )
				{
					if( ( NULL != PI ) && ( ( 1 == flag_sequence ) || ( 1 == flag_iframe ) ) ) //有i_frame,有刷新,写
					{
						total += fwrite( PI, 1, p - PI, fd_i ); /* 写入I帧 */
						flag_sequence = 0;
						flag_iframe = 0;
						PI = NULL;
					}
				}

				/* Picture header 的开始代码 0x00 */
				unsigned char picture_coding_type = (p[5]&0x38) >>3; /* 帧类型, 第42-44位 */
				if( 0x00 == p[3] && 1 == picture_coding_type )
				{   
					num_I++;
					flag_iframe = 1;
					PI = p;
				}
				if( 0x00 == p[3] && 2 == picture_coding_type )
				{   
					num_P++;

				}
				if( 0x00 == p[3] && 3 == picture_coding_type )
				{   
					num_B++;

				}
				/* Sequence header 的开始代码 0xB3 */
				if( 0xB3 == p[3] )
				{
					flag_sequence = 1;
					num_video_sequence++;
				}
				/*Group_START_CODE的开始代码为0xB8*/
				if(0xB8==p[3])
				{
					num_Group_of_picture++;
				}
			}
			p++;
		}
		/* 确定缓存中未处理数据的大小, 并移至缓存的开始处 */
		if( NULL == PI )
		{
			remain_size = gbuf+read_size+remain_size - p;
			memmove( gbuf, p, remain_size );
		}
		else
		{
			remain_size = gbuf+read_size+remain_size - PI;
			memmove( gbuf, PI, remain_size );
			num_I--;
		}
	}
	fclose( fd_es );
	fclose( fd_i );
	printf("video_sequence个数为%d\n",num_video_sequence);
	printf("Group_of_picture个数为%d\n",num_Group_of_picture);
	printf("I帧个数为%d\n",num_I);
	printf("B帧个数为%d\n",num_B);
	printf("P帧个数为%d\n",num_P);
	printf( "End get iframe.\n" );
	return 0;
} 
/* 设置Dts时间戳(90khz) */
unsigned int SetDtsTimeStamp( unsigned char *buf, unsigned int time_stemp)
{
	buf[0] = ((time_stemp >> 29) | 0x11 ) & 0x1f;
	buf[1] = time_stemp >> 22;
	buf[2] = (time_stemp >> 14) | 0x01;
	buf[3] = time_stemp >> 7;
	buf[4] = (time_stemp << 1) | 0x01;
	return 0;
}
/* 设置Pts时间戳(90khz) */
unsigned int SetPtsTimeStamp( unsigned char *buf, unsigned int time_stemp)
{
	buf[0] = ((time_stemp >> 29) | 0x31 ) & 0x3f;
	buf[1] = time_stemp >> 22;
	buf[2] = (time_stemp >> 14) | 0x01;
	buf[3] = time_stemp >> 7;
	buf[4] = (time_stemp << 1) | 0x01;
	return 0;
}
//es打包成pes
//这里构建了PES header 19个字节,6头部+3byte(标记+7flag+len)+10bytes(pts+dts)
//pes 中包含了pts,dts
int es_pes(char *es_src, char *pes_des,int num_pes)
{
	unsigned char pes_header[19];//header长度
	unsigned int  pes_packet_Length = 0;
	unsigned int  framecnt = 0;
	unsigned int  Pts = 0;
	unsigned int  Dts = 0;
	int seq_begin = 0;
	int first_pts=0;
	int last_pts=0;

	FILE *es_fp = fopen( es_src, "rb" );
	FILE *pes_fp = fopen( pes_des, "wb" );
	if( es_fp == NULL || pes_fp == NULL )
	{
		return -1;
	}
	int remain_size = 0;
	int read_size = 0;
	unsigned char *p;
	unsigned char *p_save = NULL;
	while(!feof(es_fp))
	{
		read_size = fread(gbuf + remain_size, 1, sizeof(gbuf) - remain_size, es_fp);
		p = gbuf;

		while( p + 3 < gbuf + read_size +remain_size)
		{
			memset(pes_header, 0, sizeof(pes_header));
			if (p[0] == 0x0 && p[1] == 0x0 && p[2] == 0x1 && 0xB3 == p[3])   //前缀为001+sequence_header_code
			{
				first_pts=Pts;
				if ((NULL != p_save) && (1 == seq_begin))
				{	


LAST_I:     
					pes_packet_Length = p - p_save + 13; //为什么加13? 19-6(19个字节中前6个不算在内(0x00,0x00,0x01,0xe0,len1,len2))

					if(pes_packet_Length<=65535)
					{
						pes_header[4] = (pes_packet_Length & 0xff00) >> 8; 
						pes_header[5] = pes_packet_Length & 0x00ff; 
					}
					else
					{
						pes_header[4] = 0x0;
						pes_header[5] = 0x0;
					}
					/*PES包头的相关数据*/
					pes_header[0] = 0;
					pes_header[1] = 0;
					pes_header[2] = 0x01;
					pes_header[3] = 0xE0;   //PES分组开始码流
					pes_header[6] = 0x81;   //0b10,00,0,0,0,1; 必需以10开始,最末位表示orig/copy
					pes_header[7] = 0xC0;   //0b11,0,0,0,0,0,0; 11表示带pts,dts
					pes_header[8] = 0x0A; //长度

					Dts = (framecnt+1) *100* 90;
					Pts = (framecnt) *100 * 90;
					last_pts=Pts;
					SetDtsTimeStamp(&pes_header[14], Dts); //设置时间戳 DTS 
					SetPtsTimeStamp(&pes_header[9], Pts);  //设置时间戳 PTS
					framecnt += 1;
					fwrite(pes_header, 1, sizeof(pes_header), pes_fp);  //把PES包头写入文件, 19个字节
					fwrite(p_save, 1, p-p_save, pes_fp);                //把I帧写入文件

					p_save = NULL; 

				}

				if (p[3] == 0xB3)    //判断是否到了下一个序列头
				{
					p_save = p;
					seq_begin = 1;
				}
			}

			p++;
		}
		/*把多出来的内容写入下个BUF*/
		if(NULL != p_save)
		{
			if (feof(es_fp))   //把最后一帧写入文件
			{
				goto LAST_I; 
			}
			remain_size = gbuf + read_size + remain_size - p_save; 
			memmove(gbuf, p_save, remain_size);  
		}
		else
		{
			remain_size = gbuf + read_size + remain_size - p;
			memmove(gbuf, p, remain_size);
		}

		p_save = NULL;
	} 	
	printf("PES平均间隔为%.2fms\n",(float)(last_pts-first_pts)/num_pes);
	printf("es_pes_END\n");
	fclose(es_fp);
	fclose(pes_fp);

	return 0;
}
/*打包成TS包, 加入ts包头及调整字段*/
int pes_ts(char *pesfile,char *tsfile ,int pid)
{

	FILE *pes_fp,*ts_fp;
	pes_fp = fopen(pesfile, "rb");
	ts_fp = fopen(tsfile, "wb");
	if( ts_fp == NULL || pes_fp == NULL )
	{
		return -1;
	}

	unsigned char ts_buf[188] = {0};
	/*设ts参数*/
	ts_buf[0] = 0x47;   //添加头部信息
	ts_buf[1] = 0x62;//修改了PID
	ts_buf[2] = pid&0x00ff;

	unsigned char counter = 0;
	int remain_size = 0;
	unsigned char *p_save = NULL;
	int bFindPes = 0;
	while(!feof(pes_fp))
	{
		int rdsize = fread(gbuf+remain_size, 1, sizeof(gbuf)-remain_size, pes_fp);   //读文件
		//remain_size+=rdsize;
		unsigned char *p = gbuf;

		while( p + 6 < gbuf + rdsize +remain_size)  //头部6字节
		{
			if (0 == p[0] && 0 == p[1] && 0x01 == p[2] && 0xE0 == p[3]) //进入，pes分组码
			{  
				if (bFindPes == 0)  //第一次找到PES包
				{
					p_save = p;
					bFindPes = 1;
				}
				else
				{
					int pes_packet_Length = p - p_save;   //pes包长度
					unsigned char start_indicator_flag = 0; 

					while (1)
					{
						ts_buf[3] = counter;

						if (1 != start_indicator_flag)
						{
							ts_buf[1] = ts_buf[1] | 0x40;  //payload_unit_start_indicator置为1
							start_indicator_flag = 1;
						}
						else
						{
							ts_buf[1] = ts_buf[1] & 0xBF;  //payload_unit_start_indicator置为0
						}

						if (pes_packet_Length >=184)   //打包成TS包（188）
						{
							ts_buf[3] = ts_buf[3] & 0xDF;
							ts_buf[3] = ts_buf[3] | 0x10; //指示仅包含负载
							memcpy(&ts_buf[4], p_save, 184);
							fwrite(ts_buf, 1, 188, ts_fp);   //写文件
							pes_packet_Length -=184;
							p_save += 184;
						}
						else                       //不够184B的加入调整字段，为空
						{ 
							ts_buf[3] = ts_buf[3] | 0x30;   // 有调整字段,但调整字段填空
							ts_buf[4] = 183 - pes_packet_Length;  //调整字段大小
							ts_buf[5] = 0x0;
							//	for(i=6;i<ts_buf[4];i++)
							//	{ 
							//	ts_buf[i] = 0xff;
							//	}
							if(ts_buf[4] !=0)
							{
								memset(&ts_buf[6],0xff,ts_buf[4]-1);
							}
							memcpy(&ts_buf[4] + 1 + ts_buf[4], p_save, pes_packet_Length);
							fwrite(ts_buf, 1, 188, ts_fp);   //写文件

							break;
						}

						counter = (counter + 1) % 0x10;      //ts包计数
					}  
				}
				p_save = p; 
			}

			p++;
		}

		if (1 == bFindPes)   
		{
			remain_size = gbuf + rdsize + remain_size - p_save; 
			memmove(gbuf, p_save, remain_size); 
			p_save = NULL;
			bFindPes = 0;
		}
		else
		{
			remain_size =gbuf + rdsize + remain_size - p;
			memmove(gbuf , p , remain_size);
		}  
	} 

	printf("pes_ts_END\n");

	fclose(pes_fp);
	fclose(ts_fp);
	return 0;
} 
