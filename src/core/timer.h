
#pragma once

#include "abstractthread.h"
#include "logger.h"

namespace rtplivelib {

namespace core {

/**
 * @brief The Timer class
 * 计时器
 * 指定一个时间，然后启动，时间到了之后会触发回调
 * 回调可以多种方式(全局函数指针，functor，lambda函数，要注意是无参的
 * 可以在回调里面运行自己的代码,要注意线程安全
 * 注:该计时器在loop的时候设计的不太严谨，在cb运行时间长的情况下计时不准
 */
class RTPLIVELIBSHARED_EXPORT Timer : public AbstractThread
{
public:
	template< typename CallBack>
	Timer(CallBack cb):
		_cb(cb)
	{		}
	
	/*工具类的话，想new就new，没必要拷贝一个*/
	Timer(const Timer&) = delete;
	Timer& operator = (const Timer&) = delete;
	
	virtual ~Timer() override{
		stop();
		exit_thread();
	}
	
	inline void set_loop(bool flag) noexcept{
		_loop_flag = flag;
	}
	
	inline void set_wait_time(uint32_t millisecond) noexcept {
		_wait_time = millisecond;
	}
	
	inline void start(uint32_t millisecond) noexcept {
		set_wait_time(millisecond);
		start();
	}
	
	inline void start() noexcept {
		_start_flag = true;
		start_thread();
		notify_thread();
	}
	
	inline void restart() noexcept{
		_wait_cond_var.notify_one();
	}
	
	inline void stop() noexcept {
		_start_flag = false;
		_wait_cond_var.notify_all();
	}
	
	inline bool is_running() noexcept {
		return _start_flag;
	}
protected:
	virtual void on_thread_run() noexcept override{
		std::unique_lock<decltype (_mutex)> lk(_mutex);
		while(_start_flag){
			if(_wait_cond_var.wait_for(lk,std::chrono::milliseconds(_wait_time)) == std::cv_status::timeout){
				_cb();
				if(_loop_flag == false){
					stop();
					return;
				}
			}
		}
	}
	
	virtual bool get_thread_pause_condition() noexcept override{
		return _start_flag == false;
	}
private:
	std::function<void ()>			_cb;
	std::condition_variable			_wait_cond_var;
	std::mutex						_mutex;
	volatile uint32_t				_wait_time{0};
	volatile bool					_loop_flag{false};
	volatile bool					_start_flag{false};
};

} // namespace rtp_network

} // namespace rtplivelib

