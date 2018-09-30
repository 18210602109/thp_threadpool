#include "thp_threadpool.h"

#include <signal.h>

#define MAX_TASK_SIZE 99999999

static pthread_key_t key;

void threadpool_cycle(void* args);

int check_conf(thp_threadpool_conf_t * conf)
{
	if(NULL == conf)
		return -1;

	if(conf->thread_num < 1)
		return -1;

	if(conf->task_max_num < 1)
		conf->task_max_num = MAX_TASK_SIZE;

	return 0;
}

void task_queue_init(thp_task_queue_t *task_queue)
{
	task_queue->head = NULL;
	task_queue->tail = &task_queue->head;
}

int thread_mutex_init(pthread_mutex_t *mutex)
{
	int nret = 0;

	pthread_mutexattr_t attr;

	if(0 != pthread_mutexattr_init(&attr))
		return -1;

	if(0 != pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK))
	{
		pthread_mutexattr_destroy(&attr);
		return -1;
	}

	nret = pthread_mutex_init(mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	return nret;
}

void thread_mutex_destroy(pthread_mutex_t *mutex)
{
	pthread_mutex_destroy(mutex);
}

int thread_cond_init(pthread_cond_t *cond)
{
	return pthread_cond_init(cond, NULL);
}

int thread_cond_destroy(pthread_cond_t *cond)
{
	return pthread_cond_destroy(cond);
}

//初始化线程池
int threadpool_init(thp_threadpool_t *pool)
{
	int i = 0;
	pthread_t  pid;
	pthread_attr_t attr;

	if (pthread_attr_init(&attr) != 0){
		return -1;
	}

	if (pool->thread_stack_size != 0)
	{
		if (pthread_attr_setstacksize(&attr, pool->thread_stack_size) != 0){
			pthread_attr_destroy(&attr);
			return -1;
		}
	}

	//创建线程池
	for (i = 0; i < pool->thread_num; ++i)
	{
		pthread_create(&pid, &attr, threadpool_cycle, pool);
	}	
	pthread_attr_destroy(&attr);

	return 0;
}

//添加线程
int thread_add(thp_threadpool_t *pool)
{
	int ret = 0;
	pthread_t  pid;
	pthread_attr_t attr;
	
	if (pthread_attr_init(&attr) != 0){
		return -1;
	}
	if (pool->thread_stack_size != 0)
	{
		if (pthread_attr_setstacksize(&attr, pool->thread_stack_size) != 0){
			pthread_attr_destroy(&attr);
			return -1;
		}
	}

	ret = pthread_create(&pid, &attr, threadpool_cycle, pool);
	if (ret == 0)
		pool->thread_num++;
	
	pthread_attr_destroy(&attr);
	return ret;
}

//修改最大任务数
void task_max_modify(thp_threadpool_t *pool, unsigned int task_max_num)
{
	pool->task_queue.task_max_num = task_max_num;
	if (pool->task_queue.task_max_num < 1)
	{
		pool->task_queue.task_max_num = MAX_TASK_SIZE;
	}
}

//工作线程
void threadpool_cycle(void* args)
{
	unsigned int exit_flag = 0;
	sigset_t set;
	thp_task_t *ptask = NULL;
	thp_threadpool_t *pool = (thp_threadpool_t*)args;

	//只注册以下致命信号，其他全部屏蔽
	sigfillset(&set);
	sigdelset(&set, SIGILL);
	sigdelset(&set, SIGFPE);
	sigdelset(&set, SIGSEGV);
	sigdelset(&set, SIGBUS);
	
	if (pthread_setspecific(key,(void*)&exit_flag) != 0)//设置exit_flag = 0
		return;

	if (pthread_sigmask(SIG_BLOCK, &set, NULL) != 0)
		return;

	while(!exit_flag)//exit_flag为1时线程退出
	{         
		if (pthread_mutex_lock(&pool->mutex) != 0)  //加锁
			return;

		while(pool->task_queue.head == NULL)
		{
			if (pthread_cond_wait(&pool->cond, &pool->mutex) != 0)
			{
				pthread_mutex_unlock(&pool->mutex);
				return;
			}
		}
		
		ptask = pool->task_queue.head;     //从任务队列中获取一个任务任务节点
		pool->task_queue.head = ptask->next;

		pool->task_queue.task_curr_num--;   //当前任务数--

		if (pool->task_queue.head == NULL){
			pool->task_queue.tail = &pool->task_queue.head;
		}

		if (pthread_mutex_unlock(&pool->mutex) != 0){ //解锁
			return;
		}

		ptask->fn(ptask->args);  //执行任务。
		free(ptask);
		ptask = NULL;
	}
	pthread_exit(0);
}


//线程池退出函数  
void threadpool_exit_cb(void* argv)
{
	unsigned int *lock = argv;
	unsigned int *pexit_flag = NULL;

	pexit_flag = (unsigned int *)pthread_getspecific(key);
	*pexit_flag = 1;    //将exit_flag置1
	pthread_setspecific(key, (void*)pexit_flag);
	*lock = 0;
	printf("线程退出!!!, pid[%u]\n", pthread_self());
}

int thread_key_init()
{
	return pthread_key_create(&key, NULL);
}

void thread_key_destroy()
{
	pthread_key_delete(key);
}
