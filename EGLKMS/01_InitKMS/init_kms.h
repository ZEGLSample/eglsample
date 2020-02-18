#include <stdio.h>
#include <stdlib.h>
#include <xf86drmMode.h>

struct kms
{
	drmModeConnector connector;
	drmModeEncoder encoder;
	drmModeModeInfo mode;
	unsigned int fb_id;
};

extern BOOL init_kms(int fd, struct kms* kms);
