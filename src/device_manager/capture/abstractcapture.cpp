
#include "abstractcapture.h"
#include <iostream>

extern "C"
{
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
}

namespace rtplivelib::device_manager::capture {


/**
 * @def AbstractCapture
 * @brief
 * 生成线程，并将自己移到该线程，该类的子类所有slot都是异步的
 * @param parent
 * @param type
 * 捕捉数据的类型，在子类的构造函数中输入
 */
AbstractCapture::AbstractCapture(CaptureType type) noexcept:
	current_device_name("no have name"),
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
//   /*清空以前的帧,重开后可能是时间过了很久*/
//   clear();
   /*notify需要先解锁再调用，因为线程需要获取锁的使用权*/
   /*如果程序因为这个原因出错，则取消注释*/
//		_mutex->lock();
   _is_running_flag = true;
//		_mutex->unlock();
   /*如果线程启动了，则唤醒线程处理，否则自己处理*/
   if(!get_exit_flag())
	   notify_thread();
   else
	   on_start();
}

 /**
  * @brief stopCapture
  * 停止捕捉数据
  */
void AbstractCapture::stop_capture() noexcept
{
   /*只需设置标志位.其实已经设置了volatile标志了,没必要上锁了*/
   /*如果程序因为这个原因出错，则取消注释*/
//		std::lock_guard<std::mutex> lk(*_mutex);
   _is_running_flag = false;
}

/**
 * @brief SendErrorMsg
 * 通过错误代码num，打印错误文本
 * @param num
 */
void AbstractCapture::PrintErrorMsg(int num)
{
	char ch[1024];
	std::cout << av_make_error_string(ch,1024,num)<<std::endl;
}

/**
 * @brief YUYV2RGB32
 * YUYV(摄像头默认的格式)格式转为RGB32(这个是捕捉屏幕的格式)格式
 * 该接口方便自己处理显示图像时，又不支持YUV格式图像的情况(说的就是Qt)
 * RGB32占4字节，YUYV占2字节，所以转完后大小加倍了
 * @return 
 * 返回一个新的FramePacket包，输入的参数仍然有效
 * 所以要记住手动调用ReleasePacket释放
 */
rtplivelib::core::FramePacket *AbstractCapture::YUYV2RGB32(FramePacket *ptr)
{
	// is stored using a 32-bit RGB format (0xAARRGGBB).
	//小端
	return ChangePixelFormat(AV_PIX_FMT_YUYV422,AV_PIX_FMT_BGRA,ptr);
}

/**
 * @brief RGB322YUYV
 * RGB32(这个是捕捉屏幕的格式)格式转为YUYV(摄像头默认的格式)格式
 * 该接口主要用于内部编码使用，为了统一图像格式使用
 * 一般不会只支持YUV而不支持RGB的
 * @return 
 * 返回一个新的FramePacket包，输入的参数仍然有效
 * 所以要记住手动调用ReleasePacket释放
 */
rtplivelib::core::FramePacket *AbstractCapture::RGB322YUYV(FramePacket *ptr)
{
	return ChangePixelFormat(AV_PIX_FMT_BGRA,AV_PIX_FMT_YUYV422,ptr);
}

rtplivelib::core::FramePacket *AbstractCapture::ChangePixelFormat(int src_fromat, int dst_format, FramePacket *src_packet)
{
	if(src_packet == nullptr)
		return src_packet;
	AVPixelFormat src_fmt = static_cast<AVPixelFormat>(src_fromat);
	AVPixelFormat dst_fmt = static_cast<AVPixelFormat>(dst_format);
	
	//分配上下文
	SwsContext *ctx = sws_getContext(src_packet->format.width, src_packet->format.height, src_fmt,
									 src_packet->format.width, src_packet->format.height, dst_fmt,
									 SWS_BICUBIC,
									 nullptr, nullptr, nullptr);
	//Structures
	uint8_t *src_data[4];
	int src_linesize[4];
 
	uint8_t *dst_data[4];
	int dst_linesize[4];
	
	int ret;
	//分配临时空间，用于转换的时候
	ret = av_image_alloc(src_data,src_linesize,src_packet->format.width,src_packet->format.height,src_fmt,1);
	if(ret < 0){
		std::cout << "ChangePixelFormat error" << std::endl;
		sws_freeContext(ctx);
		return nullptr;
	}
	ret = av_image_alloc(dst_data,dst_linesize,src_packet->format.width,src_packet->format.height,dst_fmt,1);
	if(ret < 0){
		std::cout << "ChangePixelFormat error" << std::endl;
		sws_freeContext(ctx);
		av_freep(&src_data[0]);
		return nullptr;
	}
	
	//拷贝图像数据过来
	memcpy(src_data[0],src_packet->data[0],static_cast<size_t>(src_packet->size));
	//开始转换
	sws_scale(ctx, src_data, src_linesize, 0, src_packet->format.height, dst_data, dst_linesize);
	//生成本程序使用的FramePacket
	FramePacket * dst_packet = new FramePacket;
	dst_packet->format = src_packet->format;
	dst_packet->size = dst_linesize[0] * dst_packet->format.height;
	dst_packet->data[0] = new uint8_t[static_cast<size_t>(dst_packet->size)];
	memcpy(dst_packet->data[0],dst_data[0],static_cast<size_t>(dst_packet->size));
	//释放空间
	sws_freeContext(ctx);
	av_freep(&src_data[0]);
	av_freep(&dst_data[0]);
	return dst_packet;
}

/**
 * @brief on_thread_run
 * 改为私有权限，向子类屏蔽线程实现细节
 * 其实是子类实现需要获取返回值，中间再包装一层
 */
void AbstractCapture::on_thread_run()
{
	/*从子类实现中获取到数据包*/
	auto packet = this->on_start();
	/*如果有子类重写on_frame_data函数并返回false，则不加入队列*/
	/*这里不判断包是否为空*/
	if(this->on_frame_data(packet)){
		this->push_packet(packet);
	}
}

} // namespace capture
