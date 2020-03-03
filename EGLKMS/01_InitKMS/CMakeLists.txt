SET(SRC_LIST init_kms.c
             kms_test.c)

ADD_EXECUTABLE(kms_test ${SRC_LIST})

TARGET_LINK_LIBRARIES(kms_test drm)