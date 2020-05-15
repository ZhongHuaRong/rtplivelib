
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
	SingleIOQueue() {}
};


} // namespace core

}// namespace rtplivelib
