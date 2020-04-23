#include "videodecoder.h"
#include "../core/logger.h"
#include "hardwaredevice.h"
extern "C"{
#include "libavcodec/avcodec.h"
}

namespace rtplivelib {

namespace codec {

/**
 * @brief The VideoDecoderPrivateData class
 * 这个类负责真正的解码操作，VideoDecoder只负责拿包
 * 这里有一个需要说明的是这个类只会被VideoDecoder调用
 * 所以不需要上锁，不存在同步
 */
class VideoDecoderPrivateData {
public:
	using PayloadType = codec::VideoDecoder::Packet::first_type;
	using Packet = codec::VideoDecoder::Packet::second_type;
public:
	AVCodec								*decoder{nullptr};
	AVCodecContext						*decoder_ctx{nullptr};
	AVCodecParserContext				*parser_ctx{nullptr};
	PayloadType							cur_pt{PayloadType::RTP_PT_NONE};
	core::Format						cur_fmt;
	player::VideoPlayer					*player{nullptr};
	AVPacket							*pkt{nullptr};
	AVFrame								*frame{nullptr};
	AVFrame								*sw_frame{nullptr};
	
	//以下参数用于硬件加速
	HardwareDevice						*hwdevice{nullptr};
	//用于用户设置
	volatile HardwareDevice::HWDType	hwd_type_user{HardwareDevice::Auto};
	//目前正在使用的类型,用于判断用户是否修改硬件加速方案
	HardwareDevice::HWDType				hwd_type_cur{HardwareDevice::None};
	bool								use_hw_flag{true};
	
	
	VideoDecoderPrivateData(){
	}
	
	~VideoDecoderPrivateData(){
		close_parser_ctx();
		close_codec_ctx();
		av_packet_free(&pkt);
		av_frame_free(&frame);
		
		if(hwdevice != nullptr)
			delete hwdevice;
	}
	
	/**
	 * @brief close_ctx
	 * 关闭解码器的上下文
	 */
	inline void close_codec_ctx() noexcept {
		if(decoder_ctx != nullptr){
			if(pkt != nullptr){
				//清空缓存
				pkt->data = nullptr;
				pkt->size = 0;
				decode();
			}
			avcodec_free_context(&decoder_ctx);
		}
	}
	
	/**
	 * @brief close_parser_ctx
	 * 关闭解析器上下文
	 */
	inline void close_parser_ctx() noexcept {
		if( parser_ctx != nullptr){
			av_parser_close(parser_ctx);
			parser_ctx = nullptr;
		}
	}
	
