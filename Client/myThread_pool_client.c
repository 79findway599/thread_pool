#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <netdb.h>

#define PORT_TCP 9001

struct info 
{
	char flag;                 //标记。1：请求文件属性。2：请求文件内容
	char buf[256];             //文件的文件名
	int local_begin;           //文件起始位置
	int length;                //文件的长度
};

int main(int argc,char *argv[])
{
	int client_fd;
	char buffer[1024];
	int length;
	int iSendLen;
	struct hostent *host;//主机信息
	struct sockaddr_in server_addr;
	if(argc<1)
	{
		printf("./%s hostname \n",argv[0]);
		exit(0);
	}
	
	if((host=gethostbyname(argv[1]))==NULL)//通过域名或主机名获取 ip 地址
	{
		exit(1);
	}
	
	if((client_fd=socket(AF_INET,SOCK_STREAM,0))==-1)
	{
		exit(1);
	}
	
	memset(&server_addr,0,sizeof(struct sockaddr_in));
	server_addr.sin_family=AF_INET;
	server_addr.sin_addr=*((struct in_addr *)host->h_addr);//ip地址
	server_addr.sin_port=htons(PORT_TCP);
	
	if(connect(client_fd,(struct sockaddr *)(&server_addr),sizeof(struct sockaddr))==-1)
	{
    	exit(1);
    }
    
    struct info sendinfo;
    
    sendinfo.flag = 1;//请求文件属性
    sprintf(sendinfo.buf,"%s",argv[2]);//文件名
    
    if(-1 == send(client_fd,&sendinfo, sizeof(sendinfo),0))
    {
    	perror("send");
    	exit(1);
    }
    
    char buf[1024];
    memset(buf,'\0',1024);
    if(-1 == recv(client_fd,buf,1024,0))
    {
    	perror("recv");
    	exit(1);
    }
    
    close(client_fd);
    
    //继续连接并请求文件内容
    if(-1==(client_fd=socket(AF_INET,SOCK_STREAM,0)))
    {
    	perror("socket");
    	exit(1);
    }
    
    if(-1 == connect(client_fd,(struct sockaddr *)&server_addr,sizeof(server_addr)))
    {
    	perror("connect");
    	exit(1);
    }
    
    sendinfo.flag = 2;//请求文件内容
    sendinfo.local_begin = 0;//起始偏移位置
    sendinfo.length = atoi(buf);//文件长度
    
    if(-1 == send(client_fd,&sendinfo, sizeof(sendinfo),0))
    {
    	perror("send");
    	exit(1);
    }
    
    int file_fd;
    file_fd = open(argv[3],O_WRONLY|O_CREAT,0644);
    
    lseek(file_fd,sendinfo.local_begin,SEEK_SET);
	
    
    printf("length = %d\n",ntohl(sendinfo.length));
    int ret = 0;
    int sum = 0;
    do
    {
    	ret = recv(client_fd,buf,1024,0);
    	write(file_fd,buf,ret);
    	sum  = sum + ret;
    	printf("sum=%d\n",sum);
    	//getchar();//测试用，可以不要
    }while(sum != ntohl(sendinfo.length));
    close(client_fd);
    close(file_fd);
}