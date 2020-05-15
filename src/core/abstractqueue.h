#pragma once

#include "abstractthread.h"
#include <iostream>
#include <queue>

namespace rtplivelib {

namespace core {

/**
 * @brief The AbstractQueue class
 * 该类实现了队列的基本操作，继承于AbstractThread
 * 虽然这个类没有实现AbstractThread的接口，
 * 但是理论上来说队列的等待资源和获取资源是异步操作的
 * 所以继承thread类，只要不调用start_thread就不是异步操作了
 * 
 * 模板第一个参数是对象类型
 * 该模板使用智能指针作为基本对象，智能指针的类型为Type
 * 拒绝使用裸指针作为参数
 */
template<typename Type>
class AbstractQueue : public AbstractThread
{
public:
	using value_type			= std::shared_ptr<Type>;
	using reference				= value_type&;
	using const_reference		= const value_type&;
	using pointer				= value_type*;
	using const_pointer			= const value_type&;
	using queue					= std::queue<value_type>;
public:
	AbstractQueue():
		_max_size(10u)
	{	}
	
	virtual ~AbstractQueue() override{
		clear();
	}
	
	/**
	 * @brief wait_resource_push
	 * 该函数会阻塞该线程，直到有新的资源推送进队列
	 * 可以调用循环has_data直到没有数据可读
	 * get_next可以获取下一个数据包
	 */
	inline void wait_resource_push(){
		if(has_data())
			return;
		std::unique_lock<std::mutex> lk(_mutex);
		_queue_read_condition.wait(lk);
	}
	
	/**
	 * @brief wait_for_resource_push
	 * 等待资源推送进队列，超时millisecond毫秒
	 * @param millisecond
	 * 等待毫秒数
	 * @return 
	 * 如果是因为获取到资源而唤醒的则返回true
	 * 如果是因为超时而唤醒的则返回false
	 */
	inline bool wait_for_resource_push(int millisecond){
		if(has_data())
			return true;
		std::unique_lock<std::mutex> lk(_mutex);
		//		auto flag = _queue_read_condition.wait_for(lk,std::chrono::milliseconds(millisecond));
		auto flag = _queue_read_condition.wait_until(lk,
													 std::chrono::system_clock::now() + 
													 std::chrono::milliseconds(millisecond));
		return flag != std::cv_status::timeout;
	}
	
	/**
	 * @brief exit_wait_resource
	 * 提供一个接口，让等待资源的线程退出等待
	 */
	inline void exit_wait_resource(){
		_queue_read_condition.notify_all();
	}
	
	/**
	 * @brief has_data
	 * 队列是否含有数据
	 * @return 
	 * 如果含有则返回true
	 */
	inline virtual bool has_data() noexcept{
		return !_queue.empty();
	}
	
	/**
	 * @brief get_next
	 * 获取下一帧数据，如果不存在则返回nullptr
	 * 该函数获取到的数据都需要调用ReleasePacket释放空间(智能指针的话不需要)
	 * @return 
	 */
	inline virtual value_type get_next() noexcept{
		std::lock_guard<std::mutex> lk(_mutex);
		if(_queue.empty())
			return nullptr;
		auto ptr = _queue.front();
		_queue.pop();
		return ptr;
	}
	
	/**
	 * @brief get_latest
	 * 获取最新的包，过期的包全部清除
	 * @return 
	 * 返回最新的包，如果没有则返回nullptr
	 */
	inline virtual value_type get_latest() noexcept{
		if(_queue.empty())
			return nullptr;
		std::lock_guard<std::mutex> lk(_mutex);
		while(_queue.size() > 1){
			_queue.pop();
		}
		auto ptr = _queue.front();
		_queue.pop();
		return ptr;
	}
	
	/**
	 * @brief push_packet
	 * 添加新的包进队列，同时唤醒需要等待包的条件变量
	 * 如果队列已经满了，则删除第一个包
	 * 直到队列长度小于队列最大长度才添加新的包
	 * @param newPacket
	 * 新的包
	 */
	inline void push_one(value_type newPacket) noexcept{
		while(_queue.size() >= _max_size){
			erase_first();
		}
		std::lock_guard<std::mutex> lk(_mutex);
		_queue.push(newPacket);
		_queue_read_condition.notify_one();
	}
	
	/**
	 * @brief erase_first
	 * 抹除首个元素
	 */
	inline void erase_first() noexcept{
		std::lock_guard<std::mutex> lk(_mutex);
		if( _queue.empty())
			return;
		_queue.pop();
	}
	
	/**
	 * @brief clear
	 * 清空队列
	 */
	inline void clear() noexcept{
		std::lock_guard<std::mutex> lk(_mutex);
		while(has_data()){
			_queue.pop();
		}
	}
	
	/**
	 * @brief set_max_size
	 * 设置队列最大长度
	 * @param size
	 * 队列新的长度
	 */
	inline void set_max_size(uint32_t size) noexcept {
		if(size <= 0 )
			return;
		_max_size = size;
	}
private:
	std::mutex					_mutex;
	std::condition_variable		_queue_read_condition;
	queue						_queue;
	volatile uint32_t			_max_size;
};

} // namespace core

}// namespace core

