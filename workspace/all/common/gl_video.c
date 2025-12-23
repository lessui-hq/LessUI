/**
 * gl_video.c - OpenGL ES rendering backend implementation
 *
 * Provides hardware-accelerated rendering support for libretro cores
 * that require OpenGL ES. Only compiled on platforms with HAS_OPENGLES.
 */

#include "gl_video.h"

#if HAS_OPENGLES

#include "libretro.h"
#include "api.h"
#include "log.h"
#include "scaler.h" // For scaling constants/types if needed
#include <SDL.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

///////////////////////////////
// Types
///////////////////////////////

/**
 * GLVideoState - Hardware render state and resources
 */
typedef struct GLVideoState {
	// State flags
	bool enabled; // HW rendering is active for current core
	bool context_ready; // GL context is created and ready

	// Core's callback structure (copy of what core provided)
	struct retro_hw_render_callback hw_callback;

	// Actual context version created (may differ from requested if fallback occurred)
	unsigned int context_major;
	unsigned int context_minor;

	// SDL GL context (cast from SDL_GLContext)
	void* gl_context;

	// FBO resources
	unsigned int fbo; // Framebuffer object ID
	unsigned int fbo_texture; // Color attachment texture
	unsigned int fbo_depth_rb; // Depth/stencil renderbuffer (0 if not used)

	// FBO dimensions
	unsigned int fbo_width;
	unsigned int fbo_height;

	// Last rendered frame dimensions (for capture)
	unsigned int last_frame_width;
	unsigned int last_frame_height;

	// Presentation resources
	unsigned int present_program; // Shader program for FBO->screen blit

	// UI surface texture (for menu rendering via GL)
	unsigned int ui_texture;
	unsigned int ui_texture_width;
	unsigned int ui_texture_height;

	// HUD overlay texture (for debug HUD rendering via GL with alpha blending)
	unsigned int hud_texture;
	unsigned int hud_texture_width;
	unsigned int hud_texture_height;

	// Cached shader locations (to avoid glGet* calls per frame)
	int loc_mvp; // u_mvp uniform (4x4 MVP matrix)
	int loc_texture; // u_texture uniform
	int loc_position; // a_position attribute
	int loc_texcoord; // a_texcoord attribute

	// Software rendering resources (triple buffered)
	unsigned int sw_textures[3];
	unsigned int sw_tex_index; // Current index being written to
	unsigned int sw_disp_index; // Index to display
	unsigned int sw_width;
	unsigned int sw_height;
} GLVideoState;

// OpenGL ES 2.0 types and constants (minimal subset needed)
// Using SDL_GL_GetProcAddress for functions, define constants ourselves
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef float GLfloat;
typedef char GLchar;
typedef unsigned int GLbitfield;

// GL constants we actually use
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_FLOAT 0x1406
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER 0x8D40
#define GL_RENDERBUFFER 0x8D41
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_STENCIL_ATTACHMENT 0x8D20
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH24_STENCIL8_OES 0x88F0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_TRIANGLE_STRIP 0x0005
#define GL_VERTEX_SHADER 0x8B31
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_DEPTH_TEST 0x0B71
#define GL_STENCIL_TEST 0x0B90
#define GL_BLEND 0x0BE2
#define GL_CULL_FACE 0x0B44
#define GL_SCISSOR_TEST 0x0C11
#define GL_ARRAY_BUFFER 0x8892
#define GL_SRC_ALPHA 0x0302
#define GL_NO_ERROR 0
#define GL_ONE_MINUS_SRC_ALPHA 0x0303

// GL function pointers (loaded via SDL_GL_GetProcAddress)
static void (*glGenFramebuffers)(GLsizei, GLuint*);
static void (*glBindFramebuffer)(GLenum, GLuint);
static void (*glGenTextures)(GLsizei, GLuint*);
static void (*glBindTexture)(GLenum, GLuint);
static void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
                            const void*);
static void (*glTexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum,
                               const void*);
