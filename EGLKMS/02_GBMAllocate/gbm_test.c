#include <stdio.h>
#include <stdlib.h>
#include <gbm.h>

int main(int argc, const char* argv[]){
	int fd;
	struct gbm_device *gdevice = NULL;
	struct gbm_surface *gsurface = NULL;
	struct gbm_bo *gbo = NULL;

	fd = open("/dev/dri/renderD128", O_RDWR);
	if(fd < 0){
		printf("Can't open fd!\n");
		return -1;
	}

	gdevice = gbm_create_device(fd);
	if(!gdevice)
	{
		printf("Create gbm device failed!\n");
		return -1;
	}

	gsurface = gbm_surface_create(gdevice, 300, 300, GBM_BO_FORMAT_ARGB8888, GBM_BO_USE_RENDRING);

	if(gsurface)
		gbm_surface_destroy(gsurface);

	if(gdevice)
		gbm_device_destroy(gdevice);

	close(fd);

	return 0;
}
