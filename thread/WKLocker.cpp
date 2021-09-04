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
		std::atomic<int> currCount = m_spinCount;
		while (!m_currentThreadId.compare_exchange_weak(m_exceptValue, curThrId))
		{
			if (!currCount.fetch_sub(1))
				break;
		}

		if (m_currentThreadId == curThrId)
		{
			m_lockCount++;
		}
		else
		{
			std::unique_lock<std::mutex> locker(m_muetex);
			m_condition.wait(locker, [&]()
				{return m_currentThreadId == m_exceptValue; });
			locker.unlock();
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
			m_currentThreadId = m_exceptValue;
			m_condition.notify_one();
		}

	}

}