static void (*glTexParameteri)(GLenum, GLenum, GLint);
static void (*glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
static void (*glGenRenderbuffers)(GLsizei, GLuint*);
static void (*glBindRenderbuffer)(GLenum, GLuint);
static void (*glRenderbufferStorage)(GLenum, GLenum, GLsizei, GLsizei);
static void (*glFramebufferRenderbuffer)(GLenum, GLenum, GLenum, GLuint);
static GLenum (*glCheckFramebufferStatus)(GLenum);
static void (*glDeleteFramebuffers)(GLsizei, const GLuint*);
static void (*glDeleteTextures)(GLsizei, const GLuint*);
static void (*glDeleteRenderbuffers)(GLsizei, const GLuint*);
static GLuint (*glCreateShader)(GLenum);
static void (*glShaderSource)(GLuint, GLsizei, const GLchar**, const GLint*);
static void (*glCompileShader)(GLuint);
static void (*glGetShaderiv)(GLuint, GLenum, GLint*);
static void (*glGetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
static void (*glDeleteShader)(GLuint);
static GLuint (*glCreateProgram)(void);
static void (*glAttachShader)(GLuint, GLuint);
static void (*glLinkProgram)(GLuint);
static void (*glGetProgramiv)(GLuint, GLenum, GLint*);
static void (*glGetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*);
static void (*glDeleteProgram)(GLuint);
static void (*glClearColor)(GLfloat, GLfloat, GLfloat, GLfloat);
static void (*glClear)(GLbitfield);
static void (*glUseProgram)(GLuint);
static GLint (*glGetUniformLocation)(GLuint, const GLchar*);
static void (*glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat*);
static void (*glUniform1i)(GLint, GLint);
static void (*glActiveTexture)(GLenum);
static GLint (*glGetAttribLocation)(GLuint, const GLchar*);
static void (*glEnableVertexAttribArray)(GLuint);
static void (*glDisableVertexAttribArray)(GLuint);
static void (*glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
static void (*glDrawArrays)(GLenum, GLint, GLsizei);
static void (*glViewport)(GLint, GLint, GLsizei, GLsizei);
static void (*glDisable)(GLenum);
static void (*glEnable)(GLenum);
static void (*glBlendFunc)(GLenum, GLenum);
static void (*glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
static void (*glBindBuffer)(GLenum, GLuint);
static GLenum (*glGetError)(void);
static void (*glReadPixels)(GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, void*);

///////////////////////////////
// Module State
///////////////////////////////

static GLVideoState gl_state = {0};
static int gl_error_total = 0; // Track GL errors to avoid log spam

/**
 * Load GL function pointers via SDL_GL_GetProcAddress.
 *
 * @return true if all required functions loaded successfully
 */
static bool loadGLFunctions(void) {
#define LOAD_GL_FUNC(name)                                                                         \
	do {                                                                                           \
		name = SDL_GL_GetProcAddress(#name);                                                       \
		if (!name) {                                                                               \
			LOG_error("GL video: failed to load GL function: %s", #name);                          \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

	    LOAD_GL_FUNC(glGenFramebuffers);
	LOAD_GL_FUNC(glBindFramebuffer);
	LOAD_GL_FUNC(glGenTextures);
	LOAD_GL_FUNC(glBindTexture);
	LOAD_GL_FUNC(glTexImage2D);
	LOAD_GL_FUNC(glTexSubImage2D);
	LOAD_GL_FUNC(glTexParameteri);
	LOAD_GL_FUNC(glFramebufferTexture2D);
	LOAD_GL_FUNC(glGenRenderbuffers);
	LOAD_GL_FUNC(glBindRenderbuffer);
	LOAD_GL_FUNC(glRenderbufferStorage);
	LOAD_GL_FUNC(glFramebufferRenderbuffer);
	LOAD_GL_FUNC(glCheckFramebufferStatus);
	LOAD_GL_FUNC(glDeleteFramebuffers);
	LOAD_GL_FUNC(glDeleteTextures);
	LOAD_GL_FUNC(glDeleteRenderbuffers);
	LOAD_GL_FUNC(glCreateShader);
	LOAD_GL_FUNC(glShaderSource);
	LOAD_GL_FUNC(glCompileShader);
	LOAD_GL_FUNC(glGetShaderiv);
	LOAD_GL_FUNC(glGetShaderInfoLog);
	LOAD_GL_FUNC(glDeleteShader);
	LOAD_GL_FUNC(glCreateProgram);
	LOAD_GL_FUNC(glAttachShader);
	LOAD_GL_FUNC(glLinkProgram);
	LOAD_GL_FUNC(glGetProgramiv);
	LOAD_GL_FUNC(glGetProgramInfoLog);
	LOAD_GL_FUNC(glDeleteProgram);
	LOAD_GL_FUNC(glClearColor);
	LOAD_GL_FUNC(glClear);
	LOAD_GL_FUNC(glUseProgram);
	LOAD_GL_FUNC(glGetUniformLocation);
	LOAD_GL_FUNC(glUniformMatrix4fv);
	LOAD_GL_FUNC(glUniform1i);
	LOAD_GL_FUNC(glActiveTexture);
	LOAD_GL_FUNC(glGetAttribLocation);
	LOAD_GL_FUNC(glEnableVertexAttribArray);
	LOAD_GL_FUNC(glDisableVertexAttribArray);
	LOAD_GL_FUNC(glVertexAttribPointer);
	LOAD_GL_FUNC(glDrawArrays);
	LOAD_GL_FUNC(glViewport);
	LOAD_GL_FUNC(glDisable);
	LOAD_GL_FUNC(glEnable);
	LOAD_GL_FUNC(glBlendFunc);
	LOAD_GL_FUNC(glColorMask);
	LOAD_GL_FUNC(glBindBuffer);
	LOAD_GL_FUNC(glGetError);
	LOAD_GL_FUNC(glReadPixels);

#undef LOAD_GL_FUNC

	LOG_debug("GL video: all GL functions loaded successfully");
	return true;
}

///////////////////////////////
// Shader Sources (RetroArch-style)
///////////////////////////////

// Vertex shader: MVP matrix transforms vertices, texcoords passed through
// Following RetroArch's stock shader pattern for simplicity and correctness
static const char* vertex_shader_src = "#version 100\n"
                                       "attribute vec2 a_position;\n"
                                       "attribute vec2 a_texcoord;\n"
                                       "uniform mat4 u_mvp;\n"
                                       "varying vec2 v_texcoord;\n"
                                       "void main() {\n"
                                       "    gl_Position = u_mvp * vec4(a_position, 0.0, 1.0);\n"
                                       "    v_texcoord = a_texcoord;\n"
                                       "}\n";

// Fragment shader: sample texture
static const char* fragment_shader_src = "#version 100\n"
                                         "precision mediump float;\n"
                                         "varying vec2 v_texcoord;\n"
                                         "uniform sampler2D u_texture;\n"
                                         "void main() {\n"
                                         "    gl_FragColor = texture2D(u_texture, v_texcoord);\n"
                                         "}\n";

///////////////////////////////
// Matrix Math (RetroArch-style)
///////////////////////////////

// 4x4 matrix stored in column-major order (OpenGL convention)
// mat[col][row] = mat[col * 4 + row]

/**
 * Create orthographic projection matrix.
 * Maps (left,bottom) to (-1,-1) and (right,top) to (1,1).
 */
static void matrix_ortho(float* mat, float left, float right, float bottom, float top) {
	// Zero out matrix
	for (int i = 0; i < 16; i++) {
		mat[i] = 0.0f;
	}

	// Orthographic projection (z-near=-1, z-far=1)
	mat[0] = 2.0f / (right - left);
	mat[5] = 2.0f / (top - bottom);
	mat[10] = -1.0f;
	mat[12] = -(right + left) / (right - left);
	mat[13] = -(top + bottom) / (top - bottom);
	mat[15] = 1.0f;
}

/**
 * Create Z-axis rotation matrix.
 * @param radians Rotation angle in radians (positive = CCW)
 */
static void matrix_rotate_z(float* mat, float radians) {
	float c = cosf(radians);
	float s = sinf(radians);

	// Identity with rotation in XY plane
	for (int i = 0; i < 16; i++) {
		mat[i] = 0.0f;
	}

	mat[0] = c;
	mat[1] = s;
	mat[4] = -s;
	mat[5] = c;
	mat[10] = 1.0f;
	mat[15] = 1.0f;
}

/**
 * Multiply two 4x4 matrices: result = a * b
 * @param result Output matrix (can alias a or b)
 */
static void matrix_multiply(float* result, const float* a, const float* b) {
	float tmp[16];

	for (int col = 0; col < 4; col++) {
		for (int row = 0; row < 4; row++) {
			tmp[col * 4 + row] = a[0 * 4 + row] * b[col * 4 + 0] + a[1 * 4 + row] * b[col * 4 + 1] +
			                     a[2 * 4 + row] * b[col * 4 + 2] + a[3 * 4 + row] * b[col * 4 + 3];
		}
	}

	for (int i = 0; i < 16; i++) {
		result[i] = tmp[i];
	}
}

/**
 * Build MVP matrix for presentation.
 * Combines orthographic projection (0-1 to NDC) with optional rotation.
 *
 * @param mvp Output MVP matrix
 * @param rotation Rotation in 90-degree increments (0, 1, 2, 3)
 */
static void build_mvp_matrix(float* mvp, unsigned rotation) {
	// Start with orthographic projection mapping 0-1 to -1,1
	float ortho[16];
	matrix_ortho(ortho, 0.0f, 1.0f, 0.0f, 1.0f);

	if (rotation == 0) {
		// No rotation - just use ortho
		for (int i = 0; i < 16; i++) {
			mvp[i] = ortho[i];
		}
	} else {
		// Apply rotation: MVP = Rotation * Ortho
		float rot[16];
		float radians = (float)M_PI * (float)(rotation * 90) / 180.0f;
		matrix_rotate_z(rot, radians);
		matrix_multiply(mvp, rot, ortho);
	}
}

///////////////////////////////
// Internal Helpers
///////////////////////////////

/**
 * Compile a shader from source.
 *
 * @param type GL_VERTEX_SHADER or GL_FRAGMENT_SHADER
 * @param source Shader source code
 * @return Shader ID, or 0 on failure
 */
static GLuint compileShader(GLenum type, const char* source) {
	GLuint shader = glCreateShader(type);
	if (!shader) {
		LOG_error("GL video: glCreateShader failed");
		return 0;
	}

	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);

	// Check compilation status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLchar log[512];
		glGetShaderInfoLog(shader, sizeof(log), NULL, log);
		LOG_error("GL video: shader compilation failed: %s", log);
		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

/**
 * Create shader program from vertex and fragment shaders.
 *
 * @return Program ID, or 0 on failure
 */
static GLuint createShaderProgram(void) {
	// Compile shaders
	GLuint vertex_shader = compileShader(GL_VERTEX_SHADER, vertex_shader_src);
	if (!vertex_shader) {
		return 0;
	}

	GLuint fragment_shader = compileShader(GL_FRAGMENT_SHADER, fragment_shader_src);
	if (!fragment_shader) {
		glDeleteShader(vertex_shader);
		return 0;
	}

	// Link program
	GLuint program = glCreateProgram();
	if (!program) {
		LOG_error("GL video: glCreateProgram failed");
		glDeleteShader(vertex_shader);
		glDeleteShader(fragment_shader);
		return 0;
	}

	glAttachShader(program, vertex_shader);
	glAttachShader(program, fragment_shader);
	glLinkProgram(program);

	// Check link status
	GLint linked = 0;
	glGetProgramiv(program, GL_LINK_STATUS, &linked);
	if (!linked) {
		GLchar log[512];
		glGetProgramInfoLog(program, sizeof(log), NULL, log);
		LOG_error("GL video: shader linking failed: %s", log);
		glDeleteProgram(program);
		program = 0;
	}

	// Shaders can be deleted after linking
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}

/**
 * Destroy presentation resources (shader program and UI texture).
 */
static void destroyPresentResources(void) {
	if (gl_state.present_program) {
		glDeleteProgram(gl_state.present_program);
		gl_state.present_program = 0;
	}
	if (gl_state.ui_texture) {
		glDeleteTextures(1, &gl_state.ui_texture);
		gl_state.ui_texture = 0;
		gl_state.ui_texture_width = 0;
		gl_state.ui_texture_height = 0;
	}
	if (gl_state.hud_texture) {
		glDeleteTextures(1, &gl_state.hud_texture);
		gl_state.hud_texture = 0;
		gl_state.hud_texture_width = 0;
		gl_state.hud_texture_height = 0;
	}
}

/**
 * Create FBO with color, depth, and/or stencil attachments.
 *
 * @param width FBO width in pixels
 * @param height FBO height in pixels
 * @param need_depth Whether depth buffer is needed
 * @param need_stencil Whether stencil buffer is needed
 * @return true if FBO created successfully
 */
static bool createFBO(unsigned width, unsigned height, bool need_depth, bool need_stencil) {
	LOG_debug("createFBO: creating %ux%u FBO (depth=%d, stencil=%d)", width, height, need_depth,
	          need_stencil);

	// Generate and bind FBO
	glGenFramebuffers(1, &gl_state.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, gl_state.fbo);
	LOG_debug("createFBO: FBO generated (id=%u)", gl_state.fbo);

	// Create color texture attachment
	LOG_debug("createFBO: creating color texture");
	glGenTextures(1, &gl_state.fbo_texture);
	glBindTexture(GL_TEXTURE_2D, gl_state.fbo_texture);
	LOG_debug("createFBO: texture generated (id=%u), setting up RGBA8888 %ux%u",
	          gl_state.fbo_texture, width, height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	LOG_debug("createFBO: attaching texture to FBO");
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                       gl_state.fbo_texture, 0);

	// Create depth/stencil renderbuffer if requested
	if (need_depth || need_stencil) {
		glGenRenderbuffers(1, &gl_state.fbo_depth_rb);
		glBindRenderbuffer(GL_RENDERBUFFER, gl_state.fbo_depth_rb);

		// Use packed depth24_stencil8 if both are needed
		if (need_depth && need_stencil) {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
			                          gl_state.fbo_depth_rb);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
			                          gl_state.fbo_depth_rb);
		} else if (need_depth) {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
			                          gl_state.fbo_depth_rb);
		} else {
			// Stencil-only is invalid per libretro spec, but handle gracefully
			LOG_warn("GL video: stencil-only requested (invalid), using depth24_stencil8");
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
			                          gl_state.fbo_depth_rb);
		}
	}

	// Check FBO completeness
	LOG_debug("createFBO: checking FBO completeness");
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		LOG_error("GL video: FBO incomplete (status=0x%x)", status);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	LOG_debug("createFBO: FBO is complete");

	// Unbind FBO (core will bind it via get_current_framebuffer)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	LOG_info("GL video: FBO created %ux%u (depth=%d, stencil=%d)", width, height, need_depth,
	         need_stencil);
	return true;
}

/**
 * Destroy FBO and associated attachments.
 */
static void destroyFBO(void) {
	if (gl_state.fbo_depth_rb) {
		glDeleteRenderbuffers(1, &gl_state.fbo_depth_rb);
		gl_state.fbo_depth_rb = 0;
	}

	if (gl_state.fbo_texture) {
		glDeleteTextures(1, &gl_state.fbo_texture);
		gl_state.fbo_texture = 0;
	}

	if (gl_state.fbo) {
		glDeleteFramebuffers(1, &gl_state.fbo);
		gl_state.fbo = 0;
	}
}


static const char* getContextTypeName(enum retro_hw_context_type type) {
	switch (type) {
	case RETRO_HW_CONTEXT_NONE:
		return "NONE";
	case RETRO_HW_CONTEXT_OPENGL:
		return "OpenGL";
	case RETRO_HW_CONTEXT_OPENGLES2:
		return "OpenGL ES 2.0";
	case RETRO_HW_CONTEXT_OPENGL_CORE:
		return "OpenGL Core";
	case RETRO_HW_CONTEXT_OPENGLES3:
		return "OpenGL ES 3.0";
	case RETRO_HW_CONTEXT_OPENGLES_VERSION:
		return "OpenGL ES (versioned)";
	case RETRO_HW_CONTEXT_VULKAN:
		return "Vulkan";
	case RETRO_HW_CONTEXT_D3D11:
		return "Direct3D 11";
	case RETRO_HW_CONTEXT_D3D10:
		return "Direct3D 10";
	case RETRO_HW_CONTEXT_D3D12:
		return "Direct3D 12";
	case RETRO_HW_CONTEXT_D3D9:
		return "Direct3D 9";
	default:
		return "Unknown";
	}
}

///////////////////////////////
// Initialization / Shutdown
///////////////////////////////

/**
 * Helper to determine target GLES version from context type.
 * Follows RetroArch's version mapping:
 * - OPENGLES2 -> 2.0
 * - OPENGLES3 -> 3.0
 * - OPENGLES_VERSION -> use version_major.version_minor
 */
static void getTargetGLESVersion(enum retro_hw_context_type context_type, unsigned req_major,
                                 unsigned req_minor, unsigned* out_major, unsigned* out_minor) {
	switch (context_type) {
	case RETRO_HW_CONTEXT_OPENGLES3:
		*out_major = 3;
		*out_minor = 0;
		break;
	case RETRO_HW_CONTEXT_OPENGLES_VERSION:
		// Use the version specified by the core
		*out_major = req_major;
		*out_minor = req_minor;
		break;
	case RETRO_HW_CONTEXT_OPENGLES2:
	default:
		*out_major = 2;
		*out_minor = 0;
		break;
	}
}

/**
 * Try to create a GL context with fallback to lower versions.
 *
 * Version fallback order:
 * - Try requested version first
 * - If GLES 3.2 fails, try 3.1
 * - If GLES 3.1 fails, try 3.0
 * - If GLES 3.0 fails, try 2.0
 *
 * @param window SDL window to create context for
 * @param requested_major Requested GLES major version
 * @param requested_minor Requested GLES minor version
 * @param debug_context Whether to request a debug context
 * @param actual_major Output: actual major version created
 * @param actual_minor Output: actual minor version created
 * @return SDL_GLContext on success, NULL on failure
 */
static SDL_GLContext createGLContextWithFallback(SDL_Window* window, unsigned requested_major,
                                                 unsigned requested_minor, bool debug_context,
                                                 unsigned* actual_major, unsigned* actual_minor) {
	// Version fallback table: try from requested down to GLES 2.0
	struct {
		unsigned major;
		unsigned minor;
	} versions[] = {
	    {3, 2},
	    {3, 1},
	    {3, 0},
	    {2, 0},
	};
	int num_versions = sizeof(versions) / sizeof(versions[0]);

	// Find starting point in fallback table
	int start_idx = num_versions - 1; // Default to GLES 2.0
	for (int i = 0; i < num_versions; i++) {
		if (versions[i].major == requested_major && versions[i].minor == requested_minor) {
			start_idx = i;
			break;
		}
		// Also handle case where requested is between table entries
		if (versions[i].major < requested_major ||
		    (versions[i].major == requested_major && versions[i].minor < requested_minor)) {
			start_idx = (i > 0) ? i - 1 : 0;
			break;
		}
	}

	SDL_GLContext ctx = NULL;

	// Set debug context flag if requested
	if (debug_context) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		LOG_debug("GL video: debug context requested");
	}

	// Try each version from requested down to 2.0
	for (int i = start_idx; i < num_versions; i++) {
		unsigned major = versions[i].major;
		unsigned minor = versions[i].minor;

		LOG_debug("GL video: trying GLES %u.%u context", major, minor);

		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, (int)major);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (int)minor);

		ctx = SDL_GL_CreateContext(window);
		if (ctx) {
			*actual_major = major;
			*actual_minor = minor;

			if (major != requested_major || minor != requested_minor) {
				LOG_info("GL video: requested GLES %u.%u, got %u.%u (fallback)", requested_major,
				         requested_minor, major, minor);
			}
			return ctx;
		}

		LOG_debug("GL video: GLES %u.%u failed: %s", major, minor, SDL_GetError());
	}

	return NULL;
}

