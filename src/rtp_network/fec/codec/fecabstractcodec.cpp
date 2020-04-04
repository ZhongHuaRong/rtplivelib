
#include "fecabstractcodec.h"

namespace rtplivelib {

namespace rtp_network {

namespace fec {


FECAbstractCodec::FECAbstractCodec(FECAbstractCodec::CodesType codesT,
                                   FECAbstractCodec::CodecType codecT):
    codes_type(codesT),codec_type(codecT)                                               {}
FECAbstractCodec::~FECAbstractCodec()                                                   {}


} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

