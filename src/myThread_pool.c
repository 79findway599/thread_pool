#include "myThread_pool.h"

void *child_work(void *ptr) 
{
	//临时线程节点指针接传入的参数
	THREAD_NODE * self = (THREAD_NODE *)ptr;
	
	pthread_mutex_lock(&self->mutex);
	//得到真实的线程 id
	self->tid = syscall(SYS_gettid);
	pthread_mutex_unlock(&self->mutex);
	
	while(1)
	{
		pthread_mutex_lock(&self->mutex);
		
		//没有任务
		if (NULL == self->work)
		{
			//无条件等待，阻塞在线程自己的条件变量上，等待被激活 
			pthread_cond_wait(&self->cond, &self->mutex);
		}

		//一旦有任务了
		pthread_mutex_lock(&self->work->mutex);
		self->work->fun(self->work->arg);//重点：执行任务
		
		//完成后 
		self->work->fun = NULL;
		self->work->flag = 0;
		self->work->tid = 0;
		self->work->next = NULL;
		free(self->work->arg);
		
		pthread_mutex_unlock(&self->work->mutex); 
		pthread_mutex_destroy(&self->work->mutex);		
		free(self->work);
		
		//设置当前线程空闲 
		self->work = NULL;
		self->flag = 0;

		//锁任务池里面的互斥锁
		pthread_mutex_lock(&task_queue_head->mutex);		

		if (task_queue_head->head != NULL)
		{
            //任务队列有节点
			TASK_NODE * temp = task_queue_head->head;
			
			task_queue_head->head = task_queue_head->head->next;
			self->flag = 1;
			self->work = temp;
			
			temp->tid = self->tid;		
			temp->next = NULL;	
			temp->flag = 1;
			
			task_queue_head->number--;
			
			pthread_mutex_unlock(&task_queue_head->mutex);			
			pthread_mutex_unlock(&self->mutex);			
			continue;
		}
		else
		{
            //任务队列无节点
			pthread_mutex_unlock(&task_queue_head->mutex);
			pthread_mutex_lock(&pthread_queue_busy->mutex);
			
			if (pthread_queue_busy->head == self && pthread_queue_busy->rear == self)
			{
                //self为忙碌线程中节点，且忙碌线程只有这个节点
				pthread_queue_busy->head = pthread_queue_busy->rear = NULL;
				self->next = self->prev = NULL;
			}
			else if (pthread_queue_busy->head == self && pthread_queue_busy->rear != self)
			{
				//self为忙碌线程队列中的头节点
				pthread_queue_busy->head = pthread_queue_busy->head->next; 
				pthread_queue_busy->head->prev = NULL;				
				self->next = self->prev = NULL;
			}
			else if (pthread_queue_busy->head != self && pthread_queue_busy->rear == self)
			{
                //self为忙碌线程队列的尾节点
				pthread_queue_busy->rear = pthread_queue_busy->rear->prev;
				pthread_queue_busy->rear->next = NULL;
				self->next = self->prev = NULL; 
			}
			else
			{
                //self为忙碌线程队列的中间节点
				self->next->prev = self->prev;
				self->prev->next = self->next; 
				self->next = self->prev = NULL;
			}
			pthread_mutex_unlock(&pthread_queue_busy->mutex);
			//到这一步是不是当前线程节点从忙碌线程池中剥离
			
			//从忙碌线程添加到空闲线程
			pthread_mutex_lock(&pthread_queue_idle->mutex);
			
			if (pthread_queue_idle->head == NULL)
            {
            	pthread_queue_idle->head = pthread_queue_idle->rear = self;
            	self->next = self->prev = NULL;
            }
            else
            {
            	self->next = pthread_queue_idle->head;
            	self->prev = NULL;
            	self->next->prev = self;
            	
            	pthread_queue_idle->head = self;
            	pthread_queue_idle->number++;
            }
            
            pthread_mutex_unlock(&pthread_queue_idle->mutex);
            pthread_mutex_unlock(&self->mutex); 
            
            pthread_cond_signal(&pthread_queue_idle->cond);
        }
    }
}

