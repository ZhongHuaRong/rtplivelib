
#pragma once

#if defined (WIN32)

#if defined(RTPLIVELIB_LIBRARY)
#  define RTPLIVELIBSHARED_EXPORT __declspec(dllexport)
#else
#  define RTPLIVELIBSHARED_EXPORT __declspec(dllimport)
#endif

#elif defined (unix)

#define RTPLIVELIBSHARED_EXPORT

#endif

#include <stdint.h>
namespace rtplivelib{

constexpr char LOGGERNAME[] = "Debug";
#if (DEBUG)
constexpr char LOGGER_ERROR_NAME[] = "Debug_Error";
#else
constexpr char LOGGERFILENAME[] = "Rtplivelib.txt";
#endif

//调试用的宏定义，注释后就是广域网，没注释就是局域网P2P
#define TEST

//音视频分成两个会话,一个rtp会话会占用２个端口
constexpr uint16_t VIDEO_PORTBASE = 20000;
constexpr uint16_t AUDIO_PORTBASE = VIDEO_PORTBASE + 2;

//将使用单个端口，rtp和rtcp将使用同一个端口
//默认是单个端口
#define SINGLEPORT

#if defined (TEST)
constexpr uint8_t SERVER_IP[] = { 192,168,31,75 };
#else
constexpr uint8_t SERVER_IP[] = { 54,223,240,118 };
#endif

constexpr int RTPPACKET_MAX_SIZE = 1300;

#define UNUSED(x) (void)x;

//Logger类不外露
enum LogLevel{
	///该等级将不会输出任何日志信息
	NOOUTPUT_LEVEL = 0,
	///将会输出致命错误信息	
	ERROR_LEVEL,
	///将会输出警告信息
	WARNING_LEVEL,
	///将会输出一般信息,默认是该等级
	INFO_LEVEL,
	///将会输出逻辑错误信息(调试使用),逻辑信息比一般信息优先级要低一些
	DEBUG_LEVEL,
	///将会输出更多的一般信息,不是非常必要的信息
	MOREINFO_LEVEL,
	///将会输出所有信息,看起来将会非常糟糕,不建议使用该等级
	ALLINFO_LEVEL
};


} // namespace rtplivelib
