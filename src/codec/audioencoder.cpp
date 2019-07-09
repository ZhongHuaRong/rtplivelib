#include "audioencoder.h"
#include "../rtp_network/rtpsession.h"
#include "../core/logger.h"
#include "../audio_processing/resample.h"
extern "C"{
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"
#include "libavutil/imgutils.h"
}

namespace rtplivelib {

namespace codec {

class AudioEncoderPrivateData{
public:
	AVCodec * encoder{nullptr};
	AVCodecContext * encoder_ctx{nullptr};
	AudioEncoder *queue{nullptr};
	//用于保存上一次输入时的音频格式，通过前后对比来重新设置编码器参数
	//现在改为格式不一致则进行重采样(本意是不想通过重采样来处理的,但是好像只有S16是可以成功打开编码器)
	core::Format ifmt;
	//用于重采样的默认格式
	const core::Format default_resample_format{0,0,0,44100,2,16};
	//编码成功后用于赋值FramePacket参数
	rtp_network::RTPSession::PayloadType payload_type{rtp_network::RTPSession::PayloadType::RTP_PT_NONE};
	//用于判断是否给frame设置参数
	bool reassignment{false};
	//实际用去编码的数据结构
	AVFrame * encode_frame{nullptr};
	//当输入格式不符合编码格式时则需要重采样
	audio_processing::Resample * resample{nullptr};
	
	AudioEncoderPrivateData(AudioEncoder *p):
		queue(p){}
	
	~AudioEncoderPrivateData(){
		close_ctx();
		if( encode_frame != nullptr){
			av_frame_free(&encode_frame);
		}
		
		if(resample != nullptr){
			delete resample;
		}
	}
	
	/**
	 * @brief alloc_encode_frame
	 * 为编码帧分配空间
	 * @return 
	 */
	bool alloc_encode_frame() noexcept {
		if( encode_frame == nullptr){
			encode_frame = av_frame_alloc();
			
			if( encode_frame == nullptr)
				return false;
		}
		
		if( encode_frame != nullptr && reassignment == true){
			encode_frame->nb_samples     = encoder_ctx->frame_size;
			encode_frame->format         = encoder_ctx->sample_fmt;
			encode_frame->channel_layout = encoder_ctx->channel_layout;
			reassignment = false;
			return true;
		}
		return true;
	}
	
	/**
	 * @brief check_sample_fmt
	 * 检查编码器是否支持该编码格式
	 * @param codec
	 * 编码器
	 * @param sample_fmt
	 * 格式
	 */
	bool check_sample_fmt(const AVCodec *codec, enum AVSampleFormat sample_fmt) noexcept 
	{
		const enum AVSampleFormat *p = codec->sample_fmts;
	
		while (*p != AV_SAMPLE_FMT_NONE) {
			if (*p == sample_fmt)
				return true;
			p++;
		}
		return false;
	}
	
	/**
	 * @brief select_sample_rate
	 * 选择最大采样率
	 */
	int select_sample_rate(const AVCodec *codec) noexcept 
	{
		const int *p;
		int best_samplerate = 0;
	
		if (!codec->supported_samplerates)
			return 44100;
	
		p = codec->supported_samplerates;
		while (*p) {
			if (!best_samplerate || abs(44100 - *p) < abs(44100 - best_samplerate))
				best_samplerate = *p;
			p++;
		}
		return best_samplerate;
	}
	
	/**
	 * @brief select_channel_layout
	 * 选择通道数最多的布局
	 */
	uint64_t select_channel_layout(const AVCodec *codec) noexcept 
	{
		if (!codec->channel_layouts)
			return AV_CH_LAYOUT_STEREO;
		
		const uint64_t *p;
		uint64_t best_ch_layout = 0;
		int best_nb_channels   = 0;
	
		p = codec->channel_layouts;
		while (*p) {
			int nb_channels = av_get_channel_layout_nb_channels(*p);
	
			if (nb_channels > best_nb_channels) {
				best_ch_layout    = *p;
				best_nb_channels = nb_channels;
			}
			p++;
		}
		return best_ch_layout;
	}
	
