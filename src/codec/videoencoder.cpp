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

VideoEncoder::VideoEncoder(bool use_hw_acceleration,HardwareDevice::HWDType hwa_type):
	Encoder(use_hw_acceleration,hwa_type)
{
}

VideoEncoder::VideoEncoder(VideoEncoder::Queue *queue,
						   bool use_hw_acceleration,
						   HardwareDevice::HWDType hwa_type):
	Encoder(queue,use_hw_acceleration,hwa_type)
{
}

VideoEncoder::~VideoEncoder()
{
	_close_ctx();
	if(scale_ctx != nullptr)
		delete scale_ctx;
}

void VideoEncoder::encode(core::FramePacket *packet) noexcept
{
	int ret;
	constexpr char api[] = "codec::VideoEncoder::encode";
	
	if(packet != nullptr){
		if(use_hw_flag == true){
			//硬件初始化失败的话，转为纯CPU操作
			if(_select_hwdevice(packet) == false){
				set_hardware_acceleration(false,HardwareDevice::None);
				_set_sw_encoder_ctx(packet);
			}
		}
		else
			_set_sw_encoder_ctx(packet);
	}
	else if(encoder_ctx == nullptr){
		//参数传入空一般是用在清空剩余帧
		//如果上下文也是空，则不处理
		return;
	}

	if(packet != nullptr){
		//如果帧在之前并没有分配成功，则需要在这里重新分配一次
		if(encode_sw_frame == nullptr)
			encode_sw_frame = _alloc_frame();
		if(encode_sw_frame == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			return;
		}
		//设置数据
		if(_set_frame_data(encode_sw_frame,packet) == false){
			return;
		}
	}
	
	//avcodec_is_open不会检查参数是否为空
	if(encoder_ctx ==nullptr || avcodec_is_open(encoder_ctx) == 0){
		return;
	}

    //检查硬件编码上下文，决定用不用加速
    if( use_hw_flag == true && hwdevice != nullptr && hwdevice->get_init_result() == true){
        if( encoder_ctx->hw_frames_ctx == nullptr ){
            return;
        }
        
        if( encode_hw_frame == nullptr){
            encode_hw_frame = _alloc_hw_frame();
        }
        if( encode_hw_frame == nullptr){
            core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
                                         api,
                                         LogLevel::WARNING_LEVEL);
            return;
        }
        
        //拷贝帧到显存
        ret = av_hwframe_transfer_data(encode_hw_frame, encode_sw_frame, 0);
        if( ret < 0){
            core::Logger::Print("av_hwframe_transfer_data error",
                                api,
                                LogLevel::MOREINFO_LEVEL);

            core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::ERROR_LEVEL);
            return;
        }
        ret = avcodec_send_frame(encoder_ctx,encode_hw_frame);
    }
    else
        ret = avcodec_send_frame(encoder_ctx,encode_sw_frame);
    
    if(ret < 0){
        core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
        return;
    }
    
    receive_packet();
}

void VideoEncoder::set_encoder_param(const core::Format &format) noexcept
{
//	std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
	//设置好格式
	encoder_ctx->width = format.width;
	encoder_ctx->height = format.height;
	encoder_ctx->time_base.num = format.frame_rate;
	encoder_ctx->time_base.den = 1;
	
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
	constexpr char api[] = "codec::VideoEncoder::receive_packet";
	AVPacket * src_packet = nullptr;
	
	//其实只有输入nullptr结束编码的时候循环才起作用
	while(ret >= 0){
		src_packet = av_packet_alloc();
		if(src_packet == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			return;
		}
//		std::lock_guard<decltype (encoder_mutex)> lk(encoder_mutex);
		ret = avcodec_receive_packet(encoder_ctx,src_packet);
		if(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF){
			break;
		}
		else if(ret < 0){
			core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
			break;
		}
		//以下操作是拷贝数据
		auto dst_packet = core::FramePacket::Make_Shared();
		if(dst_packet == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			break;
		}
		dst_packet->size = src_packet->size;

		//浅拷贝,减少数据的拷贝次数
		dst_packet->data[0] = src_packet->data;
		dst_packet->packet = src_packet;
		//这里设置格式是为了后面发送包的时候设置各种参数
		//感觉这里不需要赋值,对于编码数据，不需要设置格式,只需要设置编码格式
		//好像音频需要用到
		dst_packet->format = format;
		//设置有效负载类型，也就是编码格式
		dst_packet->payload_type = payload_type;
		
		dst_packet->pts = src_packet->pts;
		dst_packet->dts = src_packet->dts;
		core::Logger::Print("video size:{}",
							api,
							LogLevel::ALLINFO_LEVEL,
							dst_packet->size);
		
		//让退出循环时不要释放掉该packet
		src_packet = nullptr;
		this->push_one(dst_packet);
	}
	
	if(src_packet != nullptr)
		av_packet_free(&src_packet);
}

