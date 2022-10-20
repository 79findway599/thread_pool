#include "myThread_pool.h"
PTHREAD_QUEUE_T * pthread_queue_idle;//空闲线程队列 
PTHREAD_QUEUE_T *pthread_queue_busy;//忙碌线程队列 
TASK_QUEUE_T *task_queue_head;//任务队列

int main (int argc, char *argv[])
{
	pthread_t thread_manager_tid, task_manager_tid, monitor_id;
	init_system();
	
	pthread_create(&thread_manager_tid, NULL, thread_manager, NULL);//创建管理线程
	pthread_create(&task_manager_tid, NULL, task_manager, NULL);    //  创建任务线程
	pthread_create(&monitor_id, NULL, monitor, NULL);               //创建监控线程

	pthread_join(thread_manager_tid, NULL);
	pthread_join(task_manager_tid, NULL);
	pthread_join(monitor_id, NULL);
	
	sys_clean();
	return 0;
}