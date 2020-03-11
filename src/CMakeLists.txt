cmake_minimum_required(VERSION 3.5)
project(rtplive)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD 14)

ADD_DEFINITIONS(-D RTPLIVELIB_LIBRARY)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE MATCHES Release)
    ADD_DEFINITIONS(-D NDEBUG)
else()
    ADD_DEFINITIONS(-D DEBUG)
endif()

if(UNIX)
    ADD_DEFINITIONS(-D unix)
    SET(SDK_PATH ${CMAKE_SOURCE_DIR}/../SDK/UNIX)
elseif(WIN64)
    ADD_DEFINITIONS(-D WIN64)
    SET(SDK_PATH ${CMAKE_SOURCE_DIR}/../SDK/win64)
endif(UNIX)
 
#SET(CMAKE_MODULE_PATH ./cmake)
INCLUDE_DIRECTORIES(${SDK_PATH}/include/)
INCLUDE_DIRECTORIES(${SDK_PATH}/../include/)
LINK_DIRECTORIES(${SDK_PATH}/lib/)
LINK_DIRECTORIES(${SDK_PATH}/bin/)

SET(HEADERS_INSTALL_DIR include/${PROJECT_NAME})
AUX_SOURCE_DIRECTORY(. files)
ADD_LIBRARY(${PROJECT_NAME} SHARED ${files})

ADD_SUBDIRECTORY(audio_processing)
ADD_SUBDIRECTORY(codec)
ADD_SUBDIRECTORY(core)
ADD_SUBDIRECTORY(device_manager)
ADD_SUBDIRECTORY(image_processing)
ADD_SUBDIRECTORY(player)
ADD_SUBDIRECTORY(rtp_network)

TARGET_LINK_LIBRARIES(${PROJECT_NAME}
                        rtp_network
                        device_manager
                        player 
                        codec 
                        audio_processing 
                        image_processing 
                        core )

if(UNIX)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME}
                          avdevice
                          avformat
                          avutil
                          swscale
                          avfilter
                          swresample
                          SDL2
                          jrtp
                          jthread
                          openfec
                          wirehair)
    INSTALL(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
            )
elseif(WIN64)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME}
                          avdevice-58
                          avcodec-58
                          avformat-58
                          avutil-56
                          swscale-5
                          avfilter-7
                          swresample-3
                          SDL2
                          libjrtp
                          libopenfec
                          wirehair
                          Ws2_32
                          Advapi32
                          Ole32
                          OleAut32
                          Strmiids)
    INSTALL(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION bin
            ARCHIVE DESTINATION lib
            )
endif(UNIX)

INSTALL(FILES ./liveengine.h
        DESTINATION ${HEADERS_INSTALL_DIR})

