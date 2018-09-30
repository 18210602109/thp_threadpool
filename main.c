#include<stdio.h>
#include<pthread.h>

#include "thp_threadpool.h"

int testfun(void *argv)
{
	int *num = (int*)argv;
	printf("testfun threadid = %u  num = %d\n",pthread_self(),*num);
	//sleep(3);
	return 0;
}

int main()
{
	int array[10000] = {0};
	int i = 0;
	thp_threadpool_conf_t conf = {5,0,5}; //实例化启动参数
	thp_threadpool_t *pool = thp_threadpool_init(&conf);//初始化线程池
	if (pool == NULL)
		return 0;
	
	for (i = 0; i < 10000; i++)
	{
		array[i] = i;

		if (i == 80)
		{
			thp_thread_add(pool); //增加线程
			thp_thread_add(pool);
		}
		
		if (i == 100)
			thp_task_max_modify(pool, 0); //改变最大任务数   0为不做上限

		while(1)
		{
			if (thp_task_add(pool, testfun, &array[i]) == 0)
				break;
			
			printf("error in i = %d\n",i);
		}
	}

	thp_threadpool_destroy(pool);

	while(1){
		sleep(5);
	}
	return 0;
}
