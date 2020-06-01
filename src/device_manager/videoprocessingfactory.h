
#pragma once

#include "cameracapture.h"
#include "desktopcapture.h"
#include "../image_processing/crop.h"
#include "../core/globalcallback.h"

namespace rtplivelib {

namespace device_manager {

class VideoProcessingFactoryPrivateData;

/**
 * @brief The VideoCapture class
 * 该类负责统一视频处理
 * 统一帧数，然后图像合成
 * 这个类会提供原始数据回调，让外部调用可以做数据前处理
 */
class  RTPLIVELIBSHARED_EXPORT VideoProcessingFactory :
		public core::TaskThread
{
public:
	/**
	 * @brief VideoProcessingFactory
	 * 该类需要传入CameraCapture和DesktopCapture子类的实例
	 * 该类将会统一两个对象的所有操作，方便管理
	 * 传入对象后不建议在外部使用这两个对象
	 */
	VideoProcessingFactory(device_manager::CameraCapture * cc = nullptr,
						   device_manager::DesktopCapture * dc = nullptr);
	
	virtual ~VideoProcessingFactory() override;
	
	/**
	 * @brief set_camera_capture_object
	 * 给你第二次机会设置对象指针
	 * @return
	 * 如果在运行过程中设置，则会返回false
	 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
	 */
	bool set_camera_capture_object(device_manager::CameraCapture * cc) noexcept;
	
	/**
	 * @brief set_desktop_capture_object
	 * 给你第二次机会设置对象指针
	 * @return
	 * 如果在运行过程中设置，则会返回false
	 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
	 */
	bool set_desktop_capture_object(device_manager::DesktopCapture * dc) noexcept;
	
	/**
	 * @brief set_crop_rect
	 * 设置裁剪区域，只留rect区域
	 */
	void set_crop_rect(const image_processing::Rect & rect) noexcept;
	
	/**
	 * @brief set_overlay_rect
	 * 设置重叠区域，当摄像头和桌面一起开启的时候，就会由该rect决定如何重叠
	 * @param rect
	 * 该参数保存的是相对位置
	 * 采用百分比，也就是说摄像头的位置全部由桌面的位置计算出来
	 * 不会随着桌面大小的变化而使得摄像头位置改变
	 * (摄像头画面的比例会保持原来比例，所以其实只要设置width就可以了)
	 */
	void set_overlay_rect(const image_processing::FRect &rect) noexcept;
	
	/**
	 * @brief set_capture
	 * 设置是否捕捉数据
	 * 如果没有实例，设置什么都没有作用
	 * 两个同时为false则处理完所有剩余数据后暂停线程
	 * 所以不要太快开启捕捉
	 * @param camera
	 * true:捕捉摄像头数据
	 * false:不捕捉
	 * @param desktop
	 * true:捕捉桌面数据
	 * false:不捕捉
	 */
	void set_capture(bool camera,bool desktop) noexcept;
	
	/**
	 * @brief notify_capture
	 * 外部调用start_capture接口捕捉的，需要调用此函数
	 */
	void notify_capture() noexcept;
	
	/**
	 * @brief set_fps
	 * 统一设置帧数,为了能够正确编码，最好使用该接口设置帧数
	 * @param value
	 * 帧数
	 */
	void set_fps(int value) noexcept;
	
	/**
	 * @brief set_display_win_id
	 * 设置播放窗口ID，用于显示加工过后的图像
	 * 可以理解为显示自己画面(毕竟要和显示别人的画面分开处理)
	 * @param id
	 * 窗口id
	 */
	void set_display_win_id(void * id) noexcept;
	
	/**
	 * @brief set_display_screen_size
	 * 用于手动设置显示窗口的大小
	 * (要不是linux，不会加入该接口)
	 */
	void set_display_screen_size(const int &win_w,const int & win_h,
								 const int & frame_w,const int & frame_h) noexcept;
	
	/**
	 * 这里提供接口获取底层对象，直接使用对象的接口更加方便的获取各种参数
	 */
	device_manager::CameraCapture * get_camera_capture_object() noexcept;
	device_manager::DesktopCapture * get_desktop_capture_object() noexcept;
	
protected:
	
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() noexcept override;
	
	/**
	 * @brief on_thread_pause
	 * 线程暂停时的回调
	 * 这个回调可能需要耗费一点时间，需要处理队列剩余数据
	 * 所以暂停后，不要马上开始
	 */
	virtual void on_thread_pause() noexcept override;
	
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
	virtual bool get_thread_pause_condition() noexcept override;
private:
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
	core::FramePacket::SharedPacket _merge_frame(core::FramePacket::SharedPacket dp,
												 core::FramePacket::SharedPacket cp);
private:
	device_manager::CameraCapture			*cc_ptr{nullptr};
	device_manager::DesktopCapture			*dc_ptr{nullptr};
	//裁剪
	image_processing::Crop					*crop{nullptr};
	VideoProcessingFactoryPrivateData * const d_ptr;
};

inline device_manager::CameraCapture *VideoProcessingFactory::get_camera_capture_object() noexcept			{		return cc_ptr;}
inline device_manager::DesktopCapture *VideoProcessingFactory::get_desktop_capture_object() noexcept		{		return dc_ptr;}
inline void VideoProcessingFactory::notify_capture() noexcept												{		this->start_thread();}

} // namespace device_manager

} // namespace rtplivelib
