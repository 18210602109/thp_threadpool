#include "thp_threadpool.h"

extern void threadpool_exit_cb(void* argv);

//线程池初始化
thp_threadpool_t * thp_threadpool_init(thp_threadpool_conf_t *conf)
{
	thp_threadpool_t * pool;
	int error_flag_mutex;
	int error_flag_cond;
	pthread_attr_t attr;

	do
	{
		//检测配置是否合法
		if(0 != check_conf(conf))
			break;

		//初始化线程池基本参数
		pool = (thp_threadpool_t *)malloc(sizeof(thp_threadpool_t));
		if(NULL == pool)
			break;

		pool->thread_num = conf->thread_num;
		pool->thread_stack_size = conf->thread_stack_size;
		pool->task_queue.task_max_num = conf->task_max_num;

		task_queue_init(&pool->task_queue);

		if (thread_key_init() != 0)//创建一个pthread_key_t，用以访问线程全局变量。
		{
			free(pool);
			break;
		}

		if (thread_mutex_init(&pool->mutex) != 0)//初始化互斥锁
		{ 
			thread_key_destroy();
			free(pool);
			break;
		}

		if (thread_cond_init(&pool->cond) != 0)//初始化条件锁
		{  
			thread_key_destroy();
			thread_mutex_destroy(&pool->mutex);
			free(pool);
			break;
		}

		if (threadpool_init(pool) != 0)//创建线程池
		{       
			thread_key_destroy();
			thread_mutex_destroy(&pool->mutex);
			thread_cond_destroy(&pool->cond);
			free(pool);
			break;
		}

		return pool;
	}
	while (0);

	return NULL;
}

//销毁线程池
void thp_threadpool_destroy(thp_threadpool_t *pool)
{
	unsigned int n = 0;
	volatile unsigned int  lock;

	//z_threadpool_exit_cb函数会使对应线程退出
	for (; n < pool->thread_num; n++)
	{
		lock = 1;

		if (thp_task_add(pool, threadpool_exit_cb, &lock) != 0)
			return;
		
		while (lock)
			usleep(1);
		
	}

	thread_mutex_destroy(&pool->mutex);
	thread_cond_destroy(&pool->cond);
	thread_key_destroy();
	free(pool);
}

//添加任务
int thp_task_add(thp_threadpool_t *pool, TASK_FUN fn, void *args)
{
	thp_task_t *task = NULL;
	//申请一个任务节点并赋值
	task = (thp_task_t *)malloc(sizeof(thp_task_t));
	if (task == NULL)
		return -1;
	
	task->fn = fn;
	task->args = args;
	task->next = NULL;

	if (pthread_mutex_lock(&pool->mutex) != 0)//加锁
	{ 
		free(task);
		return -1;
	}

	do{

		if (pool->task_queue.task_curr_num >= pool->task_queue.task_max_num)//判断工作队列中的任务数是否达到限制
			break;

		//将任务节点尾插到任务队列
		*(pool->task_queue.tail) = task;
		pool->task_queue.tail = &task->next;
		pool->task_queue.task_curr_num++;

		//通知阻塞的线程
		if (pthread_cond_signal(&pool->cond) != 0)
			break;

		//解锁
		pthread_mutex_unlock(&pool->mutex);

		return 0;
	}while(0);

	pthread_mutex_unlock(&pool->mutex);
	free(task);

	return -1;
}

//给线程池中增加一个工作线程
int thp_thread_add(thp_threadpool_t *pool)
{
	int ret = 0;

	if (pthread_mutex_lock(&pool->mutex) != 0)
		return -1;
	
	ret = thread_add(pool);
	pthread_mutex_unlock(&pool->mutex);

	return ret;
}

//设置新的最大任务限制   0为不限制
void thp_task_max_modify(thp_threadpool_t *pool, unsigned int task_max_num)
{
	if (pthread_mutex_lock(&pool->mutex) != 0)
		return -1;
	
	task_max_modify(pool, task_max_num);  //改变最大任务限制
	pthread_mutex_unlock(&pool->mutex);
}

