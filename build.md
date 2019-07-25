# 依赖库的构建
优先使用自带的lib库，已经经过测试没有问题！如果出现问题，可以参考以下步骤自己手动编译lib库。
## 编译环境的搭建
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

## jrtplib,jthread
[jrtplib下载地址](http://research.edm.uhasselt.be/jori/page/CS/Jrtplib.html)<br>
[jthread下载地址](http://research.edm.uhasselt.be/jori/page/CS/Jthread.html)<br>
里面自带编译教程，就不说了

## SDL2
直接去官网下载,我自己试过编译，不过没成功，官网地址:https://www.libsdl.org/

## openfec
[下载地址](http://www.openfec.org/)<br>
Linux系统编译的话就比较方便，直接进入到根目录，然后输入: 
* cmake ./
* make -j8 <br>
<br>
Windows下则需要一点点的修改,如果环境变量没有设置好，则需要在根目录下的CMakeLists.txt添加下面三行<br>
路径需要修改成自己的msys安装路径<br>
set(CMAKE_C_FLAGS "-g -Wall  -I /d/msys64/mingw64/include -L /d/msys64/mingw64/lib")<br>
set(CMAKE_CXX_FLAGS "-g -Wall  -I /d/msys64/mingw64/include -L /d/msys64/mingw64/lib")<br>
set(CMAKE_CXX_COMPILER "g++")<br>

然后从控制台进入到根目录输入下面的命令<br>
* cmake -G "MinGW Makefiles" -DDEBUG:STRING=OFF<br>
生成makefile后直接make就可以了，不过没有make install，需要手动copy

# mfx
[下载地址](https://github.com/lu-zero/mfx_dispatch)库，没有这个库编译的时候将会提示缺少头文件
* ./configure
* make clean
* make -j8
* make install

## ffmpeg
编译这个就有点复杂了，我们需要定制一个ffmpeg动态链接库，需要x264,x265,libfdk-aac,libmfx(qsv需要)<br>
x264,x265,libfdk-aac的编译方法，给一个帖子大家参考一下:https://www.cnblogs.com/yaoz/p/6944942.html<br>
libmfx的编译很简单，只需要下面几步
* ./configure
* make clean
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
* 在Linux系统编译时，可能会出现下面这种情况<br>
![](https://github.com/ZhongHuaRong/rtplivelib/blob/master/img/not_found_x265.png)<br>
在x265.pc文件里的Libs.private一行添加-lpthread，如下图所示:<br>
![](https://github.com/ZhongHuaRong/rtplivelib/blob/master/img/x265_pkg_config.png)<br>
* 还有一些问题我没有记录下来，如果遇到一些包not found的话，可能是pkg-config的问题，也可能是包编译错误的问题。遇到问题多谷歌多百度就很容易解决了。
