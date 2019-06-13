
#ifndef VIDEOCAPTURE_H
#define VIDEOCAPTURE_H

#include "abstractcapture.h"
#include <math.h>

class AVInputFormat;
class AVFormatContext;
class AVOutputFormat;
class SwsContext;
class AVFilterContext;
class AVFilterGraph;

namespace rtplivelib {

class MediaDataCallBack;

struct FloatCompare {
	template<typename Number>
	int operator() (const Number &a,const Number &b){
		const Number & c = a - b;
		if( c > static_cast<Number>(0.000001) )
			return 1;
		else if( c < static_cast<Number>(0.000001) )
			return -1;
		else 
			return 0;
	}
};

struct Rect {
	int x{0};
	int y{0};
	int width{0};
	int height{0};
	
	bool operator ==(const Rect &rect){
		if( this->x == rect.x &&
			this->y == rect.y &&
			this->width == rect.width &&
			this->height == rect.height)
			return true;
		else
			return false;
	}
	
	bool operator !=(const Rect &rect){
		return !this->operator ==(rect);
	}
};

struct FRect{
	float x{0.0};
	float y{0.0};
	float width{0.0};
	float height{0.0};
	
	bool operator ==(const FRect &rect){
		if( FloatCompare()(this->x,rect.x) == 0 &&
			FloatCompare()(this->y,rect.y) == 0 &&
			FloatCompare()(this->width,rect.width) == 0 &&
			FloatCompare()(this->height,rect.height) == 0 )
			return true;
		else
			return false;
	}
	
	bool operator !=(const FRect &rect){
		return !this->operator ==(rect);
	}
};

/**
 * @brief The DesktopCapture class
 * 该类负责桌面画面捕捉
 * 在类初始化后将会尝试打开设备
 * 如果打开设备失败则说明该系统不支持
 * 备注:
 * Windows系统
 * 底层采用GDI采集桌面，好像效率不太高，而且采集到的数据带有BMP结构头，
 * 大小为54字节，可以跳过前54个字节获取RGB数据，格式为RGB32
 * 通过接口获取到的数据包实际上是内部处理过了,去掉了头部54字节，如果需要，
 * 你可以自己往前偏移地址54个字节
 * set_current_device_name等接口暂时未实现(只限于Windows)，只能采集主屏幕，也不能截取特定窗口
 * 可以通过设置rect来决定需要截取的区域
 * (这个以后会取消，将会在DeviceCapture引入该接口，通过截取图像来实现)
 * set_window 用于截取特定的窗口，该接口暂时没有实现
 * 
 * Linux系统
 * 底层采用fbdev采集桌面，目前采集到的是tty2，并不是桌面，以后有待研究
 * 图像格式是bgra32,在测试电脑里测试到只能跑15帧，跑不起30帧
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 * @author 钟华荣
 * @date 2018-11-28
 */
class RTPLIVELIBSHARED_EXPORT DesktopCapture :public AbstractCapture
{
public:
	DesktopCapture();
	
	~DesktopCapture() override;

    /**
     * @brief set_fps
     * 设置帧数，每秒捕捉多少张图片
     * 该fps将会和camera的fps统一
     * 以较小值为标准
     * 这个函数会重新启动设备
     * @param value
     * 每秒帧数
     */
	void set_fps(uint value) noexcept;

	/**
	 * @brief get_fps
	 * 获取预设的帧数
	 */
	uint get_fps() noexcept;
	
    /**
     * @brief set_window
     * 设置将要捕捉的窗口(暂未实现)
     * @param id
     * 窗口句柄，0为桌面
     */
	void set_window(const uint64_t & id) noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向用户的，value是面向程序的
	 * key存的是设备名字，value存的是程序需要调用的字符串
	 * 通过这个接口也可以获取到最新的设备数量
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info() noexcept override;
	
	/**
	 * @brief set_current_device_name
	 * 根据名字设置当前设备，成功则返回true，失败则返回false 
	 */
	virtual bool set_current_device_name(std::string name) noexcept override;
protected:
    /**
     * @brief on_start
     * 开始捕捉画面后的回调,实际开始捕捉数据的函数
     */
	virtual FramePacket * on_start() override;

    /**
     * @brief on_stop
     * 结束捕捉画面后的回调，用于stop_capture后回收工作的函数
     * 可以通过重写该函数来处理暂停后的事情
     */
	virtual void on_stop() noexcept override;
	
	/**
	 * @brief open_device
	 * 尝试打开设备
	 * @return 
	 */
	bool open_device() noexcept;
private:
	uint _fps;
	uint64_t _wid;
	AVInputFormat *ifmt;
	AVFormatContext *fmtContxt;
	std::mutex _fmt_ctx_mutex;
};

inline uint DesktopCapture::get_fps() noexcept												{		return _fps;}

/**
 * @brief The CameraCapture class
 * 该类负责摄像头的捕捉
 * 其实该类很多操作都和桌面捕捉一样，所以等以后有空会统一一下
 * 备注:
 * Windows系统
 * 采集设备用的是dshow，说实话，辣鸡东西来的
 * 摄像头默认采集的格式是yuyv422
 * Linux系统
 * 采用v4l2采集摄像头，老实说，这个是所有采集里面实现的最好的
 * 所有接口都已实现，可以枚举摄像头
 * 采集的格式和Windows一样，都是yuyv422
 * 
 * 设备获取接口Windows的还没有实现
 * 
 * 如果需要用该类自己处理数据，则可以考虑继承该类，然后重写on_frame_data接口来前处理数据
 * 然后返回false，也可以用wait_resource_push()和get_next()来获取队列的数据
 * 也可以通过实例化DeviceCapture类来捕捉，该类已经封装好其他四个类了，然后关闭其他三个的捕捉
 * @author 钟华荣
 * @date 2018-11-28
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
	void set_fps(uint value);
	
	/**
	 * @brief get_fps
	 * 获取预设的帧数
	 */
	uint get_fps() noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向用户的，value是面向程序的
	 * key存的是设备名字，value存的是程序需要调用的字符串
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info() noexcept override;
	
