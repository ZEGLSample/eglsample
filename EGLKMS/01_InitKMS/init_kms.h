#include <xf86drmMode.h>

struct kms
{
	drmModeConnector connector;
	drmModeEncoder encoder;
	drmModeModeInfo mode;
	unsigned int fb_id;
};
