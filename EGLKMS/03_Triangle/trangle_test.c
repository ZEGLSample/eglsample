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
static struct gbm_bo *gPrevBo = NULL;
static EGLDisplay eglDisplay = NULL;
static EGLContext eglContext = NULL;
static EGLSurface eglSurface = NULL;
static unsigned int prevFb = 0;

static const EGLint attribs[] = {
	EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
	EGL_RED_SIZE, 8,
	EGL_GREEN_SIZE, 8,
	EGL_BLUE_SIZE, 8,
	EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
	EGL_NONE
};

const char *vertexShaderSource = "#version 450\n"
	"layout (location=0) in vec3 aPos;\n"
	"void main()\n"
	"{\n"
	"	gl_Position = vec4(aPos.x, aPos.y, aPos.z, 1.0);\n"
	"}\0";

const char *fragmentShaderSource = "#version 450\n"
	"out vec4 FragColor;\n"
	"void main()\n"
	"{\n"
	" 	FragColor = vec4(1.0f, 0.5f, 0.2f, 1.0f);\n"
	"}\n\0";

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

void render_trangles(int width, int height){
	int vertexShader, fragmentShader;
	int shaderProgram;
	int success;
	char infoLog[512] = {0};
	float vertices[] = {
		-0.5f, -0.5f, 0.0f, //left
		 0.5f, -0.5f, 0.0f, //right
		 0.0f,  0.5f, 0.0f, //top
	}
	unsigned int VBO, VAO;

	glViewport(0, 0, (GLint)width, (GLint)height);

	vertexShader = glCreateShader(GL_VERTEX_SHADER);
	glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
	glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success)
	if(!success){
		glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
		printf("Compile vertex shader failed: %s\n", infoLog);
	}

	fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
	glCompileShader(fragmentShader);
	glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success)
	if(!success){
		glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
		printf("Compile fragment shader failed: %s\n", infoLog);
	}

	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success)
	if(!success){
		glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
		printf("Link shader programe failed: %s\n", infoLog);
	}

	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glClearColor(0.2f, 0.3f, 0.4f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(shaderProgram);
	glBindVertexArray(VAO);
	glDrawArrays(GL_TRIANGLES, 0, 3);

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

	render_trangles(kms.mode.hdisplay, kms.mode.vdisplay);

	swap_buffers();

	clean_kms();

	finish_gbm_egl();

	close(fd);

	return 0;
}