inline bool VideoEncoder::_init_hwdevice(HardwareDevice::HWDType hwdtype,const core::Format& format) noexcept
{
	bool ret;
	switch(hwdtype){
	case HardwareDevice::QSV:
		//这里说明一下，这里只是用于测试，等测试完成了将会换回HEVC 
		ret = create_encoder("h264_qsv");
		//在初始化编码器上下文后，设置参数
		if(ret == true)
			set_encoder_param(format);
		if(ret == false || hwdevice->init_device(encoder_ctx,encoder,hwdtype) == false){
			ret = create_encoder("h264_qsv");
			if(ret == true)
				set_encoder_param(format);
			if( ret == false || hwdevice->init_device(encoder_ctx,encoder,hwdtype) == false){
				return false;
			}
			else {
				payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_H264;
			}
		}
		else {
			payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_H264;
		}
		break;
	case HardwareDevice::NVIDIA:
		break;
	default:
		hwd_type_user = HardwareDevice::None;
		return false;
	}
	
	return true;
}

bool VideoEncoder::_select_hwdevice(const core::FramePacket *packet) noexcept
{
	//只要用户不设置，直返回false
	if( hwd_type_user == HardwareDevice::None)
		return false;
	//如果用户设置了，但是是和当前使用的一样，不操作直接返回true
	else if( hwd_type_cur == hwd_type_user ){
		//如果当前格式一样才直接返回，否则还是需要重新设置
		if(format == packet->format)
			return true;
	}
	//当用户设置了Auto，只要当前使用了硬件加速(应该是上一次优化过的)，也不操作直接返回true
	//因为存在手动选择其他加速方案再选择自动的情况
	//所以需要手动选择关闭硬件加速才能正确使用Auto
	else if( hwd_type_user == HardwareDevice::Auto && hwd_type_cur != HardwareDevice::None)
		return true;
	
	//先释放之前的编码器上下文
	_close_ctx();
	constexpr char api[] = "codec::VideoEncoder::init_hwdevice";
	
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
		auto ret = _init_hwdevice(HardwareDevice::QSV,packet->format);
		if(ret == true){
			hwd_type_cur = HardwareDevice::QSV;
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
		core::Logger::Print_APP_Info(core::MessageNum::Function_not_implemented,
									 api,
									 LogLevel::WARNING_LEVEL);
		hwd_type_cur = hwd_type_user = HardwareDevice::None;
		return false;
	}
	return false;
}

void VideoEncoder::_set_sw_encoder_ctx(const core::FramePacket *packet) noexcept
{
	//根据输入的图像格式和帧率来决定编码器的上下文
	//不过貌似重启上下文会导致有异常情况，所以还是尽量少重启上下文
//		if(format == dst_format && format.frame_rate == dst_format.frame_rate)
	//为了减少重启次数，先暂时不判断帧率，只判断格式
	if(format == packet->format)
		return;
	constexpr char api[] = "codec::VEPD::set_encoder_ctx";
	if(packet->format.frame_rate <= 0){
		core::Logger::Print_APP_Info(core::MessageNum::Codec_frame_rate_must_more_than_zero,
									 api,
									 LogLevel::WARNING_LEVEL);
		return;
	}
	
	_close_ctx();
	
	//寻找纯CPU编码器
	{
		//这里说明一下，只是为了测试使用，测试完成后会换成libx265
		auto ret = create_encoder("libx264");
		if(ret == false){
			ret = create_encoder("libx264");
			if(ret == false){
				return;
			}
			else {
				payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_H264;
			}
		}
		else {
			payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_H264;
		}
		set_encoder_param(packet->format);
	}
	
	//默认编码YUV420P,如果是RGB,最好先转完格式再编码,不转码的话，内部自动转码
	encoder_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
	
	auto ret = avcodec_open2(encoder_ctx,encoder,nullptr);
	if(ret < 0){
		core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_open_failed,
									 api,
									 LogLevel::WARNING_LEVEL);
		core::Logger::Print_FFMPEG_Info(ret,
									 api,
									 LogLevel::WARNING_LEVEL);
	}
	
	format = packet->format;
	scale_ctx->set_default_output_format(format.width,format.height,AV_PIX_FMT_YUV420P);
}

void VideoEncoder::_close_ctx() noexcept
{
	close_encoder();
    if(hwdevice != nullptr){
		delete hwdevice;
        hwdevice = nullptr;
    }
	//重置硬件加速方案
	hwd_type_cur = HardwareDevice::None;
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
	if(frame == nullptr || encoder_ctx->hw_frames_ctx == nullptr){
		return nullptr;
	}
	if(av_hwframe_get_buffer(encoder_ctx->hw_frames_ctx, frame, 0) <0){
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

bool VideoEncoder::_set_frame_data(AVFrame *frame, core::FramePacket *packet) noexcept
{
	if(frame == nullptr || packet == nullptr)
		return false;
	//需要注意两种情况，一个是frame格式和packet格式一样，一个是不一样的情况
	//还有一个情况是frame未设置格式
	if( frame->format == AV_PIX_FMT_NONE || packet->format.pixel_format == AV_PIX_FMT_NONE)
		return false;
	if( frame->format == packet->format.pixel_format ) {
		memcpy(frame->data,packet->data,sizeof(packet->data));
		memcpy(frame->linesize,packet->linesize,sizeof(packet->linesize));
	} else {
		scale_ctx->set_default_input_format(packet->format);
		scale_ctx->scale(packet->data,packet->linesize,frame->data,frame->linesize);
		
	}
	frame->pts = packet->pts;
	return true;
}

} // namespace codec

} // namespace rtplivelib
