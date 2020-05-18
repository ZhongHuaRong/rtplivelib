
#pragma once
#include "abstractqueue.h"
#include <list>

namespace rtplivelib {

namespace core {

/**
 * @brief The MultiOutputQueue class
 * SingleIOQueue队列存在着一个缺点，就是只有单输出
 * 这对于需要多个输出来说是致命的，所以需要设计一个多输出接口的队列
 * 编码器编完码之后需要推流的同时也需要保存到本地，就需要多输出队列
 * 
 * 该类可以设置输入接口，然后把该类对象设置为输出，也可以实现多输出
 */
template<typename Type>
class MultiOutputQueue : public AbstractQueue<Type>
{
public:
	using queue = AbstractQueue<Type>;
public:
	MultiOutputQueue() {}
	
	virtual ~MultiOutputQueue() {
		this->exit_thread();
	}
	
	inline bool is_input() const noexcept{
		return this == input;
	}
	
	inline bool is_output() noexcept{
		return this->contain_output(this);
	}
	
	inline queue get_input() const noexcept{
		return input;
	}
	
	inline const std::list<queue*> get_output() const noexcept{
		return output_list;
	}
	
	inline void set_input(queue * iqueue) noexcept{
		if(iqueue == input)
			return;
		
		//先让他解锁，才能lock
		if(input)
			input->exit_wait_resource();
		input = iqueue;
		
		if(!get_thread_pause_condition()){
			if(this->get_exit_flag())
				this->start_thread();
			else
				this->notify_thread();
		}
	}
	
	inline void insert_output(queue * oqueue) noexcept{
		if(contain_output(oqueue))
			return;
		
		mutex.lock();
		output_list.push_back(oqueue);
		mutex.unlock();
		
		if(!get_thread_pause_condition()){
			if(this->get_exit_flag())
				this->start_thread();
			else
				this->notify_thread();
		}
		
	}
	
	inline void remove_output(queue * oqueue) noexcept{
		auto i = _contain_output(oqueue);
		if(i == output_list.end())
			return;
		std::lock_guard<decltype (mutex)> lk(mutex);
		output_list.erase(i);
	}
	
	inline void clear_output() noexcept{
		std::lock_guard<decltype (mutex)> lk(mutex);
		output_list.clear();
	}
	
	inline bool has_input() const noexcept{
		return input != nullptr;
	}
	
	inline bool has_output() const noexcept{
		return !output_list.empty();
	}
	
	inline bool contain_output(queue * oqueue) noexcept{
		return _contain_output(oqueue) != output_list.end();
	}
protected:
	inline typename std::list<queue*>::iterator
	_contain_output(queue * oqueue) noexcept{
		std::lock_guard<decltype (mutex)> lk(mutex);
		for(auto i = output_list.begin();i != output_list.end();++i){
			if(*i == oqueue)
				return i;
		}
		return output_list.end();
	}
	
	inline virtual void on_thread_run() noexcept override final{
		std::lock_guard<decltype (mutex)> lk(mutex);
		input->wait_for_resource_push(100);
		//循环这里只判断指针
		while(input->has_data()){
			auto pack = input->get_next();
			pack = this->deal_pack(pack);
			for(auto i = output_list.begin();i != output_list.end();++i){
				(*i)->push_one(pack);
			}
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
		return !has_input() || !has_output();
	}
private:
	std::mutex				mutex;
	queue					*input{nullptr};
	std::list<queue*>		output_list;
};


} // namespace core

}// namespace rtplivelib
