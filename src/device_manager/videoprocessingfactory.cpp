#include "videoprocessingfactory.h"

#include <algorithm>
#include "../core/time.h"
#include "../core/logger.h"
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

namespace rtplivelib {

namespace device_manager {

/**
 * @brief The VideoProcessingFactoryPrivataData class
 * 视频工厂处理类的私有数据类
 * 以后会放到头文件处理该类
 * 里面存放了VideoProcessingFactory类的私有成员和一些处理接口
 */
class VideoProcessingFactoryPrivateData{
public:
	//转换格式用的结构体
	core::Format scale_format_current;
	core::Format scale_format_privious;
	//重叠用的结构体
	//FRect overlay_rect;
	//std::mutex overlay_mutex;
	//保存上一帧
	core::FramePacket::SharedPacket privious_camera_frame;
	core::FramePacket::SharedPacket privious_desktop_frame;
	//上一秒的时间戳，采用1秒多少张图片来判断每秒帧数
	int64_t privious_ts{0};
	uint8_t count{0};
	
	/**
	 * @brief get_latest_frame
	 * 获取最新的帧
	 * 这个接口是为了统一获取桌面和摄像头的获取帧方式
	 * 参数current_frame和privious_frame不共享，都是独立的
	 * @param caputre
	 * 捕捉类，用于获取帧
	 * @param current_frame
	 * 当前帧,这个应该是外部调用所关注的
	 * @param privious_frame
	 * 上一帧
	 * @param wait_time
	 * 用于等待资源的时间间隔
	 */
	inline core::FramePacket::SharedPacket get_latest_frame(AbstractCapture* caputre,
															core::FramePacket::SharedPacket &privious_frame,
															const int &wait_time) {
		
		//等待一个帧的时间
		if(caputre->wait_for_resource_push(wait_time)){
			//超时前获取到帧(也可能是队列本来就有帧，没有等待即返回)
			//这里不采取特别措施，只获取最后一帧
			privious_frame = caputre->get_latest();
			return privious_frame;
		}
		else {
			if(privious_frame == nullptr){
				//如果连上一帧都没有，则返回空
				//一般在设备刚启动的时候，会比较慢，获取到的是空
				return core::FramePacket::SharedPacket(nullptr);
			}
			auto new_packet{core::FramePacket::Make_Shared()};
			if(new_packet == nullptr)
				return new_packet;
			*new_packet = *privious_frame;
			//然后修改pts和dts,为了保险起见，减6ms
			auto time = static_cast<int64_t>(wait_time - 6);
			//不能让time小于等于0
			new_packet->pts +=  (time <= 0 ? 1:time)* 1000;
			new_packet->dts = privious_frame->pts;
			
			privious_frame = new_packet;
			return new_packet;
		}
	}
	
