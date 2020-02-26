#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <drm_fourcc.h>
#include <string.h>
#include "init_kms.h"

static int fd = 0;
static struct kms kms = {0};
static struct gbm_device *gDevice = NULL;
static struct gbm_surface *gSurface = NULL;
static struct gbm_bo *gBo = NULL;
static struct gbm_bo *gPrevBo = NULL;
static EGLDisplay eglDisplay = NULL;
static EGLContext eglContext = NULL;
static EGLSurface eglSurface = NULL;
static unsigned int prevFb = 0;
static struct gbm_bo *gBoTest = NULL;

typedef EGLImageKHR (*CreateImageKHR)(EGLDisplay, EGLContext, EGLenum, EGLClientBuffer, const EGLint *);
typedef EGLBoolean (*DestroyImageKHR)(EGLDisplay, EGLImageKHR);
CreateImageKHR pfnCreateImageKHR = NULL;
DestroyImageKHR pfnDestroyImageKHR = NULL;

static const EGLint attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE
};

int setup_gbm_egl(void){
	EGLConfig config = NULL;
	EGLint num_config = 0;

	gDevice = gbm_create_device(fd);
	if(!gDevice){
		printf("Create gbm device failed\n");
		return -1;
	}

	eglDisplay = eglGetDisplay(gDevice);
	if(!eglDisplay){
		printf("EGL get display failed\n");
		return -1;
	}

	pfnCreateImageKHR = (void*)eglGetProcAddress("eglCreateImageKHR");
	pfnDestroyImageKHR = (void*)eglGetProcAddress("eglDestroyImageKHR");
	if(!pfnCreateImageKHR || !pfnDestroyImageKHR){
		printf("eglGetProcAddress CreateImageKHR or DestroyImageKHR failed\n");
		return -1;
	}

	eglInitialize(eglDisplay, NULL, NULL);

	eglBindAPI(EGL_OPENGL_API);

	eglChooseConfig(eglDisplay, attribs, &config, 1, &num_config);

	eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, NULL);
	if(!eglContext){
		printf("EGL create context failed\n");
		return -1;
	}

	gSurface = gbm_surface_create(gDevice, kms.mode.hdisplay, kms.mode.vdisplay, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
	if(!gSurface){
		printf("GBM create surface failed\n");
		return -1;
	}

	eglSurface = eglCreateWindowSurface(eglDisplay, config, gSurface, NULL);
	if(!eglSurface){
		printf("EGL create surface failed\n");
		return -1;
	}

	eglMakeCurrent(eglDisplay, eglSurface, eglSurface, eglContext);

	return 0;
}

void finish_gbm_egl(void){
	if(eglSurface)
		eglDestroySurface(eglDisplay, eglSurface);

	if(gSurface)
		gbm_surface_destroy(gSurface);

	if(eglContext)
		eglDestroyContext(eglDisplay, eglContext);

	eglTerminate(eglDisplay);

	if(gDevice)
		gbm_device_destroy(gDevice);
}

