#include <queue>
#include <thread>
#include <atomic>
#include <deque>
#include <mutex>
#include <condition_variable>
class WKLocker;
class WKThread;
class WKThreadTask;
class WKThreadPool
{
public:
	WKThreadPool();
	WKThreadPool(int maxThreadSize, int maxTaskSize);
	~WKThreadPool();
	void start();
	void stop();
	bool isStop() const;
	bool addTask(WKThreadTask* task);

	int waitThreadCount() const; //空闲线程数目
	int doneThreadCount() const; //工作线程数目
	int maxThreadSize() const;	//线程池最大线程数目
	int maxTaskSize() const;	//线程最多缓存任务数目
	int currentTaskSize() const; // 当前任务数目

private:
	void _benginScheduler();
	void _createThread();
	void _checkThreadQueue();
	void _stopAllThread();
	void _clearTaskQueue();
	void _sleepIntrval(const int ms);
	WKThreadTask* _getTask();
	
private:
	std::thread* m_pollingThread = nullptr;
	std::deque<WKThreadTask*> m_taskQueue;
	std::queue<WKThread*> m_threadDoneQueue;
	std::queue<WKThread*> m_threadWaitQueue;
	std::atomic<bool> m_bStoped = true;
	std::atomic<int> m_threadDoneCount = 0;
	std::atomic<int> m_threadWaitCount = 0;
	std::atomic<int> m_taskCount = 0;
	int m_maxThreadSize = 0;
	int m_maxTaskSize = 30;
	std::atomic<bool> m_bExiting = false;
	std::condition_variable m_condition;
	std::mutex m_mutex;
	int m_lastWaitThreadcount = 0;
	int m_waitThreadCountGradient = 0;
	int m_sleepIntrval = 0;
};