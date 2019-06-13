
#include "time.h"

namespace rtplivelib {

namespace core {

Time::Time() : time(0)
{
	
	
}

Time::Time(int hours,int minutes,int seconds,int milliseconds) noexcept
{
	std::chrono::hours h(hours);
	std::chrono::minutes m(minutes);
	std::chrono::seconds s(seconds);
	std::chrono::milliseconds M(milliseconds);
	
	time = std::chrono::duration_cast<std::chrono::milliseconds>(h) + 
			std::chrono::duration_cast<std::chrono::milliseconds>(m) + 
			std::chrono::duration_cast<std::chrono::milliseconds>(s) + 
			M;
}

int Time::hours() noexcept
{
	return std::chrono::duration_cast<std::chrono::hours>(time).count() % 24;
}

int Time::minutes() noexcept
{
	return std::chrono::duration_cast<std::chrono::minutes>(time).count() % 60;
}

int Time::seconds() noexcept
{
	return std::chrono::duration_cast<std::chrono::seconds>(time).count() % 60;
}

int Time::milliseconds() noexcept
{
	return time.count() % 1000;
}

std::string Time::to_string() 
{
	if(time.count() == 0){
		return std::string("00:00:00:000");
	}
	
	std::string str;
	char ch[16];
	
	sprintf(ch,"%02d:%02d:%02d:%03d",
			hours(), minutes(), seconds(), milliseconds());
	str = ch;
	return str;
}

Time Time::Now() noexcept 
{
	constexpr auto h = 8 * 60 * 60 * 1000;
	return Time::FromTimeStamp(std::chrono::duration_cast<std::chrono::milliseconds>(
								std::chrono::system_clock::now().time_since_epoch()) +
							   std::chrono::milliseconds(h));
}

Time Time::FromTimeStamp(int64_t time_stamp) noexcept
{
	return Time::FromTimeStamp(std::chrono::milliseconds(time_stamp));
}

Time Time::FromTimeStamp(std::chrono::milliseconds time_stamp) noexcept
{
	Time t;
	t.time = time_stamp;
	return t;
}

Time Time::operator - (const Time & t) noexcept
{
	return FromTimeStamp(this->time - t.time);
}

Time Time::operator +(const Time &t) noexcept
{
	return FromTimeStamp(this->time + t.time);
}

Time &Time::operator -=(const Time &t) noexcept
{
	this->time -= t.time;
	return *this;
}

Time &Time::operator +=(const Time &t) noexcept
{
	this->time += t.time;
	return *this;
}

bool Time::operator == (const Time & t) noexcept
{
	return this->time == t.time;
}

bool Time::operator != (const Time &t) noexcept
{
	return !this->operator ==(t);
}


} // namespace core

} // namespace rtplivelib
