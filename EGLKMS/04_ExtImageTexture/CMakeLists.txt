SET(SRC_LIST ${PROJECT_SOURCE_DIR}/01_InitKMS/init_kms.c
             ext_image_texture_test.c)

ADD_DEFINITIONS(-DGL_GLEXT_PROTOTYPES)

ADD_EXECUTABLE(ext_image_texture_test ${SRC_LIST})

TARGET_LINK_LIBRARIES(ext_image_texture_test drm gbm EGL GL m)