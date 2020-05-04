
project(core)

aux_source_directory(. src_dir)
ADD_LIBRARY(${PROJECT_NAME} STATIC ${src_dir})

file(GLOB headers "*.h")
list(REMOVE_ITEM headers ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/abstractobject.h
                         ${CMAKE_SOURCE_DIR}/${PROJECT_NAME}/logger.h)
INSTALL(FILES ${headers}
        DESTINATION ${HEADERS_INSTALL_DIR}/${PROJECT_NAME})