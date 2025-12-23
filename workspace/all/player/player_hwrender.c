/**
 * player_hwrender.c - OpenGL ES hardware rendering implementation
 *
 * Provides hardware-accelerated rendering support for libretro cores
 * that require OpenGL ES. Only compiled on platforms with HAS_OPENGLES.
 */

#include "player_hwrender.h"

#if HAS_OPENGLES

#include "api.h"
#include "log.h"
#include <SDL.h>
#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_FLOAT 0x1406
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

// GL function pointers (loaded via SDL_GL_GetProcAddress)
static void (*glGenFramebuffers)(GLsizei, GLuint*);
static void (*glBindFramebuffer)(GLenum, GLuint);
static void (*glGenTextures)(GLsizei, GLuint*);
static void (*glBindTexture)(GLenum, GLuint);
static void (*glTexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum,
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
static void (*glColorMask)(GLboolean, GLboolean, GLboolean, GLboolean);
static void (*glBindBuffer)(GLenum, GLuint);

///////////////////////////////
// Module State
///////////////////////////////

static PlayerHWRenderState hw_state = {0};

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
			LOG_error("HW render: failed to load GL function: %s", #name);                         \
			return false;                                                                          \
		}                                                                                          \
	} while (0)

	LOAD_GL_FUNC(glGenFramebuffers);
	LOAD_GL_FUNC(glBindFramebuffer);
	LOAD_GL_FUNC(glGenTextures);
	LOAD_GL_FUNC(glBindTexture);
	LOAD_GL_FUNC(glTexImage2D);
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
	LOAD_GL_FUNC(glColorMask);
	LOAD_GL_FUNC(glBindBuffer);

#undef LOAD_GL_FUNC

	LOG_debug("HW render: all GL functions loaded successfully");
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
// Vertex Data (RetroArch-style - separate arrays, not interleaved)
///////////////////////////////

// Position vertices: 4 vertices * 2 floats (x, y)
// These are in 0-1 range, transformed by MVP to NDC
static const float vertexes[8] = {
    0.0f, 0.0f, // Bottom-left
    1.0f, 0.0f, // Bottom-right
    0.0f, 1.0f, // Top-left
    1.0f, 1.0f, // Top-right
};

// Texture coordinates for normal case (sample full texture)
static const float tex_coords[8] = {
    0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
};

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
		LOG_error("HW render: glCreateShader failed");
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
		LOG_error("HW render: shader compilation failed: %s", log);
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
		LOG_error("HW render: glCreateProgram failed");
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
		LOG_error("HW render: shader linking failed: %s", log);
		glDeleteProgram(program);
		program = 0;
	}

	// Shaders can be deleted after linking
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	return program;
}

/**
 * Destroy presentation resources (shader program).
 */
static void destroyPresentResources(void) {
	if (hw_state.present_program) {
		glDeleteProgram(hw_state.present_program);
		hw_state.present_program = 0;
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
	glGenFramebuffers(1, &hw_state.fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, hw_state.fbo);
	LOG_debug("createFBO: FBO generated (id=%u)", hw_state.fbo);

	// Create color texture attachment
	LOG_debug("createFBO: creating color texture");
	glGenTextures(1, &hw_state.fbo_texture);
	glBindTexture(GL_TEXTURE_2D, hw_state.fbo_texture);
	LOG_debug("createFBO: texture generated (id=%u), setting up RGBA8888 %ux%u",
	          hw_state.fbo_texture, width, height);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	LOG_debug("createFBO: attaching texture to FBO");
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
	                       hw_state.fbo_texture, 0);

	// Create depth/stencil renderbuffer if requested
	if (need_depth || need_stencil) {
		glGenRenderbuffers(1, &hw_state.fbo_depth_rb);
		glBindRenderbuffer(GL_RENDERBUFFER, hw_state.fbo_depth_rb);

		// Use packed depth24_stencil8 if both are needed
		if (need_depth && need_stencil) {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
			                          hw_state.fbo_depth_rb);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
			                          hw_state.fbo_depth_rb);
		} else if (need_depth) {
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
			                          hw_state.fbo_depth_rb);
		} else {
			// Stencil-only is invalid per libretro spec, but handle gracefully
			LOG_warn("HW render: stencil-only requested (invalid), using depth24_stencil8");
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8_OES, width, height);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
			                          hw_state.fbo_depth_rb);
		}
	}

	// Check FBO completeness
	LOG_debug("createFBO: checking FBO completeness");
	GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		LOG_error("HW render: FBO incomplete (status=0x%x)", status);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return false;
	}
	LOG_debug("createFBO: FBO is complete");

	// Unbind FBO (core will bind it via get_current_framebuffer)
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	LOG_info("HW render: FBO created %ux%u (depth=%d, stencil=%d)", width, height, need_depth,
	         need_stencil);
	return true;
}

