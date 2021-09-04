#ifndef WKTHREAGTASK_H
#define WKTHREAGTASK_H

class WKThreadTask
{
public:
	enum PriortyLevel
	{
		Low,
		Middle,
		Hight,
	};
	WKThreadTask() {};
	virtual ~WKThreadTask() {};
	virtual void run() = 0;
	virtual void callback() {};
	virtual unsigned int getTaskId() { return hash_task_fn(this); }
	PriortyLevel taskPriorty() { return m_priorty; }
	void setTaskPriorty(PriortyLevel level) { m_priorty = level; }
	bool isAutoDelete() { return m_autoDelete; } const
	void setAutoDelete(const bool isAutoDeleted = false) { m_autoDelete = isAutoDeleted; }
private:
	std::hash<WKThreadTask*> hash_task_fn;
	unsigned int m_taskId = 0;
	PriortyLevel m_priorty = PriortyLevel::Low;
	bool m_autoDelete = false;
};

#endif //WKTHREAGTASK_H