bool GLVideo_init(struct retro_hw_render_callback* callback, unsigned max_width,
                  unsigned max_height) {
	LOG_debug("GLVideo_init: called with max_width=%u, max_height=%u", max_width, max_height);

	if (!callback) {
		LOG_error("GLVideo_init: NULL callback");
		return false;
	}

	LOG_debug("GLVideo_init: context_type=%d (%s), version=%u.%u, depth=%d, stencil=%d",
	          callback->context_type, getContextTypeName(callback->context_type),
	          callback->version_major, callback->version_minor, callback->depth, callback->stencil);
	LOG_debug("GLVideo_init: bottom_left_origin=%d, cache_context=%d, debug_context=%d",
	          callback->bottom_left_origin, callback->cache_context, callback->debug_context);

	// Check if context type is supported
	if (!GLVideo_isContextSupported(callback->context_type)) {
		LOG_info("GL video: unsupported context type %s",
		         getContextTypeName(callback->context_type));
		return false;
	}

	LOG_debug("GLVideo_init: context type supported, proceeding with initialization");

	LOG_info("GL video: initializing %s context (v%u.%u, depth=%d, stencil=%d, max=%ux%u)",
	         getContextTypeName(callback->context_type), callback->version_major,
	         callback->version_minor, callback->depth, callback->stencil, max_width, max_height);

	// Get SDL window from platform
	LOG_debug("GLVideo_init: getting SDL window from platform");
	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		LOG_error("GL video: failed to get SDL window");
		return false;
	}
	LOG_debug("GLVideo_init: got SDL window successfully");

	// Determine target GLES version based on context type (following RetroArch)
	unsigned target_major = 2, target_minor = 0;
	getTargetGLESVersion(callback->context_type, callback->version_major, callback->version_minor,
	                     &target_major, &target_minor);

	LOG_debug("GLVideo_init: target GLES version is %u.%u", target_major, target_minor);

	// Create GL context with fallback to lower versions if needed
	unsigned actual_major = 0, actual_minor = 0;
	gl_state.gl_context = createGLContextWithFallback(
	    window, target_major, target_minor, callback->debug_context, &actual_major, &actual_minor);

	if (!gl_state.gl_context) {
		LOG_error("GL video: failed to create any GL context");
		return false;
	}

	// Store actual context version
	gl_state.context_major = actual_major;
	gl_state.context_minor = actual_minor;

	LOG_info("GL video: OpenGL ES %u.%u context created successfully", actual_major, actual_minor);

	// Make context current
	LOG_debug("GLVideo_init: making GL context current");
	if (SDL_GL_MakeCurrent(window, gl_state.gl_context) < 0) {
		LOG_error("GL video: SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}
	LOG_debug("GLVideo_init: GL context is current");

	// Load GL function pointers
	LOG_debug("GLVideo_init: loading GL function pointers");
	if (!loadGLFunctions()) {
		LOG_error("GL video: failed to load GL functions");
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}
	LOG_debug("GLVideo_init: GL functions loaded");

	// Load additional GL functions for querying capabilities
	const char* (*glGetStringFunc)(GLenum) = (void*)SDL_GL_GetProcAddress("glGetString");
	void (*glGetIntegervFunc)(GLenum, GLint*) = (void*)SDL_GL_GetProcAddress("glGetIntegerv");

	if (glGetStringFunc && glGetIntegervFunc) {
		LOG_info("GL video: GL vendor=%s, renderer=%s, version=%s",
		         glGetStringFunc(0x1F00), // GL_VENDOR
		         glGetStringFunc(0x1F01), // GL_RENDERER
		         glGetStringFunc(0x1F02)); // GL_VERSION

		GLint max_tex_size = 0, max_fbo_size = 0;
		glGetIntegervFunc(0x0D33, &max_tex_size); // GL_MAX_TEXTURE_SIZE
		glGetIntegervFunc(0x84E8, &max_fbo_size); // GL_MAX_RENDERBUFFER_SIZE
		LOG_info("GL video: max_texture_size=%d, max_renderbuffer_size=%d", max_tex_size,
		         max_fbo_size);
	}

	// Create FBO for core to render into
	LOG_debug("GLVideo_init: creating FBO (%ux%u, depth=%d, stencil=%d)", max_width, max_height,
	          callback->depth, callback->stencil);
	if (!createFBO(max_width, max_height, callback->depth, callback->stencil)) {
		LOG_error("GL video: FBO creation failed");
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}

	// Create shader program for presenting FBO to screen
	LOG_debug("GLVideo_init: creating shader program");
	gl_state.present_program = createShaderProgram();
	if (!gl_state.present_program) {
		LOG_error("GL video: shader program creation failed");
		destroyFBO();
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}
	LOG_debug("GLVideo_init: shader program created (id=%u)", gl_state.present_program);

	// Cache uniform and attribute locations (RetroArch-style MVP shader)
	LOG_debug("GLVideo_init: caching shader locations");
	gl_state.loc_mvp = glGetUniformLocation(gl_state.present_program, "u_mvp");
	gl_state.loc_texture = glGetUniformLocation(gl_state.present_program, "u_texture");
	gl_state.loc_position = glGetAttribLocation(gl_state.present_program, "a_position");
	gl_state.loc_texcoord = glGetAttribLocation(gl_state.present_program, "a_texcoord");
	LOG_debug("GLVideo_init: shader locations cached (mvp=%d, tex=%d, pos=%d, tc=%d)",
	          gl_state.loc_mvp, gl_state.loc_texture, gl_state.loc_position, gl_state.loc_texcoord);

	// Provide our callbacks to the core
	LOG_debug("GLVideo_init: setting up core callbacks");
	callback->get_current_framebuffer = GLVideo_getCurrentFramebuffer;
	callback->get_proc_address = GLVideo_getProcAddress;

	// Store callback info (after setting our callbacks so memcpy includes them)
	memcpy(&gl_state.hw_callback, callback, sizeof(struct retro_hw_render_callback));

	gl_state.fbo_width = max_width;
	gl_state.fbo_height = max_height;
	gl_state.enabled = true;
	gl_state.context_ready = true;

	// NOTE: We do NOT call context_reset here. According to libretro spec,
	// context_reset must be called AFTER retro_load_game() returns, not during.
	// The caller should call GLVideo_contextReset() after load_game.

	LOG_info("GL video: initialized successfully (context_reset pending)");
	return true;
}

bool GLVideo_initSoftware(void) {
	if (gl_state.gl_context) {
		return true; // Already initialized
	}

	LOG_info("GL video: initializing software render context");

	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		LOG_error("GL video: failed to get SDL window");
		return false;
	}

	// Create GLES 2.0 context
	unsigned actual_major = 0, actual_minor = 0;
	gl_state.gl_context = createGLContextWithFallback(window, 2, 0, false, &actual_major,
	                                                  &actual_minor);

	if (!gl_state.gl_context) {
		LOG_error("GL video: failed to create GL context");
		return false;
	}

	gl_state.context_major = actual_major;
	gl_state.context_minor = actual_minor;

	if (SDL_GL_MakeCurrent(window, gl_state.gl_context) < 0) {
		LOG_error("GL video: SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}

	if (!loadGLFunctions()) {
		LOG_error("GL video: failed to load GL functions");
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}

	gl_state.present_program = createShaderProgram();
	if (!gl_state.present_program) {
		LOG_error("GL video: shader program creation failed");
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
		return false;
	}

	// Cache locations
	gl_state.loc_mvp = glGetUniformLocation(gl_state.present_program, "u_mvp");
	gl_state.loc_texture = glGetUniformLocation(gl_state.present_program, "u_texture");
	gl_state.loc_position = glGetAttribLocation(gl_state.present_program, "a_position");
	gl_state.loc_texcoord = glGetAttribLocation(gl_state.present_program, "a_texcoord");

	gl_state.context_ready = true;
	gl_state.enabled = false; // Not HW rendering

	LOG_info("GL video: software render context initialized");
	return true;
}

void GLVideo_shutdown(void) {
	if (!gl_state.enabled) {
		return;
	}

	LOG_info("GL video: shutting down");

	// Call core's context_destroy if provided
	if (gl_state.hw_callback.context_destroy) {
		LOG_debug("GL video: calling core context_destroy");
		gl_state.hw_callback.context_destroy();
	}

	// Destroy presentation resources
	destroyPresentResources();

	// Destroy software textures
	if (gl_state.sw_textures[0]) {
		glDeleteTextures(3, gl_state.sw_textures);
		memset(gl_state.sw_textures, 0, sizeof(gl_state.sw_textures));
	}

	// Destroy FBO resources
	destroyFBO();

	// Destroy GL context
	if (gl_state.gl_context) {
		SDL_GL_DeleteContext(gl_state.gl_context);
		gl_state.gl_context = NULL;
	}

	memset(&gl_state, 0, sizeof(gl_state));
	gl_error_total = 0;
}

///////////////////////////////
// State Queries
///////////////////////////////

bool GLVideo_isEnabled(void) {
	return gl_state.enabled && gl_state.context_ready;
}

bool GLVideo_isContextSupported(int context_type) {
	// We support OpenGL ES 2.0 and 3.x
	switch (context_type) {
	case RETRO_HW_CONTEXT_OPENGLES2:
		return true;

	case RETRO_HW_CONTEXT_OPENGLES3:
	case RETRO_HW_CONTEXT_OPENGLES_VERSION:
		// GLES3 is supported - actual version negotiation happens in init
		return true;

	// Desktop GL and other APIs not supported on these devices
	case RETRO_HW_CONTEXT_OPENGL:
	case RETRO_HW_CONTEXT_OPENGL_CORE:
	case RETRO_HW_CONTEXT_VULKAN:
	case RETRO_HW_CONTEXT_D3D9:
	case RETRO_HW_CONTEXT_D3D10:
	case RETRO_HW_CONTEXT_D3D11:
	case RETRO_HW_CONTEXT_D3D12:
	case RETRO_HW_CONTEXT_NONE:
	default:
		return false;
	}
}

bool GLVideo_isVersionSupported(unsigned major, unsigned minor) {
	// Get SDL window from platform for probing
	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		LOG_warn("GL video: cannot probe version support - no window");
		return false;
	}

	// Try to create a context with the requested version
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, (int)major);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, (int)minor);

	SDL_GLContext probe_ctx = SDL_GL_CreateContext(window);
	if (probe_ctx) {
		SDL_GL_DeleteContext(probe_ctx);
		LOG_debug("GL video: GLES %u.%u is supported", major, minor);
		return true;
	}

	LOG_debug("GL video: GLES %u.%u not supported: %s", major, minor, SDL_GetError());
	return false;
}

