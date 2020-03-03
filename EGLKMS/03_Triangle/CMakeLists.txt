SET(SRC_LIST ${PROJECT_SOURCE_DIR}/01_InitKMS/init_kms.c
             trangle_test.c)

ADD_DEFINITIONS(-DGL_GLEXT_PROTOTYPES)

ADD_EXECUTABLE(triangle_test ${SRC_LIST})

TARGET_LINK_LIBRARIES(triangle_test drm gbm EGL GL)