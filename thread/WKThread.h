#pragma once
#ifndef WKThread_H
#define WKThread_H
#include "stdafx.h"

namespace WKUtils
{
	class WKLocker;
	class WKThreadTask;
	class WKThreadTaskQueue;
}

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
		Unknown
	};
	WKThread();
	virtual ~WKThread();	//析构时，仅保证当前任务被执行，其他任务会被丢弃
	void start();	//启动线程
	void quit();	//退出线程
	void wake();	//唤醒线程
	void wait();	//挂起线程
	void addTask(WKUtils::WKThreadTask* task);	//添加线程任务，并唤醒线程，
	const int getNextTaskSize();	//获取任务队列任务数

	size_t getThreadId() const;	//获取线程id

	bool isRunning() const;	//当前线程是否正在运行
	bool isFinished() const;	//线程任务是否执行完成
	bool isWaiting() const;	//线程是否处于挂起等待状态

	bool setGetNextTaskFunc(std::function<WKUtils::WKThreadTask* (void)>&& func);	//为线程池暴漏一个接口，在任务结束后可以通过func获取线程池下一个任务
	bool isHaveGetNextTaskFunc() const;

	bool isThreadRelased() const; //申请的线程资源是否被释放

protected:
	virtual void run();	//线程执行（用户）任务入口，若该方法未被重写使用方式2创建线程任务，否则使用方式1
private:
	void _runTask();	//通过std库获取到的线程入口
	void _updateState(ThreadState state);	//更新线程状态

private:
	size_t m_threadId = 0;
	bool m_haveGetNextTaskFunc = false;
	std::atomic<bool> m_bWaitFlag = false;
	std::atomic<bool> m_bQuitFlag = false;
	std::atomic<enum ThreadState> m_state = ThreadState::Unknown;
	std::mutex m_mutex;
	std::condition_variable m_condition;
	std::unique_ptr<std::thread>m_stdThreadPtr = nullptr;
	std::unique_ptr<WKUtils::WKLocker> m_WKLockerPtr = nullptr;
	std::unique_ptr<WKUtils::WKThreadTaskQueue> m_currTaskQueuePtr = nullptr;
	std::function<WKUtils::WKThreadTask*(void)> m_func;
};
#endif // WKThread_H
