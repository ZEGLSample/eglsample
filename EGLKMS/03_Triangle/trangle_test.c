#include <fcntl.h>
#include <unistd.h>
#include <gbm.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>
#include "init_kms.h"

static int fd = 0;
static struct kms kms = {0};
static struct gbm_device *gDevice = NULL;
static struct gbm_surface *gSurface = NULL;
static struct gbm_bo *gBo = NULL;
static EGLDisplay eglDisplay = NULL;
static EGLContext eglContext = NULL;
static EGLSurface eglSurface = NULL;

static const EGLint attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE
};

int setup_gbm_egl(int fd){
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

}

void render_trangles(void){

}

int main(int argc, const char *argv[]){
	fd = open("/dev/dri/renderD128", O_RDWR);
	if(fd < 0){
		printf("Can't open fd\n");
		return -1;
	}

	if(!init_kms(fd, &kms)){
		printf("Init kms failed\n");
		return -1;
	}

	if(setup_gbm_egl(fd)){
		printf("Init gbm and egl failed\n");
		return -1;
	}

	render_trangles();

	finish_gbm_egl();

	close(fd);

	return 0;
}
