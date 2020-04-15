#include "videoprocessingfactory.h"

#include <algorithm>
#include "../player/videoplayer.h"
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
	//因为该类处理视频时，只有注册回调才能获取到数据
	//所以内部设置播放器用于播放
	player::VideoPlayer *player{nullptr};
	core::AbstractQueue<core::FramePacket> *player_queue{nullptr};
	std::mutex player_mutex;
	//转换格式用的结构体
	core::Format scale_format_current;
	core::Format scale_format_privious;
	//重叠用的结构体
	//FRect overlay_rect;
	//	std::mutex overlay_mutex;
	//保存上一帧
	core::FramePacket::SharedPacket privious_camera_frame;
	core::FramePacket::SharedPacket privious_desktop_frame;
	//上一帧的时间戳
	int64_t privious_ts{0};
	
	///////////////////////
	//转换格式用的上下文
	
	~VideoProcessingFactoryPrivateData(){
		release_player();
	}
	
	inline void release_player() noexcept{
		if(player != nullptr) {
			delete player;
			player = nullptr;
		}
		if(player_queue != nullptr){
			delete player_queue;
			player_queue = nullptr;
		}
	}
	
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
		auto &pri_frame = privious_frame;
		
		//等待一个帧的时间
		if(caputre->wait_for_resource_push(wait_time)){
			//超时前获取到帧(也可能是队列本来就有帧，没有等待即返回)
			//这里不采取特别措施，只获取最后一帧
			pri_frame = caputre->get_latest();
			return pri_frame;
		}
		else {
			if(pri_frame == nullptr){
				//如果连上一帧都没有，则返回空
				//一般在设备刚启动的时候，会比较慢，获取到的是空
				return core::FramePacket::SharedPacket(nullptr);
			}
			//深拷贝一份数据
			//			{
			//				core::FramePacket * new_packet{core::FramePacket::Make_Packet()};
			//				core::FramePacket::Copy(new_packet,pri_frame.get());
			//				pri_frame = std::make_shared<core::FramePacket>(new_packet);
			//			}
			//然后修改pts和dts,为了保险起见，减6ms
			auto time = static_cast<int64_t>(wait_time - 6);
			//不能让time小于等于0
			pri_frame->pts +=  (time <= 0 ? 1:time)* 1000;
			pri_frame->dts = pri_frame->pts;
			//如果没有获取到帧,则只需要拿一份上一份的样本去调用
			return pri_frame;
		}
	}
	
	inline void on_real_time_fps(int64_t ts) noexcept{
		core::GlobalCallBack::Get_CallBack()->on_video_real_time_fps(1000000.0f / (ts - privious_ts));
		privious_ts = ts;
	}
};

/////////////////////////////////////////////////////////////////////////////////////////////

VideoProcessingFactory::VideoProcessingFactory(
		device_manager::CameraCapture *cc,
		device_manager::DesktopCapture *dc):
	cc_ptr(cc),
	dc_ptr(dc),
	d_ptr(new VideoProcessingFactoryPrivateData)
{
	//	d_ptr->overlay_rect.x = 0.6f;
	//	d_ptr->overlay_rect.y = 0.7f;
	//	d_ptr->overlay_rect.width = 0.3f;
	
	d_ptr->scale_format_current.pixel_format = AV_PIX_FMT_YUV420P;
	d_ptr->scale_format_current.bits = 12;
	d_ptr->scale_format_current.width = 1920;
	d_ptr->scale_format_current.height = 1080;
	start_thread();
}

VideoProcessingFactory::~VideoProcessingFactory()
{
	set_capture(false,false);
	exit_thread();
	if(crop != nullptr)
		delete crop;
	delete d_ptr;
}

bool VideoProcessingFactory::set_camera_capture_object(
		device_manager::CameraCapture *cc) noexcept
{
	if(get_thread_pause_condition()){
		cc_ptr = cc;
		return true;
	}
	else
		return false;
}

bool VideoProcessingFactory::set_desktop_capture_object(
		device_manager::DesktopCapture *dc) noexcept
{
	if(get_thread_pause_condition()){
		dc_ptr = dc;
		return true;
	}
	else
		return false;
}

