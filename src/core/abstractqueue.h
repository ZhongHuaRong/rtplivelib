#pragma once

#include "abstractthread.h"
#include <iostream>
#include <queue>

namespace rtplivelib {

namespace core {

class NeedDelete{};
class NotDelete{};

/**
 * @brief The AbstractQueue class
 * 该类实现了队列的基本操作，继承于AbstractThread
 * 虽然这个类没有实现AbstractThread的接口，
 * 但是理论上来说队列的等待资源和获取资源是异步操作的
 * 所以继承thread类，只要不调用start_thread就不是异步操作了
 * 
 * 模板第一个参数是对象类型，而第二个参数指的是队列保存的基本单位
 * 默认是保存裸指针，也可以换成智能指针
 * 第三个参数是对应第二个参数
 */
template<typename Packet,typename Unit = Packet*,typename Delete = NeedDelete >
class RTPLIVELIBSHARED_EXPORT AbstractQueue : public AbstractThread
{
public:
	using value_type			= Packet;
	using reference				= Packet&;
	using const_reference		= const Packet&;
	using pointer				= Packet*;
	using const_pointer			= const Packet&;
	using PacketQueue			= std::queue<Unit>;
public:
	AbstractQueue():
		_queue(new PacketQueue()),
		_max_size(10u)
	{	}
	
	virtual ~AbstractQueue() override{
		clear();
		delete _queue;
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
		return !_queue->empty();
	}
	
	/**
	 * @brief get_next
	 * 获取下一帧数据，如果不存在则返回nullptr
	 * 该函数获取到的数据都需要调用ReleasePacket释放空间
	 * @return 
	 */
	inline virtual Unit get_next() noexcept{
		std::lock_guard<std::mutex> lk(_mutex);
		if(_queue->empty())
			return nullptr;
		auto ptr = _queue->front();
		_queue->pop();
		return ptr;
	}
	
	/**
	 * @brief get_latest
	 * 获取最新的包，过期的包全部清除
	 * @return 
	 * 返回最新的包，如果没有则返回nullptr
	 */
	inline virtual Unit get_latest() noexcept{
		std::lock_guard<std::mutex> lk(_mutex);
		if(_queue->empty())
			return nullptr;
		while(_queue->size() > 1){
			erase_first();
		}
		auto ptr = _queue->front();
		_queue->pop();
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
	inline void push_one(Unit newPacket) noexcept{
		while(_queue->size() >= _max_size){
			erase_first();
		}
		std::lock_guard<std::mutex> lk(_mutex);
		_queue->push(newPacket);
		_queue_read_condition.notify_one();
	}
	
	/**
	 * @brief erase_first
	 * 抹除首个元素
	 */
	inline void erase_first() noexcept{
		if( _queue->empty())
			return;
		auto ptr = _queue->front();
		_erase_first(ptr,Delete());
		_queue->pop();
	}
	
	/**
	 * @brief clear
	 * 清空队列
	 */
	inline void clear() noexcept{
		std::lock_guard<std::mutex> lk(_mutex);
		while(has_data()){
			erase_first();
		}
	}
	
	/**
	 * @brief set_max_size
	 * 设置队列最大长度
	 * @param size
	 * 队列新的长度
	 */
	inline void set_max_size(int size) noexcept {
		if(size <= 0 )
			return;
		_max_size = static_cast<uint32_t>(size);
	}
private:
	inline void _erase_first(Unit& ptr,NeedDelete) noexcept {
		if(ptr != nullptr)
			delete ptr;
	}
	
	inline void _erase_first(Unit&,NotDelete) noexcept {
	}
private:
	std::mutex _mutex;
	std::condition_variable _queue_read_condition;
	PacketQueue	* const  _queue;
	volatile uint32_t _max_size;
};

} // namespace core

}// namespace core