void GLVideo_getContextVersion(unsigned* major, unsigned* minor) {
	if (major)
		*major = gl_state.context_major;
	if (minor)
		*minor = gl_state.context_minor;
}

///////////////////////////////
// Core Callbacks
///////////////////////////////

uintptr_t GLVideo_getCurrentFramebuffer(void) {
	// Return FBO handle for core to render into
	LOG_debug("GL video: getCurrentFramebuffer called, returning FBO %u", gl_state.fbo);
	return (uintptr_t)gl_state.fbo;
}

retro_proc_address_t GLVideo_getProcAddress(const char* sym) {
	if (!sym) {
		return NULL;
	}

	retro_proc_address_t proc = (retro_proc_address_t)SDL_GL_GetProcAddress(sym);
	if (!proc) {
		LOG_warn("GL video: getProcAddress FAILED for '%s'", sym);
	} else {
		LOG_debug("GL video: getProcAddress('%s') = %p", sym, (void*)proc);
	}
	return proc;
}

///////////////////////////////
// Frame Operations
///////////////////////////////

void GLVideo_drawFrame(unsigned int texture_id, unsigned int tex_w, unsigned int tex_h,
                       const SDL_Rect* src_rect, const SDL_Rect* dst_rect, unsigned rotation,
                       int sharpness, bool bottom_left_origin) {
	if (!gl_state.gl_context) {
		return;
	}

	GLVideo_makeCurrent();

	// Clear entire screen first (glClear only affects current viewport)
	// Only if we are not covering the whole screen?
	// Actually, clearing is good practice to avoid garbage.
	// But if we call this multiple times (overlays?), we shouldn't clear.
	// `GLVideo_present` clears. `SDL2_present` clears (SDL_RenderClear).
	// So `drawFrame` should probably NOT clear, or take a flag.
	// Or the caller clears.
	// `GLVideo_present` clears.
	// `SDL2_present` clears.
	// So `drawFrame` should just DRAW.

	// Set viewport
	glViewport(dst_rect->x, dst_rect->y, dst_rect->w, dst_rect->h);

	// Use shader
	glUseProgram(gl_state.present_program);

	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture_id);

	// Filter
	GLint filter = (sharpness == 0) ? GL_NEAREST : GL_LINEAR;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

	glUniform1i(gl_state.loc_texture, 0);

	// MVP Matrix (handling rotation)
	float mvp[16];
	build_mvp_matrix(mvp, rotation);
	glUniformMatrix4fv(gl_state.loc_mvp, 1, GL_FALSE, mvp);

	// Texture Coordinates
	float tex_x_start = (float)src_rect->x / (float)tex_w;
	float tex_y_start = (float)src_rect->y / (float)tex_h;
	float tex_x_end = (float)(src_rect->x + src_rect->w) / (float)tex_w;
	float tex_y_end = (float)(src_rect->y + src_rect->h) / (float)tex_h;

	float texco[8];
	if (bottom_left_origin) {
		// OpenGL convention (bottom-left)
		texco[0] = tex_x_start;
		texco[1] = tex_y_start; // bottom-left
		texco[2] = tex_x_end;
		texco[3] = tex_y_start; // bottom-right
		texco[4] = tex_x_start;
		texco[5] = tex_y_end; // top-left
		texco[6] = tex_x_end;
		texco[7] = tex_y_end; // top-right
	} else {
		// Top-left origin (Y-flip needed)
		texco[0] = tex_x_start;
		texco[1] = tex_y_end; // top-left (V=1)
		texco[2] = tex_x_end;
		texco[3] = tex_y_end; // top-right (V=1)
		texco[4] = tex_x_start;
		texco[5] = tex_y_start; // bottom-left (V=0)
		texco[6] = tex_x_end;
		texco[7] = tex_y_start; // bottom-right (V=0)
	}

	static const float verts[8] = {0, 0, 1, 0, 0, 1, 1, 1};

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(gl_state.loc_position);
	glEnableVertexAttribArray(gl_state.loc_texcoord);
	glVertexAttribPointer(gl_state.loc_position, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(gl_state.loc_texcoord, 2, GL_FLOAT, GL_FALSE, 0, texco);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(gl_state.loc_position);
	glDisableVertexAttribArray(gl_state.loc_texcoord);
}

