#include "utils.h"
#ifdef WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace WKUtils
{
/////////////////// method  //////////////////
	unsigned int currentThreadId()
	{
#ifdef WIN32
		return GetCurrentThreadId();
#else
		return thread_self()
#endif
	};


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