
#pragma once

#include "config.h"
#include <chrono>
#include <string>

namespace rtplivelib {

namespace core {

/**
 * @brief The Time class
 * 时间，可以理解成一个时间点或者一段持续时间
 */
class RTPLIVELIBSHARED_EXPORT Time
{
public:
	Time();
	
	Time(int hours,int minutes,int seconds = 0,int milliseconds = 0) noexcept;
	
	int hours() noexcept;
	
	int minutes() noexcept;
	
	int seconds() noexcept;
	
	int milliseconds() noexcept;
	
	int64_t to_timestamp() noexcept;
	
	std::string to_string() ;
	
	static Time Now() noexcept;
	
	static Time FromTimeStamp(int64_t time_stamp) noexcept;
	
	static Time FromTimeStamp(std::chrono::milliseconds time_stamp) noexcept;
	
	Time operator - (const Time & t) noexcept;
	
	Time operator + (const Time & t) noexcept;
	
	Time & operator -= (const Time & t) noexcept;
	
	Time & operator += (const Time & t) noexcept;
	
	bool operator == (const Time & t) noexcept;
	
	bool operator != (const Time &t) noexcept;
private:
	std::chrono::milliseconds time;
};

inline int64_t Time::to_timestamp() noexcept									{		return time.count();}

} // namespace core

} // namespace rtplivelib


