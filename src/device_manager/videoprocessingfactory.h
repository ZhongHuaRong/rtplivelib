
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
 * 这个类会提供原始数据回调，让外部调用可以做数据前处理
 * 
 * 该类有两个输入，可以同时输入两个，也可以只输入一个
 * 如果输入两个则会进行画面重叠(默认的两个一个是摄像头一个是桌面)
 * 当然也可以自定义成两个摄像头,两个桌面
 */
class  RTPLIVELIBSHARED_EXPORT VideoProcessingFactory :
		public core::TaskThread
{
public:
	VideoProcessingFactory();
	
	virtual ~VideoProcessingFactory() override;
	
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
protected:
	virtual void on_thread_run() noexcept override;
	
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
	//裁剪
	image_processing::Crop					*crop{nullptr};
	VideoProcessingFactoryPrivateData * const d_ptr;
};
} // namespace device_manager

} // namespace rtplivelib
