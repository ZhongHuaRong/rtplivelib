#include "videoencoder.h"
#include "../core/logger.h"
#include "../core/time.h"
#include "../image_processing/scale.h"
#include "hardwaredevice.h"
#include "../rtp_network/rtpsession.h"
extern "C"{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

namespace rtplivelib {

namespace codec {

class VideoEncoderPrivateData{
public:
	AVCodec * encoder{nullptr};
	AVCodecContext * encoder_ctx{nullptr};
	VideoEncoder *queue{nullptr};
	core::Format format;
	//用于格式转换
	image_processing::Scale * scale_ctx{new image_processing::Scale};
	//用来编码的数据结构，只在设置format的时候分配一次
	//随着格式的改变和上下文一起重新分配
	AVFrame * encode_sw_frame{nullptr};
	AVFrame * encode_hw_frame{nullptr};
	HardwareDevice * hwdevice{nullptr};
	//用于用户设置
	HardwareDevice::HWDType hwd_type_user{HardwareDevice::None};
	//目前正在使用的类型,用于判断用户是否修改硬件加速方案
	HardwareDevice::HWDType hwd_type_cur{HardwareDevice::None};
	bool use_hw_flag{true};
	rtp_network::RTPSession::PayloadType payload_type{rtp_network::RTPSession::PayloadType::RTP_PT_NONE};
	
	VideoEncoderPrivateData(VideoEncoder *p):
		queue(p){}
	
	~VideoEncoderPrivateData(){
		close_ctx();
		if(scale_ctx != nullptr)
			delete scale_ctx;
	}
	
	/**
	 * @brief init_hwdevice
	 * 根据当前情况选择最优的硬件加速方案
	 * 目前只设置qsv
	 * @param packet
	 * 将要编码的包信息
	 * @return 
	 */
	inline bool init_hwdevice(const core::FramePacket * packet) noexcept {
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
		close_ctx();
		constexpr char api[] = "codec::VEPD::init_hwdevice";
		
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
	
	/**
	 * @brief set_encoder_ctx
	 * 设置编码器上下文并打开编码器
	 * 这个接口将会启动纯CPU进行编码
	 * 在输入图像的时候，要严格按照格式设置
	 * @param dst_format
	 * 目标格式
	 * @param fps
	 * 每秒帧数 
	 */
	inline void set_sw_encoder_ctx(const core::FramePacket * packet) noexcept{
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
		
		close_ctx();
		
		//寻找纯CPU编码器
		{
			auto ret = _init_encoder("libx264",packet->format);
			if(ret == false){
				ret = _init_encoder("libx264",packet->format);
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
	
	/**
	 * @brief close_ctx
	 * 关闭上下文,同时也会释放帧
	 */
	inline void close_ctx() noexcept{
		if(encoder_ctx == nullptr)
			return;
		encode(nullptr);
		avcodec_free_context(&encoder_ctx);
		if(hwdevice)
			delete hwdevice;
		//重置硬件加速方案
		hwd_type_cur = HardwareDevice::None;
		//重置格式，以便关闭上下文后，用同样的格式也会重新开启上下文
		format = core::Format();
		free_frame();
	}
	
	/**
	 * @brief free_frame
	 * 释放软件帧和硬件帧
	 */
	inline void free_frame() noexcept {
		if(encode_sw_frame != nullptr){
			frame_free(encode_sw_frame);
		}
		if(encode_hw_frame != nullptr){
			frame_free(encode_hw_frame);
		}
	}
	
	/**
	 * @brief use_hwa
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
	inline void use_hwa(bool flag,HardwareDevice::HWDType hwdtype= HardwareDevice::HWDType::Auto) noexcept {
		use_hw_flag = flag;
		hwd_type_user = hwdtype;
	}
	
	/**
	 * @brief alloc_and_init_frame
	 * 分配并初始化AVFrame
	 * 会根据提前设定好的格式进行初始化，不需要额外输入格式,简化操作
	 */
	inline AVFrame * alloc_frame() noexcept{
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
	
	/**
	 * @brief alloc_hw_frame
	 * 因为硬件帧和软件帧不一样，分开实现
	 * 这个函数必须在硬件编码器上下文初始化后调用
	 * @param packet
	 * 给硬件帧分配空间
	 * @return 
	 */
	inline AVFrame * alloc_hw_frame() noexcept {
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
	
	/**
	 * @brief set_frame_data
	 * 通过packet设置AVFAVFrame的数据
	 * @return 
	 * 如果设置失败则返回false
	 */
	inline bool set_frame_data(AVFrame * frame,
							   core::FramePacket *packet) noexcept{
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
	
	/**
	 * @brief frame_free
	 * 释放AVFrame,因为需要在释放前操作一下，所以
	 * @param frame
	 */
	inline void frame_free(AVFrame * frame) noexcept{
		if(frame == nullptr)
			return;
		av_frame_unref(frame);
		av_frame_free(&frame);
	}
	
	/**
	 * @brief encode
	 * 编码，然后推进队列
	 * 参数不使用智能指针的原因是参数需要传入nullptr来终止编码
	 * @param packet
	 * 传入空指针，将会终止编码并吧剩余帧数flush
	 */
	inline void encode(core::FramePacket * packet) noexcept{
		int ret;
		constexpr char api[] = "codec::VEPD::encode";
		
		if(packet != nullptr){
			if(use_hw_flag == true){
				//硬件初始化失败的话，转为纯CPU操作
				if(init_hwdevice(packet) == false){
					use_hwa(false,HardwareDevice::None);
					set_sw_encoder_ctx(packet);
				}
			}
			else
				set_sw_encoder_ctx(packet);
		}
		else if(encoder_ctx == nullptr){
			//参数传入空一般是用在清空剩余帧
			//如果上下文也是空，则不处理
			return;
		}

		if(packet != nullptr){
			//如果帧在之前并没有分配成功，则需要在这里重新分配一次
			if(encode_sw_frame == nullptr)
				encode_sw_frame = alloc_frame();
			if(encode_sw_frame == nullptr){
				core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
											 api,
											 LogLevel::WARNING_LEVEL);
				return;
			}
			//设置数据
			if(set_frame_data(encode_sw_frame,packet) == false){
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
                encode_hw_frame = alloc_hw_frame();
            }
            if( encode_hw_frame == nullptr){
                core::Logger::Print_APP_Info(core::MessageNum::FramePacket_frame_alloc_failed,
											 api,
											 LogLevel::WARNING_LEVEL);
				return;
            }
			
            //拷贝帧到显存
            if( av_hwframe_transfer_data(encode_hw_frame, encode_sw_frame, 0) < 0){
                core::Logger::Print("av_hwframe_transfer_data error",
									api,
									LogLevel::MOREINFO_LEVEL);
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
	
	/**
	 * @brief receive_packet
	 * 获取编码后的包，并推进队列
	 */
	inline void receive_packet() noexcept{
		int ret = 0;
		constexpr char api[] = "codec::VEPD::receive_packet";
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
			core::Logger::Print("size:{}",
								api,
								LogLevel::ALLINFO_LEVEL,
								dst_packet->size);
			queue->push_one(dst_packet);
		}
		
		if(src_packet != nullptr)
			av_packet_free(&src_packet);
	}
private:
	/**
	 * @brief _init_encoder
	 * 通过编码器名字初始化编码器,同时初始化编码器上下文
	 * @param name
	 * 硬件加速的编码器名字
	 * @param format
	 * 用于初始化编码器参数
	 * @return 
	 */
	inline bool _init_encoder(const char * name,const core::Format& format) noexcept {
		encoder = avcodec_find_encoder_by_name(name);
		constexpr char api[] = "codec::VEPD::_init_encoder";
		if(encoder == nullptr){
			core::Logger::Print_APP_Info(core::MessageNum::Codec_encoder_not_found,
										 api,
										 LogLevel::WARNING_LEVEL,
										 name);
			return false;
		}
		else {
			if(encoder_ctx != nullptr)
				avcodec_free_context(&encoder_ctx);
			encoder_ctx = avcodec_alloc_context3(encoder);
			if(encoder_ctx == nullptr){
				core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_context_alloc_failed,
											 api,
											 LogLevel::WARNING_LEVEL);
				return false;
			}
			
			//初始化编码器上下文后，设置编码参数
			_set_encoder_param(format);
			core::Logger::Print_APP_Info(core::MessageNum::Codec_encoder_init_success,
										 api,
										 LogLevel::INFO_LEVEL,
										 encoder->long_name);
			return true;
		}
	}
	
	/**
	 * @brief _init_hwdevice
	 * 初始化硬件设备并启动编码器
	 * 将会优先选择HEVC，如果不支持则换成264
	 * @param hwdtype
	 * 硬件设备类型
	 * @param format
	 * 图像格式
	 * @return 
	 */
	inline bool _init_hwdevice(HardwareDevice::HWDType hwdtype,const core::Format& format) noexcept {
		bool ret;
		switch(hwdtype){
		case HardwareDevice::QSV:
			ret = _init_encoder("h264_qsv",format);
			//在初始化编码器上下文后，设置参数
			_set_encoder_param(format);
			if(ret == false || hwdevice->init_device(encoder_ctx,encoder,hwdtype) == false){
				ret = _init_encoder("h264_qsv",format);
				_set_encoder_param(format);
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
	
	
	
	/**
	 * @brief _set_encoder
	 * 在启动硬件设备前，设置编码器上下文
	 * @param format
	 */
	inline void _set_encoder_param(const core::Format & format) noexcept{
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
	
};

//////////////////////////////////////////////////////////////////////////////////

VideoEncoder::VideoEncoder(int codec_id,bool use_hw_acceleration,HardwareDevice::HWDType hwa_type):
	_queue(nullptr),
	d_ptr(new VideoEncoderPrivateData(this))
{
	set_encoder_id(codec_id);
	set_hardware_acceleration(use_hw_acceleration,hwa_type);
}

VideoEncoder::VideoEncoder(VideoEncoder::Queue *queue,
						   int codec_id,
						   bool use_hw_acceleration,
						   HardwareDevice::HWDType hwa_type):
	_queue(queue),
	d_ptr(new VideoEncoderPrivateData(this))
{
	set_encoder_id(codec_id);
	set_hardware_acceleration(use_hw_acceleration,hwa_type);
	start_thread();
}

VideoEncoder::~VideoEncoder()
{
	set_input_queue(nullptr);
	exit_thread();
	delete d_ptr;
}

bool VideoEncoder::set_encoder_id(int codec_id) noexcept
{
//	if(get_exit_flag())
//		return false;
//	return d_ptr->init_encoder(static_cast<AVCodecID>(codec_id));
	UNUSED(codec_id)
			return false;
}

int VideoEncoder::get_encoder_id() noexcept
{
	if(d_ptr->encoder == nullptr)
		return 0;
	return d_ptr->encoder->id;
}

void VideoEncoder::set_hardware_acceleration(bool flag,HardwareDevice::HWDType hwa_type) noexcept
{
	d_ptr->use_hwa(flag,hwa_type);
}

void VideoEncoder::on_thread_run() noexcept
{
	if(_queue == nullptr){
		return;
	}
	_queue->wait_for_resource_push(100);
	//循环这里只判断指针
	while(_queue != nullptr && _queue->has_data()){
		auto pack = _get_next_packet();
		d_ptr->encode(pack.get());
	}
}

void VideoEncoder::on_thread_pause() noexcept
{
	d_ptr->encode(nullptr);
}

bool VideoEncoder::get_thread_pause_condition() noexcept
{
	return _queue == nullptr;
}

/**
 * @brief get_next_packet
 * 从队列里获取下一个包
 * @return 
 * 如果失败则返回nullptr
 */
core::FramePacket::SharedPacket VideoEncoder::_get_next_packet() noexcept 
{
	std::lock_guard<std::mutex> lk(_mutex);
	//真正需要判断数据的语句放在锁里面
	if(_queue == nullptr || !_queue->has_data()){
		return nullptr;
	}
	return _queue->get_next();
}

} // namespace codec

} // namespace rtplivelib
