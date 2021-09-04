#ifndef WKLOCKER_H
#define WKLOCKER_H
#include <mutex>
#include <atomic>
#include <condition_variable>

class WKLocker
{
public:
	WKLocker(int spinTimes = 10);
	~WKLocker();
	
	void lock();
	void unlock();

private:
	std::atomic<unsigned int> m_currentThreadId = 0;
	std::condition_variable m_condition;
	int m_lockCount = 0;
	std::mutex m_muetex;
	unsigned int m_exceptValue = 0;
	int m_spinCount;
	std::atomic<bool> m_delete = false;
};

#endif