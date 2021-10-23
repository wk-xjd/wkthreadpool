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
	void setEnableAutoRecoveryThread(const bool enable = true); //设置自动回收空闲线程
	bool isEnableAutoRecoveryThread() const;	//是否自动回收空闲线程

private:
	void _init();	//初始化
	void _benginScheduler();	//调度管理
	void _createThread();	//创建一个新的线程
	void _checkThreadQueue();	//检查线程运行情况，将空闲线程置于空闲线程队列中
	void _stopAllThread();	//停止加入新任务，执行完队列中的任务
	void _sleepIntrval(const int ms);	//调度间隔
	WKUtils::WKThreadTask* _getTask();	//取一个队首任务
	bool _needRecovery();	//判断是否需要回线程
	void _recoveryNoCoreThread(); //回收非核心线程
	void _freeWaitThread(int threadNum); //释放等待线程
	void _freeDoneThread(int threadNum);	//释放工作线程
	void _updateWaitThreadCount();	//更新等待线程数
	void _updateDoneThreadCount();	//更新工作线程数

private:
	int m_maxThreadSize = 0;	//最大线程数
	int m_maxTaskSize = 30;		//最大任务数
	int m_lastWaitThreadcount = 0;	//上次空闲线程数
	int m_coreThreadSize = 0;	//线程池维护最少核心线程数
	int m_currCheckWaitThreadTimes = 0;


	std::atomic<bool> m_isEnableAutoRecoveryThread = true;	//是否启用空闲线程自动回收
	std::atomic<bool> m_bStoped = true;
	std::atomic<int> m_threadDoneCount = 0;	//工作线程数
	std::atomic<int> m_threadWaitCount = 0;	//空闲线程数
	std::atomic<int> m_taskCount = 0;	// 当前任务数
	std::atomic<bool> m_bExiting = false;

	std::condition_variable m_condition;
	std::mutex m_mutex;
	std::queue<WKThread*> m_threadDoneQueue;	//工作线程队列
	std::queue<WKThread*> m_threadWaitQueue;	//等待（空闲）线程队列
	
	std::unique_ptr<std::thread> m_pollingThreadPtr = nullptr;
	std::unique_ptr<WKUtils::WKThreadTaskQueue> m_taskQueuePtr = nullptr;
	std::unique_ptr<WKUtils::WKLocker> m_threadQueueWKLocker = nullptr;
};
#endif