	/**
	 * @brief open_ctx
	 * 打开编码器
	 * @param packet
	 * @return 
	 */
	inline bool open_ctx(const core::FramePacket * packet) noexcept{
		if(encoder_ctx != nullptr && ifmt == packet->format){
			return true;
		} else {
			//先关闭之前的编码器(如果存在的话)
			//可能需要更改格式
			close_ctx();
			if( _init_encoder("libfdk_aac",packet->format) == false){
				return false;
			}
		}
		constexpr char api[] = "codec::AEPD::open_ctx";
		auto ret = avcodec_open2(encoder_ctx,encoder,nullptr);
		if(ret < 0){
			core::Logger::Print_APP_Info(core::MessageNum::Codec_codec_open_failed,
										 api,
										 LogLevel::WARNING_LEVEL);
			core::Logger::Print_FFMPEG_Info(ret,
										 api,
										 LogLevel::WARNING_LEVEL);
			return false;
		}
		ifmt = packet->format;
		reassignment = true;
		payload_type = rtp_network::RTPSession::PayloadType::RTP_PT_AAC;
		
		_init_resample();
		return true;
	}
	
	/**
	 * @brief close_ctx
	 * 关闭
	 */
	inline void close_ctx() noexcept{
		if(encoder_ctx == nullptr)
			return;
		encode(nullptr);
		avcodec_free_context(&encoder_ctx);
	}
	
	/**
	 * @brief encode
	 * 编码，然后推进队列
	 * 参数不使用智能指针的原因是参数需要传入nullptr来终止编码
	 * @param packet
	 * 传入空指针，将会终止编码并吧剩余帧数flush
	 */
	inline void encode(core::FramePacket * packet) noexcept{
		//先初始化编码器上下文
		if(packet != nullptr){
			if( open_ctx(packet) == false){
				return;
			}
		}
		else if(encoder_ctx == nullptr){
			//参数传入空一般是用在清空剩余帧
			//如果上下文也是空，则不处理
			return;
		}
		
		//这里设置好用于编码的frame
		if( alloc_encode_frame() == false)
			return;
		
		int ret;
		constexpr char api[] = "codec::AEPD::encode";
		bool free_flag{false};
		uint8_t **dst_data = nullptr;
		
		if( packet == nullptr){
			//当编码结束时传入nullptr
			ret = avcodec_send_frame(encoder_ctx, nullptr);
		} else {
			if( ifmt != default_resample_format){
				//需要重采样
				
				//只有在resample重新生成才需要初始化,一般在open_ctx里面就初始化了
				if( resample == nullptr && _init_resample() == false)
					return ;
				
				if( packet->format.channels == 0 && packet->format.bits == 0){
					core::Logger::Print_APP_Info(core::MessageNum::Format_invalid_format,
												 api,
												 LogLevel::WARNING_LEVEL);
					return;
				}
				
				int nb{0},size{0};
				uint8_t **src_data = static_cast<uint8_t**>(packet->data);
				if( resample->resample(&src_data,packet->size / (packet->format.bits * 4 / packet->format.channels),
									   &dst_data,nb,size) == false){
					//日志在resample里面已经输出，这里就不输出了
					return;
				}
				
//				memcpy(encode_frame->data,dst_data,sizeof(uint8_t**) * 4);
				encode_frame->data[0] = dst_data[0];
				free_flag = true;
				encode_frame->nb_samples = nb;
				
			}
			else {
				//不需要重采样
//				memcpy(encode_frame->data,packet->data,sizeof(packet->data));
				//只用到第一个
				encode_frame->data[0] = packet->data[0];
				encode_frame->nb_samples = packet->size / (packet->format.bits * 4 / packet->format.channels);
			}
			
			/* send the frame for encoding */
			ret = avcodec_send_frame(encoder_ctx, encode_frame);
		}
		
		if(ret < 0){
			core::Logger::Print_FFMPEG_Info(ret,api,LogLevel::WARNING_LEVEL);
			if(free_flag){
				av_free(dst_data[0]);
				av_free(dst_data);
			}
			return;
		}
		
		receive_packet();
		
		if(free_flag){
			av_free(dst_data[0]);
			av_free(dst_data);
		}
	}
	
