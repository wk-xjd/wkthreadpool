#pragma once
#ifndef WKThread_H
#define WKThread_H

#include <thread>
#include <atomic>
#include <mutex>   
#include <deque>
#include <condition_variable>
#include "WKLocker.h"

class WKThreadTask;

/*
	WKThread线程类使用方式：
	1.继承WKThread类，并重写run方法，
		通过start方法启动线程并执行，线程执行完毕自动释放该线程
	2.实例化WKThread对象，并将继承了WKThreadTask的线程任务添加到线程，
		2.1 通过start方法启动线程，
		2.2 线程任务执行完毕后，线程不会立即释放，
		2.3 线程任务执行完毕后进入挂起状态，等待下一次添加任务唤醒
		2.4 线程随实例化对象释放而释放，也可以通过quit方法释放线程
*/
class WKThread
{
public:
	enum ThreadState
	{
		Running,
		Finished,
		Waiting,
	};
	WKThread();
	virtual ~WKThread();
	void start();	//启动线程
	void quit();	//退出线程
	void wake();	//唤醒线程
	void wait();	//挂起线程
	bool addTask(WKThreadTask* task);	//添加线程任务，并唤醒线程，返回任务是否添加成功
	void setNextTaskQueue(std::deque<WKThreadTask*>* nextTaskQueue);	//传递一个任务队列，当前线程任务执行完毕后自动从任务队列中取任务
	void removeNextTaskQueue();	//移除任务队列
	const bool hasNextTaskQueue();	//是否有任务队列

	WKThread* getInstance();	//获取当前线程对象地址
	unsigned int getThreadId() const;	//获取线程id

	bool isRunning() const;	//当前线程是否正在运行
	bool isFinished() const;	//线程任务是否执行完成
	bool isWaiting() const;	//线程是否处于挂起等待状态

protected:
	virtual void run();	//线程执行（用户）任务入口，若该方法未被重写使用方式2创建线程任务，否则使用方式1

private:
	void _runTask();	//通过std库获取到的线程入口
	void _updateState(ThreadState state);	//更新线程状态

private:
	std::thread* m_stdThread = nullptr; 
	std::atomic<bool> m_bWaitFlag = false;
	std::atomic<bool> m_bQuitFlag = true;
	std::atomic<enum ThreadState> m_state = ThreadState::Waiting;
	std::mutex m_mutex;
	std::condition_variable m_condition;
	unsigned int m_threadId = 0;
	WKThreadTask* m_task = nullptr;
	WKLocker m_wkLocker;
	WKLocker m_nextQueueWKLocker;
	std::deque<WKThreadTask*>* m_nextTaskQueue = nullptr;
};
#endif // WKThread_H
