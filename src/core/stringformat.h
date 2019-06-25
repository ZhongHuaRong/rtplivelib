
#pragma once

#include "config.h"
#include <string>

namespace rtplivelib{

namespace core {

/**
 * @brief The StringFormat class
 * 用于字符串格式转换
 */
class RTPLIVELIBSHARED_EXPORT StringFormat
{
public:
    static std::wstring String2WString(const std::string& str) noexcept;
    
    static std::string WString2String(const std::wstring& str) noexcept;
};

} // namespace core

} // namespace rtplivelib
