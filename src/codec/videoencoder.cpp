#include "videoencoder.h"
#include "../core/logger.h"
#include "../core/time.h"
#include "hardwaredevice.h"
#include "../rtp_network/rtpsession.h"
extern "C"{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

namespace rtplivelib {

namespace codec {

VideoEncoder::VideoEncoder(bool use_hw_acceleration,
						   HardwareDevice::HWDType hwa_type,
						   EncoderType enc_type):
	Encoder(use_hw_acceleration,hwa_type,enc_type)
{
	auto p = std::make_shared<image_processing::Scale>();
	scale_ctx.swap(p);
}

VideoEncoder::VideoEncoder(VideoEncoder::Queue *queue,
						   bool use_hw_acceleration,
						   HardwareDevice::HWDType hwa_type,
						   EncoderType enc_type):
	Encoder(queue,use_hw_acceleration,hwa_type,enc_type)
{
	auto p = std::make_shared<image_processing::Scale>();
	scale_ctx.swap(p);
}

VideoEncoder::~VideoEncoder()
{
	_close_ctx();
}

void VideoEncoder::encode(core::FramePacket::SharedPacket packet) noexcept
{
	int ret{core::Result::Success};
	
	//传入空的数据+编码器没有初始化，直接返回
	//空数据主要是用于终止编码并拿到最后几帧图像
	//编码器没有初始化表明这个空数据并没有什么用
	if(packet == nullptr){
		if(encoder_ctx ==nullptr || avcodec_is_open(encoder_ctx) == 0)
			return;
		else {
			//终止编码
			ret = avcodec_send_frame(encoder_ctx,nullptr);
			
			if(ret < 0){
				core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
												LogLevel::WARNING_LEVEL);
				return;
			}
			
			receive_packet();
			return;
		}
	}
	
	if(_select_encoder(packet) == false){
		set_encoder_type(Encoder::None);
		_close_ctx();
		return;
	}
	
	//在这里判断编码器上下文是否初始化成功，不成功则直接返回
	if(encoder_ctx ==nullptr || avcodec_is_open(encoder_ctx) == 0)
		return;
	
	//如果帧在之前并没有分配成功，则需要在这里重新分配一次
	if(encode_sw_frame == nullptr)
		encode_sw_frame = _alloc_frame();
	if(encode_sw_frame == nullptr){
		core::Logger::Print_APP_Info(core::Result::FramePacket_frame_alloc_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return;
	}
	//设置数据
	if(_set_frame_data(encode_sw_frame,packet) == false){
		return;
	}
	
	if( use_hw_flag == true){
		
		//先判断相关结构体是否初始化完毕,这里一般是不会出错的
		if( hwdevice == nullptr ||
			hwdevice->get_init_result() == false || 
			encoder_ctx->hw_frames_ctx == nullptr ){
			return;
		}
		
		if( encode_hw_frame == nullptr){
			encode_hw_frame = _alloc_hw_frame();
			if( encode_hw_frame == nullptr){
				core::Logger::Print_APP_Info(core::Result::FramePacket_frame_alloc_failed,
											 __PRETTY_FUNCTION__,
											 LogLevel::WARNING_LEVEL);
				return;
			}
		}
		
		//拷贝帧到显存
		ret = av_hwframe_transfer_data(encode_hw_frame, encode_sw_frame, 0);
		if( ret < 0){
			core::Logger::Print("av_hwframe_transfer_data error",
								__PRETTY_FUNCTION__,
								LogLevel::MOREINFO_LEVEL);
			
			core::Logger::Print_FFMPEG_Info(ret,
											__PRETTY_FUNCTION__,
											LogLevel::ERROR_LEVEL);
			return;
		}
		ret = avcodec_send_frame(encoder_ctx,encode_hw_frame);
	}
	else
		ret = avcodec_send_frame(encoder_ctx,encode_sw_frame);
	
	if(ret < 0){
		core::Logger::Print_FFMPEG_Info(ret,
										__PRETTY_FUNCTION__,
										LogLevel::WARNING_LEVEL);
		return;
	}
	
	receive_packet();
}

void VideoEncoder::set_encoder_param(const core::Format &format) noexcept
{
	//设置好格式
	encoder_ctx->width = format.width;
	encoder_ctx->height = format.height;
	encoder_ctx->time_base.num = 1;
	encoder_ctx->time_base.den = format.frame_rate;
	
	//以下是用于软压用的参数设置，在硬压的时候会在hw里面再设置一次
	encoder_ctx->gop_size = 10;
	encoder_ctx->max_b_frames = 1;
	
	//需要动态调整一下比特率,先不实行
	if(format.height == 480 && format.width == 640)
		encoder_ctx->bit_rate = 1536;
	else if(format.height == 1080 && format.width == 1920)
		encoder_ctx->bit_rate = 1000 * 12;
}

void VideoEncoder::receive_packet() noexcept
{
	int ret = 0;
	AVPacket * src_packet = nullptr;
	
	//其实只有输入nullptr结束编码的时候循环才起作用
	while(ret >= 0){
		src_packet = av_packet_alloc();
		if(src_packet == nullptr){
			core::Logger::Print_APP_Info(core::Result::FramePacket_frame_alloc_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			return;
		}
		src_packet->data = nullptr;
		src_packet->size = 0;
		ret = avcodec_receive_packet(encoder_ctx,src_packet);
		if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
			break;
		}
		else if(ret < 0){
			core::Logger::Print_FFMPEG_Info(ret,
											__PRETTY_FUNCTION__,
											LogLevel::WARNING_LEVEL);
			break;
		}
		//以下操作是拷贝数据
		auto dst_packet = core::FramePacket::Make_Shared();
		if(dst_packet == nullptr){
			core::Logger::Print_APP_Info(core::Result::FramePacket_frame_alloc_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			break;
		}
		
		dst_packet->data->set_packet_no_lock(src_packet);
		//这里设置格式是为了后面发送包的时候设置各种参数
		//感觉这里不需要赋值,对于编码数据，不需要设置格式,只需要设置编码格式
		//好像音频需要用到
		dst_packet->format = format;
		//设置有效负载类型，也就是编码格式
		dst_packet->payload_type = payload_type;
		
		dst_packet->pts = src_packet->pts;
		dst_packet->dts = src_packet->dts;
		core::Logger::Print("video size:{}",
							__PRETTY_FUNCTION__,
							LogLevel::ALLINFO_LEVEL,
							dst_packet->data->size);
		
		//让退出循环时不要释放掉该packet
		src_packet = nullptr;
		
		std::lock_guard<decltype (list_mutex)> lk(list_mutex);
		for(auto ptr: output_list){
			ptr->push_one(dst_packet);
		}
	}
	
	if(src_packet != nullptr)
		av_packet_free(&src_packet);
}

inline bool VideoEncoder::_init_hwdevice(HardwareDevice::HWDType hwdtype,const core::Format& format) noexcept
{
	bool ret;
	std::string first_init;
	EncoderType first_type;
	std::string second_init;
	EncoderType second_type;
	
	switch(enc_type_user){
	case Encoder::Auto:
		//初始化两次,首先初始化HEVC，不支持再初始化264
		first_init = "hevc_";
		second_init = "h264_";
		first_type = EncoderType::HEVC;
		second_type =  EncoderType::H264;
		break;
	case Encoder::HEVC:
		first_init = "hevc_";
		first_type = EncoderType::HEVC;
		second_type =  EncoderType::Auto;
		break;
	case Encoder::H264:
		first_init = "h264_";
		first_type = EncoderType::H264;
		second_type =  EncoderType::Auto;
		break;
	default:
		core::Logger::Print_APP_Info(core::Result::Codec_encoder_not_found,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return false;
	}
	
	switch(hwdtype){
	case HardwareDevice::QSV:
		first_init += "qsv";
		if(second_init.size()!=0)
			second_init += "qsv";
		break;
	case HardwareDevice::NVIDIA:
		first_init += "nvenc";
		if(second_init.size()!=0)
			second_init += "nvenc";
		break;
	default:
		hwd_type_user = HardwareDevice::None;
		return false;
	}
	
	ret = create_encoder(first_init.c_str());
	//在初始化编码器上下文后，设置参数
	if(ret == true) {
		set_encoder_param(format);
		ret = hwdevice->init_device(encoder_ctx,encoder,hwdtype);
		enc_type_cur = first_type;
	}
	
	if(ret == false && second_init.size() != 0){
		ret = create_encoder(second_init.c_str());
		if(ret){
			set_encoder_param(format);
			ret = hwdevice->init_device(encoder_ctx,encoder,hwdtype);
			enc_type_cur = second_type;
		}
	}
	
	if(ret == false)
		return false;
	
	if(enc_type_cur == EncoderType::HEVC)
		payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_HEVC;
	else if(enc_type_cur == EncoderType::H264)
		payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_H264;
	return true;
}

inline bool VideoEncoder::_select_encoder(const core::FramePacket::SharedPacket &packet) noexcept
{
	//如果编码器未设置，则返回false
	if(enc_type_user == EncoderType::None)
		return false;
	//使用硬件加速时的初始化方案
	if(use_hw_flag == false || _set_hw_encoder_ctx(packet) == false){
		//硬件初始化失败的话，转为纯CPU操作
		set_hardware_acceleration(false,HardwareDevice::None);
		if(hwd_type_cur != HardwareDevice::None){
			_close_ctx();
		}
		return _set_sw_encoder_ctx(packet);
	}
	return true;
}

bool VideoEncoder::_set_hw_encoder_ctx(const core::FramePacket::SharedPacket &packet) noexcept
{
	//只要用户不设置，直接返回false进入软编
	if( hwd_type_user == HardwareDevice::None)
		return false;
	//如果用户设置了，但是是和当前使用的一样，不操作直接返回true
	else if( hwd_type_cur == hwd_type_user ){
		//如果用户设置Auto且当前不为空，则可以进行下一步判断
		if( enc_type_user == enc_type_cur ||
				(enc_type_user == EncoderType::Auto && enc_type_cur != EncoderType::None)){
			//如果当前格式一样才直接返回，否则还是需要重新设置
			if(format == packet->format)
				return true;
		}
	}
	//当用户设置了Auto，只要当前使用了硬件加速(应该是上一次优化过的)，也不操作直接返回true
	//因为存在手动选择其他加速方案再选择自动的情况
	//所以需要手动选择关闭硬件加速才能正确使用Auto
	else if( hwd_type_user == HardwareDevice::Auto && hwd_type_cur != HardwareDevice::None)
		return true;
	
	if(packet->format.frame_rate <= 0){
		core::Logger::Print_APP_Info(core::Result::Codec_frame_rate_must_more_than_zero,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return false;
	}
	
	//先释放之前的编码器上下文
	_close_ctx();
	
	switch(hwd_type_user){
	case HardwareDevice::None:
	{
		//如果检测到硬件设备类型为空，则将硬件使用标志置位false，使用CPU
		use_hw_flag = false;
		return false;
	}
	case HardwareDevice::Auto:
	{
		if(hwdevice == nullptr)
			hwdevice = new HardwareDevice();
		//暂时没有实现
		auto ret = _init_hwdevice(HardwareDevice::NVIDIA,packet->format);
		if(ret == true){
			hwd_type_cur = HardwareDevice::NVIDIA;
			this->format = packet->format;
			scale_ctx->set_default_output_format(format.width,format.height,hwdevice->get_swframe_pix_fmt());
		}
		return ret;
	}
	case HardwareDevice::QSV:
	case HardwareDevice::NVIDIA:
	{
		if(hwdevice == nullptr)
			hwdevice = new HardwareDevice();
		auto ret = _init_hwdevice(hwd_type_user,packet->format);
		if(ret == true){
			hwd_type_cur = hwd_type_user;
			this->format = packet->format;
			scale_ctx->set_default_output_format(format.width,format.height,hwdevice->get_swframe_pix_fmt());
		}
		return ret;
	}
	default:
		core::Logger::Print_APP_Info(core::Result::Function_not_implemented,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		hwd_type_cur = hwd_type_user = HardwareDevice::None;
		return false;
	}
	return false;
}

bool VideoEncoder::_set_sw_encoder_ctx(const core::FramePacket::SharedPacket &packet) noexcept
{
	
	//根据输入的图像格式和帧率来决定编码器的上下文
	//不过貌似重启上下文会导致有异常情况，所以还是尽量少重启上下文
	//		if(format == dst_format && format.frame_rate == dst_format.frame_rate)
	//为了减少重启次数，先暂时不判断帧率，只判断格式
	if(format == packet->format)
		return true;
	if(packet->format.frame_rate <= 0){
		core::Logger::Print_APP_Info(core::Result::Codec_frame_rate_must_more_than_zero,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return false;
	}
	
	_close_ctx();
	//寻找纯CPU编码器
	//软压和硬压不同，软压不存在初始化失败，所以Auto默认选择HEVC
	switch(enc_type_user){
	case Encoder::Auto:
	case Encoder::HEVC:;
		create_encoder("libx265");
		enc_type_cur = EncoderType::HEVC;
		payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_HEVC;
		break;
	case Encoder::H264:
		create_encoder("libx264");
		enc_type_cur = EncoderType::H264;
		payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_H264;
		break;
	default:
		core::Logger::Print_APP_Info(core::Result::Codec_encoder_not_found,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		return false;
	}
	if(encoder_ctx == nullptr || encoder == nullptr){
		return false;
	}
	
	set_encoder_param(packet->format);
	
	//默认编码YUV420P,如果是RGB,最好先转完格式再编码,不转码的话，内部自动转码
	encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	
	auto ret = avcodec_open2(encoder_ctx,encoder,nullptr);
	if(ret < 0){
		core::Logger::Print_APP_Info(core::Result::Codec_codec_open_failed,
									 __PRETTY_FUNCTION__,
									 LogLevel::WARNING_LEVEL);
		core::Logger::Print_FFMPEG_Info(ret,
										__PRETTY_FUNCTION__,
										LogLevel::WARNING_LEVEL);
	}
	
	format = packet->format;
	scale_ctx->set_default_output_format(format.width,format.height,AV_PIX_FMT_YUV420P);
	return true;
}

void VideoEncoder::_close_ctx() noexcept
{
	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
	close_encoder();
	if(hwdevice != nullptr){
		delete hwdevice;
		hwdevice = nullptr;
	}
	//重置硬件加速方案
	hwd_type_cur = HardwareDevice::None;
	enc_type_cur = EncoderType::None;
	//重置格式，以便关闭上下文后，用同样的格式也会重新开启上下文
	format = core::Format();
	_free_frame();
}

AVFrame *VideoEncoder::_alloc_frame() noexcept
{
	AVFrame * frame = nullptr;
	frame = av_frame_alloc();
	if(frame == nullptr){
		return nullptr;
	}
	//在alloc的时候也设置一部分参数，在set_frame_data的时候只负责拷贝数据
	//毕竟每次只有数据部分不一样，其他数据都是一样的
	frame->width = format.width;
	frame->height = format.height;
	
	//注意，这里的像素格式是原来的图像格式,不是转换后的
	//frame保存的是转换后的数据,也是到时候编码用的数据
	//这里需要区分两点，一个是使用了硬件加速的，一个是没有使用
	if(use_hw_flag && hwd_type_cur !=  HardwareDevice::None && hwdevice != nullptr){
		//通过硬件加速设备的接口获取软件帧格式
		frame->format = hwdevice->get_swframe_pix_fmt();
	} else {
		//在不需要加速的情况下，可以直接获取格式
		frame->format = AV_PIX_FMT_YUV420P;
	}
	av_frame_get_buffer(frame, 0);
	//		av_image_alloc(frame->data,frame->linesize,frame->width,frame->height,static_cast<AVPixelFormat>(frame->format),0);
	return frame;
}

AVFrame * VideoEncoder::_alloc_hw_frame() noexcept {
	if( encoder_ctx->hw_frames_ctx == nullptr)
		return nullptr;
	AVFrame * frame = nullptr;
	frame = av_frame_alloc();
	if(frame == nullptr){
		return nullptr;
	}
	if(av_hwframe_get_buffer(encoder_ctx->hw_frames_ctx, frame, 0) < 0 ||  !frame->hw_frames_ctx){
		av_frame_free(&frame);
		return nullptr;
	}
	return frame;
}

void VideoEncoder::_free_frame() noexcept
{
	if(encode_sw_frame != nullptr){
		av_frame_unref(encode_sw_frame);
		av_frame_free(&encode_sw_frame);
	}
	if(encode_hw_frame != nullptr){
		av_frame_unref(encode_hw_frame);
		av_frame_free(&encode_hw_frame);
	}
}

bool VideoEncoder::_set_frame_data(AVFrame *frame, core::FramePacket::SharedPacket packet) noexcept
{
	if(frame == nullptr || packet == nullptr)
		return false;
	//需要注意两种情况，一个是frame格式和packet格式一样，一个是不一样的情况
	//还有一个情况是frame未设置格式
	if( frame->format == AV_PIX_FMT_NONE || packet->format.pixel_format == AV_PIX_FMT_NONE)
		return false;
	if( frame->format == packet->format.pixel_format ) {
		memcpy(frame->data,&(*packet->data)[0],core::DataBuffer::GetDataPtrSize());
		memcpy(frame->linesize,packet->data->linesize,sizeof(packet->data->linesize));
	} else {
		scale_ctx->set_default_input_format(packet->format);
		scale_ctx->scale(&(*packet->data)[0],packet->data->linesize,frame->data,frame->linesize);
		
	}
	frame->pts = packet->pts;
	return true;
}

} // namespace codec

} // namespace rtplivelib