	inline void on_real_time_fps(int64_t ts) noexcept{
		if( ts - privious_ts > 1000000){
			privious_ts = ts;
			core::GlobalCallBack::Get_CallBack()->on_video_real_time_fps(count);
			count = 1;
		} else {
			count += 1;
		}
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////

VideoProcessingFactory::VideoProcessingFactory():
	d_ptr(new VideoProcessingFactoryPrivateData)
{
	//	d_ptr->overlay_rect.x = 0.6f;
	//	d_ptr->overlay_rect.y = 0.7f;
	//	d_ptr->overlay_rect.width = 0.3f;
	
	d_ptr->scale_format_current.pixel_format = AV_PIX_FMT_YUV420P;
	d_ptr->scale_format_current.bits = 12;
	d_ptr->scale_format_current.width = 1920;
	d_ptr->scale_format_current.height = 1080;
}

VideoProcessingFactory::~VideoProcessingFactory()
{
	this->clear_input_queue();
	this->clear_output_queue();
	exit_thread();
	if(crop != nullptr)
		delete crop;
	delete d_ptr;
}

void VideoProcessingFactory::set_crop_rect(const image_processing::Rect &rect) noexcept
{
	if(crop == nullptr){
		crop = new (std::nothrow)image_processing::Crop();
		if(crop == nullptr)
			return;
	}
	
	crop->set_default_crop_rect(rect);
}

void VideoProcessingFactory::set_overlay_rect(const image_processing::FRect &rect) noexcept
{
	//	if(d_ptr->overlay_rect == rect)
	//		return;
	//	std::lock_guard<std::mutex> lk(d_ptr->overlay_mutex);
	//	d_ptr->overlay_rect = rect;
}

void VideoProcessingFactory::on_thread_run() noexcept
{
	using namespace core;
	
	if(cc_ptr != nullptr&& cc_ptr->is_running() &&
			dc_ptr != nullptr && dc_ptr->is_running()){
		//双开,使用的较少，因为桌面1080P需要优化，所以这里
		//优化会推迟一点
		int32_t wait_time;
		wait_time = cc_ptr->get_fps() > dc_ptr->get_fps() ?
					cc_ptr->get_fps():dc_ptr->get_fps();
		wait_time = 1000 / wait_time;
		
		auto before_time = Time::Now();
		auto camera_frame = d_ptr->get_latest_frame(cc_ptr,d_ptr->privious_camera_frame,wait_time);
		auto desktop_frame =d_ptr->get_latest_frame(dc_ptr,d_ptr->privious_desktop_frame,
													wait_time - 
													static_cast<int32_t>( (Time::Now() - before_time).to_timestamp() ));
		
		//裁剪的判断
		bool is_crop;
		if( crop == nullptr){
			is_crop = false;
		}
		else {
			auto rect = crop->get_crop_rect();
			is_crop = rect.width != 0 && rect.height != 0;
		}
		
		if( is_crop ) {
			auto new_frame = core::FramePacket::Make_Shared();
			if (crop->crop(new_frame,desktop_frame) != core::Result::Success)
				return;
			desktop_frame = new_frame;
		}
		
		//判断合成
		if(camera_frame != nullptr && desktop_frame != nullptr){
			//合成图像,接口暂时置空不处理
			auto merge_packet = _merge_frame(camera_frame,desktop_frame);
			if(merge_packet == nullptr)
				return;
			//回调合成图像
			if(GlobalCallBack::Get_CallBack() != nullptr){
				GlobalCallBack::Get_CallBack()->on_video_frame_merge(merge_packet);
				d_ptr->on_real_time_fps(merge_packet->pts);
			}
			std::lock_guard<std::mutex> lk(d_ptr->player_mutex);
			if(d_ptr->player != nullptr)
				d_ptr->player_queue->push_one(merge_packet);
			push_one(merge_packet);
		}
	}
	else if( cc_ptr != nullptr&& cc_ptr->is_running() ){
		//只开摄像头
		auto packet = d_ptr->get_latest_frame(cc_ptr,d_ptr->privious_camera_frame,
											  1000 / cc_ptr->get_fps());
		if(packet == nullptr)
			return;
		//第一时间回调
		if(GlobalCallBack::Get_CallBack() != nullptr){
			GlobalCallBack::Get_CallBack()->on_camera_frame(packet);
			d_ptr->on_real_time_fps(packet->pts);
		}
		std::lock_guard<std::mutex> lk(d_ptr->player_mutex);
		if(d_ptr->player != nullptr)
			d_ptr->player_queue->push_one(packet);
		push_one(packet);
		return;
	}
	else if(dc_ptr != nullptr && dc_ptr->is_running()){
		//只开桌面
		auto packet =d_ptr->get_latest_frame(dc_ptr,d_ptr->privious_desktop_frame,
											 1000 / dc_ptr->get_fps());
		if(packet == nullptr)
			return;
		
		bool is_crop;
		if( crop == nullptr){
			is_crop = false;
		}
		else {
			auto rect = crop->get_crop_rect();
			is_crop = rect.width != 0 && rect.height != 0;
		}
		//裁剪的判断
		if( is_crop ){
			auto new_frame = core::FramePacket::Make_Shared();
			if (crop->crop(new_frame,packet) != core::Result::Success)
				return;
			packet = new_frame;
		}
		//裁剪后回调
		if(GlobalCallBack::Get_CallBack() != nullptr){
			GlobalCallBack::Get_CallBack()->on_desktop_frame(packet);
			d_ptr->on_real_time_fps(packet->pts);
		}
		
		std::lock_guard<std::mutex> lk(d_ptr->player_mutex);
		if(d_ptr->player != nullptr)
			d_ptr->player_queue->push_one(packet);
		push_one(packet);
		return;
	}
	else{
		//都不开，直接返回暂停
		return;
	}
}

bool VideoProcessingFactory::get_thread_pause_condition() noexcept
{
	return input_list.empty();
}

core::FramePacket::SharedPacket VideoProcessingFactory::_merge_frame(
		core::FramePacket::SharedPacket dp, core::FramePacket::SharedPacket cp)
{
	UNUSED(cp)
	return dp;
}

} // namespace device_manager

} // namespace rtplivelib
