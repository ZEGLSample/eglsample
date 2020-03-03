#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <gbm.h>

int main(int argc, char *argv[]){
	int fd;
	struct gbm_device *gdevice = NULL;
	struct gbm_surface *gsurface = NULL;
	struct gbm_bo *gbo = NULL;
	unsigned long long modifiers = 0;
	int stride=0, primeFd=0, planeCnt=0;
	int i = 0, j = 0;
	struct gbm_bo *gboImportFd = NULL;
	struct gbm_bo *gboImportFdModifier = NULL;
	struct gbm_import_fd_data fdData = {0};
	struct gbm_import_fd_modifier_data fdModData = {0};
	char *pRawData = NULL;
	int *pData = NULL;
	unsigned int mapStride = 0;
	void *mapData = NULL;

	fd = open("/dev/dri/card0", O_RDWR);
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

	gbm_device_is_format_supported(gdevice, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);

	gsurface = gbm_surface_create(gdevice, 300, 300, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);

	gbo = gbm_bo_create(gdevice, 300, 300, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
	if(gbo){
		stride = gbm_bo_get_stride(gbo);
		primeFd = gbm_bo_get_fd(gbo);
		modifiers = gbm_bo_get_modifier(gbo);
		planeCnt = gbm_bo_get_plane_count(gbo);

		for(i=0; i<planeCnt; i++){
			gbm_bo_get_handle_for_plane(gbo, i);
		}

		fdData.fd = primeFd;
		fdData.width = 300;
		fdData.height = 300;
		fdData.stride = stride;
		fdData.format = GBM_BO_FORMAT_XRGB8888;
		gboImportFd = gbm_bo_import(gdevice, GBM_BO_IMPORT_FD, &fdData, GBM_BO_USE_RENDERING);
		if(gboImportFd)
			gbm_bo_destroy(gboImportFd);

		fdModData.width = 300;
		fdModData.height = 300;
		fdModData.format = GBM_BO_FORMAT_XRGB8888;
		fdModData.num_fds = 1;
		fdModData.fds[0] = primeFd;
		fdModData.strides[0] = stride;
		fdModData.offsets[0] = 0;
		fdModData.modifier = modifiers;
		gboImportFdModifier = gbm_bo_import(gdevice, GBM_BO_IMPORT_FD_MODIFIER, &fdModData, GBM_BO_USE_RENDERING);
		if(gboImportFdModifier)
			gbm_bo_destroy(gboImportFdModifier);

		pRawData = (char*)gbm_bo_map(gbo, 0, 0, 300, 300, GBM_BO_TRANSFER_READ_WRITE, &mapStride, &mapData);
		if(pRawData){
			pData = (int *)pRawData;
			for(i=0; i<300; i++){
				for(j=0; j<300; j++){
					*pData++ = 0x1234abcd;
				}
				pData = (int*)(pRawData + mapStride*(i+1));
			}
			gbm_bo_unmap(gbo, mapData);
		}

		gbm_bo_destroy(gbo);

		//gbo = gbm_bo_create_with_modifiers(gdevice, 400, 400, GBM_BO_FORMAT_XRGB8888, modifiers, 1);
	}

	if(gsurface)
		gbm_surface_destroy(gsurface);

	if(gdevice)
		gbm_device_destroy(gdevice);

	close(fd);

	return 0;
}
