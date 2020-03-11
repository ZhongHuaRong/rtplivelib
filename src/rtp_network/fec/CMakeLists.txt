
project(fec)

ADD_SUBDIRECTORY(codec feccodec)
aux_source_directory(. src_dir)
ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_dir})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} feccodec)

file(GLOB headers "*.h")
INSTALL(FILES ${headers}
        DESTINATION ${HEADERS_INSTALL_DIR}/rtp_network/${PROJECT_NAME})