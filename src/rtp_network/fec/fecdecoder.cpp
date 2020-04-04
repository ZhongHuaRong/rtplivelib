#include "fecdecoder.h"
#include "fecencoder.h"
#include "codec/wirehair.h"
#include "../rtpsession.h"
#include "jrtplib3/rtppacket.h"
#include <map>
extern "C"{
#include "libavcodec/avcodec.h"
}

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECDecoderPrivateData{
private:
    struct NOFec{
        std::vector<RTPPacket::SharedRTPPacket> vector;
        std::vector<uint8_t*> data_vector;
        int count{0};
        int unit_size{0};
        int last_size{0};
        
        inline void get_data(uint8_t * d) noexcept{
            auto i = 0;
            for(;i < data_vector.size() - 1;++i){
                memcpy(d,data_vector[i],unit_size);
                d += unit_size;
            }
            memcpy(d,data_vector[i],last_size);
        }
    };
public:
    using Codec = std::shared_ptr<Wirehair>;
    std::map<uint32_t,Codec> fec_map; 
    std::map<uint32_t,NOFec> nofec_map;
    core::FramePacket::SharedPacket next_pack;
    
    inline Codec Make_Codec(){
        return std::make_shared<Wirehair>(Wirehair::Decoder);
    }
    
    inline uint32_t get_correct_timestamp(const uint32_t &ts, const uint32_t &pos) noexcept{
        return ts - pos;
    }
    
    inline void erase_fec_map(const uint32_t &timestamp) noexcept{
        for(auto i = fec_map.begin();i != fec_map.end();){
            if(i->first < timestamp){
                fec_map.erase(i++);
            } else {
                break;
            }
        }
    }
    
    inline void erase_nofec_map(const uint32_t &timestamp) noexcept{
        for(auto i = nofec_map.begin();i != nofec_map.end();){
            if(i->first < timestamp){
                nofec_map.erase(i++);
            } else {
                break;
            }
        }
    }
    
    inline core::Result pop(uint8_t *data, const uint64_t &len,
                            const uint32_t &timestamp, 
                            const int32_t &src_nb,
                            const int32_t &repair_nb, 
                            const int32_t &fill_size,
                            const int32_t &symbol_size,
                            const int32_t &total_size,
                            const int32_t &pos,
                            const int &payload_type,
                            const int32_t &flag,
                            RTPPacket::SharedRTPPacket &rtp_packet) noexcept{
        UNUSED(src_nb)
        UNUSED(fill_size)
        UNUSED(repair_nb)
        
        auto ts = get_correct_timestamp(timestamp,pos);
        core::Result ret;
        if(flag){
            //使用了FEC的情况
            auto & ptr = fec_map[ts];
            if(ptr == nullptr){
                Codec && c = Make_Codec();
                ptr.swap(c);
            }
            
            ret = ptr->decode(pos,data,len,total_size);
            if(ret == core::Result::Success){
                //如果解码成功，则把之前的FEC包删除
                erase_fec_map(ts);
                push(total_size,static_cast<RTPSession::PayloadType>(payload_type),flag);
            }
            
        } else {
            //没有使用FEC的情况
            
            //只有一个包的情况
            if(src_nb == 1){
                erase_nofec_map(ts);
                //直接返回
                uint8_t * p = static_cast<uint8_t *>(av_malloc(static_cast<size_t>(total_size)));
                if(p != nullptr){
                    auto ptr = core::FramePacket::Make_Shared();
                    
                    if( ptr != nullptr){
                        memcpy(p,data,total_size);
                        ptr->data[0] = p;
                        ptr->size = total_size;
                        ptr->payload_type = payload_type;
                        ptr->dts = ptr->pts = ts;
                    } else  {
                        av_free(p);
                    }
                    
                    next_pack.swap(ptr);
                }
                return core::Result::Success;
            }
            
            auto & ptr = nofec_map[ts];
            if(ptr.vector.size() == 0){
                ptr.vector.resize(src_nb);
                ptr.data_vector.resize(src_nb);
                ptr.count = 0;
                ptr.unit_size = symbol_size;
                ptr.last_size = symbol_size - fill_size;
            }
            
            ptr.vector[pos].swap(rtp_packet);
            ptr.data_vector[pos] = data;
            if(++ptr.count == src_nb){
                erase_nofec_map(ts);
                push(total_size,static_cast<RTPSession::PayloadType>(payload_type),flag);
            }
        }
        return core::Result::Success;
    }
    
    inline void push(const int32_t &total_size,
                     const int &payload_type,
                     const int32_t &flag) noexcept {
        if(flag){
            
            auto i = fec_map.begin();
            uint8_t * p = static_cast<uint8_t *>(av_malloc(static_cast<size_t>(total_size)));
            if(p != nullptr){
                auto ret = i->second->data_recover(p);
                
                auto ptr = core::FramePacket::Make_Shared();
                
                if(ret == core::Result::Success && ptr != nullptr){
                    ptr->data[0] = p;
                    ptr->size = total_size;
                    ptr->payload_type = payload_type;
                    ptr->dts = ptr->pts = i->first;
                } else  {
                    av_free(p);
                }
                
                next_pack.swap(ptr);
            }
            fec_map.erase(i);
            
        } else {
            auto i = nofec_map.begin();
            uint8_t * p = static_cast<uint8_t *>(av_malloc(static_cast<size_t>(total_size)));
            if(p != nullptr){
                auto ptr = core::FramePacket::Make_Shared();
                
                if( ptr != nullptr){
                    i->second.get_data(p);
                    ptr->data[0] = p;
                    ptr->size = total_size;
                    ptr->payload_type = payload_type;
                    ptr->dts = ptr->pts = i->first;
                } else  {
                    av_free(p);
                }
                
                next_pack.swap(ptr);
            }
            nofec_map.erase(i);
        }
    }
   
};

///////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////


FECDecoder::FECDecoder():
    d_ptr(new FECDecoderPrivateData)
{
    
}

FECDecoder::~FECDecoder()
{
    delete d_ptr;
}

core::Result FECDecoder::decode(RTPPacket::SharedRTPPacket rtp_packet) noexcept
{
    jrtplib::RTPPacket * packet = static_cast<jrtplib::RTPPacket*>(rtp_packet->get_packet());
	core::Result ret;
	if(packet->GetExtensionData() != nullptr){
		FECParam param;
		memcpy(&param,packet->GetExtensionData(),sizeof(FECParam));
		ret = d_ptr->pop(packet->GetPayloadData(),packet->GetPayloadLength(),
                         packet->GetTimestamp(),
                         param.get_src_nb(),
                         param.repair_nb,
                         param.get_fill_size(),
                         param.symbol_size,
                         param.size,
                         packet->GetExtensionID(),
                         packet->GetPayloadType(),
                         param.flag,
                         rtp_packet);
	}
	else
        //没有fec的情况,默认只有一个包
		ret = d_ptr->pop(packet->GetPayloadData(),packet->GetPayloadLength(),
                         packet->GetTimestamp(),
                         1,
                         0,
                         0,
                         packet->GetPayloadLength(),
                         packet->GetPayloadLength(),
                         0,
                         packet->GetPayloadType(),
                         0,
                         rtp_packet);
	
    return ret;
}

core::FramePacket::SharedPacket FECDecoder::get_packet() noexcept
{
    auto p = d_ptr->next_pack;
    d_ptr->next_pack.reset();
    return p;
}

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

