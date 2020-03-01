#include "fecencoder.h"
#include "../../core/logger.h"
#include <memory>
#include <math.h>
#include <string.h>
extern "C"{
#include "libopenfec/lib_common/of_openfec_api.h"
}

namespace rtplivelib {

namespace rtp_network {

namespace fec {

class FECEncoderPrivateData;

class FECEncoderPrivateData{
public:
    /*不同算法对应的结构体*/
    of_rs_2_m_parameters_t	*rs_2_m_params{nullptr};
    of_ldpc_parameters_t * ldpc_params{nullptr};
    of_session_t	*rs_2_m_ses{nullptr};
    of_session_t	*ldpc_ses{nullptr};
    /*用来编码的二维数组(包括源数据空间和冗余数据空间)*/
    void**		enc_symbols_tab{nullptr};
    /* 该数组用于存放临时分配的空间，用于保存冗余数据，
     * 为了防止多次释放分配，所以和enc_symbols_tab分开存放
     * 只有当空间不够时才考虑重新分配*/
    void**      tmp_alloc{nullptr};
    /*enc_symbols_tab数组的第一维长度，第二维长度是cur_sym_len*/
    uint32_t total_size{0};
    /*tmp_alloc数组的第一维长度，第二维长度是cur_sym_len*/
    uint32_t repair_size{0};
    /*外部设置的总包数设置，用于判断是否需要初始化*/
    uint32_t total_pack_nb{0};
    /*外部设置的冗余包数，用于判断是否需要初始化*/
    uint32_t repair_pack_nb{0};
    /*块大小*/
    uint32_t cur_sym_len{1024};
    /*原数据占有比例,越低冗余包越多*/
    float code_rate{0.8f};
    
