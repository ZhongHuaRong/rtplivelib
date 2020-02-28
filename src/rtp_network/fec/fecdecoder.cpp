#include "fecdecoder.h"
#include <memory>
#include <math.h>
#include <string.h>
#include "../../core/logger.h"
#include "fecdecodecache.h"
extern "C"{
#include "libopenfec/lib_common/of_openfec_api.h"
}

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECDecoderPrivateData{
public:
    /*缓冲区*/
    FECDecodeCache cache;
    /*不同算法对应的结构体*/
    of_rs_2_m_parameters_t	*rs_2_m_params{nullptr};
    of_ldpc_parameters_t * ldpc_params{nullptr};
    of_session_t	*rs_2_m_ses{nullptr};
    of_session_t	*ldpc_ses{nullptr};
    /*源数据包数,用于判断是否需要初始化会话*/
    uint32_t src_pack_nb{0};
    /*冗余数据包数,用于判断是否需要初始化会话*/
    uint32_t repair_pack_nb{0};
    /*每个包的长度,用于判断是否需要初始化会话*/
    uint32_t sym_len{0};
	/**
	 * 保存解码前的数据
	 * 也可以说是用于解码所需要的参数
	 */
	void ** before_decode_list{nullptr};
	
	/**
	 * 保存解码后的数据
	 * 需要注意的一点是，解码得到的原数据是内部生成的，需要另外释放
	 */
	void ** after_decode_list{nullptr};
    
    ~FECDecoderPrivateData(){
        release_rs_2_m();
        release_ldpc();
    }
    
    /**
     * @brief release_rs_2_m
     * 释放rs_2_m算法的结构体
     */
    inline void release_rs_2_m() noexcept {
        if (rs_2_m_ses){
            of_release_codec_instance(rs_2_m_ses);
        }
        if (rs_2_m_params){
            free(rs_2_m_params);
        }
    }
    
    /**
     * @brief release_ldpc
     * 释放ldpc算法的结构体
     */
    inline void release_ldpc() noexcept {
        if(ldpc_ses){
            of_release_codec_instance(ldpc_ses);
        }
        if(ldpc_params){
            free(ldpc_params);
        }
    }
    
    
    /**
     * @brief create_rs_2_m
     * 创建rs_2_m的算法的会话
     * @return 
     */
    inline bool create_rs_2_m() noexcept
    {
        if(rs_2_m_ses != nullptr)
            return true;
        
        if (of_create_codec_instance(&rs_2_m_ses, OF_CODEC_REED_SOLOMON_GF_2_M_STABLE, OF_DECODER, 2) != OF_STATUS_OK)
            return false;

        return true;
    }
    
    /**
     * @brief set_rs_2_param
     * 对rs_2_m设置参数
     * @param nb_src_sym
     * 源数据包数
     * @param nb_rpr_sym
     * 冗余数据包数
     * @return 
     */
    inline bool set_rs_2_param(uint32_t nb_src_sym,uint32_t nb_rpr_sym,uint32_t symbol_length) noexcept {
        if(rs_2_m_params == nullptr){
            rs_2_m_params = static_cast<of_rs_2_m_parameters_t*>(calloc(1, sizeof(of_rs_2_m_parameters_t)));
            if(rs_2_m_params == nullptr)
                return false;
        }
        rs_2_m_params->m = 8;
        of_parameters_t	*params = (of_parameters_t *) rs_2_m_params;
        params->nb_source_symbols	= nb_src_sym;		/* fill in the generic part of the of_parameters_t structure */
        params->nb_repair_symbols	= nb_rpr_sym;
        params->encoding_symbol_length	= symbol_length;   
        
        if(rs_2_m_ses == nullptr) {
            create_rs_2_m();
            
            if( rs_2_m_ses == nullptr)
                return false;
        }
        if (of_set_fec_parameters(rs_2_m_ses, params) != OF_STATUS_OK)
            return false;
        return true;
    }
    
    /**
     * @brief create_ldpc
     * 创建ldpc的算法的会话
     * @return 
     */
    inline bool create_ldpc() noexcept
    {
        if(ldpc_ses != nullptr)
            return true;
        
        if (of_create_codec_instance(&ldpc_ses, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_DECODER, 2) != OF_STATUS_OK)
            return false;
        
        return true;
    }
    
    /**
     * @brief set_ldpc_param
     * 对ldpc设置参数
     * @param nb_src_sym
     * @param nb_rpr_sym
     * @return 
     */
    inline bool set_ldpc_param(uint32_t nb_src_sym,uint32_t nb_rpr_sym,uint32_t symbol_length) noexcept {
        
        if(ldpc_params == nullptr){
            ldpc_params = static_cast<of_ldpc_parameters_t*>(calloc(1, sizeof(of_ldpc_parameters_t)));
            if(ldpc_params == nullptr)
                return false;
        }
        ldpc_params->prng_seed	= rand();
        ldpc_params->N1		= 7;
        of_parameters_t	*params = (of_parameters_t *) ldpc_params;
        params->nb_source_symbols	= nb_src_sym;		/* fill in the generic part of the of_parameters_t structure */
        params->nb_repair_symbols	= nb_rpr_sym;
        params->encoding_symbol_length	= symbol_length;   
        
        if(ldpc_ses == nullptr) {
            create_ldpc();
            
            if( ldpc_ses == nullptr)
                return false;
        }
        if (of_set_fec_parameters(ldpc_ses, params) != OF_STATUS_OK)
            return false;
        
        return true;
    }
    
    inline bool decode(uint32_t src_pack_nb,uint32_t repair_pack_nb,uint32_t block_size,
					   void ** src_data,void ** dst_data) noexcept{
        //判断解码参数，如果不一致则重新设置参数
        of_session * _cur_ses{nullptr};
        if( src_pack_nb != this->src_pack_nb || repair_pack_nb != this->repair_pack_nb || block_size != this->sym_len){
            this->src_pack_nb = src_pack_nb;
            this->repair_pack_nb = repair_pack_nb;
            this->sym_len = block_size;
            
            if( src_pack_nb + repair_pack_nb <= 255){
                if( this->set_rs_2_param(src_pack_nb,repair_pack_nb,block_size) == false)
                    return false;
                else
                    _cur_ses = rs_2_m_ses;
            } else {
                if( this->set_ldpc_param(src_pack_nb,repair_pack_nb,block_size) == false)
                    return false;
                else
                    _cur_ses = ldpc_ses;
            }
        }
		
		if( of_set_available_symbols(_cur_ses,src_data) != OF_STATUS_OK)
			return false;
		
		auto ret = of_finish_decoding(_cur_ses);
		if( ret == OF_STATUS_ERROR || ret == OF_STATUS_FATAL_ERROR)
			return false;
		
        if (of_get_source_symbols_tab(_cur_ses, dst_data) != OF_STATUS_OK) {
            OF_PRINT_ERROR(("of_get_source_symbols_tab() failed\n"))
            return false;
        }
        return true;
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

core::FramePacket *FECDecoder::decode(RTPPacket *packet) noexcept
{
	d_ptr->cache.lock_source();
    auto ts = d_ptr->cache.pop(packet);
	if( ts == 0 ){
		d_ptr->cache.unlock_source();
        return nullptr;
	}
	
	auto list = d_ptr->cache.push();
	d_ptr->cache.unlock_source();
	if( list == nullptr)
		return nullptr;
    
    if( list->not_have_repair == true){
        //这个不需要解码
        core::FramePacket *frame = core::FramePacket::Make_Packet();
        if(frame == nullptr){
			delete list;
            return nullptr;
        }
		
		auto data = PackageVector::copy_data(list);
		if(data.first == nullptr){
			delete frame;
			delete list;
			return nullptr;
		}
        frame->data[0] = static_cast<uint8_t*>(data.first);
        frame->size = data.second;
        frame->payload_type = d_ptr->cache.get_payload_type();
        frame->pts = frame->dts = list->ts;
        
		delete list;
        return frame;
    } else {
        //这个需要解码
		auto pair = list->create_array();
		if( pair.first == nullptr || pair.second == nullptr){
			list->release_decode_array(pair);
			delete list;
			return nullptr;
		}
		auto && block_size = list->get_block_size();
        auto ret = d_ptr->decode(list->src_nb,list->repair_nb,block_size,
								 pair.first,pair.second);
        core::FramePacket *frame;
        if( ret == false || (frame = core::FramePacket::Make_Packet()) == nullptr){
			list->release_decode_array(pair);
			delete list;
			core::Logger::Print("decode error","",LogLevel::ERROR_LEVEL);
            return nullptr;
        }
		auto data = PackageVector::copy_data(pair.second,list->src_nb,block_size,list->fill_size);
		if(data.first == nullptr){
			list->release_decode_array(pair);
			delete frame;
			delete list;
			return nullptr;
		}
        frame->data[0] = static_cast<uint8_t*>(data.first);
        frame->size = data.second;
        frame->payload_type = d_ptr->cache.get_payload_type();
        frame->pts = frame->dts = list->ts;
		core::Logger::Print("decode success","",LogLevel::MOREINFO_LEVEL);
		list->release_decode_array(pair);
		delete list;
        return frame;
    }
}

bool FECDecoder::decode(uint32_t src_pack_nb, uint32_t repair_pack_nb, uint32_t block_size,
                        void **src_data, void **dst_data) noexcept
{
    return d_ptr->decode(src_pack_nb,repair_pack_nb,block_size,
                         src_data,dst_data);
}



} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