	/**
	 * @brief init_parser_ctx
	 * 初始化解析器上下文
	 * 如果已经初始化，则关闭之前的上下文重新初始化
	 */
	inline void init_parser_ctx() noexcept {
		close_parser_ctx();
		
		parser_ctx = av_parser_init(decoder->id);
		
		if(parser_ctx == nullptr){
			core::Logger::Print_APP_Info(core::Result::Codec_decoder_parser_init_failed,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			return;
		}
	}
	
	/**
	 * @brief init_packet
	 * 初始化AVPacket，如果已经初始化过则不进行任何操作
	 */
	inline void init_packet() noexcept {
		if( pkt != nullptr)
			return;
		
		pkt = av_packet_alloc();
	}
	
	/**
	 * @brief init_frame
	 * 初始化AVFrame，如果已经初始化过则不进行任何操作
	 */
	inline void init_frame() noexcept {
		if( frame != nullptr)
			return;
		
		frame = av_frame_alloc();
		sw_frame = av_frame_alloc();
	}
	
	/**
	 * @brief check_pt
	 * 检查包的有效载荷类型，如果发现更换了，得及时调整编码上下文
	 */
	inline void check_pt(const PayloadType pt,
						 const Packet & pack) noexcept {
		
		if(use_hw_flag == true){
			//硬解的初始化
			if( pt == cur_pt ){
				if( hwd_type_user == HardwareDevice::Auto && hwd_type_cur != HardwareDevice::None)
					//负载一样和使用了自动硬解的话，则不判断直接返回
					return;
				else if( hwd_type_user == hwd_type_cur )
					return;
			}
			
			cur_pt = pt;
			
			switch (hwd_type_user) {
			case HardwareDevice::None:{
				//没设置则进入软解
				use_hw_flag = false;
				hwd_type_cur = HardwareDevice::None;
				check_pt(pt,pack);
				return;
			}
			case HardwareDevice::Auto:{
				//暂时不实现自动
				if(hwdevice == nullptr)
					hwdevice = new HardwareDevice();
				auto ret = _init_hwdevice(HardwareDevice::QSV,pt);
				if(ret == true){
					hwd_type_cur = HardwareDevice::QSV;
				}
				else {
					hwd_type_cur = HardwareDevice::None;
					use_hw_flag = false;
					check_pt(pt,pack);
				}
				break;
			}
			case HardwareDevice::DXVA:{
				if(hwdevice == nullptr)
					hwdevice = new HardwareDevice();
				auto ret = _init_hwdevice(HardwareDevice::DXVA,pt);
				if(ret == true){
					hwd_type_cur = HardwareDevice::DXVA;
				}
				else {
					hwd_type_cur = HardwareDevice::None;
					use_hw_flag = false;
					check_pt(pt,pack);
				}
				break;
			}
			case HardwareDevice::NVIDIA:{
				if(hwdevice == nullptr)
					hwdevice = new HardwareDevice();
				auto ret = _init_hwdevice(HardwareDevice::NVIDIA,pt);
				if(ret == true){
					hwd_type_cur = HardwareDevice::NVIDIA;
				}
				else {
					hwd_type_cur = HardwareDevice::None;
					use_hw_flag = false;
					check_pt(pt,pack);
				}
				break;
			}
			case HardwareDevice::QSV:{
				if(hwdevice == nullptr)
					hwdevice = new HardwareDevice();
				auto ret = _init_hwdevice(HardwareDevice::QSV,pt);
				if(ret == true){
					hwd_type_cur = HardwareDevice::QSV;
				}
				else {
					hwd_type_cur = HardwareDevice::None;
					use_hw_flag = false;
					check_pt(pt,pack);
				}
				break;
			}
			default:
				core::Logger::Print_APP_Info(core::Result::Function_not_implemented,
											 __PRETTY_FUNCTION__,
											 LogLevel::ERROR_LEVEL);
				hwd_type_cur = HardwareDevice::None;
				use_hw_flag = false;
				check_pt(pt,pack);
				return;
			}
		} else {
			//软解的初始化
			//这里需要注意一下，判断hwd_type_cur是为了防止硬解转为软解的时候的误判
			if( pt == cur_pt && parser_ctx != nullptr && decoder_ctx != nullptr )
				return;
			
			cur_pt = pt;
			hwd_type_cur = HardwareDevice::None;
			
			_init_sw_codec_ctx(cur_pt);
		}
	}
	
	/**
	 * @brief decode
	 * 解码操作
	 * 在这里就不判断各种上下文了，直接开始解码
	 */
	inline void decode() noexcept{
		int ret = 0;
		
		ret = avcodec_send_packet(decoder_ctx,pkt);
		if( ret < 0 ){
			core::Logger::Print_FFMPEG_Info(ret,
											__PRETTY_FUNCTION__,
											LogLevel::WARNING_LEVEL);
			return;
		}
		
		while( ret >= 0){
			ret = avcodec_receive_frame(decoder_ctx,frame);
			//到达终点和解码出错都将退出解码
			if( ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				return;
			else if( ret < 0){
				core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
												LogLevel::WARNING_LEVEL);
				return;
			}
			
			if(use_hw_flag == true){
				ret = av_hwframe_transfer_data(sw_frame, frame, 0);
				if (ret < 0) {
					fprintf(stderr, "Error transferring the data to system memory\n");
				}
				return;
			}
			
//			core::Logger::Print("size:{}",api,LogLevel::INFO_LEVEL,sw_frame->format);
		}
	}
	
	/**
	 * @brief parse
	 * 解析数据
	 */
	inline void parse(uint8_t *data,int size,int64_t pts,int64_t pos) noexcept {
		auto ret = 0;
		if(parser_ctx == nullptr || decoder_ctx == nullptr){
			core::Logger::Print_APP_Info(core::Result::Codec_parser_or_codec_not_init,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL);
			return;
		}
		
		while(size > 0){
			//解析数据，这里的步骤和说明文档是一样的
			ret = av_parser_parse2(parser_ctx,decoder_ctx,&pkt->data,&pkt->size,
								   data,size,pts,pts,pos);
			if ( ret < 0 ){
				core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
												LogLevel::WARNING_LEVEL);
				return;
			}
			
			data += ret;
			size -= ret;
			
			if( pkt->size ){
				decode();
				display();
			}
		}
	}
	
