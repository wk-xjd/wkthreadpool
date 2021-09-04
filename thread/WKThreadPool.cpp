#include "WKThreadPool.h"
#include "WKThreadTask.h"
#include "WKThread.h"
#include "WKLocker.h"
#include "global.h"
#include <chrono>

namespace
{
	const static int s_pollingLongIntrvalMs = 64;
	const static int s_pollingShortIntrvalMs = 4;
	const static int s_sleepIntrvalBound = 56;
	const static int s_changeIntrvalBound = 3;
}

WKThreadPool::WKThreadPool()
{
	m_maxThreadSize = std::thread::hardware_concurrency();
}
WKThreadPool::WKThreadPool(int maxThreadSize, int maxTaskSize)
	: m_maxTaskSize(maxTaskSize)
	, m_maxThreadSize(maxThreadSize)
{

}

WKThreadPool::~WKThreadPool()
{
	stop();
	if (m_pollingThread)
	{
		if (m_pollingThread->joinable())
			m_pollingThread->join();
		delete m_pollingThread;
		m_pollingThread = nullptr;
	}
}

void WKThreadPool::start()
{
	m_bStoped = false;
	if (!m_pollingThread)
		m_pollingThread = new std::thread(&WKThreadPool::_benginScheduler, this);
}

void WKThreadPool::stop()
{
	m_bExiting = true;
	_stopAllThread();
	m_bStoped = true;
}

bool WKThreadPool::isStop() const
{
	return m_bStoped;
}

bool WKThreadPool::addTask(WKThreadTask* task)
{
	if (task && !m_bExiting && currentTaskSize() <= maxTaskSize())
	{
		g_WKTaskQueueLocker.lock();
		if (m_taskQueue.empty())
		{
			m_taskQueue.push_back(task);
			m_taskCount = m_taskQueue.size();
			g_WKTaskQueueLocker.unlock();
			return true;
		}
		if (task->taskPriorty() >= m_taskQueue.front()->taskPriorty())
		{
			m_taskQueue.push_front(task);
		}
		else
		{
			m_taskQueue.push_back(task);
		}
		m_taskCount = m_taskQueue.size();
		g_WKTaskQueueLocker.unlock();
		return true;	
	}
	return false;
}

int WKThreadPool::doneThreadCount() const
{
	return m_threadDoneCount;
}

int WKThreadPool::waitThreadCount() const
{
	return m_threadWaitCount;
}

int WKThreadPool::maxThreadSize() const
{
	return m_maxThreadSize;
}

int WKThreadPool::maxTaskSize() const
{
	return m_maxTaskSize;
}

int WKThreadPool::currentTaskSize() const
{
	return m_taskCount;
}

void WKThreadPool::_benginScheduler()
{
	while (!m_bStoped)
	{
		if (waitThreadCount() > 0)
		{
			WKThreadTask* temp_task = _getTask();
			if (!temp_task)
				goto loop_check;
			WKThread* temp_wkThr = m_threadWaitQueue.front();

			if (temp_wkThr && temp_task)
			{
				temp_wkThr->addTask(temp_task);
				g_WKTaskQueueLocker.lock();
				m_threadDoneQueue.push(temp_wkThr);
				m_threadWaitQueue.pop();
				m_threadDoneCount = m_threadDoneQueue.size();
				m_threadWaitCount = m_threadWaitQueue.size();
				g_WKTaskQueueLocker.unlock();
			}
		}
		else if (doneThreadCount() + waitThreadCount() < maxThreadSize() && currentTaskSize() > 0)
		{
			_createThread();
		}
loop_check:
		{
			if (waitThreadCount() >= m_lastWaitThreadcount && m_lastWaitThreadcount > 0)
			{
				m_lastWaitThreadcount = waitThreadCount();
				if (m_waitThreadCountGradient >= 0)
					m_waitThreadCountGradient++;
				else
					m_waitThreadCountGradient = 0;
				 
				if (m_waitThreadCountGradient >= s_changeIntrvalBound)
				{
					m_waitThreadCountGradient = 0;
					m_sleepIntrval  += s_pollingShortIntrvalMs;
					if (m_sleepIntrval > s_sleepIntrvalBound)
						m_sleepIntrval = s_sleepIntrvalBound;
				}
					
			}
			else if (waitThreadCount() <= m_lastWaitThreadcount && m_lastWaitThreadcount > 0)
			{
				m_lastWaitThreadcount = waitThreadCount();
				if (m_waitThreadCountGradient <= 0)
					m_waitThreadCountGradient--;
				else
					m_waitThreadCountGradient = 0;

				if (m_waitThreadCountGradient >= s_changeIntrvalBound)
				{
					m_waitThreadCountGradient = 0;
					m_sleepIntrval -= s_pollingShortIntrvalMs;
					if (abs(m_sleepIntrval) > s_sleepIntrvalBound)
						m_sleepIntrval = -s_sleepIntrvalBound;
				}

			}
				
			_checkThreadQueue();
			_sleepIntrval(s_pollingLongIntrvalMs + m_sleepIntrval);
		}

	}
}

