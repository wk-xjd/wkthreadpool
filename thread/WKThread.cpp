#include "WKThread.h"
#include "utils.h"
#include <iostream>

WKThread::WKThread()
{
	m_currTaskQueuePtr = std::make_unique<WKUtils::WKThreadTaskQueue>();
	m_WKLockerPtr = std::make_unique<WKUtils::WKLocker>();
}

WKThread::~WKThread()
{
	if (m_stdThreadPtr)
	{
		quit();
	}	
}

void WKThread::start()
{
	m_bWaitFlag = false;
	if (!m_stdThreadPtr)
		m_stdThreadPtr = std::make_unique<std::thread>(&WKThread::_runTask, this);
}

void WKThread::quit()
{
	m_bQuitFlag = true;
	wake();
	if (m_stdThreadPtr && m_stdThreadPtr->joinable())
		m_stdThreadPtr->join();
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

	m_currTaskQueuePtr->pushBackTask(task);
	wake();
}

const int WKThread::getNextTaskSize() 
{
	return m_currTaskQueuePtr->getTaskSize();
}

size_t WKThread::getThreadId() const
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
	if (!m_currTaskQueuePtr->isEmpty())
	{
		//执行线程任务
		m_runTaskPtr = m_currTaskQueuePtr->popFrontTask();
		std::cout << m_runTaskPtr->getTaskId() << std::endl;
		if (m_runTaskPtr)
		{
			m_runTaskPtr->run();
			m_runTaskPtr->callback();
			if (m_runTaskPtr->isAutoDelete())
				delete m_runTaskPtr;
			m_runTaskPtr = nullptr;
		}
		//当前线程没有任务尝试取任务
		if (m_currTaskQueuePtr->isEmpty() && isHaveGetNextTaskFunc() && !m_bQuitFlag)
		{
				addTask(m_func());
		}
	}
	else
	{
		wait();
	}
}

bool WKThread::setGetNextTaskFunc(std::function<WKUtils::WKThreadTask* (void)> func)
{
	if (m_threadBegin)
		return false;

	m_func = std::move(func);
	m_haveGetNextTaskFunc = true;
	return true;
}

bool WKThread::isHaveGetNextTaskFunc() const
{
	return m_haveGetNextTaskFunc;
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
	m_WKLockerPtr->lock();
	m_state = state;
	m_WKLockerPtr->unlock();
}

