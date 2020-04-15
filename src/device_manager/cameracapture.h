
#pragma once

#include "abstractcapture.h"
#include "string.h"

namespace rtplivelib {

namespace device_manager {

class CameraCapturePrivateData;

struct VideoSize{
	int widget;
	int height;
	
	std::string to_string() noexcept{
		std::string size = std::to_string(widget);
		size += "x";
		size += std::to_string(height);
		return size;
	}
	
	bool operator==(const VideoSize& size){
		if(memcmp(this,&size,sizeof(VideoSize)) == 0)
			return true;
		else
			return false;
	}
	
	bool operator!=(const VideoSize& size){
		return !this->operator==(size);
	}
};

/**
 * @brief The CameraCapture class
 * 该类负责摄像头的捕捉
 * 其实该类很多操作都和桌面捕捉一样，所以等以后有空会统一一下
 * 备注:
 * Windows系统
 * 采集设备用的是dshow
 * 这里我要说明一下
 * 采用的是ffmpeg里面集成的dshow
 * Windows的摄像头采集有研究过WPD，WIA2.0和dshow(自己实现)
 * 奈何mingw里面没有wpd和wia2.0的头文件和lib
 * dshow好像要扩展Filter和Pin才获取得到媒体数据，但是需要编译strmbase.lib
 * cl编译出来的lib不好在mingw环境编译，等以后再研究一下
 * 摄像头默认采集的格式是yuyv422
 * Linux系统
 * 采用v4l2采集摄像头
 * 采集的格式和Windows一样，都是yuyv422
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 */
class RTPLIVELIBSHARED_EXPORT CameraCapture : public AbstractCapture
{
public:
	explicit CameraCapture();
	
	~CameraCapture() override;
	
	/**
	 * @brief set_fps
	 * 设置帧数
	 * @param value
	 * 帧数
	 */
	void set_fps(int value);
	
	/**
	 * @brief get_fps
	 * 获取预设的帧数
	 */
	int get_fps() noexcept;
	
	/**
	 * @brief get_video_size
	 * 获取采集视频的分辨率大小
	 * @return
	 */
	const VideoSize& get_video_size()noexcept;
	
	/**
	 * @brief set_video_size
	 * 设置将要采集的视频分辨率
	 * @param size
	 */
	void set_video_size(const VideoSize& size) noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向程序的，value是面向用户的
	 * key存的是设备id(外部调用可以忽略该字段)，value存的是设备名字(对用户友好)
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info()  noexcept(false) override;
	
	/**
	 * @brief set_default_device
	 * 将当前设备更换成默认设备,简易接口
	 * 该接口未实现
	 */
	virtual bool set_default_device() noexcept override;
	
	/**
	 * @brief set_current_device
	 * 通过设备id更换当前设备
	 * 获取设备信息后，使用key来获取id，value是设备名字
	 * 如果失败则返回false
	 * @see get_all_device_info
	 */
	virtual bool set_current_device(std::string device_id) noexcept override;
protected:
	/**
	 * @brief on_start
	 * 开始捕捉画面后的回调
	 */
	virtual SharedPacket on_start() noexcept override;
	
	/**
	 * @brief on_stop
	 * 结束捕捉画面后的回调
	 */
	virtual void on_stop() noexcept override;
	
	/**
	 * @brief open_device
	 * 尝试打开设备
	 * @return 
	 */
	bool open_device() noexcept;
private:
	int									_fps{30};
	VideoSize							_size{640,480};
	CameraCapturePrivateData * const	d_ptr;
};

inline int CameraCapture::get_fps() noexcept												{		return _fps;}
inline const VideoSize &CameraCapture::get_video_size() noexcept							{		return _size;}

}//namespace device_manager

}//namespace rtplivelib
