// 定义应用程序的入口点。
//
#include "thread/WKThread.h"
#include "thread/WKThreadTask.h"
#include "thread/WKThreadPool.h"
#include <iostream>
class A :public WKThread
{
protected:
	virtual void run() {
		std::cout << "====run A:" << std::endl;
		std::cout << "thread id:" << std::this_thread::get_id() << std::endl;
	}
};

class B : public WKThreadTask
{
public:
	~B()
	{
		std::cout << "====delete tak B:" << getTaskId() << std::endl;
		std::cout << "thread id:" << std::this_thread::get_id() << std::endl;
	}
	virtual void run()
	{
		std::cout << "====run tak B:" << getTaskId()<< std::endl;
		std::cout << "thread id:" << std::this_thread::get_id() << std::endl;
		_sleep(1000);
	} 
};

//线程类测试
void test1()
{
	//方式一创建线程
	A a;
	a.start();

	//方式2创建线程
	WKThread thr;
	thr.addTask(new B);
	thr.start();

	std::cout << "thread state" << a.isRunning() << std::endl;
	_sleep(10);
	std::cout << "thread state" << a.isRunning() << std::endl;
	std::cout << "thread id:" << a.getThreadId() << std::endl;
	_sleep(10);
	std::cout << "thread state" << a.isFinished() << std::endl;

	std::cout << "thread state thr" << thr.isRunning() << std::endl;
	std::cout << "thread state" << thr.isWaiting() << std::endl;
}

//线程池测试
void test2()
{
	WKThreadPool* pool = new WKThreadPool; //创建线程池
	pool->start();	//启动线程池

	//添加任务
	int i = 10;
	std::cout << "========================threadpool size" << pool->maxThreadSize()<< "=================" << std::endl;
	B* b;
	while (i--)
	{
		b = new B;
		b->setAutoDelete(true);	//自动清理任务
		pool->addTask(b);
	}
	
	while (1)
	{
		_sleep(100);
		std::cout << "====================" << std::endl;
		std::cout << "donethread:" <<pool->doneThreadCount() << "   waitthread:" << pool->waitThreadCount() << "  tasksize" << pool->currentTaskSize();
		std::cout << "====================" << std::endl;
		if (pool->currentTaskSize() <= 0)
			break;

	}
}

int main()
{
	std::cout << "====main run" << std::endl;
	std::cout << "thread id:" << std::this_thread::get_id() << std::endl;
	test2();
	while (1);	//主线程阻塞
	return 0;
}
