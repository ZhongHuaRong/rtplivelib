
project(device_manager)

aux_source_directory(. src_dir)
if(UNIX)
    list(REMOVE_ITEM src_dir ./wasapi.cpp)
    list(REMOVE_ITEM src_dir ./dxgicapture.cpp)
elseif(WIN64)
    list(REMOVE_ITEM src_dir ./alsa.cpp)
endif(UNIX)
ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_dir})

file(GLOB headers "*.h")
if(UNIX)
    list(REMOVE_ITEM headers ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/wasapi.h)
    list(REMOVE_ITEM headers ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/dxgicapture.h)
elseif(WIN64)
    list(REMOVE_ITEM headers ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/alsa.h)
endif(UNIX)
INSTALL(FILES ${headers}
        DESTINATION ${HEADERS_INSTALL_DIR}/${PROJECT_NAME})