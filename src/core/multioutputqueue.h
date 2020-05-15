
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
 * @warning 需要注意的是该类本身充当输入，
 * SingleIOQueue是即可以充当输出也可以充当输入，不要搞混定位
 */
template<typename Type>
class MultiOutputQueue : public AbstractQueue<Type>
{
public:
	MultiOutputQueue() {}
};


} // namespace core

}// namespace rtplivelib