	/**
	 * @brief receive_packet
	 * 获取编码后的包，并推进队列
	 */
	inline void receive_packet() noexcept{
		int ret = 0;
		constexpr char api[] = "codec::AEPD::receive_packet";
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
			dst_packet->format = default_resample_format;
			//设置有效负载类型，也就是编码格式
			dst_packet->payload_type = payload_type;
			
			dst_packet->pts = src_packet->pts;
			dst_packet->dts = src_packet->dts;
			core::Logger::Print("audio size:{}",
								api,
								LogLevel::ALLINFO_LEVEL,
								dst_packet->size);
			
			//让退出循环时不要释放掉该packet
			src_packet = nullptr;
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
	 * 编码器名字
	 * @param format
	 * 用于初始化编码器参数
	 * @return 
	 */
	inline bool _init_encoder(const char * name,const core::Format& format) noexcept {
		encoder = avcodec_find_encoder_by_name(name);
		constexpr char api[] = "codec::AEPD::_init_encoder";
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
	 * @brief _set_encoder
	 * 设置编码器上下文
	 */
	inline void _set_encoder_param(const core::Format & format) noexcept{
		//这里好像初始化其他格式会导致open codec失败
		//所以先设置好默认的格式
		UNUSED(format)
//		switch (format.bits) {
//		case 8:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_U8;
//			break;
//		case 16:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
//			break;
//		case 32:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_FLT;
//			break;
//		case 64:
//			encoder_ctx->sample_fmt = AV_SAMPLE_FMT_DBL;
//			break;
//		}
		
		encoder_ctx->sample_fmt = AV_SAMPLE_FMT_S16;
		encoder_ctx->bit_rate = 64000;
		
		/* select other audio parameters supported by the encoder */
		encoder_ctx->sample_rate    = 44100;
		encoder_ctx->channel_layout = AV_CH_LAYOUT_STEREO;
		encoder_ctx->channels       = 2;
//		encoder_ctx->profile = FF_PROFILE_UNKNOWN;
		
	}
	
	/**
	 * @brief _init_resample
	 * 初始化采样器，如果分配失败，则该次编码跳过
	 * @return 
	 */
	inline bool _init_resample() noexcept {
		if( resample == nullptr){
			resample = new audio_processing::Resample;
			if( resample == nullptr)
				return false;
		}
		
		resample->set_default_output_format(default_resample_format);
		resample->set_default_input_format(ifmt);
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////

AudioEncoder::AudioEncoder():
	_queue(nullptr),
	d_ptr(new AudioEncoderPrivateData(this))
{
}

AudioEncoder::AudioEncoder(AudioEncoder::Queue *queue):
	_queue(queue),
	d_ptr(new AudioEncoderPrivateData(this))
{
	start_thread();
}

AudioEncoder::~AudioEncoder()
{
	set_input_queue(nullptr);
	exit_thread();
	delete d_ptr;
}

int AudioEncoder::get_encoder_id() noexcept
{
	if(d_ptr->encoder == nullptr)
		return 0;
	return d_ptr->encoder->id;
}

void AudioEncoder::on_thread_run() noexcept
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

void AudioEncoder::on_thread_pause() noexcept
{
	d_ptr->encode(nullptr);
}

bool AudioEncoder::get_thread_pause_condition() noexcept
{
	return _queue == nullptr;
}

/**
 * @brief get_next_packet
 * 从队列里获取下一个包
 * @return 
 * 如果失败则返回nullptr
 */
core::FramePacket::SharedPacket AudioEncoder::_get_next_packet() noexcept 
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