void GLVideo_drawSoftwareFrame(const SDL_Rect* src_rect, const SDL_Rect* dst_rect,
                               unsigned rotation, int sharpness) {
	if (!gl_state.enabled) { // Ensure we are in SW mode logic (though textures exist anyway)
		unsigned int tex_id = gl_state.sw_textures[gl_state.sw_disp_index];
		GLVideo_drawFrame(tex_id, gl_state.sw_width, gl_state.sw_height, src_rect, dst_rect,
		                  rotation, sharpness, false);
	}
}

void GLVideo_present(unsigned width, unsigned height, unsigned rotation, int scaling_mode,
                     int sharpness, double aspect_ratio) {
	LOG_debug("GL video: present called (%ux%u, rotation=%u, scale=%d, sharp=%d)", width, height,
	          rotation, scaling_mode, sharpness);

	if (!gl_state.gl_context) {
		LOG_debug("GL video: present skipped (no context)");
		return;
	}

	// Store frame dimensions for capture
	gl_state.last_frame_width = width;
	gl_state.last_frame_height = height;

	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		LOG_error("GL video: no window for presentation");
		return;
	}

	GLVideo_makeCurrent();

	// Bind backbuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Get actual screen dimensions from window
	int screen_w = 0;
	int screen_h = 0;
	SDL_GetWindowSize(window, &screen_w, &screen_h);
	if (screen_w <= 0 || screen_h <= 0) {
		LOG_error("GL video: invalid window size %dx%d", screen_w, screen_h);
		return;
	}

	// Calculate viewport based on scaling mode
	// Note: PlayerScaler is designed for software rendering and doesn't work correctly
	// for GL viewport calculation, so we calculate directly here.
	int vp_x = 0, vp_y = 0, vp_w = screen_w, vp_h = screen_h;
	int src_x = 0, src_y = 0, src_w = width, src_h = height;

	// Use aspect ratio from core if provided, otherwise use source dimensions
	double src_aspect = (aspect_ratio > 0) ? aspect_ratio : ((double)width / (double)height);
	double screen_aspect = (double)screen_w / (double)screen_h;

	switch (scaling_mode) {
	case 0: // PLAYER_SCALE_NATIVE - integer scale, centered
	{
		int scale = MIN(screen_w / (int)width, screen_h / (int)height);
		if (scale < 1)
			scale = 1;
		vp_w = width * scale;
		vp_h = height * scale;
		vp_x = (screen_w - vp_w) / 2;
		vp_y = (screen_h - vp_h) / 2;
		break;
	}
	case 1: // PLAYER_SCALE_ASPECT - maintain aspect ratio
	{
		if (src_aspect > screen_aspect) {
			// Content is wider - letterbox (black bars top/bottom)
			vp_w = screen_w;
			vp_h = (int)(screen_w / src_aspect);
			vp_x = 0;
			vp_y = (screen_h - vp_h) / 2;
		} else {
			// Content is taller - pillarbox (black bars left/right)
			vp_w = (int)(screen_h * src_aspect);
			vp_h = screen_h;
			vp_x = (screen_w - vp_w) / 2;
			vp_y = 0;
		}
		break;
	}
	case 2: // PLAYER_SCALE_FULLSCREEN - stretch to fill
		vp_x = 0;
		vp_y = 0;
		vp_w = screen_w;
		vp_h = screen_h;
		break;
	case 3: // PLAYER_SCALE_CROPPED - crop to fill while maintaining aspect
	{
		if (src_aspect > screen_aspect) {
			// Content is wider - crop left/right
			vp_w = screen_w;
			vp_h = screen_h;
			int visible_w = (int)(height * screen_aspect);
			src_x = (width - visible_w) / 2;
			src_w = visible_w;
		} else {
			// Content is taller - crop top/bottom
			vp_w = screen_w;
			vp_h = screen_h;
			int visible_h = (int)(width / screen_aspect);
			src_y = (height - visible_h) / 2;
			src_h = visible_h;
		}
		break;
	}
	}

	LOG_debug("GL video: viewport(%d,%d %dx%d) src_crop(%d,%d %dx%d)", vp_x, vp_y, vp_w, vp_h,
	          src_x, src_y, src_w, src_h);

	// Clear entire screen first (glClear only affects current viewport)
	glViewport(0, 0, screen_w, screen_h);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	SDL_Rect src_rect = {src_x, src_y, src_w, src_h};
	SDL_Rect dst_rect = {vp_x, vp_y, vp_w, vp_h};

	unsigned int texture_id;
	unsigned int tex_w, tex_h;
	bool bottom_left;

	if (gl_state.enabled) {
		texture_id = gl_state.fbo_texture;
		tex_w = gl_state.fbo_width;
		tex_h = gl_state.fbo_height;
		bottom_left = gl_state.hw_callback.bottom_left_origin;
	} else {
		texture_id = gl_state.sw_textures[gl_state.sw_disp_index];
		tex_w = gl_state.sw_width;
		tex_h = gl_state.sw_height;
		bottom_left = false; // SW textures are top-left
	}

	GLVideo_drawFrame(texture_id, tex_w, tex_h, &src_rect, &dst_rect, rotation, sharpness,
	                  bottom_left);

	// Note: Swap is now done separately via GLVideo_swapBuffers()
	// to allow HUD overlay rendering before the frame is displayed
}