void create_pthread_pool(void) 
{
	//线程队列
	THREAD_NODE * temp = (THREAD_NODE *)malloc(sizeof(THREAD_NODE) * THREAD_DEF_NUM);
	if (temp == NULL)
	{
		printf("malloc failure\n");
		exit(1);
	}
	
	//线程队列初始化
	int i;
	for (i = 0; i < THREAD_DEF_NUM; i++)
	{
		temp[i].work = NULL;
		temp[i].flag = 0;
		
		if (i == THREAD_DEF_NUM - 1)
			temp[i].next = NULL;
		else
			temp[i].next = &temp[i + 1];
			
		if (i == 0)
			temp[i].prev = NULL;
		else
			temp[i].prev = &temp[i - 1];
			
		pthread_cond_init(&temp[i].cond, NULL);		
		pthread_mutex_init(&temp[i].mutex, NULL);
		
		//每一个节点创建线程函数,每一个线程函数里面传入的是一个线程的结构体变量的首地址
		pthread_create(&temp[i].tid, NULL, child_work, (void *)&temp[i]); 
	}
	
	//修改空闲线程队列
	pthread_mutex_lock(&pthread_queue_idle->mutex);
	pthread_queue_idle->number = THREAD_DEF_NUM;
	pthread_queue_idle->head = &temp[0];
	pthread_queue_idle->rear = &temp[THREAD_DEF_NUM - 1];
	pthread_mutex_unlock(&pthread_queue_idle->mutex);
} 

void init_system(void) 
{
	//初始化空闲线程队列
	pthread_queue_idle = (PTHREAD_QUEUE_T *)malloc(sizeof(PTHREAD_QUEUE_T));
	pthread_queue_idle->number = 0;
	pthread_queue_idle->head = NULL;
	pthread_queue_idle->rear = NULL;
	pthread_mutex_init(&pthread_queue_idle->mutex, NULL);
	pthread_cond_init(&pthread_queue_idle->cond, NULL);
	
	//初始化工作线程队列
	pthread_queue_busy = (PTHREAD_QUEUE_T *)malloc(sizeof(PTHREAD_QUEUE_T));
	pthread_queue_busy->number = 0;
	pthread_queue_busy->head = NULL;
	pthread_queue_busy->rear = NULL;
	pthread_mutex_init(&pthread_queue_busy->mutex, NULL);
	pthread_cond_init(&pthread_queue_busy->cond, NULL);
	
	//初始化任务队列
	task_queue_head = (TASK_QUEUE_T *)malloc(sizeof(TASK_QUEUE_T));
	task_queue_head->head = NULL;
	task_queue_head->number = 0;
	pthread_cond_init(&task_queue_head->cond, NULL);
	pthread_mutex_init(&task_queue_head->mutex, NULL);
	
	//创建空闲线程池对象
	create_pthread_pool();
}

void sys_clean(void) 
{
	printf("the system exit abnormally\n");//异常退出
	//清空空闲线程队列
	if(pthread_queue_idle != NULL)
	{
		while(pthread_queue_idle->head != NULL)
		{
			THREAD_NODE * temp = pthread_queue_idle->head;
			pthread_queue_idle->head = pthread_queue_idle->head->next;
			if(temp->work)
			{
				if(temp->work->arg)
					free(temp->work->arg);
				free(temp->work);
			}
			free(temp);
		}
		free(pthread_queue_idle);
	}
	
	//清空忙碌线程队列
	if(pthread_queue_busy != NULL)
	{
		while(pthread_queue_busy->head != NULL)
		{
			THREAD_NODE * temp = pthread_queue_busy->head;
			pthread_queue_busy->head = pthread_queue_busy->head->next;
			if(temp->work)
			{
				if(temp->work->arg)
					free(temp->work->arg);
				free(temp->work);
			}
			free(temp);
		}
		free(pthread_queue_busy);
	}
	
	//清空任务队列
	if(task_queue_head != NULL)
	{
		while(task_queue_head->head)
		{
			TASK_NODE * temp = task_queue_head->head;
			task_queue_head->head = task_queue_head->head->next;
			if(temp->arg)
				free(temp->arg);
			free(temp);
		}			
		free(task_queue_head);
	}
	
	exit(1);
} 

