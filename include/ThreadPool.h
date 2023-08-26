#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <iostream>
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <unordered_map>

// Any类型: 可以接收任意数据的类型
class Any
{
public:
	Any() = default;
	~Any() = default;
	Any(const Any&) = delete;
	Any& operator=(const Any&) = delete;
	Any(Any&&) = default;
	Any& operator=(Any&&) = default;

	// 这个构造函数可以让Any类型接收任意其他类型
	template<typename T>
	Any(T data)
		:_base(std::make_unique<Derive<T>>(data)) {}

	// 把Any对象里面存储的data数据提取出来
	template<typename T>
	T _cast()
	{
		// 我们怎么从_base找到他所指向的Derive对象，从他里面提取出data成员变量
		Derive<T>* pd = dynamic_cast<Derive<T>*>(_base.get()); // RTTI
		if (pd == nullptr)
		{
			throw "type is unmatch!";
		}
		return pd->_data;
	}

private:
	// 基类类型
	class Base
	{
	public:
		virtual ~Base() = default;
	};

	// 派生类类型
	template<typename T>
	class Derive : public Base
	{
	public:
		Derive(T data)
			:_data(data) {}

		T _data;
	};

private:
	// 定义一个基类指针
	std::unique_ptr<Base>  _base;
};

// 实现信号量
class Semaphore
{
public:
	Semaphore(int limit = 0)
		:_isExit(false)
		, _resLimit(limit)
	{}

	~Semaphore()
	{
		_isExit = true;
	}

	// 获取一个信号量资源
	void wait() // P操作
	{
		if (_isExit)
			return;
		std::unique_lock<std::mutex> lock(_mtx);
		// 等待信号量资源，没有资源的话阻塞当前线程
		_cond.wait(lock, [&]()->bool {return _resLimit > 0; });
		_resLimit--;
	}

	// 增加一个信号量资源
	void post() // V操作
	{
		if (_isExit)
			return;
		std::unique_lock<std::mutex> lock(_mtx);
		_resLimit++;
		_cond.notify_all();
	}


private:
	std::atomic_bool _isExit;
	std::mutex _mtx;
	size_t _resLimit;
	std::condition_variable _cond;
};

class Result;

class Task
{
public:
	Task();
	~Task() = default;

	// 声明纯虚函数
	virtual Any run() = 0;

	void exec();
	void setResult(Result* result);

private:
	Result* _result;  // result对象生命周期强于task
};

// 实现接收提交到线程池的任务完成的返回值类型
class Result
{
public:
	Result(std::shared_ptr<Task> task, bool isValid = true);
	~Result() = default;

	// 1.setValue方法，如何获取任务执行完的返回值?
	void setVal(Any any);
	// 2.如何实现用户调用get方法获取task的返回值？
	Any get();

private:
	Any _any; // 存储任务返回值
	Semaphore _sem; // 信号量(用于线程通信)
	std::shared_ptr<Task> _task; // 指向对应获取返回值的任务对象
	std::atomic_bool _isValid; // 返回值是否有效
};


// 线程池支持的模式
enum class ThreadPoolMode
{
	FIXED, //固定数量的线程
	CACHED, //线程数量可以动态增长
};

// 线程池执行任务的基类(用户继承重写run方法，实现自定义任务处理 )


// 线程类型
class Thread
{
public:
	using ThreadFunc = std::function<void(size_t)>;

	Thread(ThreadFunc func);

	~Thread();

	// 启动线程
	void start();

	// 获取线程id
	size_t getId() const;

private:
	ThreadFunc _func;
	static size_t _generateId;
	size_t _threadId; // 报错线程id

};

// 线程池类型
class ThreadPool
{
public:

	ThreadPool();
	~ThreadPool();

	// 开启线程池
	void Start(size_t initThreadSize = std::thread::hardware_concurrency());

	// 设置线程池工作模式
	void SetThreadPoolMode(ThreadPoolMode mode);

	// 在cache模式下设置线程的阈值
	void SetThreadSizeThreshHold(size_t threshhold);

	// 设置task任务队列上限的阈值
	void SetTaskQueueMaxThreshHold(size_t threshhold);

	// 给线程池提交任务
	Result SubmitTask(std::shared_ptr<Task> sp);

	ThreadPool(const ThreadPool&) = delete;
	ThreadPool& operator=(const ThreadPool&) = delete;

private:
	// 定义线程函数
	void threadFunc(size_t threadid);

	// 检查pool的运行状态
	bool CheckRunningState() const;

private:
	//std::vector<std::unique_ptr<Thread>> _threads; // 线程列表
	std::unordered_map<size_t, std::unique_ptr<Thread>> _threads;

	size_t _initThreadSize; // 初始的线程数量
	size_t _threadSizeThreshHold;	// 线程上限阈值
	std::atomic_uint _idleThreadSize; // 空闲线程的数量


	std::queue<std::shared_ptr<Task>> _taskQueue; //任务队列
	std::atomic_uint _taskSize;	// 任务数量
	size_t _taskSizeThreshold; //任务队列数量上限的阈值

	std::mutex _taskQueueMtx; // 保证任务队列的线程安全
	std::condition_variable _notFull;  // 任务队列是否已满
	std::condition_variable _notEmpty; // 任务队列是否为空
	std::condition_variable _exitCond; // 等待线程资源全部回收

	ThreadPoolMode _threadPoolMode; //线程池工作模式
	std::atomic_bool _isPoolRunning; // 表示当前线程池的启动状态


};
#endif
