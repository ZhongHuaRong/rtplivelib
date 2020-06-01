
#pragma once
#include "abstractqueue.h"
#include "format.h"

namespace rtplivelib {

namespace core {

class TaskThread;

/**
 * @brief The TaskQueue class
 * 任务队列,载体为多媒体数据包
 * input或者output是对Task Queue来定义的
 * 如果TaskThread输出数据到队列则为input thread
 * 可以通过接口获取TaskThread从而遍历整个树
 * 也可以通过设置输入输出来改变整个项目的流程走向从而实现自定义功能
 */
class TaskQueue : public AbstractQueue<FramePacket>
{
public:
	TaskQueue() {}
	
	virtual ~TaskQueue() {
	}
	
	inline TaskThread* get_input_thread() noexcept{
		return input;
	}
	
	inline TaskThread* get_output_thread() noexcept{
		return output;
	}
private:
	//提供接口给Thread设置
	inline void set_input_thread(TaskThread* task) noexcept{
		input = task;
	}
	
	inline void set_output_thread(TaskThread* task) noexcept{
		output = task;
	}
private:
	TaskThread				*input{nullptr};
	TaskThread				*output{nullptr};
	
	friend class TaskThread;
};


} // namespace core

}// namespace rtplivelib
