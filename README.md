# wkthreadpool简介
### WKThread线程类使用方式：
#### 1.继承WKThread类，并重写run方法，
#### 1.1通过start方法启动线程并执行，线程执行完毕自动释放该线程
#### 2.实例化WKThread对象，并将继承了WKThreadTask的线程任务添加到线程，
##### 2.1 通过start方法启动线程，
##### 2.2 线程任务执行完毕后，线程不会立即释放，
##### 2.3 线程任务执行完毕后进入挂起状态，等待下一次添加任务唤醒
##### 2.4 线程随实例化对象释放而释放，也可以通过quit方法释放线程
    
### WKThreadPool线程池
#### 1.懒创建线程，有任务且线程数小于最大线程数时创建新的线程
#### 2.动态调整轮询检查线程状态的间隔,
#### 3.支持自动释放任务对象

### 实例
#### main.cpp 为具体实例
