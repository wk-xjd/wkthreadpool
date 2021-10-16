#include "WKThread.h"
#include "utils.h"

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
	m_bWaitFlag.store(false);
	m_bQuitFlag.store(false);
	if (!m_stdThreadPtr)
		m_stdThreadPtr = std::make_unique<std::thread>(&WKThread::_runTask, this);
}

void WKThread::quit()
{
	m_bQuitFlag.store(true);
	wake();
	if (m_stdThreadPtr && m_stdThreadPtr->joinable())
		m_stdThreadPtr->join();
}

void WKThread::wait()
{
	m_bWaitFlag.store(true);
}

void WKThread::wake()
{
	if (!isWaiting())
		return;

	m_bWaitFlag.store(false);
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

bool WKThread::isRunning() const
{
	return m_state.load() == ThreadState::Running;
}

bool WKThread::isFinished() const
{
	return m_state.load() == ThreadState::Finished;
}

bool WKThread::isWaiting() const
{
	return m_state.load() == ThreadState::Waiting;
}

void WKThread::run()
{		
	if (!m_currTaskQueuePtr->isEmpty())
	{
		//执行线程任务
		WKUtils::WKThreadTask* const runTaskPtr = m_currTaskQueuePtr->popFrontTask();
		if (runTaskPtr)
		{
			runTaskPtr->run();
			runTaskPtr->callback();
			if (runTaskPtr->isAutoDelete())
				delete runTaskPtr;
		}

		if (m_currTaskQueuePtr->isEmpty() && isHaveGetNextTaskFunc() && !m_bQuitFlag.load())
		{
			//当前线程最后一个任务执行完毕后，尝试取任务新的任务
			addTask(m_func());
		}
	}
	else
	{
		wait();
	}
}

bool WKThread::setGetNextTaskFunc(std::function<WKUtils::WKThreadTask* (void)>&& func)
{
	if (!isThreadRelased())
		return false;

	m_func = func;
	m_haveGetNextTaskFunc = true;
	return true;
}

bool WKThread::isHaveGetNextTaskFunc() const
{
	return m_haveGetNextTaskFunc;
}

bool WKThread::isThreadRelased() const
{
	return m_stdThreadPtr == nullptr;
}

void WKThread::_runTask()
{
	m_threadId = WKUtils::currentThreadId();
	do
	{
		_updateState(ThreadState::Running);
		run();
		if (m_bWaitFlag.load())
		{
			_updateState(ThreadState::Waiting);
			std::unique_lock<std::mutex> locker(m_mutex);
			while (m_bWaitFlag.load())
			{
				m_condition.wait(locker, [=]() {return !m_bWaitFlag.load();});
			}
			locker.unlock();
		}
		_updateState(ThreadState::Finished);
	} while (!m_bQuitFlag.load());
	//释放线程
	m_stdThreadPtr.reset(nullptr);
}

void WKThread::_updateState(ThreadState state)
{
	m_state.store(state);
}

