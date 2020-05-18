
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
class SingleIOQueue : public AbstractQueue<Type>
{
public:
	using queue = AbstractQueue<Type>;
public:
	SingleIOQueue(){
		
	}
	
	virtual ~SingleIOQueue() override{
		this->exit_thread();
	}
	
	inline bool is_input() const noexcept{
		return this == input;
	}
	
	inline bool is_output() const noexcept{
		return this == output;
	}
	
	inline queue* get_input() const noexcept{
		return input;
	}
	
	inline queue* get_output() const noexcept{
		return output;
	}
	
	/**
	 * @brief set_input
	 * 设置输入队列，这时，该对象就会默认设置为输出队列
	 * 当oqueue是空或者自己的时候，如果iqueue是自己，则不处理
	 * @param iqueue
	 */
	inline void set_input(queue * iqueue) noexcept{
		if(iqueue == input)
			return;
		if((output == this || output == nullptr) && iqueue == this)
			return;
		
		//先让他解锁，才能lock
		if(input)
			input->exit_wait_resource();
		mutex.lock();
		
		//设置当前队列，如果输入为空则不设置输出
		//如果输入不为空，输出则默认设置为自己
		input = iqueue;
		output = input != nullptr?this:nullptr;
		mutex.unlock();
		
		if(!get_thread_pause_condition()){
			this->start_thread();
		}
	}
	
	/**
	 * @brief set_output
	 * 设置输出队列，这时，该对象就会默认设置为输入队列
	 * 当iqueue是空或者自己的时候，如果oqueue是自己，则不处理
	 * @param oqueue
	 */
	inline void set_output(queue * oqueue) noexcept{
		if(oqueue == output)
			return;
		if((input == this || input == nullptr) && oqueue == this)
			return;
		
		//先让他解锁，才能lock
		if(input)
			input->exit_wait_resource();
		mutex.lock();
		
		output = oqueue;
		input = output != nullptr?this:nullptr;
		mutex.unlock();
		
		if(!get_thread_pause_condition()){
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
	inline virtual void on_thread_run() noexcept override final{
		std::lock_guard<std::mutex> lk(mutex);
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