	inline void display() noexcept {
		if( player == nullptr)
			return;
		
		uint8_t * data[4] = {nullptr,nullptr,nullptr,nullptr};
		int linesize[4] = {0,0,0,0};
		bool is_alloc = false;
		
		switch( frame->format ){
		//这个我感觉不可能的，不过还是加上去
		case AV_PIX_FMT_YUYV422:
			memcpy(data,frame->data,sizeof(data));
			memcpy(linesize,frame->linesize,sizeof(linesize));
			break;
		case AV_PIX_FMT_YUV422P:
		{
			is_alloc = true;
			linesize[0] = frame->linesize[0] * 2;
			auto size = static_cast<size_t>(linesize[0]) * 
						static_cast<size_t>(frame->height);
			data[0] = static_cast<uint8_t *>(av_malloc( size ));
			
			auto _y = frame->data[0] - 1;
			auto _u = frame->data[1] - 1;
			auto _v = frame->data[2] - 1;
			auto _d = data[0] - 1;
			for(decltype (size) n = 0; n < size; n += 4){
				*++_d = *++_y;
				*++_d = *++_u;
				*++_d = *++_y;
				*++_d = *++_v;
			}
			break;
		}
		case AV_PIX_FMT_YUV420P:{
			memcpy(data,frame->data,sizeof(data));
			memcpy(linesize,frame->linesize,sizeof(linesize));
			break;
		}
		case AV_PIX_FMT_QSV:
		case AV_PIX_FMT_NV12:{
			memcpy(data,sw_frame->data,sizeof(data));
			memcpy(linesize,sw_frame->linesize,sizeof(linesize));
			break;
		}
		default:
			core::Logger::Print_APP_Info(core::Result::SDL_not_supported_format,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 frame->format);
			return;
		}
		
		if(use_hw_flag == true){
			cur_fmt.width = sw_frame->width;
			cur_fmt.height = sw_frame->height;
			cur_fmt.pixel_format = sw_frame->format;
		} else {
			cur_fmt.width = frame->width;
			cur_fmt.height = frame->height;
			cur_fmt.pixel_format = frame->format;
		}
		if( player != nullptr)
			//这里是解码后立即渲染，可否考虑异步渲染?
			player->play(cur_fmt,data,linesize);
		
		//释放本次分配的空间
		if(is_alloc){
			for(auto _d : data){
				if(_d != nullptr)
					av_free(_d);
			}
		}
	}
	
	inline void deal_with_pack(const codec::VideoDecoder::Packet& pack) noexcept {
		if(pack.second.get() == nullptr)
			return;
		
		//检查格式并初始化解码器
		check_pt(pack.first,pack.second);
		
		//在这里，需要先初始化Packet和Frame，
		//后面的步骤少不了这两个数据结构
		if(pkt == nullptr){
			init_packet();
			if(pkt == nullptr)
				return;
		}
		
		if(frame == nullptr){
			init_frame();
			if(frame == nullptr)
				return;
		}
		
		if(hwdevice != nullptr && hwdevice->get_init_result() == true){
			pkt->data = (*pack.second->data)[0];
			pkt->size = pack.second->data->size;
			//硬件加速，不需要解析
			decode();
			display();
		} else {
			//解析并解码
			parse((*pack.second->data)[0],pack.second->data->size,pack.second->pts,pack.second->pos);
		}
	}
	
private:
	/**
	 * @brief _init_encoder
	 * 通过解码器名字初始化编码器,同时初始化解码器上下文
	 * @param name
	 * 硬件加速的解码器名字
	 * @param format
	 * 用于初始化解码器参数
	 * @return 
	 */
	inline bool _init_decoder(const char * name) noexcept {
		decoder = avcodec_find_decoder_by_name(name);
		if(decoder == nullptr){
			core::Logger::Print_APP_Info(core::Result::Codec_decoder_not_found,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 name);
			return false;
		}
		else {
			close_codec_ctx();
			decoder_ctx = avcodec_alloc_context3(decoder);
			if(decoder_ctx == nullptr){
				core::Logger::Print_APP_Info(core::Result::Codec_codec_context_alloc_failed,
											 __PRETTY_FUNCTION__,
											 LogLevel::WARNING_LEVEL);
				return false;
			}
			
			core::Logger::Print_APP_Info(core::Result::Codec_decoder_init_success,
										 __PRETTY_FUNCTION__,
										 LogLevel::INFO_LEVEL,
										 decoder->long_name);
			return true;
		}
	}
	