void * thread_manager(void *ptr) 
{
	while (1)
	{
		THREAD_NODE * temp_thread = NULL;
		TASK_NODE * temp_task = NULL;
		
		/*
		 得到一个新的任务，并修改任务池的任务链表
		 如果没有任务，阻塞在任务池的条件变量上
		*/
		pthread_mutex_lock(&task_queue_head->mutex); 
		
		if (task_queue_head->number == 0)
			pthread_cond_wait(&task_queue_head->cond,&task_queue_head->mutex);
			
		temp_task = task_queue_head->head;
		task_queue_head->head = task_queue_head->head->next;
		task_queue_head->number--;
		pthread_mutex_unlock(&task_queue_head->mutex);
		
		/*
		 得到一个新的空闲线程，并修改空闲线程池的线程链表
         如果没有空闲线程，阻塞在空闲线程的条件变量上
        */
        pthread_mutex_lock(&pthread_queue_idle->mutex);
        
        if (pthread_queue_idle->number == 0)
        	pthread_cond_wait(&pthread_queue_idle->cond,&pthread_queue_idle->mutex);

        //忙碌线程队列中的线程转到空闲线程会放在头节点上，所以这里从头节点取空闲线程
        temp_thread = pthread_queue_idle->head;
        
        if (pthread_queue_idle->head == pthread_queue_idle->rear)
        {
            //空闲线程队列只有一个节点
        	pthread_queue_idle->head = NULL;
        	pthread_queue_idle->rear = NULL;
        }
        else
        {
        	//空闲线程队列不止一个节点
        	pthread_queue_idle->head = pthread_queue_idle->head->next;
        	pthread_queue_idle->head->prev = NULL;
        }
        pthread_queue_idle->number--;
        pthread_mutex_unlock(&pthread_queue_idle->mutex);
        
        //修改任务节点属性
        pthread_mutex_lock(&temp_task->mutex);
        temp_task->tid = temp_thread->tid;//任务节点里面的表示执行线程的id为当前得到的空闲线程的id
        temp_task->next = NULL;
        temp_task->flag = 1;//当前任务已分配
        pthread_mutex_unlock(&temp_task->mutex);
        
        //修改空闲线程节点属性 
        pthread_mutex_lock(&temp_thread->mutex);
        temp_thread->flag = 1;//标记为忙碌
        temp_thread->work = temp_task;//需要执行的任务节点
        temp_thread->next = NULL;
        temp_thread->prev = NULL;
        pthread_mutex_unlock(&temp_thread->mutex);
        
        //添加到忙碌线程队列
        pthread_mutex_lock(&pthread_queue_busy->mutex);
        
        if (pthread_queue_busy->head == NULL)
        {
        	//忙碌线程队列为空
        	pthread_queue_busy->head = temp_thread;
        	pthread_queue_busy->rear = temp_thread;
        	temp_thread->prev = temp_thread->next = NULL; 
        }
        else 
        {
        	//忙碌线程队列非空
            pthread_queue_busy->head->prev = temp_thread;          
            temp_thread->prev = NULL;
            temp_thread->next = pthread_queue_busy->head;
            pthread_queue_busy->head = temp_thread; 
            pthread_queue_busy->number++;
        }
        pthread_mutex_unlock(&pthread_queue_busy->mutex);
        
        //发信号，有忙碌的线程，线程函数得到这个条件变量开始执行
        pthread_cond_signal(&temp_thread->cond);
    }
}

void * prcoess_client (void *ptr) 
{
	int net_fd;
	//ptr: socket_fd
	net_fd = atoi((char *) ptr);
	
	struct info client_info;
	memset(&client_info, '\0', sizeof(client_info));
	
	if (-1 == recv(net_fd, &client_info, sizeof(client_info), 0))
	{
		printf("recv msg error\n");
		close(net_fd);
		goto clean;
	}
	
	if (client_info.flag == 1)
	{
		struct stat mystat;
				
		if (-1 == stat(client_info.buf, &mystat))
		{
			printf("stat %s error\n", client_info.buf);
			close(net_fd);
			goto clean;
		}
		
		char msgbuf[1024];
		memset(msgbuf, '\0', 1024);
		sprintf(msgbuf, "%d", htonl(mystat.st_size));
		
		//发送文件的长度数据
		if (-1 == send(net_fd, msgbuf, strlen(msgbuf) + 1, 0))
		{
			printf("send msg error\n");
			close(net_fd);
			goto clean;
		}
		
		close(net_fd);
		return ;
	}
	else 
	{
		//需要一个文件的内容
		int local_begin = ntohl(client_info.local_begin);
		int length = ntohl(client_info.length);
		
		int file_fd;
		
		if (-1 == (file_fd = open(client_info.buf, O_RDONLY)))
		{
			printf("open file %s error\n", client_info.buf);
			close(net_fd);
			goto clean;
		}

		lseek(file_fd, local_begin, SEEK_SET);//文件定位
		
		int need_send = length;
		
		int ret;
        sleep(10);//测试用，为了让大家能看到线程的执行,正常不用
		do
		{
			char buf[1024];
			memset(buf, '\0', 1024);
			
			if (need_send < 1024)
				ret = read(file_fd, buf, need_send);
			else
			{
				ret = read(file_fd, buf, 1024);
				need_send -= 1024;
			}
			
			if (ret == -1)
			{
				//读文件失败
				printf("read file %s error\n", client_info.buf);
				close(net_fd);
				close(file_fd);
				return ;
			}
			
			if (-1 == send(net_fd, buf, ret, 0))
			{
				printf("send file %s error\n", client_info.buf);
				close(net_fd);
				close(file_fd);
				return;
			}
		}while (ret > 0);
		
		close(net_fd);
		close(file_fd);
	}
	return ;
	
clean:
	sys_clean();
}