/**
 * Destroy FBO and associated attachments.
 */
static void destroyFBO(void) {
	if (hw_state.fbo_depth_rb) {
		glDeleteRenderbuffers(1, &hw_state.fbo_depth_rb);
		hw_state.fbo_depth_rb = 0;
	}

	if (hw_state.fbo_texture) {
		glDeleteTextures(1, &hw_state.fbo_texture);
		hw_state.fbo_texture = 0;
	}

	if (hw_state.fbo) {
		glDeleteFramebuffers(1, &hw_state.fbo);
		hw_state.fbo = 0;
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

bool PlayerHWRender_init(struct retro_hw_render_callback* callback, unsigned max_width,
                         unsigned max_height) {
	LOG_debug("PlayerHWRender_init: called with max_width=%u, max_height=%u", max_width,
	          max_height);

	if (!callback) {
		LOG_error("PlayerHWRender_init: NULL callback");
		return false;
	}

	LOG_debug("PlayerHWRender_init: context_type=%d (%s), version=%u.%u, depth=%d, stencil=%d",
	          callback->context_type, getContextTypeName(callback->context_type),
	          callback->version_major, callback->version_minor, callback->depth, callback->stencil);

	// Check if context type is supported
	if (!PlayerHWRender_isContextSupported(callback->context_type)) {
		LOG_info("HW render: unsupported context type %s",
		         getContextTypeName(callback->context_type));
		return false;
	}

	LOG_debug("PlayerHWRender_init: context type supported, proceeding with initialization");

	LOG_info("HW render: initializing %s context (v%u.%u, depth=%d, stencil=%d, max=%ux%u)",
	         getContextTypeName(callback->context_type), callback->version_major,
	         callback->version_minor, callback->depth, callback->stencil, max_width, max_height);

	// Get SDL window from platform
	LOG_debug("PlayerHWRender_init: getting SDL window from platform");
	SDL_Window* window = PLAT_getWindow();
	if (!window) {
		LOG_error("HW render: failed to get SDL window");
		return false;
	}
	LOG_debug("PlayerHWRender_init: got SDL window successfully");

	// Set GL attributes for OpenGL ES 2.0
	LOG_debug("PlayerHWRender_init: setting GL attributes for GLES 2.0");
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	// Create GL context
	LOG_debug("PlayerHWRender_init: creating GL context");
	hw_state.gl_context = SDL_GL_CreateContext(window);
	if (!hw_state.gl_context) {
		LOG_error("HW render: SDL_GL_CreateContext failed: %s", SDL_GetError());
		return false;
	}

	LOG_info("HW render: OpenGL ES 2.0 context created successfully");

	// Make context current
	LOG_debug("PlayerHWRender_init: making GL context current");
	if (SDL_GL_MakeCurrent(window, hw_state.gl_context) < 0) {
		LOG_error("HW render: SDL_GL_MakeCurrent failed: %s", SDL_GetError());
		SDL_GL_DeleteContext(hw_state.gl_context);
		hw_state.gl_context = NULL;
		return false;
	}
	LOG_debug("PlayerHWRender_init: GL context is current");

	// Load GL function pointers
	LOG_debug("PlayerHWRender_init: loading GL function pointers");
	if (!loadGLFunctions()) {
		LOG_error("HW render: failed to load GL functions");
		SDL_GL_DeleteContext(hw_state.gl_context);
		hw_state.gl_context = NULL;
		return false;
	}
	LOG_debug("PlayerHWRender_init: GL functions loaded");

	// Create FBO for core to render into
	LOG_debug("PlayerHWRender_init: creating FBO (%ux%u, depth=%d, stencil=%d)", max_width,
	          max_height, callback->depth, callback->stencil);
	if (!createFBO(max_width, max_height, callback->depth, callback->stencil)) {
		LOG_error("HW render: FBO creation failed");
		SDL_GL_DeleteContext(hw_state.gl_context);
		hw_state.gl_context = NULL;
		return false;
	}

	// Create shader program for presenting FBO to screen
	LOG_debug("PlayerHWRender_init: creating shader program");
	hw_state.present_program = createShaderProgram();
	if (!hw_state.present_program) {
		LOG_error("HW render: shader program creation failed");
		destroyFBO();
		SDL_GL_DeleteContext(hw_state.gl_context);
		hw_state.gl_context = NULL;
		return false;
	}
	LOG_debug("PlayerHWRender_init: shader program created (id=%u)", hw_state.present_program);

	// Cache uniform and attribute locations (RetroArch-style MVP shader)
	LOG_debug("PlayerHWRender_init: caching shader locations");
	hw_state.loc_mvp = glGetUniformLocation(hw_state.present_program, "u_mvp");
	hw_state.loc_texture = glGetUniformLocation(hw_state.present_program, "u_texture");
	hw_state.loc_position = glGetAttribLocation(hw_state.present_program, "a_position");
	hw_state.loc_texcoord = glGetAttribLocation(hw_state.present_program, "a_texcoord");
	LOG_debug("PlayerHWRender_init: shader locations cached (mvp=%d, tex=%d, pos=%d, tc=%d)",
	          hw_state.loc_mvp, hw_state.loc_texture, hw_state.loc_position, hw_state.loc_texcoord);

	// Provide our callbacks to the core
	LOG_debug("PlayerHWRender_init: setting up core callbacks");
	callback->get_current_framebuffer = PlayerHWRender_getCurrentFramebuffer;
	callback->get_proc_address = PlayerHWRender_getProcAddress;

	// Store callback info (after setting our callbacks so memcpy includes them)
	memcpy(&hw_state.hw_callback, callback, sizeof(struct retro_hw_render_callback));

	hw_state.fbo_width = max_width;
	hw_state.fbo_height = max_height;
	hw_state.enabled = true;
	hw_state.context_ready = true;

	// Call core's context_reset now that GL context and FBO are ready
	// This must happen before retro_load_game() so cores can create GL resources
	if (hw_state.hw_callback.context_reset) {
		LOG_info("HW render: calling core context_reset");
		hw_state.hw_callback.context_reset();
	}

	LOG_info("HW render: initialized successfully");
	return true;
}

void PlayerHWRender_shutdown(void) {
	if (!hw_state.enabled) {
		return;
	}

	LOG_info("HW render: shutting down");

	// Call core's context_destroy if provided
	if (hw_state.hw_callback.context_destroy) {
		LOG_debug("HW render: calling core context_destroy");
		hw_state.hw_callback.context_destroy();
	}

	// Destroy presentation resources
	destroyPresentResources();

	// Destroy FBO resources
	destroyFBO();

	// Destroy GL context
	if (hw_state.gl_context) {
		SDL_GL_DeleteContext(hw_state.gl_context);
		hw_state.gl_context = NULL;
	}

	memset(&hw_state, 0, sizeof(hw_state));
}

///////////////////////////////
// State Queries
///////////////////////////////

bool PlayerHWRender_isEnabled(void) {
	return hw_state.enabled && hw_state.context_ready;
}

bool PlayerHWRender_isContextSupported(enum retro_hw_context_type context_type) {
	// We only support OpenGL ES 2.0 for now
	switch (context_type) {
	case RETRO_HW_CONTEXT_OPENGLES2:
		return true;

	// GLES3 can be added later
	case RETRO_HW_CONTEXT_OPENGLES3:
	case RETRO_HW_CONTEXT_OPENGLES_VERSION:
		LOG_debug("HW render: GLES3 not yet supported, core may fall back to GLES2");
		return false;

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

///////////////////////////////
// Core Callbacks
///////////////////////////////

uintptr_t PlayerHWRender_getCurrentFramebuffer(void) {
	// Return FBO handle for core to render into
	return (uintptr_t)hw_state.fbo;
}

retro_proc_address_t PlayerHWRender_getProcAddress(const char* sym) {
	if (!sym) {
		return NULL;
	}

	// TODO: Phase 2 - use SDL_GL_GetProcAddress
	retro_proc_address_t proc = (retro_proc_address_t)SDL_GL_GetProcAddress(sym);
	if (!proc) {
		LOG_debug("HW render: getProcAddress failed for '%s'", sym);
	}
	return proc;
}

///////////////////////////////
// Frame Operations
///////////////////////////////

void PlayerHWRender_present(unsigned width, unsigned height, unsigned rotation) {
	(void)rotation;

	if (!hw_state.enabled || !hw_state.context_ready) {
		return;
	}

	SDL_Window* window = PLAT_getWindow();
	PlayerHWRender_makeCurrent();

	// Bind backbuffer
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	// Calculate aspect-preserving viewport
	int screen_w = 1280;
	int screen_h = 720;
	float src_aspect = (float)width / (float)height;
	float dst_aspect = (float)screen_w / (float)screen_h;

	int vp_x = 0, vp_y = 0, vp_w = screen_w, vp_h = screen_h;
	if (src_aspect > dst_aspect) {
		// Letterbox (black bars top/bottom)
		vp_h = (int)(screen_w / src_aspect);
		vp_y = (screen_h - vp_h) / 2;
	} else {
		// Pillarbox (black bars left/right)
		vp_w = (int)(screen_h * src_aspect);
		vp_x = (screen_w - vp_w) / 2;
	}

	glViewport(vp_x, vp_y, vp_w, vp_h);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	// Use shader
	glUseProgram(hw_state.present_program);

	// Bind texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hw_state.fbo_texture);
	glUniform1i(hw_state.loc_texture, 0);

	// Identity MVP (no transformation)
	float identity[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, -1, -1, 0, 1};
	glUniformMatrix4fv(hw_state.loc_mvp, 1, GL_FALSE, identity);

	// Scale texture coords to sample only rendered portion
	float tex_scale_x = (float)width / (float)hw_state.fbo_width;
	float tex_scale_y = (float)height / (float)hw_state.fbo_height;

	float texco[8] = {0.0f, 0.0f, tex_scale_x, 0.0f, 0.0f, tex_scale_y, tex_scale_x, tex_scale_y};

	static const float verts[8] = {0, 0, 1, 0, 0, 1, 1, 1};

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glEnableVertexAttribArray(hw_state.loc_position);
	glEnableVertexAttribArray(hw_state.loc_texcoord);
	glVertexAttribPointer(hw_state.loc_position, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(hw_state.loc_texcoord, 2, GL_FLOAT, GL_FALSE, 0, texco);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glDisableVertexAttribArray(hw_state.loc_position);
	glDisableVertexAttribArray(hw_state.loc_texcoord);

	SDL_GL_SwapWindow(window);
}

bool PlayerHWRender_resizeFBO(unsigned width, unsigned height) {
	if (!hw_state.enabled) {
		return false;
	}

	if (width == hw_state.fbo_width && height == hw_state.fbo_height) {
		return true; // No change needed
	}

	LOG_info("HW render: resizing FBO %ux%u -> %ux%u", hw_state.fbo_width, hw_state.fbo_height,
	         width, height);

	// Make context current before GL operations
	PlayerHWRender_makeCurrent();

	// Destroy existing FBO
	destroyFBO();

	// Recreate FBO with new dimensions
	bool need_depth = hw_state.hw_callback.depth;
	bool need_stencil = hw_state.hw_callback.stencil;

	if (!createFBO(width, height, need_depth, need_stencil)) {
		LOG_error("HW render: FBO resize failed");
		hw_state.enabled = false; // Prevent further HW rendering attempts
		return false;
	}

	hw_state.fbo_width = width;
	hw_state.fbo_height = height;

	return true;
}

///////////////////////////////
// Context Management
///////////////////////////////

void PlayerHWRender_makeCurrent(void) {
	if (!hw_state.gl_context) {
		return;
	}

	SDL_Window* window = PLAT_getWindow();
	if (window && SDL_GL_MakeCurrent(window, hw_state.gl_context) < 0) {
		LOG_warn("HW render: SDL_GL_MakeCurrent failed: %s", SDL_GetError());
	}
}

void PlayerHWRender_contextReset(void) {
	if (!hw_state.enabled) {
		return;
	}

	if (hw_state.hw_callback.context_reset) {
		LOG_info("HW render: calling core context_reset");
		hw_state.hw_callback.context_reset();
	}
}

void PlayerHWRender_bindFBO(void) {
	if (!hw_state.enabled || !hw_state.context_ready) {
		return;
	}

	LOG_debug("PlayerHWRender_bindFBO: binding FBO %u for core rendering", hw_state.fbo);
	PlayerHWRender_makeCurrent();
	glBindFramebuffer(GL_FRAMEBUFFER, hw_state.fbo);
}

#endif /* HAS_OPENGLES */
