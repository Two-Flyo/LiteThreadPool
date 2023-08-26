#include "ThreadPool.h"


const size_t TASK_MAX_THRESHHOLD = INT32_MAX;
const size_t THREAD_MAX_THRESHHOLD = 1024;
const size_t THREAD_MAX_IDLE_TIME = 60; // 单位: s  


///////////////////// ThreadPool方法实现 ////////////////////////////

// 线程池构造函数
ThreadPool::ThreadPool()
	: _initThreadSize(0)
	, _threadSizeThreshHold(THREAD_MAX_THRESHHOLD)
	, _idleThreadSize(0)
	, _taskSize(0)
	, _taskSizeThreshold(TASK_MAX_THRESHHOLD)
	, _threadPoolMode(ThreadPoolMode::FIXED)
	, _isPoolRunning(false)
{}

// 线程池析构函数
ThreadPool::~ThreadPool()
{
	_isPoolRunning = false;
	_notEmpty.notify_all();
	// 等待线程池里所有的线程返回 => 有两种状态：阻塞/正在执行任务
	std::unique_lock<std::mutex> lock(_taskQueueMtx);
	_exitCond.wait(lock, [&]() {return _threads.size() == 0; });


}

// 开启线程池
void ThreadPool::Start(size_t initThreadSize)
{
	// 设置线程池的启动状态
	_isPoolRunning = true;

	// 记录初始的线程个数
	_initThreadSize = initThreadSize;

	// 创建线程对象
	for (size_t i = 0; i < _initThreadSize; i++)
	{
		// 创建thread线程对象的时候，把线程函数给到thread线程对象
		auto ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		//_threads.emplace_back(std::move(ptr));
		size_t threadId = ptr->getId();
		_threads.emplace(threadId, std::move(ptr));
	}



	// 启动所有线程
	for (size_t i = 0; i < _initThreadSize; i++)
	{
		// 启动线程
		_threads[i]->start();
		_idleThreadSize++;
	}
}

// 定义线程函数
void ThreadPool::threadFunc(size_t threadid)
{
	auto lastTime = std::chrono::high_resolution_clock().now();

	while (_isPoolRunning)
	{
		std::shared_ptr<Task> task;
		{
			// 先获取锁
			std::unique_lock<std::mutex> lock(_taskQueueMtx);

			std::cout << "tid: " << std::this_thread::get_id() << "尝试获取任务..." << std::endl;

			// 在cache模式下，也可能已经创建了很多线程，但是空闲时间超过60s,应该把多余线程结束回收掉
			// (超过_initThreadSize数量的线程要进行回收)
			// 当前时间 - 上一次线程执行的时间 > 60s


		// 每一秒返回一次 怎么区分: 超时返回/有任务待执行返回
			while (_taskQueue.size() == 0)
			{
				if (_threadPoolMode == ThreadPoolMode::CACHED)
				{
					// 条件变量超时返回
					if (_notEmpty.wait_for(lock, std::chrono::seconds(1)) == std::cv_status::timeout)
					{
						auto now = std::chrono::high_resolution_clock().now();
						auto dur = std::chrono::duration_cast<std::chrono::seconds>(now - lastTime);
						if (dur.count() >= 60 && _threads.size() > _initThreadSize)
						{
							// 开始回收当前线程
							// 记录线程数量的相关变量值进行修改
							// 把线程对象从线程列表容器中删除  没有办法匹配 threadFunc 对应哪一个thread对象
							// threadid <=> thread对象 => 删除
							_threads.erase(threadid); //不要传入
							_idleThreadSize--;

							std::cout << "threadid: " << std::this_thread::get_id() << "exit" << std::endl;
							return;
						}
					}
				}
				else
				{
					// 等待_notEmpty条件
					_notEmpty.wait(lock);
				}

				// 线程池要结束，回收线程资源
				if (!_isPoolRunning)
				{
					_threads.erase(threadid);
					std::cout << "threadid:" << std::this_thread::get_id() << "exit!" << std::endl;
					// 线程都要结束了，没必要去维护相关变量了
					_exitCond.notify_all();
					return;
				}
			}

			_idleThreadSize--;

			std::cout << "tid: " << std::this_thread::get_id() << "获取任务成功..." << std::endl;

			// 从任务队列取一个任务出来
			task = _taskQueue.front();
			_taskQueue.pop();
			_taskSize--;

			// 如果依然有剩余任务，继续通知其他线程执行任务
			if (_taskQueue.size() > 0)
			{
				_notEmpty.notify_all();
			}
			// 取出一个任务，进行通知可以继续进行生成任务了
			_notFull.notify_all();
		}// 把锁释放掉
		// 当前线程负责执行这个任务
		if (task != nullptr)
		{
			// 执行任务,把任务的返回值setVal方法给到Result
			//task->run();
			task->exec();
			// 更新线程执行完线程的时间
		}

		_idleThreadSize++;
		lastTime = std::chrono::high_resolution_clock().now();
	}

	_threads.erase(threadid);
	std::cout << "threadid:" << std::this_thread::get_id() << "exit" << std::endl;
	_exitCond.notify_all();


}