void VideoProcessingFactory::set_crop_rect(const image_processing::Rect &rect) noexcept(false)
{
	if(crop == nullptr){
		try {
			crop = new image_processing::Crop();
		} catch (const std::bad_alloc& except) {
			throw except;
		}
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

void VideoProcessingFactory::set_fps(int value) noexcept
{
	if(cc_ptr)
		cc_ptr->set_fps(value);
	if(dc_ptr)
		dc_ptr->set_fps(value);
}

void VideoProcessingFactory::set_display_win_id(void *id) noexcept(false)
{
	std::lock_guard<std::mutex> lk(d_ptr->player_mutex);
	if(id == nullptr) {
		if(d_ptr->player != nullptr) {
			delete d_ptr->player;
			d_ptr->player = nullptr;
		}
		else
			return;
	}
	else {
		if(d_ptr->player == nullptr) {
			try {
				d_ptr->player = new player::VideoPlayer;
				d_ptr->player_queue = new core::AbstractQueue<core::FramePacket>;
			} catch (const std::bad_alloc& except) {
				if(d_ptr->player != nullptr)
					d_ptr->release_player();
				throw except;
			}
		}
		d_ptr->player->set_player_object(d_ptr->player_queue,id);
	}
}

void VideoProcessingFactory::set_display_screen_size(const int &win_w,const int &win_h,
													 const int &frame_w,const int& frame_h) noexcept
{
	std::lock_guard<std::mutex> lk(d_ptr->player_mutex);
	if(d_ptr->player == nullptr){
		return;
	}
	d_ptr->player->show_screen_size_changed(win_w,win_h,frame_w,frame_h);
}

void VideoProcessingFactory::on_thread_run() noexcept
{
	/**
	 * 在这里说明一下该线程的流程
	 * 对象实例生成后，处于等待状态，调用set_camera_capture_object
	 * 和set_desktop_capture_object设置捕捉类(需要线程暂停才可以设置)，
	 * 然后set_capture传入true开始捕捉，如果是外部调用捕捉类的start_capture接口
	 * 要使用notify_thread唤醒线程
	 * 
	 * 可以通过外部捕捉类的stop_capture来停止捕捉，该线程就会自动停止(参考
	 * get_thread_pause_condition接口),也可以通过set_capture传入false来
	 * 暂停线程
	 * 
	 * 当然，只要有一个捕捉类正在运行，则编码就会继续
	 * 
	 * 在这里分成三种情况来处理
	 * 只开摄像头，只开桌面，双开
	 * 
	 * 一开始则是先获取桌面帧，并设置等待时间，等待时间为一帧大概时间，然后获取
	 * 摄像头帧，等待时间为一帧大概时间-桌面帧等待时间(等待时间通过最大帧率决定)
	 * 帧率可以通过外部调用捕捉类的set_fps接口，也可以用该类的set_fps统一设置
	 * 
	 * 最近测试得知，桌面捕捉启动后，首帧获取需要大约83ms(Linux测试的)
	 * 摄像头则需要300ms(也是Linux系统测试的),硬件参数也会有所干扰，
	 * 刚开始捕捉时大约前5帧是空帧，没有任何数据,空循环来的,
	 * 这里需要注意一下
	 */
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
			if (crop->crop(new_frame,desktop_frame) == false)
				return;
			desktop_frame = new_frame;
		}
		
		//判断合成
		if(camera_frame != nullptr && desktop_frame != nullptr){
			//合成图像,接口暂时置空不处理
			auto merge_packet = _merge_frame(camera_frame,desktop_frame);
			//回调合成图像
			if(GlobalCallBack::Get_CallBack() != nullptr)
				GlobalCallBack::Get_CallBack()->on_video_frame_merge(merge_packet);
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
			if (crop->crop(new_frame,packet) == false)
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

void VideoProcessingFactory::on_thread_pause() noexcept
{
	//继续处理队列剩余的数据
	if( dc_ptr != nullptr ){
		dc_ptr->get_latest();
	}
	
	if( cc_ptr != nullptr){
		cc_ptr->get_latest();
	}
}

bool VideoProcessingFactory::get_thread_pause_condition() noexcept
{
	if(cc_ptr == nullptr && dc_ptr == nullptr){
		return true;
	}
	return !( ( cc_ptr != nullptr && cc_ptr->is_running() ) ||
			  ( dc_ptr != nullptr && dc_ptr->is_running() ) );
}

core::FramePacket::SharedPacket VideoProcessingFactory::_merge_frame(
		core::FramePacket::SharedPacket dp, core::FramePacket::SharedPacket cp)
{
	UNUSED(cp)
			return dp;
}

} // namespace device_manager

} // namespace rtplivelib
