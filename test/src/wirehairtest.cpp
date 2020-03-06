
#include "fectest.h"
#include "rtp_network/fec/codec/wirehair.h"
#include <gtest/gtest.h>

/**
 * 用于测试Wirehair的编解码是否正常
 */

using namespace rtplivelib;
using namespace rtplivelib::rtp_network;
using namespace rtplivelib::core;
using namespace rtplivelib::rtp_network::fec;

//TEST(WirehairTest,Encode_Decode){
    
//    ASSERT_TRUE(Wirehair::InitCodec());
//    Wirehair encoder(Wirehair::Encoder,size);
//    Wirehair decoder(Wirehair::Decoder,size);
    
//    auto data_vector = read_file("config.log");
//    Result ret{Success};
//    std::vector<std::vector<int8_t>> enc_pkt;
//    ret = encoder.encode(data_vector.data(),data_vector.size(),0.8f,enc_pkt);
//    ASSERT_EQ(ret,Success);
    
//    auto pack_num_list = simulated_packet_loss(enc_pkt.size() * 0.5 + 10,enc_pkt.size() * 0.2);
//    for(auto i = pack_num_list.begin();i != pack_num_list.end();++i){
//        ret = decoder.decode(*i,enc_pkt[*i],data_vector.size());
//        if(ret == Success){
//            break;
//        } else {
//            ASSERT_EQ(ret,FEC_Decode_Need_More);
//        }
//    }
    
//    std::vector<int8_t> dec_pkt;
//    ret = decoder.data_recover(dec_pkt);
//    ASSERT_EQ(ret,Success);
//    write_file("./debug/1.txt",dec_pkt);
//    ASSERT_EQ(memcmp(data_vector.data(),dec_pkt.data(),dec_pkt.size()),0);
//}

TEST(WirehairTest,Decode){
    ASSERT_TRUE(Wirehair::InitCodec());
    Wirehair encoder(Wirehair::Encoder,size);
    Wirehair decoder(Wirehair::Decoder,size);
    
    Result ret{Success};
    std::vector<std::vector<int8_t>> enc_pkt;
    //+100是为了最后一个包不足size字节
    std::vector<int8_t> data_vector(size * size + 100,7);
    ret = encoder.encode(data_vector.data(),data_vector.size(),0.8f,enc_pkt);
    ASSERT_EQ(ret,Success);
    
    //复原所需要的包数略大于用于编码的包数
    auto pack_num_list = simulated_packet_loss(enc_pkt.size() * 0.8 + 10,enc_pkt.size() * 0.2);
    
    for(auto i = pack_num_list.begin();i != pack_num_list.end();++i){
        ret = decoder.decode(*i,enc_pkt[*i],data_vector.size());
        if(ret == Success){
            break;
        } else {
            ASSERT_EQ(ret,FEC_Decode_Need_More);
        }
    }
    ASSERT_EQ(ret,Success);
    std::vector<int8_t> dec_pkt;
    ret = decoder.data_recover(dec_pkt);
    ASSERT_EQ(ret,Success);
    ASSERT_EQ(memcmp(data_vector.data(),dec_pkt.data(),dec_pkt.size()),0);
}
