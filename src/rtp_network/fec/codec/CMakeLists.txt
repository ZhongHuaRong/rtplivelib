
project(feccodec)

aux_source_directory(. src_dir)
ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_dir})    
TARGET_LINK_LIBRARIES(${PROJECT_NAME} wirehair)

file(GLOB headers "*.h")
INSTALL(FILES ${headers}
        DESTINATION ${HEADERS_INSTALL_DIR}/rtp_network/fec/codec)