bool GLVideo_resizeFBO(unsigned width, unsigned height) {
	if (!gl_state.enabled) {
		return false;
	}

	if (width == gl_state.fbo_width && height == gl_state.fbo_height) {
		return true; // No change needed
	}

	LOG_info("GL video: resizing FBO %ux%u -> %ux%u", gl_state.fbo_width, gl_state.fbo_height,
	         width, height);

	// Make context current before GL operations
	GLVideo_makeCurrent();

	// Destroy existing FBO
	destroyFBO();

	// Recreate FBO with new dimensions
	bool need_depth = gl_state.hw_callback.depth;
	bool need_stencil = gl_state.hw_callback.stencil;

	if (!createFBO(width, height, need_depth, need_stencil)) {
		LOG_error("GL video: FBO resize failed");
		gl_state.enabled = false; // Prevent further HW rendering attempts
		return false;
	}

	gl_state.fbo_width = width;
	gl_state.fbo_height = height;

	return true;
}

///////////////////////////////
// Context Management
///////////////////////////////

void GLVideo_makeCurrent(void) {
	if (!gl_state.gl_context) {
		return;
	}

	SDL_Window* window = PLAT_getWindow();
	if (window && SDL_GL_MakeCurrent(window, gl_state.gl_context) < 0) {
		LOG_warn("GL video: SDL_GL_MakeCurrent failed: %s", SDL_GetError());
	}
}