// 设置线程池工作模式
void ThreadPool::SetThreadPoolMode(ThreadPoolMode mode)
{
	if (CheckRunningState())
		return;
	_threadPoolMode = mode;
}


// 设置task任务队列上限的阈值
void ThreadPool::SetTaskQueueMaxThreshHold(size_t threshhold)
{
	if (CheckRunningState())
		return;
	_taskSizeThreshold = threshhold;
}

void ThreadPool::SetThreadSizeThreshHold(size_t threshhold)
{
	if (CheckRunningState())
		return;

	if (_threadPoolMode == ThreadPoolMode::CACHED)
		_threadSizeThreshHold = threshhold;
}


// 给线程池提交任务
Result ThreadPool::SubmitTask(std::shared_ptr<Task> sp)
{
	// 获取锁
	std::unique_lock<std::mutex> lock(_taskQueueMtx);

	// 线程通信: 等待任务队列不满
	// 用户提交任务，最长不能阻塞超过1s，否则判定任务提交失败   
	// lambda表达式结果为false就阻塞
	if (!_notFull.wait_for(lock, std::chrono::seconds(1), [&]()->bool {return _taskQueue.size() < _taskSizeThreshold; }))
	{
		// wait_for ->return false; 表示等待1s，条件依然没有满足
		std::cerr << "task queue is full, submit task failed" << std::endl;
		// return task->getResult(); 不可取: 线程执行结束后就析构掉了
		return Result(sp, false);
	}
	// 如果有空余，把任务放到任务队列中
	_taskQueue.emplace(sp);
	_taskSize++;

	// 因为新放了任务，任务队列肯定不为空，在_isEmpty上通知
	_notEmpty.notify_all();

	// cache模式需要根据任务数量和空闲线程的数量，判断是否需要创建新的线程？
	if (_threadPoolMode == ThreadPoolMode::CACHED
		&& _taskSize > _idleThreadSize
		&& _threads.size() < _threadSizeThreshHold)
	{
		std::cout << ">>> create new thread... " << std::endl;
		// 创建新线程
		std::unique_ptr<Thread> ptr = std::make_unique<Thread>(std::bind(&ThreadPool::threadFunc, this, std::placeholders::_1));
		size_t threadId = ptr->getId();
		_threads.emplace(threadId, std::move(ptr));
		_threads[threadId]->start(); //启动线程

		// 修改线程个数相关的变量
		_idleThreadSize++;
	}

	// 返回任务的result对象
	// 1.return task->getResult();
	return Result(sp);

}

bool ThreadPool::CheckRunningState() const
{
	return _isPoolRunning;
}



///////////////////// Thread方法实现 ////////////////////////////

size_t Thread::_generateId = 0;

//构造函数
Thread::Thread(ThreadFunc func)
	: _func(func)
	, _threadId(_generateId++) {}

// 析构函数
Thread::~Thread()
{}

void Thread::start()
{
	// 执行一个线程函数
	std::thread t(_func, _threadId);
	t.detach(); // 线程分离
}

size_t Thread::getId() const
{
	return _threadId;
}

/////////////////// Task方法实现 //////////////////////////

Task::Task()
	:_result(nullptr)
{

}

void Task::exec()
{
	if (_result != nullptr)
	{
		_result->setVal(run()); // 这里发生多态调用
	}
}

void Task::setResult(Result* result)
{
	_result = result;
}


/////////////// Result方法实现 //////////////////////
Result::Result(std::shared_ptr<Task> task, bool isValid)
	: _task(task)
	, _isValid(isValid)
{
	_task->setResult(this);
}

Any Result::get() // 用户调用
{
	if (!_isValid)
	{
		return "";
	}
	_sem.wait(); // task任务如果没有执行完，这里会阻塞用户的线程
	return std::move(_any);
}

void Result::setVal(Any any)
{
	// 存储task的返回值 
	this->_any = std::move(any);
	_sem.post(); // 已经获取到任务的返回值，增加信号量资源
}
