
#pragma once
#include "abstractqueue.h"
#include "format.h"
#include <list>
#include "taskqueue.h"

namespace rtplivelib {

namespace core {

/**
 * @brief The TaskThread class
 * 任务线程类，继承AbstractThread
 * 
 * 增加输入输出接口，方便子类扩展
 * 配合TaskQueue的输入输出枚举，可以遍历整棵流程树
 * 自定义流程的话，可以通过继承该类实现业务，然后插入到流程中的某一环
 */
class TaskThread : public AbstractThread
{
public:
	//该线程的输入输出类型
	//默认InputAndOutput具备输入输出
	//OnlyInput只有输入队列，没有输出队列
	//OnlyOutput只有输出队列，没有输入队列
	enum IOType{
		OnlyInput = 1,
		OnlyOutput,
		InputAndOutput
	};
public:
	TaskThread() {}
	
	virtual ~TaskThread(){
		clear_input_queue();
		clear_output_queue();
	}
	
	inline void add_input_queue(TaskQueue * queue) noexcept{
		if(io_type == OnlyOutput)
			return;
		if(contain_input_queue(queue))
			return;
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		input_list.push_back(queue);
	}
	
	inline void add_output_queue(TaskQueue * queue) noexcept{
		if(io_type == OnlyInput)
			return;
		if(contain_output_queue(queue))
			return;
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		output_list.push_back(queue);
	}
	
	inline bool contain_input_queue(TaskQueue * queue) noexcept{
		return _contain_queue(queue,input_list);
	}
	
	inline bool contain_output_queue(TaskQueue * queue) noexcept{
		return _contain_queue(queue,output_list);
	}
	
	inline void remove_input_queue(TaskQueue * queue) noexcept{
		auto i = get_iterator(queue,input_list);
		if(i == input_list.end())
			return;
		
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		input_list.erase(i);
		
		//需要离开等待资源，否则可能存在线程阻塞的情况
		queue->exit_wait_resource();
	}
	
	inline void remove_output_queue(TaskQueue * queue) noexcept{
		auto i = get_iterator(queue,output_list);
		if(i == output_list.end())
			return;
		
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		output_list.erase(i);
	}
	
	inline void clear_input_queue() noexcept{
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		for(auto i = input_list.begin();i != input_list.end();++i){
			(*i)->exit_wait_resource();
		}
		input_list.clear();
	}
	
	inline void clear_output_queue() noexcept{
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		output_list.clear();
		
	}
	
	inline IOType get_IO_type() const noexcept{
		return io_type;
	}
protected:
	inline bool _contain_queue(TaskQueue * queue,const std::list<TaskQueue*> &list) noexcept{
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		for(auto ptr:list){
			if(ptr == queue)
				return true;
		}
		return false;
	}
	
	inline std::list<TaskQueue*>::const_iterator 
	get_iterator(TaskQueue *queue,const std::list<TaskQueue*> &list) noexcept{
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		for(auto i = list.begin();i != list.end();++i){
			if((*i) == queue)
				return i;
		}
		
		return list.end();
	}
	
	inline void set_IO_type(IOType type) noexcept{
		io_type = type;
	}
	
protected:
	std::list<TaskQueue*>			input_list;
	std::list<TaskQueue*>			output_list;
	std::mutex						list_mutex;
private:
	IOType							io_type{InputAndOutput};
};


} // namespace core

}// namespace rtplivelib
