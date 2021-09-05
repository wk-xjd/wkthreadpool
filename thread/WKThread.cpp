#include "WKThread.h"
#include "WKThreadTask.h"
#include "utils.h"
#include "global.h"

WKThread::WKThread()
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
	m_nextQueueWKLocker.lock();
	m_nextTaskQueue = nextTaskQueue;
	m_nextQueueWKLocker.unlock();
}

void WKThread::removeNextTaskQueue()
{
	m_nextQueueWKLocker.lock();
	m_nextTaskQueue = nullptr;
	m_nextQueueWKLocker.unlock();
}

const bool WKThread::hasNextTaskQueue() 
{
	m_nextQueueWKLocker.lock();
	bool ret =  m_nextTaskQueue != nullptr;
	m_nextQueueWKLocker.unlock();
	return ret;
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
	return m_state == ThreadState::Running;
}

bool WKThread::isFinished() const
{
	return m_state == ThreadState::Finished;
}

bool WKThread::isWaiting() const
{
	return m_state == ThreadState::Waiting;
}

void WKThread::run()
{
	m_bQuitFlag = false;
	if (m_task)
	{
		//执行线程任务
		m_task->run();
		m_task->callback();
		if (m_task->isAutoDelete())
			delete m_task;
		m_task = nullptr;
		//任务执行完毕后，取下一个任务
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
		{
			wait(); // 没有下一个任务挂起线程
		}
			
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
		_updateState(ThreadState::Running);
		run();
		if (m_bWaitFlag)
		{
			_updateState(ThreadState::Waiting);
			std::unique_lock<std::mutex> locker(m_mutex);
			while (m_bWaitFlag)
			{
				m_condition.wait(locker, [=]() {return !m_bWaitFlag;});
			}
			locker.unlock();
		}
		_updateState(ThreadState::Finished);
	} while (!m_bQuitFlag);
}

void WKThread::_updateState(ThreadState state)
{
	m_wkLocker.lock();
	m_state = state;
	m_wkLocker.unlock();
}

