#include "WKThread.h"
#include <iostream>

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
	if (!isWaiting())
		return;

	m_bWaitFlag = false;
	m_condition.notify_one();
}

void WKThread::addTask(WKUtils::WKThreadTask* task)
{	
	if (m_bQuitFlag || !task)
		return;

	m_currTaskQueue.pushBackTask(task);
	wake();
}

const int WKThread::getNextTaskSize() 
{
	return m_currTaskQueue.getTaskSize();
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
	if (!m_currTaskQueue.isEmpty())
	{
		//执行线程任务
		m_taskPtr = m_currTaskQueue.popFrontTask();
		std::cout << m_taskPtr->getTaskId() << std::endl;
		if (m_taskPtr)
		{
			m_taskPtr->run();
			m_taskPtr->callback();
			if (m_taskPtr->isAutoDelete())
				delete m_taskPtr;
			m_taskPtr = nullptr;
		}
		//当前线程没有任务尝试取任务
		if (m_currTaskQueue.isEmpty() && m_func)
		{
				addTask((*m_func)());
		}
	}
	else
	{
		wait();
	}
}

void WKThread::setThreadPoolTaskQueueFunc(std::function<WKUtils::WKThreadTask* (void)>* func)
{
	if (m_func)
		return;

	m_func = func;
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