void GLVideo_contextReset(void) {
	if (!gl_state.enabled) {
		return;
	}

	if (gl_state.hw_callback.context_reset) {
		// Make GL context current before calling core's context_reset
		GLVideo_makeCurrent();

		LOG_info("GL video: calling core context_reset");
		gl_state.hw_callback.context_reset();

		// Drain any GL errors left by context_reset (cores often probe optional features)
		int error_count = 0;
		while (glGetError() != GL_NO_ERROR && error_count < 100) {
			error_count++;
		}
		if (error_count > 0) {
			LOG_debug("GL video: cleared %d GL errors after context_reset", error_count);
		}
	}
}

void GLVideo_bindFBO(void) {
	if (!gl_state.enabled || !gl_state.context_ready) {
		return;
	}

	LOG_debug("GL video: bindFBO called, binding FBO %u (%ux%u)", gl_state.fbo, gl_state.fbo_width,
	          gl_state.fbo_height);

	GLVideo_makeCurrent();
	glBindFramebuffer(GL_FRAMEBUFFER, gl_state.fbo);

	// Set viewport to FBO size - cores expect this to be set
	glViewport(0, 0, (GLsizei)gl_state.fbo_width, (GLsizei)gl_state.fbo_height);

	LOG_debug("GL video: FBO bound, viewport set to FBO size");

	// Drain any GL errors left by the core (feature probing, etc.)
	// Only log occasionally to avoid spam
	int errors_this_frame = 0;
	while (glGetError() != GL_NO_ERROR && errors_this_frame < 10) {
		errors_this_frame++;
		gl_error_total++;
	}
	// Log first occurrence and then every 100 errors
	if (errors_this_frame > 0 &&
	    (gl_error_total <= errors_this_frame || gl_error_total % 100 == 0)) {
		LOG_debug("GL video: drained %d GL errors (total: %d)", errors_this_frame, gl_error_total);
	}
}

void GLVideo_uploadFrame(const void* data, unsigned width, unsigned height, size_t pitch,
                         unsigned pixel_format) {
	if (!data || width == 0 || height == 0) {
		return;
	}

	if (!gl_state.gl_context) {
		return;
	}

	GLVideo_makeCurrent();

	GLenum internal_fmt = GL_RGB;
	GLenum type = GL_UNSIGNED_SHORT_5_6_5;
	size_t bpp = 2;

	if (pixel_format == GL_VIDEO_PIXEL_FORMAT_XRGB8888) {
		internal_fmt = GL_RGBA;
		type = GL_UNSIGNED_BYTE;
		bpp = 4;
	} else if (pixel_format == GL_VIDEO_PIXEL_FORMAT_RGB565) {
		internal_fmt = GL_RGB;
		type = GL_UNSIGNED_SHORT_5_6_5;
		bpp = 2;
	} else {
		// 0RGB1555 not supported yet
		return;
	}

	// Recreate textures if dimensions change
	if (gl_state.sw_width != width || gl_state.sw_height != height) {
		LOG_info("GL video: resizing SW textures %ux%u -> %ux%u", gl_state.sw_width,
		         gl_state.sw_height, width, height);

		if (gl_state.sw_textures[0]) {
			glDeleteTextures(3, gl_state.sw_textures);
		}
		glGenTextures(3, gl_state.sw_textures);

		for (int i = 0; i < 3; i++) {
			glBindTexture(GL_TEXTURE_2D, gl_state.sw_textures[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internal_fmt, (GLsizei)width, (GLsizei)height, 0,
			             internal_fmt, type, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		}
		gl_state.sw_width = width;
		gl_state.sw_height = height;
		gl_state.sw_tex_index = 0;
	}

	// Select next texture in triple-buffer
	gl_state.sw_tex_index = (gl_state.sw_tex_index + 1) % 3;
	unsigned int tex = gl_state.sw_textures[gl_state.sw_tex_index];
	glBindTexture(GL_TEXTURE_2D, tex);

	if (pitch == width * bpp) {
		// Fast path: contiguous data
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)width, (GLsizei)height, internal_fmt, type,
		                data);
	} else {
		// Slow path: row-by-row upload (GLES2 limitation: no GL_UNPACK_ROW_LENGTH)
		const uint8_t* src = (const uint8_t*)data;
		for (unsigned int y = 0; y < height; y++) {
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (GLint)y, (GLsizei)width, 1, internal_fmt, type,
			                src);
			src += pitch;
		}
	}

	// Update display index to point to the newest frame
	gl_state.sw_disp_index = gl_state.sw_tex_index;
}

void GLVideo_presentSurface(SDL_Surface* surface) {
	LOG_debug("presentSurface: enter");

	if (!gl_state.enabled || !gl_state.context_ready) {
		LOG_debug("presentSurface: not enabled, returning");
		return;
	}

	if (!surface || !surface->pixels) {
		LOG_error("GL video: NULL surface for presentSurface");
		return;
	}

	LOG_debug("presentSurface: surface %dx%d", surface->w, surface->h);

	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		LOG_error("GL video: no window for surface presentation");
		return;
	}

	LOG_debug("presentSurface: calling makeCurrent");
	GLVideo_makeCurrent();
	LOG_debug("presentSurface: makeCurrent done");

	// Bind backbuffer
	LOG_debug("presentSurface: binding FBO 0");
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	LOG_debug("presentSurface: FBO 0 bound");

	// Get screen dimensions
	int screen_w = 0;
	int screen_h = 0;
	SDL_GetWindowSize(window, &screen_w, &screen_h);
	LOG_debug("presentSurface: screen %dx%d", screen_w, screen_h);
	if (screen_w <= 0 || screen_h <= 0) {
		LOG_error("GL video: invalid window size %dx%d", screen_w, screen_h);
		return;
	}

	// Create or resize UI texture if needed
	unsigned int surf_w = (unsigned int)surface->w;
	unsigned int surf_h = (unsigned int)surface->h;

	if (!gl_state.ui_texture || gl_state.ui_texture_width != surf_w ||
	    gl_state.ui_texture_height != surf_h) {
		LOG_debug("presentSurface: creating UI texture %ux%u", surf_w, surf_h);

		if (gl_state.ui_texture) {
			glDeleteTextures(1, &gl_state.ui_texture);
		}

		glGenTextures(1, &gl_state.ui_texture);
		glBindTexture(GL_TEXTURE_2D, gl_state.ui_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, surf_w, surf_h, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
		             NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		gl_state.ui_texture_width = surf_w;
		gl_state.ui_texture_height = surf_h;

		LOG_debug("presentSurface: UI texture created (id=%u)", gl_state.ui_texture);
	}

	LOG_debug("presentSurface: uploading pixels");
	glBindTexture(GL_TEXTURE_2D, gl_state.ui_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, surf_w, surf_h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
	                surface->pixels);

	LOG_debug("presentSurface: setting viewport and clearing");
	glViewport(0, 0, screen_w, screen_h);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	LOG_debug("presentSurface: using shader program");
	glUseProgram(gl_state.present_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_state.ui_texture);
	glUniform1i(gl_state.loc_texture, 0);

	float identity[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1};
	glUniformMatrix4fv(gl_state.loc_mvp, 1, GL_FALSE, identity);

	static const float verts[8] = {0, 0, 1, 0, 0, 1, 1, 1};
	// Flip Y only (SDL surfaces are top-left origin, GL is bottom-left)
	static const float texco[8] = {0, 1, 1, 1, 0, 0, 1, 0};

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(gl_state.loc_position);
	glEnableVertexAttribArray(gl_state.loc_texcoord);
	glVertexAttribPointer(gl_state.loc_position, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(gl_state.loc_texcoord, 2, GL_FLOAT, GL_FALSE, 0, texco);

	LOG_debug("presentSurface: drawing quad");
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(gl_state.loc_position);
	glDisableVertexAttribArray(gl_state.loc_texcoord);

	LOG_debug("presentSurface: swapping window");
	SDL_GL_SwapWindow(window);
	LOG_debug("presentSurface: done");
}

void GLVideo_swapBuffers(void) {
	if (!gl_state.enabled || !gl_state.context_ready) {
		return;
	}

	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		return;
	}

	SDL_GL_SwapWindow(window);
}