void WKThreadPool::_createThread()
{
	WKThread* temp_wkThr = new WKThread();
	WKThreadTask* temp_task = _getTask();
	if (temp_task)
	{
		temp_wkThr->addTask(temp_task);
		temp_wkThr->setNextTaskQueue(&m_taskQueue);
		temp_wkThr->start();
		g_WKTaskQueueLocker.lock();
		m_threadDoneQueue.push(temp_wkThr);
		m_threadDoneCount = m_threadDoneQueue.size();
		g_WKTaskQueueLocker.unlock();
	}
	else
	{
		g_WKTaskQueueLocker.lock();
		m_threadWaitQueue.push(temp_wkThr);
		m_threadWaitCount = m_threadWaitQueue.size();
		g_WKTaskQueueLocker.unlock();
	}
}

void WKThreadPool::_checkThreadQueue()
{
	int checkLen = doneThreadCount();
	WKThread* temp_wkThr = nullptr;
	while (checkLen--)
	{
		g_WKTaskQueueLocker.lock();
		temp_wkThr = m_threadDoneQueue.front();
		m_threadDoneQueue.pop();
		
		if (temp_wkThr->isWaiting())
			m_threadWaitQueue.push(temp_wkThr);
		else
			m_threadDoneQueue.push(temp_wkThr);
		m_threadDoneCount = m_threadDoneQueue.size();
		m_threadWaitCount = m_threadWaitQueue.size();
		g_WKTaskQueueLocker.unlock();
	}
}

void WKThreadPool::_stopAllThread()
{
	//等待任务队列执行完毕
	std::unique_lock<std::mutex> locker(m_mutex);
	m_condition.wait(locker, [&]()
		{return currentTaskSize() == 0; });

	WKThread* temp_wkThr = nullptr;
	while (!m_threadWaitQueue.empty())
	{
		temp_wkThr = m_threadWaitQueue.front();
		if (temp_wkThr)
		{
			delete temp_wkThr;
			temp_wkThr = nullptr;
		}
		m_threadWaitQueue.pop();

	}

	while (!m_threadDoneQueue.empty())
	{
		temp_wkThr = m_threadDoneQueue.front();
		if (temp_wkThr)
		{
			delete temp_wkThr;
			temp_wkThr = nullptr;
		}
		m_threadDoneQueue.pop();
	}

}

void WKThreadPool::_clearTaskQueue()
{
	m_taskQueue.clear();
}

void WKThreadPool::_sleepIntrval(const int ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

WKThreadTask* WKThreadPool::_getTask()
{
	if (currentTaskSize() <= 0)
		return nullptr;

	g_WKTaskQueueLocker.lock();
	WKThreadTask* temp_task = m_taskQueue.front();
	m_taskQueue.pop_front();
	m_taskCount = m_taskQueue.size();
	g_WKTaskQueueLocker.unlock();
	if (m_bExiting)
		m_condition.notify_all();
	return temp_task;
}



