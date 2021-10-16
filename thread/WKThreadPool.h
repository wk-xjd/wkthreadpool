#ifndef WKTHREADPOOL_H
#define WKTHREADPOOL_H
#include "stdafx.h"

namespace WKUtils
{
	class WKLocker;
	class WKThreadTask;
	class WKThreadTaskQueue;
}
class WKThread;


/*
	线程池
	1.懒创建线程，有任务且线程数小于最大线程数时创建新的线程
	2.动态调整轮询检查线程状态的间隔
*/
class WKThreadPool
{
public:
	WKThreadPool();	
	WKThreadPool(int maxThreadSize, int maxTaskSize);
	virtual ~WKThreadPool();
	void start();	//启动线程池
	void stop();	//停止线程池,停止加入任务
	bool isStop() const;	//线程池是否被终止
	bool addTask(WKUtils::WKThreadTask* task);	//添加任务，返回任务添加结果
	int waitThreadCount() const; 	//挂起等待线程数
	int doneThreadCount() const; 	//运行工作线程数
	int maxThreadSize() const;	//最大线程数
	int maxTaskSize() const;	//最大任务队列最大任务数
	int currentTaskSize() const; 	//当前任务数

private:
	void _init();	//初始化
	void _benginScheduler();	//调度管理
	void _createThread();	//创建一个新的线程
	void _checkThreadQueue();	//检查线程运行情况，将空闲线程置于空闲线程队列中
	void _stopAllThread();	//停止加入新任务，执行完队列中的任务
	void _sleepIntrval(const int ms);	//调度间隔
	WKUtils::WKThreadTask* _getTask();	//取一个队首任务
	void _updateElasticInterval(); // 更新弹性的检查间隔，工作线程增多则间隔检查间隔减少，否则增加
	
private:
	int m_maxThreadSize = 0;
	int m_maxTaskSize = 30;
	int m_lastWaitThreadcount = 0;
	int m_waitThreadCountGradient = 0;
	int m_sleepIntrval = 0;
	
	std::atomic<bool> m_bStoped = true;
	std::atomic<int> m_threadDoneCount = 0;
	std::atomic<int> m_threadWaitCount = 0;
	std::atomic<int> m_taskCount = 0;
	std::atomic<bool> m_bExiting = false;

	std::condition_variable m_condition;
	std::mutex m_mutex;
	std::queue<WKThread*> m_threadDoneQueue;
	std::queue<WKThread*> m_threadWaitQueue;
	
	std::unique_ptr<std::thread> m_pollingThreadPtr = nullptr;
	std::unique_ptr<WKUtils::WKThreadTaskQueue> m_taskQueuePtr = nullptr;
	std::unique_ptr<WKUtils::WKLocker> m_threadQueueWKLocker = nullptr;
};
#endif