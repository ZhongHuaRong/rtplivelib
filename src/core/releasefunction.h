
#pragma once

#include "config.h"
#include <functional>

namespace rtplivelib {

namespace core {

/**
 * 该类用于资源管理，可以吧释放资源步骤通过lambada函数通过构造函数传入
 * 当对象超出作用域的时候，将会自动调用析构函数来释放资源，能够保证资源必定释放
 * 对于局部变量的释放会变得很方便
 * 符合RAII思想
 */
struct ReleaseFunction{
public:
	template <typename Function>
	ReleaseFunction(Function func):
		function(func)
	{
		
	}
	
//	void operator() () noexcept{
//		function();
//	}
	
	~ReleaseFunction(){
		function();
	}
	
private:
	std::function<void ()> function;
};

}  // core

}  // rtplivelib
