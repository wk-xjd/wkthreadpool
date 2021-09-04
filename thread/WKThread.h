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
	void start();
	void quit();
	void wake();
	void wait();
	bool addTask(WKThreadTask* task);
	void setNextTaskQueue(std::deque<WKThreadTask*>* nextTaskQueue);
	void removeNextTaskQueue();
	const bool hasNextTaskQueue();

	WKThread* getInstance();
	unsigned int getThreadId() const;

	bool isRunning() const;
	bool isFinished() const;
	bool isWaiting() const;

protected:
	virtual void run();

private:
	void _runTask();

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
	std::deque<WKThreadTask*>* m_nextTaskQueue = nullptr;
};
#endif // WKThread_H
