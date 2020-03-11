
project(audio_processing)

aux_source_directory(. src_dir)
ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_dir})

file(GLOB headers "*.h")
INSTALL(FILES ${headers}
        DESTINATION ${HEADERS_INSTALL_DIR}/${PROJECT_NAME})