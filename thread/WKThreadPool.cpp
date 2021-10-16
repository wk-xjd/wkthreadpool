#include "WKThreadPool.h"
#include "WKThread.h"
#include "utils.h"

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
	_init();
}
WKThreadPool::WKThreadPool(int maxThreadSize, int maxTaskSize)
	: m_maxTaskSize(maxTaskSize)
	, m_maxThreadSize(maxThreadSize)
{
	_init();
}

WKThreadPool::~WKThreadPool()
{
	//析构前释放所有线程
	stop();
	if (m_pollingThreadPtr)
	{
		if (m_pollingThreadPtr->joinable())
			m_pollingThreadPtr->join();
	}
}

void WKThreadPool::_init()
{
	m_threadQueueWKLocker = std::make_unique<WKUtils::WKLocker>();
	m_taskQueuePtr = std::make_unique<WKUtils::WKThreadTaskQueue>();
}

void WKThreadPool::start()
{
	m_bStoped = false;
	if (!m_pollingThreadPtr)
		m_pollingThreadPtr = std::make_unique<std::thread>(&WKThreadPool::_benginScheduler, this);
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

bool WKThreadPool::addTask(WKUtils::WKThreadTask* task)
{
	if (task && !m_bExiting && currentTaskSize() <= maxTaskSize())
	{
		if (m_taskQueuePtr->isEmpty())
		{
			m_taskQueuePtr->pushFrontTask(task);
			return true;
		}
		//任务优先级高从堆首插入
		if (task->taskPriorty() >= m_taskQueuePtr->getFrontTask()->taskPriorty())
		{
			m_taskQueuePtr->pushFrontTask(task);
		}
		else
		{
			m_taskQueuePtr->pushBackTask(task);
		}
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
	return m_taskQueuePtr->getTaskSize();
}

void WKThreadPool::_benginScheduler()
{
	while (!m_bStoped)
	{
		if (waitThreadCount() > 0)
		{
			WKUtils::WKThreadTask* temp_task = _getTask();
			if (!temp_task)
				goto LOOP_CHECK;
			WKThread * temp_wkThr = m_threadWaitQueue.front();

			if (temp_wkThr && temp_task)
			{
				temp_wkThr->addTask(temp_task);
				m_threadQueueWKLocker->lock();
				m_threadDoneQueue.push(temp_wkThr);
				m_threadWaitQueue.pop();
				m_threadDoneCount = m_threadDoneQueue.size();
				m_threadWaitCount = m_threadWaitQueue.size();
				m_threadQueueWKLocker->unlock();
			}
		}
		else if (doneThreadCount() + waitThreadCount() < maxThreadSize() && currentTaskSize() > 0)
		{
			_createThread();
		}
LOOP_CHECK:
		{
			_updateElasticInterval();
			_sleepIntrval(s_pollingLongIntrvalMs + m_sleepIntrval);		
			_checkThreadQueue();
		}

	}
}

void WKThreadPool::_createThread()
{
	static WKUtils::WKThreadTaskQueue* const s_taskQueuePtr = m_taskQueuePtr.get();
	static std::function<WKUtils::WKThreadTask* (void)> s_pFunc = [](){
		WKUtils::WKThreadTask* taskPtr = nullptr;
		if (s_taskQueuePtr && !s_taskQueuePtr->isEmpty())
			taskPtr =  s_taskQueuePtr->popFrontTask();
		return taskPtr;
	};
	
	WKThread* wkThrPtr = new WKThread();
	WKUtils::WKThreadTask* const taskPtr = _getTask();
	if (taskPtr)
	{
		wkThrPtr->addTask(taskPtr);
		wkThrPtr->setGetNextTaskFunc(std::forward<std::function<WKUtils::WKThreadTask* (void)> >(s_pFunc));
		wkThrPtr->start();
		m_threadQueueWKLocker->lock();
		m_threadDoneQueue.push(wkThrPtr);
		m_threadDoneCount = m_threadDoneQueue.size();
		m_threadQueueWKLocker->unlock();
	}
	else
	{
		m_threadQueueWKLocker->lock();
		m_threadWaitQueue.push(wkThrPtr);
		m_threadWaitCount = m_threadWaitQueue.size();
		m_threadQueueWKLocker->unlock();
	}
}

void WKThreadPool::_checkThreadQueue()
{
	//检查所有的工作线程，是否有挂起线程
	int checkLen = doneThreadCount();
	WKThread* temp_wkThr = nullptr;
	while (checkLen--)
	{
		m_threadQueueWKLocker->lock();
		temp_wkThr = m_threadDoneQueue.front();
		m_threadDoneQueue.pop();
		
		if (temp_wkThr->isWaiting())
			m_threadWaitQueue.push(temp_wkThr);
		else
			m_threadDoneQueue.push(temp_wkThr);
		m_threadDoneCount = m_threadDoneQueue.size();
		m_threadWaitCount = m_threadWaitQueue.size();
		m_threadQueueWKLocker->unlock();
	}
}

void WKThreadPool::_stopAllThread()
{
	//阻塞等待线程执行完毕
	std::unique_lock<std::mutex> locker(m_mutex);
	m_condition.wait(locker, [&]()
		{return currentTaskSize() == 0; });

	//释放线程对象
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

void WKThreadPool::_sleepIntrval(const int ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

WKUtils::WKThreadTask* WKThreadPool::_getTask()
{
	if (currentTaskSize() <= 0)
		return nullptr;

	WKUtils::WKThreadTask* const taskPtr = m_taskQueuePtr->popFrontTask();
	return taskPtr;
}

void WKThreadPool::_updateElasticInterval()
{
	//若空闲线程增多，则增加轮询线程休眠时间，否则缩短轮询线程休眠时间
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
}


