#include "videoprocessingfactory.h"

#include <iostream>
#include <algorithm>
#include "timer.h"
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavdevice/avdevice.h>
	#include <libavutil/dict.h>

	#include <libavfilter/buffersink.h>
	#include <libavfilter/buffersrc.h>
	#include <libswscale/swscale.h>
	#include <libavutil/imgutils.h>
}
#include "mediadatacallback.h"

namespace rtplivelib {


////////////////////////////////////////////////////////////////////////////

DesktopCapture::DesktopCapture() :
	AbstractCapture(AbstractCapture::CaptureType::Desktop),
	_fps(15),
	ifmt(nullptr),fmtContxt(nullptr)
{
	avdevice_register_all();
	
#if defined (WIN32)
	ifmt = av_find_input_format("gdigrab");
	current_device_name = "desktop";
#elif defined (unix)
	ifmt = av_find_input_format("fbdev");
	//这里只是为了设置当前名字
	AVDeviceInfoList *info_list = nullptr;
	avdevice_list_input_sources(ifmt,nullptr,nullptr,&info_list);
	if(info_list != nullptr && info_list->nb_devices != 0){
		if(info_list->default_device == -1)
			current_device_name = info_list->devices[0]->device_description;
		else
			current_device_name = info_list->devices[info_list->default_device]->device_description;
	}
#endif
	
	/*在子类初始化里开启线程*/
	start_thread();
}

DesktopCapture::~DesktopCapture()
{
	//退出线程
	exit_thread();
}

/**
 * @brief set_fps
 * 设置帧数，每秒捕捉多少张图片
 * @param value
 * 每秒帧数
 */
void DesktopCapture::set_fps(uint value)  noexcept{
	if (value == _fps)
		return;
	_fps = value;
	open_device();
}

/**
 * @brief set_window
 * 设置将要捕捉的窗口
 * @param id
 * 窗口句柄，0为桌面
 */
void DesktopCapture::set_window(const uint64_t & id)  noexcept{
	_wid = id;
//	SendErrorMsg(avformat_open_input(&fmtContxt, "title={}", ifmt, &options));
}

/**
 * @brief get_all_device_info
 * 获取所有设备的信息，其实也就是名字而已
 * @return 
 * 返回一个vector
 */
std::map<std::string,std::string> DesktopCapture::get_all_device_info() noexcept
{
	std::map<std::string,std::string> info_map;
	
	if(ifmt == nullptr){
		std::cout << "not found input format" << std::endl;
		return info_map;
	}
	
	AVDeviceInfoList *info_list = nullptr;
	auto n = avdevice_list_input_sources(ifmt,nullptr,nullptr,&info_list);
	PrintErrorMsg(n);
	if(info_list){
		for(auto index = 0;index < info_list->nb_devices;++index){
			std::pair<std::string,std::string> pair;
			pair.first = info_list->devices[index]->device_description;
			pair.second = info_list->devices[index]->device_name;
			info_map.insert(pair);
		}
	}
	return info_map;
}

/**
 * @brief set_current_device_name
 * 根据名字设置当前设备，成功则返回true，失败则返回false 
 */
bool DesktopCapture::set_current_device_name(std::string name) noexcept
{
	if(name.compare(current_device_name) == 0)
		return true;
	auto temp = current_device_name;
	current_device_name = name;
	auto result = open_device();
	if(!result){
		current_device_name = temp;
	}
	return result;
}

/**
 * @brief on_start
 * 开始捕捉画面后的回调,实际开始捕捉数据的函数
 */
FramePacket * DesktopCapture::on_start() 
{
	if(fmtContxt == nullptr){
		if(!open_device()){
			stop_capture();
			return nullptr;
		}
	}
	/*这里说明一下，为什么不需要用计时器来算每帧间隔*/
	/*这里的av_read_frame函数会阻塞，一直等到有帧可读才返回，所以不需要自己设定间隔*/
	/*加锁是怕主线程在重启设备fmtContxt*/
	_fmt_ctx_mutex.lock();
	AVPacket *packet = av_packet_alloc();
	av_read_frame(fmtContxt, packet);
		
	auto ptr = new FramePacket;
#if defined (WIN32)
	ptr->packet = packet;
	ptr->data[0] = packet->data + 54;
	ptr->size = packet->size - 54;
	/*width在头结构地址18偏移处，详情参考BMP头结构*/
	memcpy(&ptr->format.width,packet->data + 18,4);
	/*height在头结构地址22偏移处，不过总为0*/
	/*所以采用数据大小除以宽和像素位宽(RGB32是4位)得到高*/
	memcpy(&ptr->format.height,packet->data + 2,4);
	ptr->format.height = (ptr->format.height - 54) / ptr->format.width / 4;
#elif defined (unix)
	ptr->data[0] = static_cast<uint8_t*>(av_malloc(static_cast<size_t>(packet->size)));
	memcpy(ptr->data[0],packet->data,static_cast<size_t>(packet->size));
	ptr->size = packet->size;
	auto codec = fmtContxt->streams[packet->stream_index]->codecpar;
	ptr->format.width = codec->width;
	ptr->format.height = codec->height;
	
//	av_packet_unref(packet);
	av_packet_free(&packet);
#endif
	ptr->format.format = AV_PIX_FMT_BGRA;
	ptr->format.bits = 32;
	ptr->linesize[0] = ptr->format.width * 4;
	_fmt_ctx_mutex.unlock();
	return ptr;
}

/**
 * @brief on_stop
 * 结束捕捉画面后的回调，用于stop_capture后回收工作的函数
 * 可以通过重写该函数来处理暂停后的事情
 */
void DesktopCapture::on_stop() noexcept
{
	if(fmtContxt == nullptr)
		return;
	_fmt_ctx_mutex.lock();
	avformat_close_input(&fmtContxt);
	_fmt_ctx_mutex.unlock();
}

/**
 * @brief open_device
 * 尝试打开设备
 * @return 
 */
bool DesktopCapture::open_device() noexcept
{
	AVDictionary *options = nullptr;
	//windows采用GDI采集桌面屏幕，比较吃资源，以后需要更换API
	//设置帧数，经过测试，貌似最高是30，而且太吃资源，有空肯定换了这个API
	char fps_char[5];
	sprintf(fps_char,"%d",_fps);
	av_dict_set(&options,"framerate",fps_char,0);
//#if defined (WIN32)
	
//	if(_rect.width != 0 && _rect.height != 0){
//		//只要有0就使用全屏
//		char video_size[15];
//		char offset_x[5];
//		char offset_y[5];
//		sprintf(video_size,"%dx%d",_rect.width,_rect.height);
//		sprintf(offset_x,"%d",_rect.x);
//		sprintf(offset_x,"%d",_rect.y);
//		av_dict_set(&options,"video_size",video_size,0);
//		av_dict_set(&options,"offset_x",offset_x,0);
//		av_dict_set(&options,"offset_y",offset_y,0);
//	}
//#elif defined (unix)
//	/*目前Linux系统没啥特殊设置的*/
//#endif
	
	std::lock_guard<std::mutex> lk(_fmt_ctx_mutex);
	if(fmtContxt != nullptr){
		avformat_close_input(&fmtContxt);
	}
#if defined (WIN32)
	/*暂时只考虑截取屏幕的情况，截取窗口和另外的屏幕以后再考虑*/
	auto n = avformat_open_input(&fmtContxt, "desktop", ifmt, &options);
#elif defined (unix)
	auto n = avformat_open_input(&fmtContxt, get_all_device_info()[current_device_name].c_str(), ifmt, &options);
#endif
	PrintErrorMsg(n);
	return n == 0;
}

/////////////////////////////////////////////////////////////////////////

CameraCapture::CameraCapture() :
	AbstractCapture(AbstractCapture::CaptureType::Camera),
	_fps(15),
	ifmt(nullptr),
	fmtContxt(nullptr)
{
	avdevice_register_all();
	
#if defined (WIN32)
	ifmt = av_find_input_format("dshow");
#elif defined (unix)
	ifmt = av_find_input_format("v4l2");
	
	//这里只是为了设置当前名字
	AVDeviceInfoList *info_list = nullptr;
	avdevice_list_input_sources(ifmt,nullptr,nullptr,&info_list);
	if(info_list != nullptr && info_list->nb_devices != 0){
		if(info_list->default_device == -1)
			current_device_name = info_list->devices[0]->device_description;
		else
			current_device_name = info_list->devices[info_list->default_device]->device_description;
	}
#endif
	/*在子类初始化里开启线程*/
	start_thread();
}

CameraCapture::~CameraCapture()
{
	exit_thread();
}

/**
 * @brief set_fps
 * 设置帧数
 * @param value
 * 帧数
 */
void CameraCapture::set_fps(uint value) {
	if (value == _fps)
		return;
	_fps = value;
	open_device();
}

std::map<std::string,std::string> CameraCapture::get_all_device_info() noexcept
{
	std::map<std::string,std::string> info_map;
	
	if(ifmt == nullptr){
		std::cout << "not found input format" << std::endl;
		return info_map;
	}
	
	AVDeviceInfoList *info_list = nullptr;
	auto n = avdevice_list_input_sources(ifmt,nullptr,nullptr,&info_list);
	PrintErrorMsg(n);
	if(info_list){
		for(auto index = 0;index < info_list->nb_devices;++index){
			std::pair<std::string,std::string> pair;
			pair.first = info_list->devices[index]->device_description;
			pair.second = info_list->devices[index]->device_name;
			info_map.insert(pair);
		}
	}
	return info_map;
}

/**
 * @brief set_current_device_name
 * 根据名字设置当前设备，成功则返回true，失败则返回false 
 */
bool CameraCapture::set_current_device_name(std::string name) noexcept
{
	if(name.compare(current_device_name) == 0)
		return true;
	auto temp = current_device_name;
	current_device_name = name;
	auto result = open_device();
	if(!result){
		current_device_name = temp;
	}
	return result;
}

/**
 * @brief on_start
 * 开始捕捉画面后的回调
 */
FramePacket * CameraCapture::on_start()  {
	if(fmtContxt == nullptr){
		if(!open_device()){
			stop_capture();
			return nullptr;
		}
	}
	
	//开始捕捉前，睡眠1ms
	//防止刚解锁就拿到锁，其他线程饥饿
	std::chrono::milliseconds dura(1);
    std::this_thread::sleep_for(dura);
	
	std::lock_guard<std::mutex> lk(_fmt_ctx_mutex);
	
	AVPacket *packet = av_packet_alloc();
	if(av_read_frame(fmtContxt, packet) < 0){
		av_packet_free(&packet);
		return nullptr;
	}
	
	auto codec = fmtContxt->streams[packet->stream_index]->codecpar;
	
	auto ptr = new FramePacket;
	ptr->format.width = codec->width;
	ptr->format.height = codec->height;
	
	//packet在close input format后就失效
	//所以需要拷贝一份
	ptr->data[0] = static_cast<uint8_t*>(av_malloc(static_cast<size_t>(packet->size)));
	memcpy(ptr->data[0],packet->data,static_cast<size_t>(packet->size));
	ptr->size = packet->size;
	ptr->format.bits = 16;
	ptr->linesize[0] = ptr->format.width * 2;
	ptr->format.format = AV_PIX_FMT_YUYV422;
	
//	av_packet_unref(packet);
	av_packet_free(&packet);
	return ptr;
}

/**
 * @brief on_stop
 * 结束捕捉画面后的回调
 */
void CameraCapture::on_stop()  noexcept{
	if(fmtContxt == nullptr)
		return;
	std::lock_guard<std::mutex> lk(_fmt_ctx_mutex);
	avformat_close_input(&fmtContxt);
}

/**
 * @brief open_device
 * 尝试打开设备
 * @return 
 */
bool CameraCapture::open_device() noexcept
{
	char fps_char[5];
	sprintf(fps_char,"%d",_fps);
	AVDictionary *options = nullptr;
	av_dict_set(&options,"framerate",fps_char,0);
	av_dict_set(&options,"video_size","640x480",0); 
	
	std::lock_guard<std::mutex> lk(_fmt_ctx_mutex);
	if(fmtContxt != nullptr){
		avformat_close_input(&fmtContxt);
	}
#if defined (WIN32)
	auto n = avformat_open_input(&fmtContxt,"video=HD Pro Webcam C920",ifmt,&options);
#elif defined (unix)
	auto n = avformat_open_input(&fmtContxt, get_all_device_info()[current_device_name].c_str(),ifmt,&options);
#endif
	PrintErrorMsg(n);
	return n == 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief The VideoProcessingFactoryPrivataData class
 * 视频工厂处理类的私有数据类
 * 以后会放到头文件处理该类
 * 里面存放了VideoProcessingFactory类的私有成员和一些处理接口
 * @author 钟华荣
 * @date 2018-12-14
 */
class VideoProcessingFactoryPrivataData{
public:
	MediaDataCallBack *dcb_ptr{nullptr};
	Rect crop_rect;
	FRect overlay_rect;
	std::mutex crop_mutex;
	std::mutex overlay_mutex;
	Format scale_format;
	void *previous_camera_frame{nullptr};
	void *previous_desktop_frame{nullptr};
	
	///////////////////////
	//复用,可以进行格式判断，格式不一致则重新分配滤镜
	FramePacket *crop_dst_packet{new FramePacket};
	FramePacket *scale_dst_packet{new FramePacket};
	AVFilterContext *buffersink_ctx{nullptr};
	AVFilterContext *buffersrc_ctx{nullptr};
	AVFilterGraph *filter_graph{nullptr};
	SwsContext *scale_ctx{nullptr};
	
	~VideoProcessingFactoryPrivataData(){
		AbstractQueue::ReleasePacket(crop_dst_packet);
		release_filter();
		if(scale_ctx != nullptr){
			sws_freeContext(scale_ctx);
		}
		av_freep(&scale_dst_packet->data[0]);
		delete scale_dst_packet;
	}
	
	/**
	 * @brief init_filter
	 * 初始化滤镜
	 * 需要输入原格式，输出格式是默认YUYV
	 * @param filters_descr
	 * 特效
	 * @param src_format
	 * 输入的原格式
	 * @return 
	 * 是否成功,小于0是失败
	 */
	int init_filter(const char *filters_descr,FramePacket * src)
	{
		char args[512];
		int ret = 0;
		const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
		const AVFilter *buffersink = avfilter_get_by_name("buffersink");
		AVFilterInOut *outputs = avfilter_inout_alloc();
		AVFilterInOut *inputs  = avfilter_inout_alloc();
		enum AVPixelFormat pix_fmts[] = { AV_PIX_FMT_YUYV422, AV_PIX_FMT_NONE };
		
		filter_graph = avfilter_graph_alloc();
		if (!outputs || !inputs || !filter_graph) {
			ret = AVERROR(ENOMEM);
			goto end;
		}
	
		snprintf(args, sizeof(args),
				"video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
				src->format.width, src->format.height, src->format.format,
				1, 15,
				1, 1);
	
		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
										   args, nullptr, filter_graph);
		if (ret < 0) {
			av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer source\n");
			goto end;
		}
	
		/* buffer video sink: to terminate the filter chain. */
		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
										   nullptr, nullptr, filter_graph);
		if (ret < 0) {
			av_log(nullptr, AV_LOG_ERROR, "Cannot create buffer sink\n");
			goto end;
		}
	
		ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
								  AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
		if (ret < 0) {
			av_log(nullptr, AV_LOG_ERROR, "Cannot set output pixel format\n");
			goto end;
		}
	
		/*
		 * Set the endpoints for the filter graph. The filter_graph will
		 * be linked to the graph described by filters_descr.
		 */
	
		/*
		 * The buffer source output must be connected to the input pad of
		 * the first filter described by filters_descr; since the first
		 * filter input label is not specified, it is set to "in" by
		 * default.
		 */
		outputs->name       = av_strdup("in");
		outputs->filter_ctx = buffersrc_ctx;
		outputs->pad_idx    = 0;
		outputs->next       = nullptr;
	
		/*
		 * The buffer sink input must be connected to the output pad of
		 * the last filter described by filters_descr; since the last
		 * filter output label is not specified, it is set to "out" by
		 * default.
		 */
		inputs->name       = av_strdup("out");
		inputs->filter_ctx = buffersink_ctx;
		inputs->pad_idx    = 0;
		inputs->next       = nullptr;
	
		if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
										&inputs, &outputs, nullptr)) < 0)
			goto end;
	
		if ((ret = avfilter_graph_config(filter_graph, nullptr)) < 0)
			goto end;
	
	end:
		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);
		if(ret < 0){
			release_filter();
		}
		return ret;
	}
	
	/**
	 * @brief release_filter
	 * 释放滤镜相关的结构体
	 */
	void release_filter(){
		avfilter_free(buffersrc_ctx);
		avfilter_free(buffersink_ctx);
		avfilter_graph_free(&filter_graph);
		
		buffersrc_ctx = nullptr;
		buffersink_ctx = nullptr;
	}
	
	/**
	 * @brief crop
	 * 因为只有桌面需要裁剪，所以就不进行格式判断了
	 * 如果以后有其他格式使用该功能再考虑加
	 * (如果添加也只需要判断src的格式和之前的是否相同，
	 * 如果不同则重新分配就可以了，也不复杂,初始化函数已经适配好了)
	 * @param src
	 * @return 
	 */
	FramePacket * crop(FramePacket * src){
		if(src == nullptr)
			return nullptr;
		std::lock_guard<std::mutex> lk(crop_mutex);
		auto frame_out = static_cast<AVFrame*>(crop_dst_packet->frame);
		if(filter_graph == nullptr){
			char descr[512];
			sprintf(descr,"crop=%d:%d:%d:%d",crop_rect.width,
											 crop_rect.height,
											 crop_rect.x,
											 crop_rect.y);
			auto ret = init_filter(descr,src);
			std::cout << "init filter result:" << ret << std::endl;
			if(ret < 0)
				return nullptr;
			/*在分配滤镜的同时，初始化目标空间*/
			crop_dst_packet->format.height = crop_rect.height;
			crop_dst_packet->format.width = crop_rect.width;
			crop_dst_packet->format.format = AV_PIX_FMT_YUYV422;
//			crop_dst_packet->format.bits = av_get_bits_per_pixel(av_pix_fmt_desc_get(AV_PIX_FMT_YUYV422));
			//我知道是16位
			crop_dst_packet->format.bits = 16;
			
			if(frame_out == nullptr){
				frame_out = av_frame_alloc();
			}
			//不需要分配空间
//			av_image_alloc(frame_out->data,frame_out->linesize,crop_rect.width,crop_rect.height,AV_PIX_FMT_YUYV422,1);
			crop_dst_packet->frame = frame_out;
		}
		auto frame_in = av_frame_alloc();
		frame_in->width = src->format.width;
		frame_in->height = src->format.height;
		frame_in->format = src->format.format;
		
		frame_in->data[0] = src->data[0];
		frame_in->linesize[0] = src->linesize[0];
		
		int ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame_in, AV_BUFFERSRC_FLAG_PUSH);
		if(ret < 0)
			av_log(nullptr, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
		
		//先去除原来的空间
		av_frame_unref(frame_out);
		//然后获取帧
		av_buffersink_get_frame(buffersink_ctx,frame_out);
		memcpy(crop_dst_packet->data,frame_out->data,sizeof(crop_dst_packet->data));
		memcpy(crop_dst_packet->linesize,frame_out->linesize,sizeof(crop_dst_packet->linesize));
		crop_dst_packet->size = crop_dst_packet->format.height * frame_out->linesize[0];
		av_frame_free(&frame_in);
		
		return crop_dst_packet;
	}
	
	/**
	 * @brief scale
	 * 数据格式转换，用于桌面格式RGB转成YUYV
	 * 在不需要裁剪特效的时候使用
	 */
	FramePacket * scale(FramePacket * src_packet,const Format& dst_format){
		//当格式一样的时候不转换(当然不局限于像素格式，还有图片大小)
		if( src_packet == nullptr || src_packet->format == dst_format){
			return src_packet;
		}
		
		const AVPixelFormat src_fmt = static_cast<AVPixelFormat>(src_packet->format.format);
		const AVPixelFormat dst_fmt = static_cast<AVPixelFormat>(dst_format.format);
		
		//如果格式不一致(目标格式和预存格式)，则重新分配上下文和目标格式scale_dst_packet数据空间
		if( scale_dst_packet->format != dst_format){
			sws_freeContext(scale_ctx);
			scale_ctx = sws_getContext(src_packet->format.width, src_packet->format.height, src_fmt,
									   dst_format.width, dst_format.height, dst_fmt,
									   SWS_BICUBIC,
									   nullptr, nullptr, nullptr);
			
			scale_dst_packet->format = dst_format;
			av_freep(&scale_dst_packet->data[0]);
			av_image_alloc(scale_dst_packet->data,scale_dst_packet->linesize,
						   scale_dst_packet->format.width,scale_dst_packet->format.height,
						   dst_fmt,1);
		}
		
		scale_dst_packet->size = scale_dst_packet->linesize[0] * scale_dst_packet->format.height;
		/*可能存在上下文分配失败的情况*/
		if( scale_ctx == nullptr){
			std::cout << " no have scale context" << std::endl;
			return nullptr;
		}
		/*可能存在数据存放空间分配失败的情况*/
		if( scale_dst_packet->data[0] == nullptr){
			std::cout << " scale : av_image_alloc error" << std::endl;
			return nullptr;
		}
		
		uint8_t *src_data[4];
		int src_linesize[4];
		av_image_alloc(src_data,src_linesize,
					   src_packet->format.width,src_packet->format.height,
					   src_fmt,1);
		//数据对齐,src_packet->data数据可能没有内存对齐，所以拷贝到已经对齐的内存块
		//未对齐会造成速度损失
		memcpy(src_data[0],src_packet->data[0],src_packet->size);
		sws_scale(scale_ctx,src_data,src_linesize,0,src_packet->format.height,
				  scale_dst_packet->data,scale_dst_packet->linesize);
		av_freep(&src_data[0]);
		return scale_dst_packet;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief VideoProcessingFactory
 * 该类需要传入CameraCapture和DesktopCapture子类的实例
 * 该类将会统一两个对象的所有操作，方便管理
 */
VideoProcessingFactory::VideoProcessingFactory(CameraCapture *cc,DesktopCapture *dc):
	cc_ptr(cc),
	dc_ptr(dc),
	d_ptr(new VideoProcessingFactoryPrivataData)
{
	d_ptr->crop_rect.width = 0;
	d_ptr->crop_rect.height = 0;
	d_ptr->overlay_rect.x = 0.6f;
	d_ptr->overlay_rect.y = 0.7f;
	d_ptr->overlay_rect.width = 0.3f;
	
	/**
	 * 预设，因为只有转换桌面图像格式需要用到
	 * 以后扩展接口后，再设置缩放功能
	 * 缩放功能只需要修改d_ptr->scale_format的长宽就可以了
	 */
	d_ptr->scale_format.format = AV_PIX_FMT_YUYV422;
	d_ptr->scale_format.bits = 16;
	d_ptr->scale_format.width = 1920;
	d_ptr->scale_format.height = 1080;
	start_thread();
}

VideoProcessingFactory::~VideoProcessingFactory()
{
	set_capture(false,false);
	exit_thread();
	delete d_ptr;
}

/**
 * @brief set_camera_capture_object
 * 给你第二次机会设置对象指针
 * @return
 * 如果在运行过程中设置，则会返回false
 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
 */
bool VideoProcessingFactory::set_camera_capture_object(CameraCapture *cc) noexcept
{
	if(get_thread_pause_condition()){
		cc_ptr = cc;
		return true;
	}
	else
		return false;
}

/**
 * @brief set_desktop_capture_object
 * 给你第二次机会设置对象指针
 * @return
 * 如果在运行过程中设置，则会返回false
 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
 */
bool VideoProcessingFactory::set_desktop_capture_object(DesktopCapture *dc) noexcept
{
	if(get_thread_pause_condition()){
		dc_ptr = dc;
		return true;
	}
	else
		return false;
}

/**
 * @brief set_crop_rect
 * 设置裁剪区域，只留rect区域
 */
void VideoProcessingFactory::set_crop_rect(const Rect &rect) noexcept
{
	if(d_ptr->crop_rect == rect)
		return;
	std::lock_guard<std::mutex> lk(d_ptr->crop_mutex);
	d_ptr->crop_rect = rect;
	//暂时只有裁剪的时候用到滤镜，所以在裁剪锁里面释放
	//释放后，在裁剪的时候就会自动分配新的滤镜
	d_ptr->release_filter();
}

/**
 * @brief set_overlay_rect
 * 设置重叠区域，当摄像头和桌面一起开启的时候，就会由该rect决定如何重叠
 * @param rect
 * 该参数保存的是相对位置
 * 采用百分比，也就是说摄像头的位置全部由桌面的位置计算出来
 * 不会随着桌面大小的变化而使得摄像头位置改变
 * (摄像头画面的比例会保持原来比例，所以其实只要设置width就可以了)
 */
void VideoProcessingFactory::set_overlay_rect(const FRect &rect) noexcept
{
	if(d_ptr->overlay_rect == rect)
		return;
	std::lock_guard<std::mutex> lk(d_ptr->overlay_mutex);
	d_ptr->overlay_rect = rect;
}

/**
 * @brief set_capture
 * 设置是否捕捉数据
 * @param camera
 * true:捕捉摄像头数据
 * false:不捕捉
 * @param desktop
 * true:捕捉桌面数据
 * false:不捕捉
 */
void VideoProcessingFactory::set_capture(bool camera, bool desktop) noexcept
{
	if(cc_ptr !=nullptr){
		/*如果标志位和原来不一致才会执行下一步*/
		if(camera != cc_ptr->is_running()){
			/*开启捕捉*/
			if(camera){
				cc_ptr->start_capture();
			}
			/*关闭*/
			else{
				cc_ptr->stop_capture();
			}
		}
	}
	
	if(dc_ptr != nullptr){
		/*如果标志位和原来不一致才会执行下一步*/
		if(desktop != dc_ptr->is_running()){
			/*开启捕捉*/
			if(desktop){
				dc_ptr->start_capture();
			}
			/*关闭*/
			else{
				dc_ptr->stop_capture();
			}
		}
	}
	
	/*唤醒线程*/
	/*这里不需要检查条件，因为在唤醒的时候会自己检查条件*/
	notify_thread();
}

/**
 * @brief set_fps
 * 统一设置帧数,为了能够正确编码，最好使用该接口设置帧数
 * @param value
 * 帧数
 */
void VideoProcessingFactory::set_fps(uint value) noexcept
{
	if(cc_ptr)
		cc_ptr->set_fps(value);
	if(dc_ptr)
		dc_ptr->set_fps(value);
}

void VideoProcessingFactory::register_call_back_object(MediaDataCallBack *obj)noexcept
{		
	if(d_ptr->dcb_ptr != nullptr)
		delete d_ptr->dcb_ptr;
	d_ptr->dcb_ptr = obj;
}

/**
 * @brief on_thread_run
 * 线程运行时需要处理的操作
 */
void VideoProcessingFactory::on_thread_run()
{
	/**
	 * 在这里说明一下该线程的流程
	 * 对象实例生成后，处于等待状态，调用set_camera_capture_object
	 * 和set_desktop_capture_object设置捕捉类(需要编码线程暂停才可以设置)，
	 * 然后set_capture传入true开始捕捉，如果是外部调用捕捉类的start_capture接口
	 * 要使用notify_thread唤醒线程
	 * 
	 * 可以通过外部捕捉类的stop_capture来停止捕捉，该线程就会自动停止(参考
	 * get_thread_pause_condition接口),也可以通过set_capture传入false来
	 * 暂停线程
	 * 
	 * 当然，只要有一个捕捉类正在运行，则编码就会继续
	 * 
	 * 一开始则是先获取桌面帧，并设置等待时间，等待时间为一帧大概时间+5ms，然后获取
	 * 摄像头帧，等待时间为一帧大概时间+5ms-桌面帧等待时间(等待时间通过最大帧率决定)
	 * 帧率可以通过外部调用捕捉类的set_fps接口，也可以用该类的set_fps统一设置
	 */
	using namespace std::chrono;
	FramePacket * cp = nullptr;
	FramePacket * dp = nullptr;
	
	uint wait_time;
	//计算等待时间
	if(cc_ptr != nullptr&& cc_ptr->is_running() &&
			dc_ptr != nullptr && dc_ptr->is_running()){
		wait_time = cc_ptr->get_fps() > dc_ptr->get_fps() ?
					cc_ptr->get_fps():dc_ptr->get_fps();
		wait_time = 1000 / wait_time;
	}
	else if( cc_ptr != nullptr&& cc_ptr->is_running() ){
		wait_time = 1000 / cc_ptr->get_fps();
	}
	else if(dc_ptr != nullptr && dc_ptr->is_running()){
		wait_time = 1000 / dc_ptr->get_fps();
	}
	else{
		return;
	}
	
	auto before_time = system_clock::now();
	
	//桌面
	if( dc_ptr != nullptr && dc_ptr->is_running() ){
		//等待一个帧的时间
		if(dc_ptr->wait_for_resource_push(wait_time + 5)){
			//超时前获取到帧(也可能是队列本来就有帧，没有等待即返回)
			//这里设置循环，因为有可能因为机器性能不行，导致帧数溢出
			//这里不采取特别措施，只获取最后一帧
			while(dc_ptr->has_data()){
				dp = dc_ptr->get_next();
				ReleasePacket(static_cast<FramePacket*>(d_ptr->previous_desktop_frame));
				d_ptr->previous_desktop_frame = dp;
			}
		}
		else{
			//超时等待,获取上一帧来处理
			dp = static_cast<FramePacket*>(d_ptr->previous_desktop_frame);
		}
	}
	
	auto after_time = system_clock::now();
	
	auto duration = duration_cast<milliseconds>(after_time - before_time);

	//摄像头
	if( cc_ptr != nullptr&& cc_ptr->is_running() ){
		//等待一个帧时间减去桌面帧等待时间
		if( cc_ptr->wait_for_resource_push(wait_time + 5 - duration.count()) ){
			//超时前获取到帧(也可能是队列本来就有帧，没有等待即返回)
			//这里设置循环，因为有可能因为机器性能不行，导致帧数溢出
			//这里不采取特别措施，只获取最后一帧
			while(cc_ptr->has_data()){
				cp = cc_ptr->get_next();
				ReleasePacket(static_cast<FramePacket*>(d_ptr->previous_camera_frame));
				d_ptr->previous_camera_frame = cp;
			}
		}
		else{
			//超时等待
			cp = static_cast<FramePacket*>(d_ptr->previous_camera_frame);
		}
	}
	
	//桌面数据转为YUYV和裁剪
	//只要有一个0，则不裁剪
	if( dp != nullptr && d_ptr->crop_rect.width != 0 && d_ptr->crop_rect.height != 0)
		dp = d_ptr->crop(dp);
	
	//先触发回调
	if(d_ptr->dcb_ptr != nullptr){
		if(dp != nullptr){
			//未裁剪是rgb，裁剪后是yuv
			d_ptr->dcb_ptr->on_desktop_frame(dp);
		}
		
		if(cp != nullptr)
			d_ptr->dcb_ptr->on_camera_frame(cp);
	}
	
	FramePacket * merge_packet;
	if(dp != nullptr && cp != nullptr){
		//合成图像,接口暂时置空不处理
		merge_packet = _merge_frame(dp,cp);
		//回调接口
		if(d_ptr->dcb_ptr != nullptr)
			d_ptr->dcb_ptr->on_video_frame_merge(merge_packet);
	}
	else if( dp != nullptr){
		//如果不裁剪，桌面图像格式是rgb，需要格式转换
		//其实这里可以动态缩放图像大小
		if(dp->format.format != AV_PIX_FMT_YUYV422)
			merge_packet = d_ptr->scale(dp,d_ptr->scale_format);
		else
			merge_packet = dp;
	}
	else if(cp != nullptr){
		merge_packet = cp;
	}
	else
		return;
	
	//编码
	_coding_h265(merge_packet);
}

/**
 * @brief on_thread_pause
 * 线程暂停时的回调
 */
void VideoProcessingFactory::on_thread_pause()
{
//	FramePacket * ptr;
//	//继续处理队列剩余的数据
//	if( dc_ptr != nullptr ){
//		while(dc_ptr->has_data()){
//			ptr = dc_ptr->get_next();
//			ReleasePacket(ptr);
//		}
//	}
	
//	//摄像头
//	if( cc_ptr != nullptr){
//		while(cc_ptr->has_data()){
//			ptr = cc_ptr->get_next();
//			ReleasePacket(ptr);
//		}
//	}
}

/**
 * @brief get_thread_pause_condition
 * 该函数用于判断线程是否需要暂停
 * 线程不需要运行时需要让这个函数返回true
 * 如果需要重新唤醒线程，则需要让该函数返回false并
 * 调用notify_thread唤醒(顺序不要反了)
 * @return 
 * 返回true则线程睡眠
 * 默认是返回true，线程启动即睡眠
 */
bool VideoProcessingFactory::get_thread_pause_condition() noexcept
{
	if(cc_ptr == nullptr && dc_ptr == nullptr){
		return true;
	}
	return !( ( cc_ptr != nullptr && cc_ptr->is_running() ) ||
			  ( dc_ptr != nullptr && dc_ptr->is_running() ) );
}

/**
 * @brief _merge_frame
 * 合成图像帧
 * @param dp
 * 桌面图像帧
 * @param cp
 * 摄像头图像帧
 * @return 
 * 返回合成帧
 */
FramePacket *VideoProcessingFactory::_merge_frame(FramePacket *dp, FramePacket *cp)
{
//	FramePacket * merge = new FramePacket;
//	return merge;
	return dp;
}

void VideoProcessingFactory::_coding_h265(FramePacket * ptr)
{
//	AbstractQueue::ReleasePacket(ptr);
}

} // namespace rtplivelib
