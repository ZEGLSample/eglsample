#include <fcntl.h>
#include <unistd.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include <drm_fourcc.h>
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

	eglInitialize(eglDisplay, NULL, NULL);

	eglBindAPI(EGL_OPENGL_API);

	eglChooseConfig(eglDisplay, attribs, &config, 1, &num_config);

	eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, NULL);
	if(!eglContext){
		printf("EGL create context failed\n");
		return -1;
	}

	gSurface = gbm_surface_create(gDevice, 300, 300, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_SCANOUT|GBM_BO_USE_RENDERING);
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

void render_egl_image_dma_buf(void){
	unsigned int texture;
	int stride=0, primeFd=0, drmFmt=0;
	int *pData=NULL, *pRawData=NULL;
	int mapStride = 0;
	void *mapData = NULL;
	int i=0, j=0;
	EGLImageKHR eglImage = NULL;

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

	gBoTest = gbm_bo_create(gDevice, 300, 300, GBM_BO_FORMAT_XRGB8888, GBM_BO_USE_RENDERING);
	if(!gBoTest){
		printf("Create gbo for test failed\n");
		return;
	}

	stride = gbm_bo_get_stride(gBoTest);
	primeFd = gbm_bo_get_fd(gBoTest);
	drmFmt = DRM_FORMAT_XRGB8888;

	pRawData = gbm_bo_map(gBoTest, 0, 0, 300, 300, GBM_BO_TRANSFER_READ_WRITE, &mapStride, &mapData);
	if(pRawData){
		pData = pRawData;
		for(i=0; i<300; i++){
			for(j=0; j<300; j++){
				*pData++ = 0x88888888;
			}
			pData = (int*)((char*)pRawData + mapStride*(i+1));
		}

		gbm_bo_unmap(gBoTest, mapData);
	}

	glViewport(0, 0, 300, 300);

	glGenTextures(1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParamateri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParamateri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	int eglAttribs[] = {
		EGL_LINUX_DRM_FOURCC_EXT, drmFmt,
		EGL_WIDTH, 300,
		EGL_HEIGHT, 300,
		EGL_DMA_BUF_PLANE0_FD_EXT, primeFd,
		EGL_DMA_BUF_PLANE0_OFFSET_EXT, 0,
		EGL_DMA_BUF_PLANE0_PITCH_EXT, stride,
		EGL_NONE
	};
	eglImage = eglCreateImageKHR(eglDisplay, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, eglAttribs);
	if(!eglImage){
		printf("Create egl image failed\n");
		if(gBoTest){
			gbm_bo_destroy(gBoTest);
			gBoTest = NULL;
		}
		return;
	}

	glBindTexture(GL_TEXTURE_2D, texture);
	glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);

	eglDestroyImageKHR(eglDisplay, eglImage);
	gbm_bo_destroy(gBoTest);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glVertexPointer(3, GL_FLOAT, 0, vertices);
	glTexCoordPointer(2, GL_FLOAT, 0, texCoords);

	glDrawArrays(GL_QUAD, 0, 4);

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
		drmModeRmFB(prevFb);
		gbm_surface_release_buffer(gSurface, gPrevBo);
	}
}

int main(int argc, char *argv[]){
	fd = open("/dev/dri/renderD128", O_RDWR);
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

	render_egl_image_dma_buf();

	swap_buffers();

	clean_kms();

	finish_gbm_egl();

	close(fd);

	return 0;
}
