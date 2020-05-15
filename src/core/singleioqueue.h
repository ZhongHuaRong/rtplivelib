
#pragma once
#include "abstractqueue.h"

namespace rtplivelib {

namespace core {

/**
 * @brief The SingleIOQueue class
 * 该类提供输入输出接口
 * 可以通过设置输入队列或者输出队列来实现一个流程
 * 设置输入队列时，本类充当输出队列，输出的数据会push到本队列中
 * (这种思路比较好理解)
 * 设置输出队列时，本类充当输入队列，输入数据会从本类取出
 * (这种思路可以扩展成多输出)
 */
template<typename Type>
class RTPLIVELIBSHARED_EXPORT SingleIOQueue : public AbstractQueue<Type>
{
private:
	using queue = AbstractQueue<Type>;
public:
	SingleIOQueue():AbstractQueue<Type>(){
		
	}
	
	virtual ~SingleIOQueue() override{
		if(!this->get_exit_flag())
			this->exit_thread();
		this->clear();
	}
	
	inline bool is_input() const noexcept{
		return this == input;
	}
	
	inline bool is_output() const noexcept{
		return this == output;
	}
	
	inline queue get_input() const noexcept{
		return input;
	}
	
	inline queue get_output() const noexcept{
		return output;
	}
	
	/**
	 * @brief set_input
	 * 设置输入队列，这时，该对象就会默认设置为输出队列
	 * @param iqueue
	 */
	inline void set_input(queue * iqueue) noexcept{
		if(iqueue == input)
			return;
		
		mutex.lock();
		//先记录之前的队列情况
		queue *_i = input;
		
		//设置当前队列，如果输入为空则不设置输出
		//如果输入不为空，输出则默认设置为自己
		input = iqueue;
		output = input != nullptr?this:nullptr;
		mutex.unlock();
		
		//唤醒输入队列，因为可能在之前阻塞了
		//在即将失去拥有权时做好释放操作
		if(_i)
			_i->exit_wait_resource();
		
		if(has_input() && has_output() && this->get_exit_flag()){
			this->start_thread();
		}
	}
	
	/**
	 * @brief set_output
	 * 设置输出队列，这时，该对象就会默认设置为输入队列
	 * @param oqueue
	 */
	inline void set_output(queue * oqueue) noexcept{
		if(oqueue == output)
			return;
		
		mutex.lock();
		//先记录之前的队列情况
		queue *_i = input;
		
		output = oqueue;
		input = output != nullptr?this:nullptr;
		mutex.unlock();
		
		//唤醒输入队列，因为可能在之前阻塞了
		//在即将失去拥有权时做好释放操作
		if(_i)
			_i->exit_wait_resource();
		
		if(has_input() && has_output() && this->get_exit_flag()){
			this->start_thread();
		}
	}
	
	inline bool has_input() const noexcept{
		return input != nullptr;
	}
	
	inline bool has_output() const noexcept{
		return output != nullptr;
	}
protected:
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	inline virtual void on_thread_run() noexcept override final{
		std::lock_guard<std::mutex> lk(mutex);
		if(get_thread_pause_condition()){
			return;
		}
		input->wait_for_resource_push(100);
		//循环这里只判断指针
		while(input->has_data()){
			auto pack = input->get_next();
			output->push_one(this->deal_pack(pack));
		}
	}
	
	/**
	 * @brief deal_pack
	 * 该函数由子类实现，在该函数实现处理逻辑
	 * 也可以不实现默认转发
	 * @param value
	 * @return 
	 */
	virtual typename AbstractQueue<Type>::value_type 
	deal_pack(typename AbstractQueue<Type>::value_type value) noexcept {
		return value;
	}
	
	/**
	 * @brief get_thread_pause_condition
	 * 该函数用于判断线程是否需要暂停
	 * @return 
	 * 返回true则线程睡眠
	 * 默认是返回true，线程启动即睡眠
	 */
	inline virtual bool get_thread_pause_condition() noexcept override final{
		return input == nullptr || output == nullptr;
	}
private:
	std::mutex		mutex;
	queue			*input{nullptr};
	queue			*output{nullptr};
};

} // namespace core

}// namespace rtplivelib
