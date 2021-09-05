#ifndef WKTHREAGTASK_H
#define WKTHREAGTASK_H

/*
	线程任务基类（抽象类）
	1.任务入口：重写纯虚函数run函数，
	2.任务结束回调入口：重写虚函数callback函数
	3.任务有三个优先级（PriortyLevel），默认为LOW，将任务添加到线程池任务队列时，会优先处理高优先级任务
	4.getTaskId（虚函数可重写），默认使用实例对象的地址的hashcode
	5.可以设置m_autoDelete属性，任务执行结束后是否自动释放该对象，若为ture，一定要重写析构函数
*/
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