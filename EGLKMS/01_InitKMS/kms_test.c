#include <fcntl.h>
#include <unistd.h>
#include "init_kms.h"

int main(int argc, char *argv[])
{
	int fd;
	struct kms kms = {0};

	fd = open("/dev/dri/card0", O_RDWR);
	if(fd < 0)
	{
		printf("Can't open fd\n");
		return -1;
	}

	if(init_kms(fd, &kms) == 0){
		printf("Init kms failed\n");
		return -1;
	}
	
	if(kms.crtc)
		drmModeFreeCrtc(kms.crtc);		
	if(kms.encoder)
		drmModeFreeEncoder(kms.encoder);
	if(kms.connector)
		drmModeFreeConnector(kms.connector);
	if(kms.res)
		drmModeFreeResources(kms.res);

	close(fd);

	return 0;
}
