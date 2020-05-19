
#include "abstractcapture.h"
#include "../core/logger.h"
#include "../core/error.h"

extern "C"
{
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

namespace rtplivelib {

namespace device_manager {


/**
 * @def AbstractCapture
 * @brief
 * 生成线程，并将自己移到该线程，该类的子类所有slot都是异步的
 * @param parent
 * @param type
 * 捕捉数据的类型，在子类的构造函数中输入
 */
AbstractCapture::AbstractCapture(CaptureType type) noexcept:
	current_device_info("NULL","NULL"),
	current_device_value(0),
	_type(type),
	_is_running_flag(false)
{
	
}

/**
 * @brief ~AbstractCapture
 * 终止线程和资源释放
 */
AbstractCapture::~AbstractCapture()
{
}

/**
 * @brief start_capture
 * 开始捕捉数据
 * @param enable
 * 用于标志，便于子类实现全局控制的功能
 * true:正常操作，如果已经开启则不操作，否则开启
 * false:如果未开启则不操作，已经开启了则关闭（类似全局静音的效果）
 */
void AbstractCapture::start_capture(bool enable) noexcept
{
	if(!enable){
		if(is_running())
			stop_capture();
		return;
	}
	if (is_running()) {
		return;
	}
	
	_is_running_flag = true;
	start_thread();
}

/**
  * @brief stopCapture
  * 停止捕捉数据
  */
void AbstractCapture::stop_capture() noexcept
{
	_is_running_flag = false;
	core::Logger::Print_APP_Info(core::Result::Device_stop_capture,
								 __PRETTY_FUNCTION__,
								 LogLevel::INFO_LEVEL,
								 current_device_info.second.c_str());
}

/**
 * @brief on_thread_run
 * 改为私有权限，向子类屏蔽线程实现细节
 * 其实是子类实现需要获取返回值，中间再包装一层
 */
void AbstractCapture::on_thread_run() noexcept
{
	/*从子类实现中获取到数据包*/
	auto packet = this->on_start();
	/*如果有子类重写on_frame_data函数并返回false，则不加入队列*/
	/*这里不判断包是否为空*/
	if(this->on_frame_data(packet)){
		this->push_one(packet);
	}
}

}// namespace device_manger

} // namespace rtplivelib
