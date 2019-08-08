
#pragma once

#include "../core/config.h"
#include "../core/abstractqueue.h"
#include "../core/format.h"

namespace rtplivelib {

namespace device_manager{

class ALSAPrivateData;

/**
 * @brief The ALSA class
 * Linux系统下的音频采集
 * 该类所有接口都是线程安全
 * 目前在Deepin系统采集到的声音是静音，有待完善
 */
class ALSA :
        protected core::AbstractQueue<core::FramePacket::SharedPacket,core::FramePacket::SharedPacket,core::NotDelete>
{
public:
    enum FlowType{
        RENDER = 0,
        CAPTURE = 1,
        ALL = 2
    };
    
    //first:设备名字
    //second:设备名字
	//两个字段保存的是同样的数据
    using device_info = std::pair<std::string,std::string>;
public:
    /**
     * @brief ALSA
     */
    ALSA();
    
    ~ALSA() override;
    
    /**
     * @brief get_device_info
     * 获取相应设备信息
     * @param ft
     * 设备类型
     * @return 
     * 返回
     */
    std::vector<device_info> get_all_device_info(FlowType ft = ALL) noexcept(false);
    
    /**
     * @brief get_current_device_info
     * 获取当前设备名字
     * 如果没有设置，则返回默认声卡信息
     * @return 
     */
    device_info get_current_device_info() noexcept;
    
    /**
     * @brief set_current_device
     * 通过索引更改当前使用的设备
     * @param num
     * 当前设备索引
     * @param ft
     * 设备类型
     * @return 
     */
    bool set_current_device(uint64_t num,FlowType ft = ALL) noexcept;
    
    /**
     * @brief set_current_device
     * 通过设备名字更改当前使用的设备
     * 切换后需要手动调用start来开启设备
     * @param name
     * 设备名字
     * @param ft
     * 设备类型
     * @return 
     */
    bool set_current_device(const std::string& name,FlowType ft = ALL) noexcept;
    
    /**
     * @brief set_default_device
     * 设置默认的设备采集
     * @param ft
     * 设备类型,CARTURE或者RENDER
     * @return 
     */
    bool set_default_device(FlowType ft = RENDER) noexcept;
	
	/**
	 * @brief set_format
	 * 设置采集的格式
	 * 设置core::Format()则采用默认格式
	 * 如果需要参数生效，则需要先stop,然后start
	 * @warning 如果调用了set_current_device更换设备后，如果需要采用默认格式则需要重新调用一次该接口
	 *			因为start会初始化format，导致下一次start也是这个参数
	 * @see stop
	 * @see start
	 */
	void set_format( const core::Format& format) noexcept;
    
    /**
     * @brief get_format
     * 获取当前音频格式
     * 需要开始采集才能获取
     * @return 
     */
    const core::Format get_format() noexcept;
    
    /**
     * @brief start
     * 开始采集,设置句柄，外部接口可以等待这个句柄来调用get_packet
     * @return 
     */
    bool start() noexcept;
    
    /**
     * @brief stop
     * 停止采集
     * @return 
     */
    bool stop() noexcept;
    
    /**
     * @brief is_start
     * 获取设备是否正在读取数据
     * 如果正在读取则返回true
     * @return 
     */
    bool is_start() noexcept;
    
    /**
     * @brief get_packet
     * 获取音频包
     * 该函数会阻塞当前线程,直到有包返回
     */
    core::FramePacket::SharedPacket read_packet() noexcept;
protected:
    /**
	 * @brief on_thread_run
	 */
	virtual void on_thread_run() noexcept override final;
	
	/**
	 * @brief on_thread_pause
	 */
	virtual void on_thread_pause() noexcept override final;
	
	/**
	 * @brief get_thread_pause_condition
	 * @return 
	 */
	virtual bool get_thread_pause_condition() noexcept override final;
private:
	ALSAPrivateData * const d_ptr;
	volatile bool _is_running_flag{false};
};

inline bool ALSA::get_thread_pause_condition() noexcept												{	return !_is_running_flag;}


} // namespace device_manager

} // namespace rtplivelib
