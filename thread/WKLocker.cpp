#include "WKLocker.h"
#include "utils.h"

WKLocker::WKLocker(int spinTimes)
	: m_spinCount(spinTimes)
{

}
WKLocker::~WKLocker()
{
	m_delete = true;
	m_condition.notify_all();
}

void WKLocker::lock()
{
	unsigned int curThrId = WKUtils::currentThreadId();
START_GET_LOCK:
	{
		//自旋
		std::atomic<int> currCount = m_spinCount;
		while (!m_currentThreadId.compare_exchange_weak(m_exceptValue, curThrId))
		{
			if (!currCount.fetch_sub(1))
				break;
		}
		//自旋失败获取锁失败则阻塞
		if (m_currentThreadId == curThrId)
		{
			m_lockCount++;	//重入
		}
		else
		{
			std::unique_lock<std::mutex> locker(m_muetex);
			m_condition.wait(locker, [&]()
				{return m_currentThreadId == m_exceptValue; });
			locker.unlock();
			//锁被让出后，重新获取锁
			if (!m_delete)
				goto START_GET_LOCK;
		}
	}
}

void WKLocker::unlock()
{
	unsigned int curThrId = WKUtils::currentThreadId();

	if (m_currentThreadId == curThrId)
	{
		if (!(--m_lockCount))
		{
			//锁释放后唤醒所有阻塞在该锁的线程
			m_currentThreadId = m_exceptValue;
			m_condition.notify_all();
			m_lockCount = 0;
		}

	}

}
