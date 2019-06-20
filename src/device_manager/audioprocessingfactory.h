
#ifndef AUDIOCAPTURE_H
#define AUDIOCAPTURE_H

#include "abstractcapture.h"
#include <stdint.h>

class AVInputFormat;
class AVFormatContext;

namespace rtplivelib {

namespace device_manager {

/**
 * @brief The AudioCapture class
 * 该类统一音频处理
 * 将会把采集到的音频重采样(统一采样格式)，然后编码，格式为AAC
 * 该类也会提供原始音频数据获取接口，让外部调用可以做数据前处理
 */
class AudioProcessingFactory
{
public:
	AudioProcessingFactory();
};

}// namespace device_manager

} // namespace rtplivelib

#endif // AUDIOCAPTURE_H
