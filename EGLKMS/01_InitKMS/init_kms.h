#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <xf86drmMode.h>

struct kms
{
	drmModeRes *res;
	drmModeConnector *connector;
	drmModeEncoder *encoder;
	drmModeCrtc *crtc;
	drmModeModeInfo mode;
	unsigned int fb_id;
};

extern int init_kms(int fd, struct kms* kms);
