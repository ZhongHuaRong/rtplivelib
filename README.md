# rtplivelib
基于rtp协议的视频实时通信动态链接库，使用C++编写。


# 上手指南
以下指南将帮助你在本地机器上安装和运行该项目，进行开发


## 安装要求
1.GCC<br>
2.qmake<br>
3.各种依赖库(ffmpeg,SDL2,jrtplib,x264,x265,libfdk-aac,libmfx,openfec),下载的时候已经自带，如果需要自己编译则可以看安装步骤。
好像Linux的.so库的链接不能拷贝过来，所以到时候需要自己创建链接<br>


## 开发环境的搭建
1.安装IDE（QtCreator），推荐把整个Qt SDK下载下来，链接：https://download.qt.io/official_releases/qt/
  编译的时候,Windows系统推荐Mingw 64bit,Linux系统使用GCC 64bit,不要使用32bit<br>
2.打开QtCreator，打开rtplivelib项目，构建即可生成lib库


## 依赖库的构建
如果觉得麻烦，就不要瞎折腾了，用项目自带的lib库吧！
### 编译环境的搭建
* Linux<br>
1.gcc,make<br>
2.cmake<br>
3.nasm<br>
4.yasm<br>
5.pkg-config<br>

* Windows<br>
1.[msys2](http://www.msys2.org/) + mingw64<br>
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
* make -j8 <br>
Windows下则需要一点点的修改,如果环境变量没有设置好，则需要在根目录下的CMakeLists.txt添加下面三行<br>
路径需要修改成自己的msys安装路径<br>
set(CMAKE_C_FLAGS "-g -Wall  -I /d/msys64/mingw64/include -L /d/msys64/mingw64/lib")<br>
set(CMAKE_CXX_FLAGS "-g -Wall  -I /d/msys64/mingw64/include -L /d/msys64/mingw64/lib")<br>
set(CMAKE_CXX_COMPILER "g++")<br>

然后从控制台进入到根目录输入下面的命令<br>
* cmake -G "MinGW Makefiles" -DDEBUG:STRING=OFF<br>
生成makefile后就是三部曲了

### ffmpeg
编译这个就有点复杂了，我们需要定制一个ffmpeg动态链接库，需要x264,x265,libfdk-aac,libmfx(qsv需要)<br>
x264,x265,libfdk-aac的编译方法，给一个帖子大家参考一下:https://www.cnblogs.com/yaoz/p/6944942.html<br>
libmfx的编译很简单，三部曲完事
* ./configure
* make -j8
* make install

ffmpeg的编译:<br>
`PKG_CONFIG_PATH路径记得修改为自己的`<br>
* Windows下的编译:<br>
PKG_CONFIG_PATH="/usr/local/lib/pkgconfig"  \\<br>
  ./configure \\<br>
  --arch=x86_64 \\<br>
  --host-os=win64 \\<br>
  --pkg-config-flags="--static" \\<br>
  --extra-cflags="-I/usr/local/include" \\<br>
  --extra-ldflags="-L/usr/local/lib" \\<br>
  --enable-pthreads \\<br>
  --disable-w32threads \\<br>
  --disable-os2threads \\<br>
  --disable-encoders \\<br>
  --disable-decoders \\<br>
  --disable-parsers  \\<br>
  --disable-muxers \\<br>
  --disable-demuxers \\<br>
  --disable-bsfs \\<br>
  --disable-protocols \\<br>
  --disable-filters \\<br>
  --disable-doc \\<br>
  --disable-static \\<br>
  --enable-shared \\<br>
  --enable-libfdk-aac \\<br>
  --enable-libx264 \\<br>
  --enable-libx265 \\<br>
  --enable-libmfx \\<br>
  --enable-decoder=hevc \\<br>
  --enable-decoder=hevc_qsv \\<br>
  --enable-decoder=h264 \\<br>
  --enable-decoder=h264_qsv \\<br>
  --enable-decoder=aac \\<br>
  --enable-decoder=aac_fixed \\<br>
  --enable-decoder=aac_latm \\<br>
  --enable-decoder=libfdk_aac \\<br>
  --enable-encoder=libx265 \\<br>
  --enable-encoder=hevc_qsv \\<br>
  --enable-encoder=libx264 \\<br>
  --enable-encoder=libx264rgb \\<br>
  --enable-encoder=h264_qsv \\<br>
  --enable-encoder=libfdk_aac \\<br>
  --enable-encoder=aac \\<br>
  --enable-parser=aac \\<br>
  --enable-parser=aac_latm \\<br>
  --enable-parser=hevc \\<br>
  --enable-demuxer=aac \\<br>
  --enable-muxer=hevc \\<br>
  --enable-filter=crop \\<br>
  --enable-gpl \\<br>
  --enable-nonfree
  
* Linux下的编译:<br>
PKG_CONFIG_PATH="$HOME/Desktop/build-linux/lib/pkgconfig" \\<br>
   ./configure \\<br>
  --prefix="$HOME/Desktop/ffmpeg-linux" \\<br>
  --pkg-config-flags="--static" \\<br>
  --arch=x86_64 \\<br>
  --disable-encoders \\<br>
  --disable-decoders \\<br>
  --disable-parsers  \\<br>
  --disable-muxers \\<br>
  --disable-demuxers \\<br>
  --disable-bsfs \\<br>
  --disable-protocols \\<br>
  --disable-filters \\<br>
  --disable-doc \\<br>
  --disable-static \\<br>
  --disable-outdev=oss \\<br>
  --disable-indev=oss \\<br>
  --enable-shared \\<br>
  --enable-libfdk-aac \\<br>
  --enable-libx264 \\<br>
  --enable-libx265 \\<br>
  --enable-libmfx \\<br>
  --enable-decoder=hevc \\<br>
  --enable-decoder=hevc_qsv \\<br>
  --enable-decoder=h264 \\<br>
  --enable-decoder=h264_qsv \\<br>
  --enable-decoder=aac \\<br>
  --enable-decoder=aac_fixed \\<br>
  --enable-decoder=aac_latm \\<br>
  --enable-decoder=libfdk_aac \\<br>
  --enable-encoder=libx265 \\<br>
  --enable-encoder=hevc_qsv \\<br>
  --enable-encoder=libx264 \\<br>
  --enable-encoder=libx264rgb \\<br>
  --enable-encoder=h264_qsv \\<br>
  --enable-encoder=libfdk_aac \\<br>
  --enable-encoder=aac \\<br>
  --enable-parser=aac \\<br>
  --enable-parser=aac_latm \\<br>
  --enable-parser=hevc \\<br>
  --enable-demuxer=aac \\<br>
  --enable-muxer=hevc \\<br>
  --enable-filter=crop \\<br>
  --enable-gpl \\<br>
  --enable-nonfree

可能大家在编译ffmpeg的时候出现各种问题，我简单罗列一下我遇到过的问题，大部分是在Windows下编译出现的，Linux是一步就完成了<br>
* 在我没有设置disable-bsfs的时候，编译是会在avcodec的时候出现链接错误，需要自己调用gcc生成，去掉这个之后就可以直接编译成功了
* 还有一个qsv的问题，更新集显驱动就可以了