    ~FECEncoderPrivateData(){
        release_rs_2_m();
        release_ldpc();
        release_symbols_tab();
        release_tmp_alloc();
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
     * @brief release_symbols_tab
     * 释放临时存放数据的数组的空间
     */
    inline void release_symbols_tab() noexcept {
        if(enc_symbols_tab != nullptr){
            free(enc_symbols_tab);
        }
        total_size = 0;
    }
    
    /**
     * @brief create_symbols_tab
     * 创建用于编码的数组
     * @param size
     * 数组大小
     * @return 
     */
    inline bool create_symbols_tab(uint32_t size) noexcept {
        release_symbols_tab();
        enc_symbols_tab = (void**)calloc(size,sizeof(void*));
        if(enc_symbols_tab == nullptr)
            return false;
        total_size = size;
        return true;
    }
    
    /**
     * @brief release_tmp_alloc
     * 释放临时分配的空间
     */
    inline void release_tmp_alloc() noexcept{
        if(tmp_alloc == nullptr)
            return;
        if(repair_size != 0){
            for(auto n = 0u; n < repair_size;++n){
                free(tmp_alloc[n]);
            }
        }
        free(tmp_alloc);
        repair_size = 0;
    }
    
    /**
     * @brief create_tmp_alloc
     * 创建临时分配的空间
     * @param size
     * 数组大小
     * @param pack_len
     * 每个数组的大小
     * @return 
     */
    inline bool create_tmp_alloc(uint32_t size) noexcept {
        release_tmp_alloc();
        tmp_alloc = (void**)calloc(size,sizeof(void*));
        if(tmp_alloc == nullptr)
            return false;
        for (auto n = 0u; n < size; ++n) {
            if((tmp_alloc[n] = calloc(1,cur_sym_len)) == nullptr){
                //中途分配失败，全部释放并返回false
                for( auto i = 0u; i < n ;++i){
                    free(tmp_alloc[i]);
                }
                free(tmp_alloc);
                return false;
            }
        }
        repair_size = size;
        return true;
    }
    
    /**
     * @brief resize_tmp_alloc
     * 重新分配大小
     * 基础单位大小不变(第二维)
     * @param size
     * 数组大小(第一维)
     * @return 
     */
    inline bool resize_tmp_alloc(uint32_t size) noexcept {
        if(size == repair_size)
            return true;
        
        if(repair_size == 0)
            return create_tmp_alloc(size);
        
        void ** _new_tmp = (void**)calloc(size,sizeof(void*));
        if(_new_tmp == nullptr)
            return false;
        
        if( size > repair_size){
            //扩容
            auto difference = size - repair_size;
            //先分配差值
            for (auto n = 0u; n < difference; ++n) {
                if((_new_tmp[n] = calloc(1,cur_sym_len)) == nullptr){
                    //中途分配失败，全部释放并返回false
                    for( auto i = 0u; i < n ; ++i){
                        free(_new_tmp[i]);
                    }
                    free(_new_tmp);
                    return false;
                }
            }
            //剩余的做移动就可以了
            for( auto n = difference; n < size; ++n){
                _new_tmp[n] = tmp_alloc[n-difference];
            }
            
        } else {
            //缩小
            auto n = 0u;
            //先移动内存
            for(;n < size;++n){
                _new_tmp[n] = tmp_alloc[n];
            }
            //再把剩余的释放
            for(;n < repair_size;++n){
                free(tmp_alloc[n]);
            }
        }
        
        free(tmp_alloc);
        tmp_alloc = _new_tmp;
        repair_size = size;
        return true;
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
        
        if (of_create_codec_instance(&rs_2_m_ses, OF_CODEC_REED_SOLOMON_GF_2_M_STABLE, OF_ENCODER, 2) != OF_STATUS_OK)
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
    inline bool set_rs_2_param(uint32_t nb_src_sym,uint32_t nb_rpr_sym) noexcept {
        if(rs_2_m_params == nullptr){
            rs_2_m_params = static_cast<of_rs_2_m_parameters_t*>(calloc(1, sizeof(of_rs_2_m_parameters_t)));
            if(rs_2_m_params == nullptr)
                return false;
        }
        rs_2_m_params->m = 8;
        of_parameters_t	*params = (of_parameters_t *) rs_2_m_params;
        params->nb_source_symbols	= nb_src_sym;		/* fill in the generic part of the of_parameters_t structure */
        params->nb_repair_symbols	= nb_rpr_sym;
        params->encoding_symbol_length	= cur_sym_len;
        
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
        
        if (of_create_codec_instance(&ldpc_ses, OF_CODEC_LDPC_STAIRCASE_STABLE, OF_ENCODER, 2) != OF_STATUS_OK)
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
    inline bool set_ldpc_param(uint32_t nb_src_sym,uint32_t nb_rpr_sym) noexcept {
        
        if(ldpc_params == nullptr){
            ldpc_params = static_cast<of_ldpc_parameters_t*>(calloc(1, sizeof(of_ldpc_parameters_t)));
            if(ldpc_params == nullptr)
                return false;
        }
        ldpc_params->prng_seed	= rand();
        ldpc_params->N1		= 11;
        of_parameters_t	*params = (of_parameters_t *) ldpc_params;
        params->nb_source_symbols	= nb_src_sym;		/* fill in the generic part of the of_parameters_t structure */
        params->nb_repair_symbols	= nb_rpr_sym;
        params->encoding_symbol_length	= cur_sym_len;   
        
        if(ldpc_ses == nullptr) {
            create_ldpc();
            
            if( ldpc_ses == nullptr)
                return false;
        }
        if (of_set_fec_parameters(ldpc_ses, params) != OF_STATUS_OK)
            return false;
        
        return true;
    }
    
    /**
     * @brief set_symbols_length
     * 设置单元长度
     * 设置成功则所有参数都需要重新初始化
     * @param size
     * 默认是1024
     */
    inline void set_symbols_length(uint32_t size) noexcept{
        if(cur_sym_len == size)
            return;
        cur_sym_len = size;
        release_symbols_tab();
        release_tmp_alloc();
    }
    
    /**
     * @brief set_code_rate
     * 设置源数据占有比例
     * 设置成功则所有参数都需要重新初始化
     * @param value
     * 默认是0.8
     */
    inline void set_code_rate(float value) noexcept {
        if( code_rate - value <= 0.000001f){
            return;
        }
        code_rate = value;
    }
    
    /**
     * @brief encode
     * 编码，编写冗余包
     * @param data
     * 原始数据
     * @param size
     * 数据大小
     * @return 
     */
    inline bool encode(void * data,uint32_t size) noexcept{
        if(size == 0 || data == nullptr || code_rate <= 0.001f || cur_sym_len == 0)
            return false;
        
        //先计算出源数据的包数和总包数
        auto src_pack_nb = size / cur_sym_len;
		//经过测试，一到两个源码包是比较容易出错的
		//现在先不处理这个问题
        return _encode(data,size,src_pack_nb);
//        if(src_pack_nb <= 1)
//            return _encode_little_package(data,size,src_pack_nb);
//        else  
//            return _encode(data,size,src_pack_nb);
    }
    
private:
    /**
     * @brief _encode_little_package
     * 这里就不使用算法编码了，采用发送相同的包
     */
    bool _encode_little_package(void * data,uint32_t size,uint32_t src_pack_nb) noexcept{
		if(src_pack_nb == 0){
            total_pack_nb = 2;
            repair_pack_nb = 1;
        } else if( src_pack_nb == 1){
            total_pack_nb = 3;
            repair_pack_nb = 1;
        } else {
            return false;
        }
        
        //如果总包数比之前的大，则需要重新分配空间，其他情况不需要
        //想减少分配空间的开销
        if( total_pack_nb > total_size || total_size == 0){
            if(create_symbols_tab(total_pack_nb) == false)
                return false;
        }
        
        //如果冗余包数比预设的要大，则重新分配空间
        //多分配一个空间，分包时最后一个包可能不满，需要填充
        if( repair_pack_nb > ( repair_size - 1) || repair_size == 0){
            if(resize_tmp_alloc(repair_pack_nb + 1) == false)
                return false;
        }
        
        if(src_pack_nb == 0){
            enc_symbols_tab[0] = data;
            enc_symbols_tab[1] = tmp_alloc[0];
            memset(enc_symbols_tab[1],0,cur_sym_len);
            memcpy(enc_symbols_tab[1],data,size);
        } else{
            uint8_t * _d = static_cast<uint8_t*>(data);
            uint8_t * _d2 = _d + cur_sym_len;
            enc_symbols_tab[0] = _d;
            enc_symbols_tab[1] = tmp_alloc[0];
            enc_symbols_tab[2] = tmp_alloc[1];
            memset(enc_symbols_tab[1],0,cur_sym_len);
            auto pkg_size = cur_sym_len;
            memcpy(enc_symbols_tab[1],_d2,pkg_size);
            
            //异或运算
            uint64_t src1{0},src2{0},dst{0};
            uint8_t * _dst = static_cast<uint8_t*>(enc_symbols_tab[2]);
            memset(enc_symbols_tab[2],0,cur_sym_len);
            
            for(;pkg_size >= 8; pkg_size-=8){
                memcpy(&src1,_d,8);
                memcpy(&src2,_d2,8);
                dst = src1 ^ src2;
                memcpy(_dst,&dst,8);
                _d += 8;
                _d2 += 8;
                _dst += 8;
            }
            
            if(pkg_size != 0){
                memcpy(&src1,_d,pkg_size);
                memcpy(&src2,_d2,pkg_size);
                dst = src1 ^ src2;
                memcpy(_dst,&dst,pkg_size);
            }
        }
        return true;
    }
    
    /**
     * @brief _encode
     * 编码的实际操作，不过当包数小于等于两个的时候容易失败
     * 所以分开编码
     */
    bool _encode(void * data,uint32_t size,uint32_t src_pack_nb) noexcept{
        bool fill_flag;
        if( src_pack_nb*cur_sym_len != size){
            ++src_pack_nb;
            fill_flag = true;
        } else 
            fill_flag = false;
        
        float total = floor((float)src_pack_nb / code_rate);
        if( total * code_rate < src_pack_nb)
            total += 1;

        uint32_t _total = static_cast<uint32_t>(total);
        uint32_t _repair = _total - src_pack_nb;
        of_session * _cur_ses{nullptr};
        
        //如果参数和以前不一样，则重新设置参数
        if( _total != total_pack_nb || src_pack_nb != (total_pack_nb - repair_pack_nb)){
            total_pack_nb = _total;
            repair_pack_nb = _repair;
            //当包数比较少的时候采用rs_2_m,否则采用ldpc
            if(_total<=256){
                if(set_rs_2_param(src_pack_nb,repair_pack_nb)==false)
                    return false;
                _cur_ses = rs_2_m_ses;
            }
            else {
                if(set_ldpc_param(src_pack_nb,repair_pack_nb)==false)
                    return false;
                _cur_ses = ldpc_ses;
            }
        }
        
        //如果总包数比之前的大，则需要重新分配空间，其他情况不需要
        //减少分配空间的开销
        if( _total > total_size || total_size == 0){
            if(create_symbols_tab(_total) == false)
                return false;
        }
        
        //如果冗余包数比预设的要大，则重新分配空间
        //多分配一个空间，分包时最后一个包可能不满，需要填充
        if( _repair > ( repair_size - 1) || repair_size == 0){
            if(resize_tmp_alloc(_repair + 1) == false)
                return false;
        }
        
        //拷贝数据
        auto n = 0u;
        uint8_t * _d = static_cast<uint8_t*>(data);
        //源数据做迁移
        for(; n < src_pack_nb - 1; ++n){
            enc_symbols_tab[n] = _d;
            _d += cur_sym_len;
        }
        //填充最后一块
        if(fill_flag){
            //需要填充
            enc_symbols_tab[n] = tmp_alloc[repair_size - 1];
            memset(enc_symbols_tab[n],0,cur_sym_len);
            memcpy(enc_symbols_tab[n],_d,size%cur_sym_len);
        } else {
            //不需要填充
            enc_symbols_tab[n] = _d;
        }
        ++n;
        
        //分配冗余数据空间,同时进行编码
        for(; n < _total; ++n){
            enc_symbols_tab[n] = tmp_alloc[n - src_pack_nb];
            if(of_build_repair_symbol(_cur_ses, enc_symbols_tab, n) != OF_STATUS_OK){
                core::Logger::Print("build repair error.(total:{},cur_repair:{},src_nb:{},tmp_alloc:{},size:{})",
                                    "",
                                    LogLevel::ERROR_LEVEL,
                                    total_pack_nb,
                                    n - src_pack_nb,
                                    src_pack_nb,
                                    repair_size,
                                    size);
                return false;
            }
        }
        return true;
    }
};


///////////////////////////////////////////////

FECEncoder::FECEncoder():
    d_ptr(new FECEncoderPrivateData)
{
    set_symbol_size(1024);
}

FECEncoder::FECEncoder(float code_rate):
    d_ptr(new FECEncoderPrivateData)
{
    set_symbol_size(1024);
    set_code_rate(code_rate);
}

FECEncoder::~FECEncoder()
{
    delete d_ptr;
}

void FECEncoder::set_code_rate(const float &code_rate) noexcept
{
    d_ptr->set_code_rate(code_rate);
}

float FECEncoder::get_code_rate()  noexcept
{
    return d_ptr->code_rate;
}

void FECEncoder::set_symbol_size(uint32_t value) noexcept
{
    d_ptr->set_symbols_length(value);
}

uint32_t FECEncoder::get_symbol_size() noexcept
{
    return d_ptr->cur_sym_len;
}

bool FECEncoder::encode(void *data, uint32_t size) noexcept
{
    return d_ptr->encode(data,size);
}

uint32_t FECEncoder::get_all_pack_nb() noexcept
{
    return d_ptr->total_pack_nb;
}

uint32_t FECEncoder::get_repair_nb() noexcept
{
    return d_ptr->repair_pack_nb;
}

void **FECEncoder::get_data() noexcept
{
    return d_ptr->enc_symbols_tab;
}

} //namespace fec

} //namespace rtp_network

} //namespace rtplivelib

