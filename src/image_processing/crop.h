
#pragma once

#include "../core/config.h"
#include "../core/format.h"
#include "../core/error.h"
#include <string.h>

namespace rtplivelib{

namespace image_processing{

struct CropPrivateData;

struct FloatCompare {
	template<typename Number>
	int operator() (const Number &a,const Number &b){
		const Number & c = a - b;
		if( c > static_cast<Number>(0.000001) )
			return 1;
		else if( c < static_cast<Number>(-0.000001) )
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
        if( memcmp(this,&rect,sizeof(Rect)) == 0)
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
 * @brief The Crop class
 * 用于图像裁剪
 * 该类的所有操作都是线程安全
 */
class RTPLIVELIBSHARED_EXPORT Crop
{
public:
    Crop();
    
    ~Crop();
    
    /**
     * @brief set_default_input_format
     * 设置默认的输入格式
     */
    void set_default_input_format( const core::Format & input_format) noexcept;
    
    /**
     * @brief set_default_output_format
     * 设置默认的输出像素格式
     * 设置-1就是输入的像素格式
     */
    void set_default_output_pixel( int output_pixel = -1) noexcept;
    
    /**
     * @brief set_default_crop_rect
     * 设置默认的裁剪区域
     */
    void set_default_crop_rect(const Rect & rect) noexcept;
    
    /**
     * @brief crop
     * 需要注意的是，这个接口需要提前设置好默认参数，否则会直接返回false
     * @param dst
     * 转换后的图像
     * 不允许传入空指针,原有指针数据将会擦除
     * @param src
     * 源图
     * @return 
     * 裁剪成功则返回true
     */
    core::Result crop(core::FramePacket * dst,core::FramePacket *src) noexcept;
    
    /**
     * @brief crop
     * 重载函数
     * 同 bool crop(core::FramePacket * dst,core::FramePacket *src)
     * 需要注意的是参数是shared_ptr
     * @param dst
     * 转换后的图像
     * 不允许传入空指针,原有指针数据将会擦除
     * @param src
     * 源图
     * @return 
     * 裁剪成功则返回true
     */
    core::Result crop(core::FramePacket::SharedPacket &dst,core::FramePacket::SharedPacket &src) noexcept;
    
    /**
     * @brief get_crop_rect
     * 返回以前设置的裁剪区域
     */
    const Rect& get_crop_rect() noexcept;
private:
    CropPrivateData * const d_ptr;
};

} // namespace image_processing

} // namespace rtplivelib

