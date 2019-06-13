/**
 * @file abstractcapture.h
 * @author 钟华荣
 * @version 1.0
 * @date 2018-11
 */

#ifndef ABSTRACTCAPTURE_H
#define ABSTRACTCAPTURE_H

#include "../../core/abstractqueue.h"
#include "../../core/format.h"
#include <string>
#include <map>

namespace rtplivelib::device_manager::capture {


/**
 * @brief The AbstractCapture class
 * 虚拟捕捉类
 * 实现捕捉设备的大致流程
 */
class RTPLIVELIBSHARED_EXPORT AbstractCapture :
		public rtplivelib::core::AbstractQueue<rtplivelib::core::FramePacket>
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
	
	using FramePacket = rtplivelib::core::FramePacket;
public:
    /**
     * @def AbstractCapture
     * @brief
     * 生成线程，并将自己移到该线程，该类的子类所有slot都是异步的
     * @param parent
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
	virtual uint get_device_value() noexcept;
	
	/**
	 * @brief get_current_device_name
	 * 返回的是设备的名字
	 * @return 
	 */
	virtual std::string get_current_device_name() noexcept;
	
	/**
	 * @brief get_all_device_info
	 * 获取所有设备的信息，其实也就是名字而已
	 * 这里说明一下，key是面向用户的，value是面向程序的
	 * key存的是设备名字，value存的是程序需要调用的字符串(外部调用可以忽略该字段)
	 * @return 
	 * 返回一个map
	 */
	virtual std::map<std::string,std::string> get_all_device_info() noexcept = 0;
	
	/**
	 * @brief set_current_device_name
	 * 通过设备名字设置设备
	 * 如果失败则返回false
	 */
	virtual bool set_current_device_name(std::string name) noexcept = 0;

    /**
     * @brief get_type
     * 获取捕捉数据的类型
     * @return
     * 返回类型，参考CaptureType
     */
	virtual CaptureType get_type() noexcept;
	
	/**
	 * @brief SendErrorMsg
	 * 通过错误代码num，打印错误文本
	 * @param num
	 */
	static void PrintErrorMsg(int num);
	
	/**
	 * @brief YUYV2RGB32
	 * YUYV(摄像头默认的格式)格式转为RGB32(这个是捕捉屏幕的格式)格式
	 * 该接口方便自己处理显示图像时，又不支持YUV格式图像的情况(说的就是Qt)
	 * RGB32占4字节，YUYV占2字节，所以转完后大小加倍了
	 * @return 
	 * 返回一个新的FramePacket包，输入的参数仍然有效
	 * 所以要记住手动调用ReleasePacket释放
	 */
	static FramePacket * YUYV2RGB32(FramePacket *);
	
	/**
	 * @brief RGB322YUYV
	 * RGB32(这个是捕捉屏幕的格式)格式转为YUYV(摄像头默认的格式)格式
	 * 该接口主要用于内部编码使用，为了统一图像格式使用
	 * 一般不会只支持YUV而不支持RGB的
	 * @return 
	 * 返回一个新的FramePacket包，输入的参数仍然有效
	 * 所以要记住手动调用ReleasePacket释放
	 */
	static FramePacket * RGB322YUYV(FramePacket *);
	
	/**
	 * @brief ChangePixelFormat
	 * 这个是用来转换图像格式，YUYV2RGB16和RGB322YUYV是简易接口
	 * 图像格式
	 * 0   ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)
	 * 1   ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
	 * 2   ///< packed RGB 8:8:8, 24bpp, RGBRGB...
	 * 3   ///< packed RGB 8:8:8, 24bpp, BGRBGR...
	 * 25  ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
	 * 26  ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
	 * 27  ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
	 * 28  ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...
	 * 36  ///< packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), big-endian
	 * 37  ///< packed RGB 5:6:5, 16bpp, (msb)   5R 6G 5B(lsb), little-endian
	 * 38  ///< packed RGB 5:5:5, 16bpp, (msb)1X 5R 5G 5B(lsb), big-endian   , X=unused/undefined
	 * 39  ///< packed RGB 5:5:5, 16bpp, (msb)1X 5R 5G 5B(lsb), little-endian, X=unused/undefined
	 * 40  ///< packed BGR 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), big-endian
	 * 41  ///< packed BGR 5:6:5, 16bpp, (msb)   5B 6G 5R(lsb), little-endian
	 * 42  ///< packed BGR 5:5:5, 16bpp, (msb)1X 5B 5G 5R(lsb), big-endian   , X=unused/undefined
	 * 43  ///< packed BGR 5:5:5, 16bpp, (msb)1X 5B 5G 5R(lsb), little-endian, X=unused/undefined
	 * 摘自pixfmt.h
	 * @param src_fromat
	 * 原图格式
	 * @param dst_format
	 * 目标格式
	 * @param src_packet
	 * 原图数据包
	 * @return 
	 * 目标格式数据包
	 */
	static FramePacket * ChangePixelFormat(int src_fromat,int dst_format,FramePacket * src_packet);
protected:
    /**
     * @brief on_start
     * 纯虚，子类实现其功能，异步操作，用于捕捉数据
     * 其实可以不设置为纯虚，但是我不希望这个类可以实例化，所以设置一个纯虚函数
     */
	virtual FramePacket * on_start() = 0;

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
	virtual void on_thread_run() override final;
	
	/**
	 * @brief on_thread_pause
	 * 改为私有权限，向子类屏蔽线程实现细节
	 */
	virtual void on_thread_pause() override final;
	
protected:
	std::string current_device_name;
	/**
	 * @brief _current_device_value
	 * 视频指当前帧数，音频指音量值
	 */
	uint current_device_value;
private:
	CaptureType _type;
	volatile bool _is_running_flag;
};

inline bool AbstractCapture::is_running() noexcept									{		return _is_running_flag;}
inline AbstractCapture::CaptureType AbstractCapture::get_type()  noexcept			{		return _type;}
inline void AbstractCapture::on_stop() noexcept										{		}
inline bool AbstractCapture::get_thread_pause_condition() noexcept					{		return !_is_running_flag;}
inline std::string AbstractCapture::get_current_device_name() noexcept				{		return current_device_name;}
inline uint AbstractCapture::get_device_value() noexcept							{		return current_device_value;}
inline bool AbstractCapture::on_frame_data(FramePacket *)							{		return true;}
inline void AbstractCapture::on_thread_pause()										{		this->on_stop();}

} // namespace capture

		
#endif // ABSTRACTCAPTURE_H
