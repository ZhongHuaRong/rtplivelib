# rtplivelib
基于rtp协议的视频实时通信动态链接库，使用C++编写。

## 目前实现的功能
* FEC前向纠错和防抖动(采用喷泉码)
* qsv(h264,HEVC不包含解码)
* nvenc(h264,HEVC)
* 视频编解码(h264,HEVC)
* 音频编解码(AAC)
* 摄像头采集(dshow,v4l2)
* 桌面采集(gdigrab,dxgi,fbdev)
* 麦克风和声卡采集(WASAPI,alsa)
* 视频播放(SDL2)
* 音频播放(SDL2)
* 音频重采样
* 用户管理

## 下一步工作
* 重构(使项目流程更加清晰,统一队列的输入输出)
* 保存录像(将编码后的数据保存到文件)
* qsv解码和nvdec
* 完善音频编解码
* 音视频同步
* 音频合成,图像合成

## 已知的部分bug
* Windows下摄像头重名的问题(dshow问题,他不会处理重名,导致重名无法开启第二个设备)
* 在deepin系统采集桌面时，将会采集tty2，而不是当前桌面
* 用户识别出问题(主要是SSRC的更改出现的逻辑问题)
* 使用qsv有的系统会出现崩溃
* 解码的时候容易出现无效指针的问题
* 帧从内存转到显存出错时会出现内存泄漏
* audio encoder有异常

## 上手指南
以下指南将帮助你在本地机器上安装和运行该项目，进行开发


### 安装要求
1.GCC<br>
2.qmake or cmake<br>
3.各种依赖库(ffmpeg,SDL2,jrtplib,x264,x265,libfdk-aac,libmfx,openfec,wirehair),下载的时候已经自带，如果需要自己编译则可以看依赖库构建的步骤。
好像Linux的.so库的链接不能拷贝过来，所以到时候需要自己创建链接<br>

### 安装步骤
* 使用qmake<br>
  1.安装IDE（QtCreator），推荐把整个Qt SDK下载下来，链接：https://download.qt.io/official_releases/qt/<br>
  编译的时候,Windows系统推荐Mingw 64bit,Linux系统使用GCC 64bit,不要使用32bit<br>
  2.打开QtCreator，打开rtplivelib项目，构建即可生成lib库
* 使用cmake<br>
  1.先进入build目录,根据系统选择相应的脚本<br>
  2.在toolChain.cmake文件修改c/c++编译器的路径<br>
  3.运行脚本,再调用make && make install即可完成编译和安装，目前仅支持Windows系统<br>
    cd build/msys_mingw64/<br>
    ./make-mingw64-Makefiles-Release.sh<br>
    make -j8 && make install<br>


### 依赖库的构建
优先使用自带的lib库，已经经过测试没有问题！如果出现问题，可以参考[这里](https://github.com/ZhongHuaRong/rtplivelib/blob/master/build.md)自己手动编译lib库。

### 其他要求
* Windows<br>
  * 如果要使用硬件加速，则需要安装一系列的SDK。目前只使用了qsv，这里简单说一下Media SDK的安装<br>
    1.更新到最新的显卡驱动<br>
    2.去Intel官网下载[Intel Media SDK](https://software.intel.com/en-us/media-sdk/choose-download/client),需要intel账号才能下载<br>
    3.安装Intel Media SDK。如果安装失败则需要考虑是不是显卡驱动问题<br>

* Linux<br>
  * Media SDK的安装<br>
    这个我没有安装成功，先不写<br>
  * 如果提示not found -lasound的话则是缺少alsa，需要去ALSA项目主页下载[alsa-lib](http://www.alsa-project.org/main/index.php/Main_Page)并安装.<br>

## 使用
配合一个GUI应用和一个服务器应用，可以简单跑一下流程<br>
GUI应用:https://github.com/ZhongHuaRong/RTPDemo<br>
Server:https://github.com/ZhongHuaRong/RTPServer<br>
搭建好之后，就可以参考GUI的README的步骤来跑一下流程<br>
