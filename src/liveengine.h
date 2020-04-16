
#pragma once

#include "core/config.h"
#include "device_manager/devicemanager.h"
#include "codec/hardwaredevice.h"

namespace rtplivelib{

class LiveEnginePrivateData;

class RTPLIVELIBSHARED_EXPORT LiveEngine
{
public:
	LiveEngine();
	
	virtual ~LiveEngine();
	
	/**
	 * @brief set_local_microphone_audio
	 * 播放本地的麦克风音频
	 * @param flag
	 * true则是开启,false则是关闭
	 * @see AudioProcessingFactory::play_microphone_audio
	 */
	void set_local_microphone_audio(bool flag) noexcept;
	
	/**
	 * @brief set_local_display_win_id
	 * 设置本地视频显示窗口的ｉｄ
	 * @param win_id
	 * 需要显示的窗口id
	 * @see VideoProcessingFactory::set_display_win_id
	 */
	void set_local_display_win_id(void* win_id);
	
	/**
	 * @brief set_remote_display_win_id
	 * 设置远程端显示窗口
	 * 由于省事，所以这个设置不会维持
	 * 在用户退出后或者没有加入的时候使用该函数设置是不会生效的
	 * @param win_id
	 * 需要显示的窗口id
	 * @param name
	 * 用户名
	 */
	void set_remote_display_win_id(void *win_id,const std::string& name);
	
	/**
	 * @brief set_display_screen_size
	 * 用于手动设置显示窗口的大小
	 */
	void set_display_screen_size(const int &win_w,const int & win_h,
								 const int & frame_w,const int & frame_h) noexcept;
	
	/**
	 * @brief set_remote_display_screen_size
	 * 用于手动设置显示窗口的大小
	 */
	void set_remote_display_screen_size(const std::string& name,
										const int &win_w,const int & win_h,
										const int & frame_w,const int & frame_h) noexcept;
	
	/**
	 * @brief set_crop_rect
	 * 设置裁剪区域,长宽为０的时候则是全屏
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
	void set_overlay_rect(const image_processing::FRect & rect) noexcept;
	
	/**
	 * @brief register_call_back_object
	 * 注册回调函数的对象
	 */
	void register_call_back_object(core::GlobalCallBack * callback) noexcept;
	
	/**
	 * @brief set_local_name
	 * 设置会话用的用户名，用于区分各个流，所以该名字必须是唯一标识
	 * 不设置的话，无法加入房间
	 */
	bool set_local_name(const std::string& name) noexcept;
	
	/**
	 * @brief join_room
	 * 加入房间，用于分组管理
	 * 所有会话的建立以及收发都必须通过加入房间后完成
	 * 如果之前已经加入了房间而且没有退出，则默认退出之前的房间，加入新的房间
	 * 如果没有设置用户名则无法成功加入房间
	 */
	bool join_room(const std::string& name) noexcept;
	
	/**
	 * @brief exit_room
	 * 离开房间
	 * 离开房间后，所有会话全部关系,停止收发信息
	 * 如果没有加入房间，则什么也没有发生
	 */
	bool exit_room() noexcept;
	
	/**
	 * @brief enabled_push
	 * 启动推流，可以在任意时间段设置
	 * 如果不启动，就算打开摄像头也没啥作用
	 * 启动后，就算不开摄像头，也没啥问题
	 * 默认是关闭的
	 */
	bool enabled_push(bool enabled) noexcept;
	
	/**
	 * @brief get_room_name
	 * 获取已加入的房间的名字
	 * 这里可以通过名字来判断是否加入了房间
	 * 如果字符串为空，则可以确认是没有加入房间的
	 */
	const std::string& get_room_name() noexcept;
	
	/// 下面提供两个解码器解码方案相关的接口，可以设定解码方案
	/// 编码器的接口就不提供了，可以通过get_video_encoder
	/// 来获取编码器对象来设置编码方案，
	/// audio则不需要该类型接口，固定软压AAC
	/**
	 * @brief set_decoder_hwd_type
	 * 修改所有用户的硬解方案，新加入的用户也将采用这个方案
	 * @param type
	 * 硬解方案
	 */
	void set_decoder_hwd_type(codec::HardwareDevice::HWDType type) noexcept;
	
	/**
	 * @brief get_decoder_hwd_type
	 * 获取所有解码器的硬解方案
	 * @return 
	 * 返回map
	 * key:用户名
	 * value:硬解方案
	 */
	codec::HardwareDevice::HWDType get_decoder_hwd_type() noexcept;
	
	/**
	 * @brief get_audio_encoder
	 * 获取音频编码器
	 * @return 
	 */
	void * get_audio_encoder() noexcept;
	
	/**
	 * @brief get_video_encoder
	 * 获取视频编码器
	 * @return 
	 */
	void * get_video_encoder() noexcept;
	
	/**
	 * @brief set_log_level
	 * 设置日志输出等级
	 * 详情请查看LogLevel
	 * @param level
	 * 等级
	 */
	void set_log_level(LogLevel level) noexcept;
	
	/**
	 * @brief get_device_manager
	 * 获取设备管理
	 * @return 
	 */
	device_manager::DeviceManager		*get_device_manager() noexcept;
private:
	device_manager::DeviceManager  * const device;
	LiveEnginePrivateData  * const d_ptr;
};

inline device_manager::DeviceManager * LiveEngine::get_device_manager() noexcept			{	return device;		}
}

