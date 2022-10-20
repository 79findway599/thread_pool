#ifndef __PTHREAD_POOL_H__
#define __PTHREAD_POOL_H__
  
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>//SYS_gettid
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>//ifreq结构头文件
#include <sys/ioctl.h>
  
#define THREAD_DEF_NUM  20      //线程默认数量

#define PORT    9001

typedef struct task_node 
{
	void *arg;               //参数
	void *(*fun)(void *);    //任务处理函数
	pthread_t tid;
	int work_id;             //任务 id
	int flag;                //标记 1表示分配，0表示非分配
	struct task_node *next;
	pthread_mutex_t mutex;
} TASK_NODE;


typedef struct task_queue 
{
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	struct task_node *head;//任务数据
	int number;
} TASK_QUEUE_T;


typedef struct pthread_node 
{
	pthread_t tid;
	int flag;                  //标记 1表示忙碌，0表示空闲
	struct task_node *work;    //任务节点
	struct pthread_node *next;
	struct pthread_node *prev;
	pthread_cond_t cond;       
	pthread_mutex_t mutex;     
} THREAD_NODE;


typedef struct pthread_queue 
{
	int number;
	struct pthread_node *head;
	struct pthread_node *rear;//尾结点
	pthread_cond_t cond;
	pthread_mutex_t mutex;
 } PTHREAD_QUEUE_T;


struct info 
{
	char flag;                 //标记。1：请求文件属性。2：请求文件内容
	char buf[256];             //请求的文件名
	int local_begin;           //文件起始位置
	int length;                //文件的长度
};


extern PTHREAD_QUEUE_T *pthread_queue_idle; //空闲线程队列  
extern PTHREAD_QUEUE_T *pthread_queue_busy; //工作线程队列
extern TASK_QUEUE_T *task_queue_head;       //任务队列

void *child_work(void *ptr);//线程函数

void create_pthread_pool(void);//创建线程池

void init_system(void);

void sys_clean(void);//清除系统

void *thread_manager(void *ptr);//管理线程处理函数

void *prcoess_client(void *ptr);//任务执行函数

void *task_manager(void *ptr);//任务线程处理函数

void *monitor(void *ptr);//监控线程处理函数



#endif 