	/**
	 * @brief _init_hwdevice
	 * 初始化硬件设备并启动编码器
	 * @param hwdtype
	 * 硬件设备类型
	 * @param pt
	 * 负载类型，用过负载类型获取对应的解码器
	 * @return 
	 */
	inline bool _init_hwdevice(HardwareDevice::HWDType hwd_type,PayloadType pt) noexcept {
		bool ret;
		switch(hwd_type){
		case HardwareDevice::QSV:
			switch(pt){
			case PayloadType::RTP_PT_HEVC:
				ret = _init_decoder("hevc_qsv");
				break;
			case PayloadType::RTP_PT_H264:
				ret = _init_decoder("h264_qsv");
				break;
			case PayloadType::RTP_PT_VP9:
				ret = _init_decoder("vp9_qsv");
				break;
			default:
				return false;
			}
			if( ret == false || hwdevice->init_device(decoder_ctx,decoder,hwd_type) == false)
				return false;
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
	 * @brief init_ctx
	 * 初始化软解用的上下文
	 */
	inline void _init_sw_codec_ctx(PayloadType pt) noexcept {
		
		AVCodecID id;
		
		switch (pt) {
		case PayloadType::RTP_PT_HEVC:
			id = AV_CODEC_ID_HEVC;
			break;
		case PayloadType::RTP_PT_H264:
			id = AV_CODEC_ID_H264;
			break;
		case PayloadType::RTP_PT_VP9:
			id = AV_CODEC_ID_VP9;
			break;
		case PayloadType::RTP_PT_AAC:
			id = AV_CODEC_ID_AAC;
			break;
		default:
			id = AV_CODEC_ID_NONE;
			break;
		}
		
		decoder = avcodec_find_decoder(id);
		if(decoder == nullptr) {
			core::Logger::Print_APP_Info(core::Result::Codec_decoder_not_found,
										 __PRETTY_FUNCTION__,
										 LogLevel::WARNING_LEVEL,
										 id);
			//没找着格式的话，删除之前初始化过的上下文
			close_parser_ctx();
			close_codec_ctx();
		}
		else {
			//找到id后，初始化上下文
			//该接口会自己删除之前分配的上下文，所以不需要特别处理
			init_parser_ctx();
			
			close_codec_ctx();
			decoder_ctx = avcodec_alloc_context3(decoder);
			
			if(decoder_ctx == nullptr){
				core::Logger::Print_APP_Info(core::Result::Codec_codec_context_alloc_failed,
											 __PRETTY_FUNCTION__,
											 LogLevel::WARNING_LEVEL);
				return;
			}
			
			auto ret = avcodec_open2(decoder_ctx,decoder,nullptr);
			if(ret != 0){
				core::Logger::Print_APP_Info(core::Result::Codec_codec_open_failed,
											 __PRETTY_FUNCTION__,
											 LogLevel::WARNING_LEVEL);
				core::Logger::Print_FFMPEG_Info(ret,
												__PRETTY_FUNCTION__,
												LogLevel::WARNING_LEVEL);
			}
		}
	}
};

//////////////////////////////////////////////////////////////////////////////////////////

codec::VideoDecoder::VideoDecoder():
	d_ptr(new VideoDecoderPrivateData)
{
	start_thread();
}

codec::VideoDecoder::~VideoDecoder()
{
	exit_thread();
	delete d_ptr;
}

void VideoDecoder::set_player_object(player::VideoPlayer *player) noexcept
{
	d_ptr->player = player;
}

void VideoDecoder::set_hwd_type(HardwareDevice::HWDType type) noexcept
{
	d_ptr->hwd_type_user = type;
}

HardwareDevice::HWDType VideoDecoder::get_hwd_type() noexcept
{
	return d_ptr->hwd_type_cur;
}

void VideoDecoder::on_thread_run() noexcept
{
	//等待资源到来
	//100ms检查一次
	this->wait_for_resource_push(100);
	
	while(has_data()){
		auto pack = get_next();
		if(pack == nullptr)
			continue;
		if(pack->second != nullptr && pack->second->data != nullptr){
			pack->second->data->mutex.lock();
			d_ptr->deal_with_pack(*pack);
			pack->second->data->mutex.unlock();
		} else
			d_ptr->deal_with_pack(*pack);
	}
}

} // namespace codec

} // namespace rtplivelib

