
#pragma once

#include "../core/config.h"
#include "../core/format.h"
#include "../core/error.h"

namespace rtplivelib{

namespace image_processing{

struct ScalePrivateData;

/**
 * @brief The Scale class
 * 图像像素转换，或者缩放尺寸
 * 该类的所有操作都是线程安全
 */
class RTPLIVELIBSHARED_EXPORT Scale
{
public:
	Scale();
	
	~Scale();
	
	/**
	 * @brief set_default_output_format
	 * 设置默认输出格式，需要提前设置，要不后面的scale会失败
	 */
	void set_default_output_format(const core::Format& format) noexcept;
	
	/**
	 * @brief set_default_output_format
	 * 重载函数，其实只需要三个参数即可,是为了更为灵活的设置
	 */
	void set_default_output_format(const int & width, const int & height, const int & pix_fmt) noexcept;
	
	/**
	 * @brief set_default_intput_format
	 * 设置默认的输入格式,也需要提前设置
	 */
	void set_default_input_format(const core::Format& format) noexcept;
	
	/**
	 * @brief set_default_input_format
	 * 重载函数,也是只需要三个参数即可
	 */
	void set_default_input_format(const int & width, const int & height, const int & pix_fmt) noexcept;
	
	/**
	 * @brief scale
	 * 格式转换
	 */
	core::Result scale(core::FramePacket * dst,core::FramePacket *src) noexcept;
	
	/**
	 * @brief scale
	 * 重载函数，但是参数不允许出现空指针
	 * 智能指针替换指针好麻烦
	 */
	core::Result scale(core::FramePacket::SharedPacket &dst,core::FramePacket::SharedPacket &src) noexcept;
	
	/**
	 * @brief scale
	 * 重载函数,不局限于core::FramePacket
	 * @param src_data
	 * 原数据数组,不允许为nullptr
	 * @param src_linesize
	 * 原行宽
	 * @param dst_data
	 * 目标数据数组
	 * @param dst_linesize
	 * 目标行宽
	 * @return 
	 */
	core::Result scale( uint8_t * src_data[],int src_linesize[],
						uint8_t * dst_data[],int dst_linesize[]) noexcept;
private:
	ScalePrivateData * const d_ptr;
};


} // namespace image_processing

} // namespace rtplivelib
