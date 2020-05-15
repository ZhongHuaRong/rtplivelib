
#pragma once
#include "abstractqueue.h"

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
	MultiOutputQueue() {}
};


} // namespace core

}// namespace rtplivelib
