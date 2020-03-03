SET(SRC_LIST ${PROJECT_SOURCE_DIR}/01_InitKMS/init_kms.c
             egl_image_dma_buf_test.c)

ADD_DEFINITIONS(-DGL_GLEXT_PROTOTYPES)
ADD_DEFINITIONS(-DEGL_EGLEXT_PROTOTYPES)

ADD_EXECUTABLE(egl_image_dma_buf_test ${SRC_LIST})

TARGET_LINK_LIBRARIES(egl_image_dma_buf_test drm gbm EGL GL)