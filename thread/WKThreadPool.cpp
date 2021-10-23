#include "WKThreadPool.h"
#include "WKThread.h"
#include "utils.h"

namespace
{
	const static int s_pollingLongIntrvalMs = 64;
	const static int s_pollingMiddleIntrvalMs = 32;
	const static int s_pollingShortIntrvalMs = 16;
	const static int s_checkWaitThreadTimes = 32;
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
	m_coreThreadSize = std::thread::hardware_concurrency() / 2;
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
	return m_threadDoneCount.load();
}

int WKThreadPool::waitThreadCount() const
{
	return m_threadWaitCount.load();
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

void WKThreadPool::setEnableAutoRecoveryThread(const bool enbale)
{
	m_isEnableAutoRecoveryThread.store(enbale);
}

bool WKThreadPool::isEnableAutoRecoveryThread() const
{
	return m_isEnableAutoRecoveryThread.load();
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
				_updateDoneThreadCount();
				_updateWaitThreadCount();
				m_threadQueueWKLocker->unlock();
			}
		}
		else if (doneThreadCount() + waitThreadCount() < maxThreadSize() && currentTaskSize() > 0)
		{
			_createThread();
		}
LOOP_CHECK:
		{
			_checkThreadQueue();
			_sleepIntrval(s_pollingShortIntrvalMs);

			//逐级递增sleep时间
			if (!m_bExiting)
				_sleepIntrval(s_pollingMiddleIntrvalMs);

			if (!m_bExiting)
				_sleepIntrval(s_pollingLongIntrvalMs);	
			
			if (_needRecovery())
				_recoveryNoCoreThread();
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
		_updateDoneThreadCount();
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
		_updateDoneThreadCount();
		_updateWaitThreadCount();
		m_threadQueueWKLocker->unlock();
	}
}

void WKThreadPool::_stopAllThread()
{
	//阻塞等待线程执行完毕
	std::unique_lock<std::mutex> locker(m_mutex);
	m_condition.wait(locker, [&]()
		{return currentTaskSize() == 0; });

	_freeWaitThread(waitThreadCount());
	_freeDoneThread(doneThreadCount());
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

bool WKThreadPool::_needRecovery()
{
	if (!isEnableAutoRecoveryThread() || waitThreadCount() == 0)
		return false;

	if (m_lastWaitThreadcount >= waitThreadCount())
		++m_currCheckWaitThreadTimes;
	else
		m_currCheckWaitThreadTimes = 0;

	m_lastWaitThreadcount = waitThreadCount();

	return (m_currCheckWaitThreadTimes < s_checkWaitThreadTimes);
}

void WKThreadPool::_recoveryNoCoreThread()
{
	int recoverySize = (waitThreadCount() - m_coreThreadSize + 1) / 2;
	_freeWaitThread(recoverySize);
}


void WKThreadPool::_freeDoneThread(int threadNum)
{
	if (threadNum <= 0 || threadNum > doneThreadCount())
		return;

	WKThread* temp_wkThr = nullptr;
	while (threadNum--)
	{
		m_threadQueueWKLocker->lock();
		temp_wkThr = m_threadDoneQueue.front();
		m_threadDoneQueue.pop();
		_updateDoneThreadCount();
		m_threadQueueWKLocker->unlock();

		if (temp_wkThr)
		{
			delete temp_wkThr;
			temp_wkThr = nullptr;
		}
	}
}

void WKThreadPool::_freeWaitThread(int threadNum)
{
	if (threadNum <= 0 || threadNum > waitThreadCount())
		return;

	WKThread* temp_wkThr = nullptr;
	while (threadNum--)
	{
		m_threadQueueWKLocker->lock();
		WKThread* temp_wkThr = m_threadWaitQueue.front();
		m_threadWaitQueue.pop();
		_updateWaitThreadCount();
		m_threadQueueWKLocker->unlock();

		if (temp_wkThr)
		{
			delete temp_wkThr;
			temp_wkThr = nullptr;
		}
	}
}

void WKThreadPool::_updateDoneThreadCount()
{
	m_threadDoneCount.store(m_threadDoneQueue.size());
}

void WKThreadPool::_updateWaitThreadCount()
{
	m_threadWaitCount.store(m_threadWaitQueue.size());
}

