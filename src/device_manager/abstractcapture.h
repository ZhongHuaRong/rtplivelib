
#pragma once
#include "../core/abstractqueue.h"
#include "../core/format.h"
#include <string>
#include <map>

namespace rtplivelib {

namespace device_manager {


/**
 * @brief The AbstractCapture class
 * 虚拟捕捉类
 * 实现捕捉设备的大致流程
 */
class RTPLIVELIBSHARED_EXPORT AbstractCapture :
		public core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>
{
public:
	
    /**
     * @brief The CaptureType enum
     * 表示该类捕捉的数据类型
     * Unknown:表示未知数据类型
     * Desktop:表示捕捉桌面画面
     * Camera:表示捕捉摄像头画面
     * Microphone:表示捕捉麦克风声音
     * Soundcard:表示捕捉声卡声音
     */
	enum struct CaptureType {
		Unknown = 0x00,
		Desktop = 0x01,
		Camera,
		Microphone,
		Soundcard
	};
	
	using FramePacket = core::FramePacket;
	using SharedPacket = core::FramePacket::SharedPacket;
	using device_info = std::pair<std::string,std::string>;
public:
    /**
     * @def AbstractCapture
     * @brief
     * 生成线程，并将自己移到该线程，该类的子类所有slot都是异步的
     * @param type
     * 捕捉数据的类型，在子类的构造函数中输入
     */
	AbstractCapture(CaptureType type = CaptureType::Unknown) noexcept;

    /**
     * @brief ~AbstractCapture
     * 终止线程和资源释放
     */
	virtual ~AbstractCapture() override;

    /**
     * @brief is_running
     * 返回设备是否正在运行的标志，如果正在运行则返回true
     * @return
     */
	virtual bool is_running() noexcept;

    /**
     * @brief start_capture
     * 开始捕捉数据
     * @param enable
     * 用于标志，便于子类实现全局控制的功能
     * true:正常操作，如果已经开启则不操作，否则开启
     * false:如果未开启则不操作，已经开启了则关闭（类似全局静音的效果）
     */
	virtual void start_capture(bool enable = true) noexcept;

    /**
     * @brief stopCapture
     * 停止捕捉数据
     */
	virtual void stop_capture() noexcept;
	
	/**
	 * @brief get_device_value
	 * 返回设备的数值
	 * 视频指当前帧数(实际帧数，不是预设的)，音频指音量值
	 * @return 
	 */
	virtual uint32_t get_device_value() noexcept;
	
	/**
	 * @brief get_current_device_info
	 * 返回设备信息,包含设备名字和设备id，id是内部使用字段
	 * @return 
	 */
	virtual device_info get_current_device_info() noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向程序的，value是面向用户的
	 * key存的是设备id(外部调用可以忽略该字段)，value存的是设备名字(对用户友好)
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info() noexcept(false) = 0;
	
	/**
	 * @brief set_default_device
	 * 将当前设备更换成默认设备,简易接口
	 */
	virtual bool set_default_device() noexcept = 0;
	
	/**
	 * @brief set_current_device
	 * 通过设备id更换当前设备
	 * 获取设备信息后，使用key来获取id，value是设备名字
	 * 如果失败则返回false
	 * @see get_all_device_info
	 */
	virtual bool set_current_device(std::string device_id) noexcept = 0;

    /**
     * @brief get_type
     * 获取捕捉数据的类型
     * @return
     * 返回类型，参考CaptureType
     */
	virtual CaptureType get_type() noexcept;
protected:
    /**
     * @brief on_start
     * 纯虚，子类实现其功能，异步操作，用于捕捉数据
     * 其实可以不设置为纯虚，但是我不希望这个类可以实例化，所以设置一个纯虚函数
     */
	virtual SharedPacket on_start() noexcept = 0;

    /**
     * @brief on_stop
     * 子类实现其功能，异步操作，用于停止捕捉数据后的操作
     */
	virtual void on_stop() noexcept;
	
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

	/**
	 * @brief on_frame_data
	 * 获取到数据后的回调，此操作在推进资源队列前
	 * 重写该函数可以做数据前处理
	 * @param packet
	 * 数据包
	 * @return
	 * 返回true则让该数据推进队列，返回flase则在该函数结束后释放资源
	 * 这个可以说是除了get_next的另一种获取数据的方式，
	 * 如果采用这种方式读取数据，则最好返回false
	 * 返回false则自行析构packet,调用ReleasePacket
	 */
	virtual bool on_frame_data(FramePacket *packet);
private:
	
	/**
	 * @brief on_thread_run
	 * 改为私有权限，向子类屏蔽线程实现细节
	 * 其实是子类实现需要获取返回值，中间再包装一层
	 */
	virtual void on_thread_run() noexcept override final;
	
	/**
	 * @brief on_thread_pause
	 * 改为私有权限，向子类屏蔽线程实现细节
	 */
	virtual void on_thread_pause() noexcept  override final;
	
protected:
	device_info current_device_info;
	/**
	 * @brief _current_device_value
	 * 视频指当前帧数，音频指音量值
	 */
	uint32_t current_device_value;
private:
	CaptureType _type;
	volatile bool _is_running_flag;
};

inline bool AbstractCapture::is_running() noexcept											{		return _is_running_flag;}
inline AbstractCapture::CaptureType AbstractCapture::get_type()  noexcept					{		return _type;}
inline void AbstractCapture::on_stop() noexcept												{		}
inline bool AbstractCapture::get_thread_pause_condition() noexcept							{		return !_is_running_flag;}
inline AbstractCapture::device_info AbstractCapture::get_current_device_info() noexcept		{		return current_device_info;}
inline uint32_t AbstractCapture::get_device_value() noexcept								{		return current_device_value;}
inline bool AbstractCapture::on_frame_data(FramePacket *)									{		return true;}
inline void AbstractCapture::on_thread_pause() noexcept										{		this->on_stop();}

} // namespace device_manager

} // namespace capture

