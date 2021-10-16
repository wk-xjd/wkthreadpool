#include "utils.h"
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace WKUtils
{
/////////////////// method  //////////////////
	size_t currentThreadId()
	{
#ifdef WIN32
		return GetCurrentThreadId();
#else
		return thread_self()
#endif
	};
////////////////// WKLocker  //////////////////
	WKLocker::WKLocker(int spinTimes)
		: m_spinCount(spinTimes)
	{

	}
	WKLocker::~WKLocker()
	{

	}

	void WKLocker::lock()
	{
		//获取当前线程id
		size_t curThrId = WKUtils::currentThreadId();
		//自旋次数
		std::atomic<int> currCount = m_spinCount;
		//自旋失败获取锁失败则阻塞

		if (_checkLockThread(curThrId))
		{
			return;
		}

	START_GET_LOCK:
		{
			while (!m_currentThreadId.compare_exchange_weak(m_noLockState, curThrId))
			{
				if (!currCount.fetch_sub(1))
					break;
				else
					std::this_thread::yield();
			}
			//自旋失败获取锁失败则阻塞
			if (!_checkLockThread(curThrId))
			{
				std::unique_lock<std::mutex> locker(m_muetex);
				m_condition.wait(locker, [&]()
					{return m_currentThreadId == m_noLockState; });
				locker.unlock();
				//锁被让出后，重新获取锁
				goto START_GET_LOCK;
			}
		}
	}

	void WKLocker::unlock()
	{
		size_t curThrId = WKUtils::currentThreadId();

		if (m_currentThreadId == curThrId)
		{
			if (!(--m_lockCount))
			{
				//锁释放后唤醒所有阻塞在该锁的线程
				m_currentThreadId.store(m_noLockState);
				m_condition.notify_all();
				m_lockCount = 0;
			}

		}
	}

	bool WKLocker::isLock() const
	{
		return m_currentThreadId == m_noLockState;
	}

	bool WKLocker::_checkLockThread(size_t& threadId)
	{
		if (m_currentThreadId == threadId)
		{
			m_lockCount++;	//重入
			return true;
		}

		return false;
	}

/////////////////////WKThreadTaskQueue///////////////////////
	WKThreadTaskQueue::WKThreadTaskQueue()
	{

	}
	
	WKThreadTaskQueue::~WKThreadTaskQueue()
	{

	}

	void WKThreadTaskQueue::pushFrontTask(WKThreadTask* task)
	{
		if (!task)
			return;

		m_locker.lock();
		m_taskQueue.push_front(task);
		++m_taskSize;
		m_locker.unlock();
	}

	void WKThreadTaskQueue::pushBackTask(WKThreadTask* task)
	{
		if (!task)
			return;

		m_locker.lock();
		m_taskQueue.push_back(task);
		++m_taskSize;
		m_locker.unlock();
	}

	WKThreadTask* WKThreadTaskQueue::popFrontTask()
	{
		WKThreadTask* task = nullptr;
		if (isEmpty())
			return task;

		m_locker.lock();
		task = m_taskQueue.front();
		m_taskQueue.pop_front();
		--m_taskSize;
		m_locker.unlock();
		return task;
	}

	WKUtils::WKThreadTask* WKThreadTaskQueue::popBackTask()
	{
		WKUtils::WKThreadTask* task = nullptr;
		if (isEmpty())
			return task;
		m_locker.lock();
		task = m_taskQueue.back();
		m_taskQueue.pop_back();
		--m_taskSize;
		m_locker.unlock();
		return task;
	}

	void WKThreadTaskQueue::removeTask(WKThreadTask* task) 
	{
		if (!task || isEmpty())
			return;

		const unsigned int taskId = task->getTaskId();
		m_locker.lock();
		std::deque<WKThreadTask*>::const_iterator iter = m_taskQueue.cbegin();
		while (iter != m_taskQueue.cend() && (*iter)->getTaskId() != taskId)
		{
			++iter;
		}

		if (iter != m_taskQueue.cend())
		{
			m_taskQueue.erase(iter);
			m_taskSize--;
		}
		m_locker.unlock();
	}

	bool WKThreadTaskQueue::isEmpty() const
	{
		return m_taskSize == 0;
	}

	int WKThreadTaskQueue::getTaskSize() const
	{
		return m_taskSize;
	}

	WKThreadTask* WKThreadTaskQueue::getFrontTask() const
	{
		return m_taskQueue.front();
	}

	WKThreadTask* WKThreadTaskQueue::getbackTask() const
	{
		return m_taskQueue.back();
	}
/////////////////////WKThreadTaskQueue end///////////////////////
}