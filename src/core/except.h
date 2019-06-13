
#pragma once

#include "config.h"
#include <stdexcept>
#include <error.h>

namespace rtplivelib {

namespace core {

/**
 * @brief The uninitialized_error class
 * 未初始化的异常
 * 在功能处理的时候遇到未初始化或者初始化失败的结构体的时候将会抛出该异常
 */
class uninitialized_error : public std::runtime_error {
public:
    explicit uninitialized_error(const char * struct_name);
    
    explicit uninitialized_error(const std::string& struct_name);
    
    virtual ~uninitialized_error() = default;
    
};

class func_not_implemented_error : public std::runtime_error {
public:
    explicit func_not_implemented_error(const char * struct_name);
    
    explicit func_not_implemented_error(const std::string& struct_name);
    
    virtual ~func_not_implemented_error() = default;
};

} // namespace core

} // namespace rtplivelib
