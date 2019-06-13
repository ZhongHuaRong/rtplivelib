
#pragma once

#include "config.h"

namespace rtplivelib{

namespace core {

class RTPLIVELIBSHARED_EXPORT AbstractObject
{
public:
	AbstractObject();
	
	/**
	 * @brief ~AbstractThread
	 * 纯虚析构函数，目的是为了该类无法实例化，
	 * 而且该类也没有资源需要释放
	 * 想了一下，好像除了这个也没有什么函数需要设置纯虚的。。。
	 */
	virtual ~AbstractObject() = 0;
};


} // namespace core

} // namespace rtplivelib
