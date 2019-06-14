# rtplivelib
基于rtp协议的视频实时通信动态链接库，使用C++编写。


# 上手指南
以下指南将帮助你在本地机器上安装和运行该项目，进行开发


## 安装要求
1.GCC<br>
2.qmake<br>
3.各种依赖库(ffmpeg,SDL2,jrtplib,x264,x265,libfdk-aac,openfec),下载的时候已经自带，如果需要自己编译则可以看安装步骤。
好像Linux的.so库的链接不能拷贝过来，所以到时候需要自己创建链接<br>


## 安装步骤
1.安装IDE（QtCreator），推荐把整个Qt SDK下载下来，链接：https://download.qt.io/official_releases/qt/
  编译的时候,Windows系统推荐Mingw 64bit,Linux系统使用GCC 64bit,不要使用32bit<br>
2.打开QtCreator，打开rtplivelib项目，构建即可生成lib库


## 依赖库的构建
### 编译环境的搭建
* Linux
1.gcc<br>
2.cmake<br>
3.nasm<br>
4.yasm<br>
5.pkg-config

### jrtplib,jthread
[jrtplib下载地址](http://research.edm.uhasselt.be/jori/page/CS/Jrtplib.html)<br>
[jthread下载地址](http://research.edm.uhasselt.be/jori/page/CS/Jthread.html)<br>
里面自带编译教程，就不说了

### SDL2
直接去官网下载,我自己试过编译，不过没成功，官网地址:https://www.libsdl.org/

### openfec
[下载地址](http://www.openfec.org/)<br>
Linux系统编译的话就比较方便，直接进入到根目录，然后输入: 
* cmake ./
* make -j8

### ffmpeg
编译这个就有点复杂了，我们需要定制一个ffmpeg动态链接库，需要x264,x265,libfdk-aac
* x264的编译

