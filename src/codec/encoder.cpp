#include "encoder.h"
#include "../core/logger.h"
#include "../rtp_network/rtpsession.h"
extern "C"{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

namespace rtplivelib {

namespace codec {

Encoder::Encoder(bool use_hw_acceleration,HardwareDevice::HWDType hwa_type):
	_queue(nullptr)
{
	set_hardware_acceleration(use_hw_acceleration,hwa_type);
}

Encoder::Encoder(Encoder::Queue *queue,
				 bool use_hw_acceleration,
				 HardwareDevice::HWDType hwa_type):
	_queue(queue)
{
	set_hardware_acceleration(use_hw_acceleration,hwa_type);
	start_thread();
}

Encoder::~Encoder()
{
	set_input_queue(nullptr);
	exit_thread();
}

bool Encoder::set_encoder_name(const char *codec_name) noexcept
{
	return create_encoder(codec_name);
}

std::string Encoder::get_encoder_name() noexcept
{
	if(encoder == nullptr)
		return "";
	return encoder->name;
}

/**
 * @brief set_hardware_acceleration
 * 设置硬件加速
 * 同时初始化编码器
 * 因为编码器和硬件设备的初始化是一块的
 * 自动挑选最适合自己的硬件加速方案
 * 备注:先不提供接口更改硬件加速方案
 * @param flag
 * false则是使用软编
 * @param hwdtype
 * 使用的硬件设备的类型
 * 如果flag是真，而hwdtype没有设置，则自动分配硬件类型
 * 如果所有硬件类型都无法适配，则使用软压(或者无法运行)
 */
void Encoder::set_hardware_acceleration(bool flag,HardwareDevice::HWDType hwa_type) noexcept
{
	use_hw_flag = flag;
	hwd_type_user = hwa_type;
}

bool Encoder::create_encoder(const char *name) noexcept
{
	
	auto coder = avcodec_find_encoder_by_name(name);
	if(coder == nullptr){
		core::Logger::Print_APP_Info(core::Result::Codec_encoder_not_found,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL,
									 name);
		return false;
	}
	else {
		auto ctx = avcodec_alloc_context3(coder);
		if(ctx == nullptr){
			core::Logger::Print_APP_Info(core::Result::Codec_codec_context_alloc_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			return false;
		}
		
//		std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
		if(encoder_ctx != nullptr)
			avcodec_free_context(&encoder_ctx);
		
		encoder = coder;
		encoder_ctx = ctx;
		core::Logger::Print_APP_Info(core::Result::Codec_encoder_init_success,
									 __PRETTY_FUNCTION__,
									 LogLevel::INFO_LEVEL,
									 coder->long_name);
		return true;
	}
}

bool Encoder::open_encoder() noexcept
{
//	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
	if( encoder_ctx == nullptr || encoder == nullptr){
		core::Logger::Print_APP_Info(core::Result::Codec_codec_context_alloc_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return false;
	}
	
	auto ret = avcodec_open2(encoder_ctx,encoder,nullptr);
	if(ret < 0){
		core::Logger::Print_APP_Info(core::Result::Codec_codec_open_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		core::Logger::Print_FFMPEG_Info(ret,
										__PRETTY_FUNCTION__,
										LogLevel::WARNING_LEVEL);
		return false;
	}
	
	return true;
}

void Encoder::close_encoder() noexcept
{
	if(encoder_ctx == nullptr)
		return;
//	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
	this->encode(nullptr);
	avcodec_free_context(&encoder_ctx);
}

void Encoder::on_thread_run() noexcept
{
	if(_queue == nullptr){
		return;
	}
	_queue->wait_for_resource_push(100);
	//循环这里只判断指针
	while(_queue != nullptr && _queue->has_data()){
		auto pack = _get_next_packet();
		this->encode(pack.get());
	}
}

void Encoder::on_thread_pause() noexcept
{
	this->encode(nullptr);
}

bool Encoder::get_thread_pause_condition() noexcept
{
	return _queue == nullptr;
}

/**
 * @brief get_next_packet
 * 从队列里获取下一个包
 * @return 
 * 如果失败则返回nullptr
 */
core::FramePacket::SharedPacket Encoder::_get_next_packet() noexcept 
{
	std::lock_guard<std::mutex> lk(_queue_mutex);
	//真正需要判断数据的语句放在锁里面
	if(_queue == nullptr || !_queue->has_data()){
		return nullptr;
	}
	return _queue->get_next();
}

} // namespace codec

} // namespace rtplivelib
