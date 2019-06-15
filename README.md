# rtplivelib
基于rtp协议的视频实时通信动态链接库，使用C++编写。


# 上手指南
以下指南将帮助你在本地机器上安装和运行该项目，进行开发


## 安装要求
1.GCC<br>
2.qmake<br>
3.各种依赖库(ffmpeg,SDL2,jrtplib,x264,x265,libfdk-aac,libmfx,openfec),下载的时候已经自带，如果需要自己编译则可以看依赖库构建的步骤。
好像Linux的.so库的链接不能拷贝过来，所以到时候需要自己创建链接<br>


## 开发环境的搭建
1.安装IDE（QtCreator），推荐把整个Qt SDK下载下来，链接：https://download.qt.io/official_releases/qt/<br>
  编译的时候,Windows系统推荐Mingw 64bit,Linux系统使用GCC 64bit,不要使用32bit<br>
2.打开QtCreator，打开rtplivelib项目，构建即可生成lib库

## 依赖库的构建
优先使用自带的lib库，已经经过测试没有问题！如果出现问题，可以参考[这里](https://github.com/ZhongHuaRong/rtplivelib/blob/master/build.md)自己手动编译lib库。

## 其他要求
如果要使用硬件加速，则需要安装一系列的SDK。目前只使用了qsv，这里简单说一下Media SDK的安装
* Windows<br>
1.更新到最新的显卡驱动<br>
2.去Intel官网下载[Intel Media SDK](https://software.intel.com/en-us/media-sdk/choose-download/client),需要intel账号才能下载<br>
3.安装Intel Media SDK。如果安装失败则需要考虑是不是显卡驱动问题<br>
* Linux<br>
这个我没有安装成功，先不写<br>

# 使用
配合一个GUI应用和一个服务器应用，可以简单跑一下流程<br>
GUI应用:https://github.com/ZhongHuaRong/RTPDemo<br>
Server:https://github.com/ZhongHuaRong/RTPServer<br>
搭建好之后，就可以参考GUI的README的步骤来跑一下流程<br>