	/**
	 * @brief set_current_device_name
	 * 根据名字设置当前设备，成功则返回true，失败则返回false 
	 */
	virtual bool set_current_device_name(std::string name) noexcept override;

protected:
    /**
     * @brief on_start
     * 开始捕捉画面后的回调
     */
	virtual FramePacket * on_start() override;

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
	uint _fps;
	AVInputFormat *ifmt;
	AVFormatContext *fmtContxt;
	std::mutex _fmt_ctx_mutex;
};

inline uint CameraCapture::get_fps() noexcept															{		return _fps;}

class VideoProcessingFactoryPrivataData;
/**
 * @brief The VideoCapture class
 * 该类负责统一视频处理
 * 统一帧数，然后图像合成，再编码成h265
 * 这个类会提供原始数据回调，让外部调用可以做数据前处理
 * @author 钟华荣
 * @date 2018-12-13
 */
class  RTPLIVELIBSHARED_EXPORT VideoProcessingFactory :public AbstractQueue
{
public:
	/**
	 * @brief VideoProcessingFactory
	 * 该类需要传入CameraCapture和DesktopCapture子类的实例
	 * 该类将会统一两个对象的所有操作，方便管理
	 * 传入对象后不建议在外部使用这两个对象
	 */
	VideoProcessingFactory(CameraCapture * cc = nullptr,DesktopCapture * dc = nullptr);
	
	virtual ~VideoProcessingFactory() override;
	
	/**
	 * @brief set_camera_capture_object
	 * 给你第二次机会设置对象指针
	 * @return
	 * 如果在运行过程中设置，则会返回false
	 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
	 */
	bool set_camera_capture_object(CameraCapture * cc) noexcept;
	
	/**
	 * @brief set_desktop_capture_object
	 * 给你第二次机会设置对象指针
	 * @return
	 * 如果在运行过程中设置，则会返回false
	 * 设置同步机制会浪费资源，所以只要发现是线程运行过程中一律返回false
	 */
	bool set_desktop_capture_object(DesktopCapture * dc) noexcept;
	
	/**
	 * @brief set_crop_rect
	 * 设置裁剪区域，只留rect区域
	 */
	void set_crop_rect(const Rect & rect) noexcept;
	
	/**
	 * @brief set_overlay_rect
	 * 设置重叠区域，当摄像头和桌面一起开启的时候，就会由该rect决定如何重叠
	 * @param rect
	 * 该参数保存的是相对位置
	 * 采用百分比，也就是说摄像头的位置全部由桌面的位置计算出来
	 * 不会随着桌面大小的变化而使得摄像头位置改变
	 * (摄像头画面的比例会保持原来比例，所以其实只要设置width就可以了)
	 */
	void set_overlay_rect(const FRect &rect) noexcept;
	
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
	void set_fps(uint value) noexcept;
	
	/**
	 * @brief register_call_back_object
	 * 注册回调的对象
	 * 好像有点拗口
	 * 该类对该参数对象拥有所有权，析构工作将由本对象处理
	 * 实际上只需要new一个MediaDataCallBack的子类做参数就可以了
	 */
	void register_call_back_object(MediaDataCallBack*) noexcept;
	
	/**
	 * 这里提供接口获取底层对象，直接使用对象的接口更加方便的获取各种参数
	 */
	CameraCapture * get_camera_capture_object() noexcept;
	DesktopCapture * get_desktop_capture_object() noexcept;
	
protected:
	
	/**
	 * @brief on_thread_run
	 * 线程运行时需要处理的操作
	 */
	virtual void on_thread_run() override;
	
	/**
	 * @brief on_thread_pause
	 * 线程暂停时的回调
	 * 这个回调可能需要耗费一点时间，需要处理队列剩余数据
	 * 所以暂停后，不要马上开始
	 */
	virtual void on_thread_pause() override;
	
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
	FramePacket *_merge_frame(FramePacket * dp,FramePacket *cp);
	
	/**
	 * @brief _coding_h265
	 * 开始编码
	 */
	void _coding_h265(FramePacket *);
private:
	CameraCapture *cc_ptr;
	DesktopCapture *dc_ptr;
	VideoProcessingFactoryPrivataData * const d_ptr;
};

inline CameraCapture * VideoProcessingFactory::get_camera_capture_object() noexcept							{		return cc_ptr;}
inline DesktopCapture * VideoProcessingFactory::get_desktop_capture_object() noexcept						{		return dc_ptr;}
inline void VideoProcessingFactory::notify_capture() noexcept												{		this->notify_thread();}

} // namespace rtplivelib

#endif // VIDEOCAPTURE_H