void GLVideo_renderHUD(const uint32_t* pixels, int width, int height, int screen_w, int screen_h) {
	if (!gl_state.enabled || !gl_state.context_ready) {
		return;
	}

	if (!pixels || width <= 0 || height <= 0) {
		return;
	}

	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		return;
	}

	GLVideo_makeCurrent();

	// Create or resize HUD texture if needed
	unsigned int tex_w = (unsigned int)width;
	unsigned int tex_h = (unsigned int)height;

	if (!gl_state.hud_texture || gl_state.hud_texture_width != tex_w ||
	    gl_state.hud_texture_height != tex_h) {
		LOG_debug("renderHUD: creating HUD texture %ux%u", tex_w, tex_h);

		if (gl_state.hud_texture) {
			glDeleteTextures(1, &gl_state.hud_texture);
		}

		glGenTextures(1, &gl_state.hud_texture);
		glBindTexture(GL_TEXTURE_2D, gl_state.hud_texture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_w, tex_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

		gl_state.hud_texture_width = tex_w;
		gl_state.hud_texture_height = tex_h;

		LOG_debug("renderHUD: HUD texture created (id=%u)", gl_state.hud_texture);
	}

	// Upload HUD pixels
	glBindTexture(GL_TEXTURE_2D, gl_state.hud_texture);
	glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tex_w, tex_h, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	// Set viewport to full screen
	glViewport(0, 0, screen_w, screen_h);

	// Enable alpha blending
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Use shader program
	glUseProgram(gl_state.present_program);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_state.hud_texture);
	glUniform1i(gl_state.loc_texture, 0);

	// Identity MVP (fullscreen quad)
	float identity[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1};
	glUniformMatrix4fv(gl_state.loc_mvp, 1, GL_FALSE, identity);

	static const float verts[8] = {0, 0, 1, 0, 0, 1, 1, 1};
	// Flip Y (SDL surfaces are top-left origin, GL is bottom-left)
	static const float texco[8] = {0, 1, 1, 1, 0, 0, 1, 0};

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(gl_state.loc_position);
	glEnableVertexAttribArray(gl_state.loc_texcoord);
	glVertexAttribPointer(gl_state.loc_position, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(gl_state.loc_texcoord, 2, GL_FLOAT, GL_FALSE, 0, texco);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(gl_state.loc_position);
	glDisableVertexAttribArray(gl_state.loc_texcoord);

	// Disable blending after HUD rendering
	glDisable(GL_BLEND);
}

SDL_Surface* GLVideo_captureFrame(void) {
	if (!gl_state.gl_context || !gl_state.context_ready) {
		LOG_debug("GL video: captureFrame - not ready");
		return NULL;
	}

	unsigned int width = gl_state.last_frame_width;
	unsigned int height = gl_state.last_frame_height;

	if (width == 0 || height == 0) {
		LOG_warn("GL video: captureFrame - no frame rendered yet (0x0)");
		return NULL;
	}

	LOG_debug("GL video: captureFrame - capturing %ux%u", width, height);

	GLVideo_makeCurrent();

	// If using FBO (HW render), bind it.
	// If using SW render, we need to read from the texture?
	// glReadPixels reads from the currently bound FRAMEBUFFER.
	// For SW render, we render to the default framebuffer (screen).
	// So we should bind FBO 0.
	// BUT, if we are in the menu, we want to capture the GAME frame.
	// The game frame was last rendered to the screen (FBO 0).
	// So reading from FBO 0 should work for SW render.
	// For HW render, the game frame is in FBO `gl_state.fbo`.
	// AND it was also rendered to FBO 0 (screen) by `present`.
	// So we could always read from FBO 0?
	// No, FBO 0 might have HUD or other things on top if we capture late.
	// But `captureFrame` is called at start of menu loop.
	// Ideally we read from the source texture/FBO to avoid capturing overlay?
	// But `glReadPixels` reads from framebuffer, not texture.
	// To read texture, we need to bind it to an FBO.
	// For HW render, we have `gl_state.fbo`.
	// For SW render, we don't have an FBO for the texture.
	// We could create a temporary FBO or just read from backbuffer?
	// Reading from backbuffer (FBO 0) is simplest for SW render.

	if (gl_state.enabled) {
		glBindFramebuffer(GL_FRAMEBUFFER, gl_state.fbo);
	} else {
		// SW render: read from backbuffer
		// Ensure we are reading from the buffer that was just swapped?
		// SDL_GL_SwapBuffers swaps buffers.
		// Reading from FRONT buffer? GLES2 usually reads from BACK.
		// If we just swapped, BACK is new (undefined?).
		// Wait, capture happens BEFORE menu is drawn?
		// Menu loop calls capture first.
		// If called after `present` and `swap`, the game frame is on screen (FRONT).
		// We want to capture it.
		// `glReadPixels` usually reads from current read buffer.
		// For window system FBO, it's implementation dependent (Back or Front).
		// Safe bet: Bind the SW texture to a temp FBO and read it?
		// Or assume backbuffer still has it?
		// Let's try reading from FBO 0.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	// Allocate temporary RGBA buffer
	size_t rgba_size = width * height * 4;
	uint8_t* rgba_buffer = malloc(rgba_size);
	if (!rgba_buffer) {
		LOG_error("GL video: captureFrame - failed to allocate RGBA buffer");
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return NULL;
	}

	// Read pixels (note: OpenGL origin is bottom-left)
	glReadPixels(0, 0, (GLsizei)width, (GLsizei)height, GL_RGBA, GL_UNSIGNED_BYTE, rgba_buffer);

	// Check for GL errors
	GLenum err = glGetError();
	if (err != GL_NO_ERROR) {
		LOG_error("GL video: captureFrame - glReadPixels error 0x%x", err);
		free(rgba_buffer);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return NULL;
	}

	// Unbind FBO
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Create RGB565 SDL surface
	SDL_Surface* surface =
	    SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 16, 0xF800, 0x07E0, 0x001F, 0);
	if (!surface) {
		LOG_error("GL video: captureFrame - failed to create SDL surface");
		free(rgba_buffer);
		return NULL;
	}

	// Convert RGBA to RGB565, flipping Y axis
	// OpenGL origin is bottom-left, SDL is top-left
	uint16_t* dst = (uint16_t*)surface->pixels;
	int dst_pitch = surface->pitch / 2; // pitch in uint16_t units

	for (unsigned int y = 0; y < height; y++) {
		// Source row from bottom (GL bottom-left origin)
		unsigned int src_y = height - 1 - y;
		uint8_t* src_row = rgba_buffer + (src_y * width * 4);

		for (unsigned int x = 0; x < width; x++) {
			uint8_t r = src_row[x * 4 + 0];
			uint8_t g = src_row[x * 4 + 1];
			uint8_t b = src_row[x * 4 + 2];
			// Convert to RGB565: 5 bits R, 6 bits G, 5 bits B
			dst[y * dst_pitch + x] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
		}
	}

	free(rgba_buffer);

	LOG_debug("GL video: captureFrame - captured %ux%u frame", width, height);
	return surface;
}

#endif /* HAS_OPENGLES */
