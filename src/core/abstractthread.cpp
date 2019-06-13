#include "abstractthread.h"
#include <iostream>

namespace rtplivelib {

namespace core {

AbstractThread::AbstractThread():
	_thread_exit_flag(true),
	_thread(nullptr)
{
	
}

AbstractThread::~AbstractThread()
{
	
}

/**
 * @brief ThreadCallBackFunction
 * 启动线程时的初始化步骤,外部不要调用这个接口，其实调用也没有问题，只是读取步骤是阻塞的
 * (请不要主线程调用该接口)
 * @param object
 * 子类指针
 */
void AbstractThread::ThreadCallBackFunction(void *object) noexcept
{
	if(object == nullptr)
		return;
	AbstractThread* && ptr = static_cast<AbstractThread *>(object);
	
	while(true){
		if(ptr->get_thread_pause_condition() || ptr->get_exit_flag()){
			/*并不希望锁定资源，所以放在这里构建*/
			std::unique_lock<std::mutex> lk(ptr->_mutex);
			ptr->on_thread_pause();
			/*等待on_stop处理完事情再退出*/
			if(ptr->get_exit_flag())
				return;
			ptr->_therad_run_condition_variable.wait(lk,[ptr](){ 
				return !ptr->get_thread_pause_condition() || ptr->get_exit_flag();
			});
			/*暂停唤醒后，判断是否需要退出线程*/
			if(ptr->get_exit_flag())
				return;
		}
		
		ptr->on_thread_run();
	}
}

/**
 * @brief start_thread
 * 启动线程
 * 调用exit_thread终止线程
 * 凡是继承该类的子类在析构时必须调用exit_thread离开线程
 * @return 
 * 线程启动成功则返回true
 */
bool AbstractThread::start_thread() noexcept
{
	/** 
	 * 为什么不在父类默认开启呢
	 * 这里说一下我的理解，由于构造类的时候，先初始化父类，再初始化子类，那么在父类初始化的时候，子类并没有初始化，
	 * 虚函数表vtable并没有更新，导致基类指针调用虚函数还是父类的虚函数，所以在子类开启，线程调用的函数才是多态
	 */
	if(_thread)
		return true;
	_set_exit_flag(false);
	_thread = new std::thread(&AbstractThread::ThreadCallBackFunction,this);
	return _thread != nullptr;
}

/**
 * @brief exit_thread
 * 用于离开线程，所有继承该类的子类都需要在析构函数调用该函数
 */
void AbstractThread::exit_thread() noexcept
{
	if(get_exit_flag())
		return;
	_set_exit_flag(true);
	notify_thread();
	if(_thread ){
		//这里会造成本线程调用该接口造成死锁
		if(_thread->joinable())
			_thread->join();
		delete _thread;
		_thread = nullptr;
	}
}


} // namespace core

}// namespace rtplivelib
