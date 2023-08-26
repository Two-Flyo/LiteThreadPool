#include "../include/ThreadPool.h"


using ULong = unsigned long long;

class MyTask :public Task
{
public:
	MyTask(ULong begin, ULong end)
		:_begin(begin)
		, _end(end)
	{}

	// 1.怎么设计run函数的返回值，可以表示任意类型呢？
	Any run() // run方法最终在线程池分配的线程中去执行了
	{
		std::cout << "tid: " << std::this_thread::get_id() << " begin" << std::endl;
		std::this_thread::sleep_for(std::chrono::seconds());
		ULong sum = 0;
		for (ULong i = _begin; i <= _end; i++)
		{
			sum += i;
		}
		std::cout << "tid: " << std::this_thread::get_id() << " end" << std::endl;
		return sum;
	}

private:
	ULong _begin;
	ULong _end;
};


int main()


// ThreadPool 对象析构以后，怎么把线程池相关的线程资源全部回收？  
{
	ThreadPool pool;
	// 用户自己设置线程池工作模式 
	pool.SetThreadPoolMode(ThreadPoolMode::CACHED);
	pool.Start(4);

	Result res1 = pool.SubmitTask(std::make_shared<MyTask>(1, 100000000));
	Result res2 = pool.SubmitTask(std::make_shared<MyTask>(100000001, 200000000));
	Result res3 = pool.SubmitTask(std::make_shared<MyTask>(200000001, 300000000));
	pool.SubmitTask(std::make_shared<MyTask>(301, 400));
	pool.SubmitTask(std::make_shared<MyTask>(401, 500));
	pool.SubmitTask(std::make_shared<MyTask>(501, 600));

	ULong sum1 = res1.get()._cast<ULong>();
	ULong sum2 = res2.get()._cast<ULong>();
	ULong sum3 = res3.get()._cast<ULong>();
	//ULong sum4 = res4.get()._cast<ULong>();
	//ULong sum5 = res5.get()._cast<ULong>();
	//ULong sum6 = res6.get()._cast<ULong>();


	// Master - Slave线程模型
	// Master用来分解任务，然后给各个Salve线程分配任务
	// 等待各个Slave线程执行完任务，返回结果
	// Master线程合并各个任务的结果，输出
	std::cout << (sum1 + sum2 + sum3 /*+ sum4 + sum5 + sum6*/) << std::endl;

	//ULong msum = 0;
	//for (ULong i = 1; i <= 600; i++)
	//	msum += i;

	//std::cout << "msum= " << msum << std::endl;

	getchar();
	return 0;
}

