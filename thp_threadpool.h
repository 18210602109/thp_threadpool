#ifndef THP_THREADPOOL_H
#define THP_THREADPOOL_H

#include <pthread.h>

typedef void (*TASK_FUN)(void *);

//任务
typedef struct thp_task_s
{
	void * args;//任务函数参数
	TASK_FUN fn;//任务函数
	struct thp_task_s *next;
}thp_task_t;

//任务队列
typedef struct thp_task_queue_s
{
	thp_task_t *head;
	thp_task_t **tail;
	unsigned int task_max_num;//最大任务数
	unsigned int task_curr_num;//当前任务数
}thp_task_queue_t;

//线程池
typedef struct thp_threadpool_s
{
	pthread_mutex_t mutex;
	pthread_cond_t	cond;
	thp_task_queue_t task_queue;

	unsigned int thread_num;//线程数
	unsigned int thread_stack_size;//线程堆栈大小
}thp_threadpool_t;

//参数配置
typedef struct thp_threadpool_conf_s
{
	unsigned int thread_num;//线程数
	unsigned int thread_stack_size;//线程堆栈大小
	unsigned int task_max_num;//最大任务数
}thp_threadpool_conf_t;

//线程池初始化
thp_threadpool_t * thp_threadpool_init(thp_threadpool_conf_t *conf);

//销毁线程池
void thp_threadpool_destroy(thp_threadpool_t *pool);

//添加任务
int thp_task_add(thp_threadpool_t *pool, TASK_FUN fn, void *args);

//添加线程
int thp_thread_add(thp_threadpool_t *pool);

//修改最大任务数
void thp_task_max_modify(thp_threadpool_t *pool, unsigned int task_max_num);
#endif