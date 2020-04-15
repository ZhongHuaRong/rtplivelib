
#pragma once

#include "config.h"
#include "error.h"
#include "except.h"
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/stdout_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "jrtplib3/rtperrors.h"
extern "C" {
#include "libavutil/error.h"
#include "libavutil/log.h"
}

namespace rtplivelib {

namespace core {

/**
 * @brief The Logger class
 * 日志模块，输出日志
 * Debug模式默认输出到控制台
 * Release模式默认输出到文件
 */
class Logger
{
public:
	Logger() = delete;
	
	/**
	 * @brief InitLogger
	 * 初始化程序需要用到的日志器
	 */
	static inline void Init_logger(){
		if(init)
			return;
		spdlog::init_thread_pool(4096,2);
#ifdef DEBUG 
		auto ptr = spdlog::create_async_nb<spdlog::sinks::stdout_sink_mt>(LOGGERNAME);
		
#else
		auto ptr = spdlog::create_async_nb<spdlog::sinks::rotating_file_sink_mt>(
					LOGGERNAME,LOGGERFILENAME,4096,3);
#endif
		init = true;
		{
			ptr->set_pattern(" [%C-%m-%d %H:%M:%S:%e] %v ***");
			ptr->info("Initialization log module");
		}
		spdlog::set_pattern(" [%C-%m-%d %H:%M:%S:%e] [%7l] [%5t] %v ***");
	}
	
	/**
	 * @brief ClearAll
	 * 关闭所有日志
	 */
	static inline void Clear_all(){
		if(!init)
			return;
		auto ptr = spdlog::get(LOGGERNAME);
		ptr->set_pattern(" [%C-%m-%d %H:%M:%S:%e] %v ***");
		ptr->info("Close log module");
		spdlog::drop_all();
		spdlog::shutdown();
		init = false;
	}
	
	/**
	 * @brief Info
	 * 打印信息，根据等级来输出
	 * @param msg
	 * 信息
	 * @param api
	 * 信息具体出现的位置
	 * @param level
	 * 输出的信息等级
	 */
	template<typename ... _T>
	static inline void Print(const char * msg,const char * api,enum LogLevel level,const _T & ...t){
		if( level > _level || level == NOOUTPUT_LEVEL )
			return;
		Init_logger();
		auto ptr = spdlog::get(LOGGERNAME);
		switch (level) {
		case NOOUTPUT_LEVEL:
			return;
		case ERROR_LEVEL:
			ptr->error(Insert_api_format(msg).c_str(),Remove_Func_Param(api),t...);
			break;
		case WARNING_LEVEL:
			ptr->warn(Insert_api_format(msg).c_str(),Remove_Func_Param(api),t...);
			break;
		case INFO_LEVEL:
			ptr->info(Insert_api_format(msg).c_str(),Remove_Func_Param(api),t...);
			break;
		case DEBUG_LEVEL:
		case MOREINFO_LEVEL:
		case ALLINFO_LEVEL:
			ptr->info(Insert_api_format(msg).c_str(),Remove_Func_Param(api),t...);
			break;
		}
		ptr->flush();
	}
	
	/**
	 * @brief Error_app
	 * 输出错误(程序的错误代码)
	 * @param num
	 * 错误代码
	 * @param api
	 * 调用该接口的api
	 * @param level
	 * 输出的信息等级
	 */
	template<typename ... _T>
	static inline void Print_APP_Info(Result num,const char * api,enum LogLevel level,const _T &... t){
		Print(MessageString[static_cast<int>(num)],api,level,t...);
	}
	
	/**
	 * @brief Error_ffmpeg
	 * 输出错误(ffmpeg的错误代码)
	 * @param num
	 * 错误代码
	 * @param api
	 * 调用该接口的api
	 * @param level
	 * 输出的信息等级
	 */
	static inline void Print_FFMPEG_Info(int num ,const char * api,enum LogLevel level){
		char ch[128];
		Print(av_make_error_string(ch,128,num),api,level);
	}
	
	/**
	 * @brief Error_rtp
	 * 输出错误(对应jrtplib的错误码)
	 * @param num
	 * 错误代码
	 * @param api
	 * 调用该接口的API
	 * @param level
	 * 输出的信息等级
	 */
	static inline void Print_RTP_Info(int num,const char * api,enum LogLevel level){
		Print(jrtplib::RTPGetErrorString(num).c_str(),api,level);
	}
	
	/**
	 * @brief log_set_level
	 * 设置日志输出等级,只有低于该等级的信息才会打印出来
	 * @param level
	 */
	static inline void log_set_level(enum LogLevel level) noexcept {
		_level = level;
		switch (_level) {
		case NOOUTPUT_LEVEL:
			av_log_set_level(AV_LOG_QUIET);
			break;
		case ERROR_LEVEL:
			av_log_set_level(AV_LOG_ERROR);
			break;
		case WARNING_LEVEL:
			av_log_set_level(AV_LOG_WARNING);
			break;
		case INFO_LEVEL:
			av_log_set_level(AV_LOG_INFO);
			break;
		case DEBUG_LEVEL:
			av_log_set_level(AV_LOG_DEBUG);
			break;
		case MOREINFO_LEVEL:
		case ALLINFO_LEVEL:
			av_log_set_level(AV_LOG_TRACE);
			break;
		}
	}
private:
	static inline const std::string Insert_api_format(const char * msg) noexcept{
		std::string str("[{}] ");
		return str.append(msg);
	}
	
	static inline std::string Remove_Func_Param(const char * func) noexcept{
		return Remove_Func_Param(std::string(func));
	}
	
	static inline std::string Remove_Func_Param(std::string func) noexcept{
		//返回的字符串+12是去掉最外层命名空间rtplivelib
		auto first_find_pos = func.rfind('(');
		if(first_find_pos == std::string::npos){
			auto second_find_pos = func.rfind(' ');
			if(second_find_pos != std::string::npos)
				return func.substr(second_find_pos + 1 + 12);
		} else {
			auto second_find_pos = func.rfind(' ',first_find_pos);
			if(second_find_pos != std::string::npos)
				return func.substr(second_find_pos + 1 + 12,first_find_pos - second_find_pos - 1 - 12);
			else 
				return func.substr(12,first_find_pos - 12);
		}
		return func;
	}
private:
	static bool			init;
	static LogLevel		_level;
};

} // namespace core

} // namespace rtplivelib