void render_egl_image_nv12(char *path, int width, int height){
	unsigned int textures[2] = {0};
	int drmFmts[2] = {0};
	int eglImgWidths[2] = {0};
	int eglImgHeights[2] = {0};
	int primeFd = 0;
	int eglImgOffsets[2] = {0};
	int eglImgPitches[2] = {0};
	EGLImageKHR eglImages[2] = {0};
	int nv12PlaneCnt = 2;

	unsigned char *pData=NULL, *pRawData=NULL;
	int mapStride = 0;
	void *mapData = NULL;
	int i=0, j=0;
	int texFd = 0;
	unsigned char *texData = NULL;
	int texFileSize = 0;
	int texHeight = height * 3 / 2;

	float vertices[] = {
		 0.5f,  0.5f, 0.0f, //top right
		 0.5f, -0.5f, 0.0f, //bottom right
		-0.5f, -0.5f, 0.0f, //bottom left
		-0.5f,  0.5f, 0.0f  //top left
	};

	float texCoords[] = {
		1.0f, 1.0f, //top right
		1.0f, 0.0f, //bottom right
		0.0f, 0.0f, //bottom left
		0.0f, 1.0f  //top left
	};

	texFd = open(path, O_RDONLY);
	if(texFd == -1){
		printf("Open NV12 texture file failed, path is %s\n", path);
		return;
	}

	texFileSize = width * texHeight;
	texData = (unsigned char*)mmap(0, texFileSize, PROT_READ, MAP_SHARED, texFd, 0);
	if(texData == (unsigned char*)-1){
		printf("Map NV12 texture file failed\n");
		if(texFd)
			close(texFd);
		return;
	}

	//gBoTest = gbm_bo_create(gDevice, texWidth, texHeight, GBM_FORMAT_NV12, GBM_BO_USE_RENDERING);
	gBoTest = gbm_bo_create(gDevice, width, texHeight, GBM_FORMAT_R8, GBM_BO_USE_RENDERING);
	if(!gBoTest){
		printf("Create gbo for test failed\n");
		munmap(texData, texFileSize);
		close(texFd);
		return;
	}

	/* Copy texture data from NV12 texture file to gbo */
	pRawData = gbm_bo_map(gBoTest, 0, 0, width, texHeight, GBM_BO_TRANSFER_READ_WRITE, &mapStride, &mapData);
	if(pRawData){
		pData = pRawData;
		for(i=0; i<texHeight; i++){
			memcpy(pData, texData, width);
			pData = pRawData + mapStride*(i+1);
			texData += width;
		}
		gbm_bo_unmap(gBoTest, mapData);
		munmap(texData, texFileSize);
		close(texFd);
	}

	glViewport(0, 0, (GLint)kms.mode.hdisplay, (GLint)kms.mode.vdisplay);

	glGenTextures(2, &textures[0]);

	glBindTexture(GL_TEXTURE_2D, textures[0]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBindTexture(GL_TEXTURE_2D, textures[1]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	/*todo: need verify UV channel params */
	drmFmts[0] = DRM_FORMAT_R8;
	drmFmts[1] = DRM_FORMAT_GR88;
	eglImgWidths[0] = width;
	eglImgWidths[1] = width / 2;
	eglImgHeights[0] = height;
	eglImgHeights[1] = height / 2;
 	primeFd = gbm_bo_get_fd(gBoTest);
 	eglImgOffsets[0] = 0;
 	eglImgOffsets[1] = width * height;
 	eglImgPitches[0] = width;
 	eglImgPitches[1] = width;

	for(i=0; i<nv12PlaneCnt; i++){
		int eglAttribs[] = {
			EGL_LINUX_DRM_FOURCC_EXT, drmFmts[i],
			EGL_WIDTH, eglImgWidths[i],
			EGL_HEIGHT, eglImgHeights[i],
			EGL_DMA_BUF_PLANE0_FD_EXT, primeFd,
			EGL_DMA_BUF_PLANE0_OFFSET_EXT, eglImgOffsets[i],
			EGL_DMA_BUF_PLANE0_PITCH_EXT, eglImgPitches[i],
			EGL_NONE
		};

		eglImages[i] = (*pfnCreateImageKHR)(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, eglAttribs);
		if(!eglImages[i]){
			printf("Create egl image failed\n");
			if(gBoTest){
				gbm_bo_destroy(gBoTest);
				gBoTest = NULL;
			}
			return;
		}

		glBindTexture(GL_TEXTURE_2D, textures[i]);
		glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImages[i]);

		(*pfnDestroyImageKHR)(eglDisplay, eglImages[i]);
	}
	gbm_bo_destroy(gBoTest);
	
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

	glDrawArrays(GL_QUADS, 0, 4);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glFinish();
}

void swap_buffers(void){
	unsigned int handle, pitch, fb;

	eglSwapBuffers(eglDisplay, eglSurface);

	gBo = gbm_surface_lock_front_buffer(gSurface);
	handle = gbm_bo_get_handle(gBo).u32;
	pitch = gbm_bo_get_stride(gBo);

	drmModeAddFB(fd, kms.mode.hdisplay, kms.mode.vdisplay, 24, 32, pitch, handle, &fb);
	drmModeSetCrtc(fd, kms.crtc->crtc_id, fb, 0, 0, &kms.connector->connector_id, 1, &kms.mode);

	if(gPrevBo){
		drmModeRmFB(fd, prevFb);
		gbm_surface_release_buffer(gSurface, gPrevBo);
	}

	gPrevBo = gBo;
	prevFb = fb;
}

void clean_kms(void){
	drmModeSetCrtc(fd, kms.crtc->crtc_id, kms.crtc->buffer_id, kms.crtc->x, kms.crtc->y, &kms.connector->connector_id, 1, &kms.crtc->mode);
	drmModeFreeCrtc(kms.crtc);

	drmModeFreeEncoder(kms.encoder);
	drmModeFreeConnector(kms.connector);
	drmModeFreeResources(kms.res);

	if(gPrevBo){
		drmModeRmFB(fd, prevFb);
		gbm_surface_release_buffer(gSurface, gPrevBo);
	}
}

int main(int argc, char *argv[]){
	int nv12TexWidth=0, nv12TexHeight=0;
	char *nv12TexFilePath = NULL;

	if(argc != 4){
		printf("Binary wrong use. Please follow binary + nv21 texture path + width + height\n");
		return -1;
	}

	nv12TexFilePath = argv[1];
	nv12TexWidth = atoi(argv[2]);
	nv12TexHeight = atoi(argv[3]);

	fd = open("/dev/dri/card0", O_RDWR);
	if(fd < 0){
		printf("Can't open fd\n");
		return -1;
	}

	if(!init_kms(fd, &kms)){
		printf("Init kms failed\n");
		return -1;
	}

	if(setup_gbm_egl()){
		printf("Init gbm and egl failed\n");
		return -1;
	}

	render_egl_image_nv12(nv12TexFilePath, nv12TexWidth, nv12TexHeight);

	swap_buffers();

	clean_kms();

	finish_gbm_egl();

	close(fd);

	return 0;
}