void * task_manager(void *ptr)
{
	int listen_fd;
	
	if (-1 == (listen_fd = socket(AF_INET, SOCK_STREAM, 0)))
	{
		perror("socket");
		goto clean;
	}
	
	struct ifreq ifr;//用来配置和获取ip地址，掩码，MTU等接口信息
    
    //eth0 表示第一个网卡。光纤以太网接口
    strcpy(ifr.ifr_name, "eth0");
    
    //SIOCGIFADDR 标志：获取接口地址
    if (ioctl(listen_fd, SIOCGIFADDR, &ifr) < 0) 
    {
    	perror("ioctl");
    	goto clean;
    }
    
    struct sockaddr_in myaddr;
    myaddr.sin_family = AF_INET;
    myaddr.sin_port = htons(PORT);
    //接口信息里面得到的ip地址赋值到 sockaddr_in 
    myaddr.sin_addr.s_addr = ((struct sockaddr_in *)&(ifr.ifr_addr))->sin_addr.s_addr;
    
    if (-1 == bind(listen_fd, (struct sockaddr *)&myaddr, sizeof(myaddr)))
    {
    	perror("bind");
    	goto clean;
    }
    
    if (-1 == listen(listen_fd, 5))
    {
    	perror("listen");
    	goto clean;
    }
    
    int i;//任务编号id
    for (i = 1;; i++)
    {
    	int newfd;
    	
    	struct sockaddr_in client;
    	socklen_t len = sizeof (client);
    	if (-1 ==(newfd = accept(listen_fd, (struct sockaddr *)&client, &len)))
    	{
    		perror("accept");
    		goto clean;
    	}
    	
    	//创建新的任务节点
    	TASK_NODE * newtask = (TASK_NODE *)malloc(sizeof(TASK_NODE));    	
    	if (newtask == NULL)
    	{
    		printf("malloc error");
    		goto clean;
    	}
    	/*
         初始化这个任务节点的属性
         这个任务还没有添加到任务池，不需要锁互斥锁
        */
        newtask->arg = (void *)malloc(128);
        memset(newtask->arg, '\0', 128);
        sprintf(newtask->arg, "%d", newfd);//把连接的句柄当成参数，写入成任务函数需要执行的参数
        newtask->fun = prcoess_client;//设置任务执行函数
        newtask->tid = 0;//还没进线程队列，暂不设置
        newtask->work_id = i;
        newtask->next = NULL;
        pthread_mutex_init(&newtask->mutex, NULL);
        
        //任务结点加入任务队列
        pthread_mutex_lock(&task_queue_head->mutex);
        
        //任务队列尾部插入
        TASK_NODE * temp = NULL;
        temp = task_queue_head->head;
        
        if (temp == NULL)
        {
        	task_queue_head->head = newtask;
        }
        else
        {
        	while (temp->next != NULL)
        		temp = temp->next;
        	temp->next = newtask;
        }
        task_queue_head->number++;
        pthread_mutex_unlock(&task_queue_head->mutex);
        
        //发信号，表示任务队列中有任务，等待在任务队列中这个条件变量的线程得到通知后开始执行，触发管理线程
        pthread_cond_signal(&task_queue_head->cond);
    }
    return ;
clean:
	sys_clean();
}

void * monitor (void *ptr) 
{
	THREAD_NODE * temp_thread = NULL;
	
	while (1)
	{
		pthread_mutex_lock (&pthread_queue_busy->mutex);
		
		temp_thread = pthread_queue_busy->head;
		printf ("\n*******************************\n"); 
		
		while (temp_thread) 
		{
			printf ("thread %ld is  execute work_number %d\n", temp_thread->tid, 
				temp_thread->work->work_id);
			temp_thread = temp_thread->next;
		}
		printf ("*******************************\n\n");
		pthread_mutex_unlock (&pthread_queue_busy->mutex);
		sleep (10);
	}
	return;
}

