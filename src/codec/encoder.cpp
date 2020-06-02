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

Encoder::Encoder(bool use_hw_acceleration,
				 HardwareDevice::HWDType hwa_type,
				 EncoderType enc_type)
{
	set_hardware_acceleration(use_hw_acceleration,hwa_type);
	set_encoder_type(enc_type);
}

Encoder::Encoder(Encoder::Queue *queue,
				 bool use_hw_acceleration,
				 HardwareDevice::HWDType hwa_type,
				 EncoderType enc_type)
{
	set_input_queue(queue);
	set_hardware_acceleration(use_hw_acceleration,hwa_type);
	set_encoder_type(enc_type);
	if(!get_thread_pause_condition())
		start_thread();
}

Encoder::~Encoder()
{
	set_input_queue(nullptr);
	exit_thread();
}

bool Encoder::set_encoder_name(const char *codec_name) noexcept
{
	UNUSED(codec_name)
	return false;
	//	return create_encoder(codec_name);
}

void Encoder::set_encoder_type(const Encoder::EncoderType type) noexcept
{
	if(type == enc_type_user)
		return;
	//当前一次设置的是Auto且当前使用的类型和type一样，则将用户设置改为type就可以了
	if(enc_type_user == Encoder::Auto && type == enc_type_cur){
		enc_type_user = enc_type_cur;
		return;
	}
	
	enc_type_user = type;
	if(!get_thread_pause_condition())
		start_thread();
}

std::string Encoder::get_encoder_name() const noexcept
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

inline void Encoder::add_input_queue(core::TaskQueue *queue) noexcept
{
	core::TaskThread::add_input_queue(queue);
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
		
		std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
		close_encoder();
		
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
	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
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
	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
	if(encoder_ctx == nullptr)
		return;
	this->encode(nullptr);
	avcodec_free_context(&encoder_ctx);
}

void Encoder::on_thread_run() noexcept
{
	auto _queue = input_list.front();
	_queue->wait_for_resource_push(16);
	//循环这里只判断指针
	while(_queue->has_data()){
		auto pack = _queue->get_next();
		std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
		if(pack != nullptr && pack->data != nullptr){
			std::lock_guard<decltype (pack->data->mutex)> lg(pack->data->mutex);
			this->encode(pack);
		} else {
			this->encode(pack);
		}
	}
}

void Encoder::on_thread_pause() noexcept
{
	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
	this->encode(nullptr);
}

bool Encoder::get_thread_pause_condition() noexcept
{
	return !has_input_queue() || enc_type_user == EncoderType::None;
}


} // namespace codec

} // namespace rtplivelib
