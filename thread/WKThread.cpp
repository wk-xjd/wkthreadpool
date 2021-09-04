#include "WKThread.h"
#include "WKThreadTask.h"
#include "utils.h"
#include "WKLocker.h"
#include "global.h"

WKThread::WKThread()
	: m_wkLocker(new WKLocker)
{

}

WKThread::~WKThread()
{
	if (!m_stdThread)
	{
		quit();
		delete m_stdThread;
		m_stdThread = nullptr;
	}	

	if (m_wkLocker)
	{
		delete m_wkLocker;
		m_wkLocker = nullptr;
	}
}

void WKThread::start()
{
	m_bWaitFlag = false;
	if (!m_stdThread)
		m_stdThread = new std::thread(&WKThread::_runTask, this);
}

void WKThread::quit()
{
	m_bQuitFlag = true;
	wake();
	if (m_stdThread && m_stdThread->joinable())
		m_stdThread->join();
}

void WKThread::wait()
{
	m_bWaitFlag = true;
}

void WKThread::wake()
{
	m_bWaitFlag = false;
	m_condition.notify_one();
}

bool WKThread::addTask(WKThreadTask* task)
{
	if (isRunning())
		return false;

	m_task = task;
	wake();
	return true;
}

void WKThread::setNextTaskQueue(std::deque<WKThreadTask*>* nextTaskQueue)
{
	m_wkLocker->lock();
	m_nextTaskQueue = nextTaskQueue;
	m_wkLocker->unlock();
}

void WKThread::removeNextTaskQueue()
{
	m_wkLocker->lock();
	m_nextTaskQueue = nullptr;
	m_wkLocker->unlock();
}

bool WKThread::hasNextTaskQueue() const
{
	m_wkLocker->lock();
	return m_nextTaskQueue != nullptr;
	m_wkLocker->unlock();
}

unsigned int WKThread::getThreadId() const
{
	if (isRunning())
		return m_threadId;
	else
		return 0;
}

WKThread* WKThread::getInstance()
{
	return this;
}

bool WKThread::isRunning() const
{
	m_wkLocker->lock();
	bool state = m_state == ThreadState::Running;
	m_wkLocker->unlock();
	return state;
}

bool WKThread::isFinished() const
{
	m_wkLocker->lock();
	bool state = m_state == ThreadState::Finished;
	m_wkLocker->unlock();
	return state;
}

bool WKThread::isWaiting() const
{
	m_wkLocker->lock();
	bool state = m_state == ThreadState::Waiting;
	m_wkLocker->unlock();
	return state;
}

void WKThread::run()
{
	m_bQuitFlag = false;
	if (m_task)
	{
		m_task->run();
		m_task->callback();
		if (m_task->isAutoDelete())
			delete m_task;
		m_task = nullptr;

		if (hasNextTaskQueue())
		{
			if (m_nextTaskQueue->empty())
			{
				setNextTaskQueue(nullptr);
			}
			else
			{
				g_WKTaskQueueLocker.lock();
				if (!m_nextTaskQueue->empty())
				{
					if (addTask(m_nextTaskQueue->front()))
						m_nextTaskQueue->pop_front();
				}
				g_WKTaskQueueLocker.unlock();
			}
		}
		else
			wait();
	}
	else
	{
		wait();
	}
}

void WKThread::_runTask()
{
	m_threadId = WKUtils::currentThreadId();
	do
	{
		m_wkLocker->lock();
		m_state = ThreadState::Running;
		m_wkLocker->unlock();
		run();
		if (m_bWaitFlag)
		{
			m_wkLocker->lock();
			m_state = ThreadState::Waiting;
			m_wkLocker->unlock();
			std::unique_lock<std::mutex> locker(m_mutex);
			while (m_bWaitFlag)
			{
				m_condition.wait(locker, [=]() {return !m_bWaitFlag;});
			}
			locker.unlock();
		}
		m_wkLocker->lock();
		m_state = ThreadState::Finished;
		m_wkLocker->unlock();
	} while (!m_bQuitFlag);
}

