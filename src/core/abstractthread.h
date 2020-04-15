
#pragma once

#include "config.h"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <cstring>

namespace rtplivelib {

namespace core {

/**
 * @brief The AbstractThread class
 * 该类作用是启动线程，同步线程数据和关闭线程，该项目大部分类都是该类的子类，都是异步操作
 * 子类需要在构造函数或者其他合适的情况下调用start_thread来开启线程，
 * 子类需要调用notify_thread唤醒线程处理事务，所有操作都需要重写on_thread_run来实现，
 * 实现on_thread_pause可以做资源释放
 * 在子类析构时，必须在最后调用exit_thread来关闭线程，否则会出现线程没有关闭的情况
 */
class RTPLIVELIBSHARED_EXPORT AbstractThread
{
public:
	AbstractThread();
	
	/**
	 * @brief ~AbstractThread
	 * 纯虚析构函数，目的是为了该类无法实例化，
	 * 而且该类也没有资源需要释放
	 * 想了一下，好像除了这个也没有什么函数需要设置纯虚的。。。
	 */
	virtual ~AbstractThread() = 0;
	
	/**
	 * @brief thread_id
	 * 返回线程id
	 * @return 
	 */
	uint64_t thread_id() noexcept;
	
	/**
	 * @brief get_thread_id
	 * 获取线程ｉｄ
	 * @return 
	 */
	std::thread::id get_thread_id() noexcept;
	
	/**
	 * @brief sleep
	 * 线程睡眠，单位毫秒
	 * 备注:这里出现了点问题，之前好像理解错了
	 * 现在的效果是哪个线程调用该函数哪个线程睡眠
	 * @warning 
	 * 线程未启动的话，该函数不起作用
	 * @param milliseconds
	 * 睡眠多少毫秒
	 */
	void sleep(int milliseconds) noexcept;
	
	/**
	 * @brief ThreadCallBackFunction
	 * 启动线程时的初始化步骤,外部不要调用这个接口，其实调用也没有问题，只是读取步骤是阻塞的
	 * (请不要主线程调用该接口)
	 * @param object
	 * 子类指针
	 */
	static void ThreadCallBackFunction(void *object) noexcept;
protected:
	/**
	 * @brief start_thread
	 * 启动线程
	 * 调用exit_thread终止线程
	 * 凡是继承该类的子类在析构时必须调用exit_thread离开线程
	 * @return 
	 * 线程启动成功则返回true
	 */
	bool start_thread() noexcept;
	
	/**
	 * @brief exit_thread
	 * 用于离开线程，所有继承该类的子类都需要在析构函数调用该函数
	 * 请不要在on_thread_run和on_thread_pause函数里面调用该接口，会造成死锁
	 * 要遵循谁开启线程就谁关闭线程的准则
	 */
	void exit_thread() noexcept;
	
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 * 由子类实现，实现异步操作
	 * 默认实现是空，然后线程睡眠
	 */
	virtual void on_thread_run() noexcept;
	
	/**
	 * @brief on_thread_pause
	 * 线程暂停时的回调
	 * 子类可以继承这个函数处理线程暂停或者退出时的事务
	 * 线程退出和暂停时是一定会触发该回调
	 * 现在再暂停时退出不会触发，因为回调在暂停时已经触发
	 * 备注:该类没有提供线程暂停的接口，只需要重写
	 * get_thread_pause_condition接口让其返回false暂停
	 * 然后子类提供修改其返回值的接口
	 */
	virtual void on_thread_pause() noexcept;
	
	/**
	 * @brief _get_exit_flag
	 * 获取线程退出标志，如果为真，则退出线程
	 * 也可以用这个标志判断线程是否在运行
	 * true则是线程没有启动，false则是表示线程正在运行
	 */
	bool get_exit_flag() noexcept;
	
	/**
	 * @brief get_thread_pause_condition
	 * 该函数用于判断线程是否需要暂停
	 * 线程不需要运行时需要让这个函数返回true
	 * 如果需要重新唤醒线程，则需要让该函数返回false并
	 * 调用notify_thread唤醒(顺序不要反了)
	 * @return 
	 * 返回true则线程睡眠
	 * 默认是返回true，线程启动即睡眠
	 */
	virtual bool get_thread_pause_condition() noexcept;
	
	/**
	 * @brief notify_thread
	 * 唤醒线程，在线程睡眠时调用
	 */
	void notify_thread() noexcept;
private:
	/**
	 * @brief _set_exit_flag
	 * 设置线程退出标志，一般在将要退出线程时设置，然后唤醒线程退出
	 * @param flag
	 */
	void _set_exit_flag(bool flag) noexcept;
	
protected:
	
private:
	volatile bool				_thread_exit_flag;
	std::thread					*_thread;
	std::mutex					_mutex;
	std::condition_variable		_therad_run_condition_variable;
};

inline uint64_t AbstractThread::thread_id() noexcept						{
	std::thread::id id = _thread->get_id();
	//std::thread::native_handle_type value;
	uint64_t value;
	memcpy(&value,&id,sizeof(value));
	//return static_cast<uint64_t>(value);
	return value;
}
inline std::thread::id AbstractThread::get_thread_id() noexcept				{
	return _thread->get_id();
}
inline void AbstractThread::sleep(int milliseconds) noexcept				{
	if(get_exit_flag())
		return;
	std::chrono::milliseconds dura(milliseconds);
	std::this_thread::sleep_for(dura);
}
inline void AbstractThread::on_thread_run()noexcept							{		}
inline void AbstractThread::on_thread_pause()noexcept						{		}
inline bool AbstractThread::get_exit_flag() noexcept						{		return _thread_exit_flag;}
inline bool AbstractThread::get_thread_pause_condition() noexcept			{		return true;}
inline void AbstractThread::notify_thread() noexcept						{
	_therad_run_condition_variable.notify_one();
}
inline void AbstractThread::_set_exit_flag(bool flag) noexcept				{		_thread_exit_flag = flag;}

} // namespace core

}// namespace rtplivelib
