# Shader Pipeline Architecture

## Overview

This document describes the shader pipeline system for LessUI, which builds on top of the unified GL rendering pipeline (see docs/opengles-hardware-rendering.md).

**Prerequisites**: Requires unified GL rendering where both SW and HW cores produce GL textures.

**Goal**: Apply post-processing shaders (scanlines, CRT effects, LCD simulation, etc.) to all cores uniformly.

## Research Summary

Investigation of RetroArch (~/Code/RetroArch) and NextUI (~/Code/NextUI) reveals two approaches:

### RetroArch: Comprehensive

- **16+ shader passes** with complex FBO chains
- **Multiple formats**: GLSL, CG, Slang, HLSL
- **Frame history**: PREV_TEXTURES array for motion blur, ghosting
- **Complex parameter system**: Full menu integration
- **Reference quality**: Works everywhere, handles all edge cases

### NextUI: Pragmatic

- **3 shader passes** maximum (sufficient for 99% of effects)
- **GLSL only** with automatic version translation
- **Binary caching**: .bin cache for fast startup
- **Simple pragmas**: `#pragma parameter` for UI floats
- **No frame history**: Simplifies implementation significantly

## Recommended Approach for LessUI

Follow NextUI's pragmatic model with LessUI-specific refinements:

| Decision         | Choice                     | Rationale                           |
| ---------------- | -------------------------- | ----------------------------------- |
| Max passes       | 3                          | Covers upscale + effect + sharpen   |
| Shader format    | GLSL only                  | Native to GLES, simpler             |
| File format      | Unified VERTEX/FRAGMENT    | Single file easier to manage        |
| Parameters       | `#pragma parameter` floats | Simple UI integration               |
| Binary caching   | Yes                        | Critical for embedded startup time  |
| Frame history    | No                         | Complexity not worth it             |
| GLES baseline    | 2.0                        | Maximum device compatibility        |
| Version handling | Auto-transpile to GLES3    | Adapt desktop shaders automatically |

## Architecture

### Data Structures

```c
// workspace/all/common/gl_shader.h

#define MAX_SHADER_PASSES 3
#define MAX_SHADER_PARAMS 16

typedef struct ShaderParam {
    char name[64];           // Parameter name (from #pragma)
    char label[128];         // Display label
    float value;             // Current value
    float default_value;     // Default
    float min_value;         // Minimum
    float max_value;         // Maximum
    float step;              // Increment step
    GLint uniform_location;  // Cached location
} ShaderParam;

typedef struct ShaderPass {
    GLuint program;          // Compiled shader program
    char path[MAX_PATH];     // Shader file path

    // Scaling
    enum {
        SCALE_INPUT,         // Relative to input (e.g., 2x)
        SCALE_ABSOLUTE,      // Fixed size (e.g., 1920x1080)
        SCALE_VIEWPORT       // Match output viewport
    } scale_type;
    float scale_x, scale_y;
    int abs_width, abs_height;

    // Filtering
    GLenum filter;           // GL_LINEAR or GL_NEAREST

    // FBO output (NULL for final pass = screen)
    GLuint fbo;
    GLuint fbo_texture;
    unsigned fbo_width, fbo_height;

    // Standard uniforms (cached locations)
    GLint u_mvp;
    GLint u_texture;
    GLint u_input_size;      // Input texture resolution
    GLint u_output_size;     // Output resolution
    GLint u_texture_size;    // Allocated texture size
    GLint u_frame_count;

    // Shader-specific parameters
    ShaderParam params[MAX_SHADER_PARAMS];
    int num_params;
} ShaderPass;

typedef struct ShaderPipeline {
    ShaderPass passes[MAX_SHADER_PASSES];
    int num_passes;
    uint64_t frame_count;    // For animated shaders
} ShaderPipeline;
```

### Shader File Format

Unified GLSL with pragma parameters (following NextUI):

```glsl
// scanlines.glsl - Simple CRT scanline effect

#pragma parameter SCANLINE_STRENGTH "Scanline Intensity" 0.5 0.0 1.0 0.05
#pragma parameter SCANLINE_SIZE "Scanline Height" 2.0 1.0 4.0 1.0

#if defined(VERTEX)
attribute vec2 a_position;
attribute vec2 a_texcoord;
varying vec2 v_texcoord;
uniform mat4 u_mvp;

void main() {
    gl_Position = u_mvp * vec4(a_position, 0.0, 1.0);
    v_texcoord = a_texcoord;
}
#endif

#if defined(FRAGMENT)
#ifdef GL_ES
precision mediump float;
#endif

varying vec2 v_texcoord;
uniform sampler2D u_texture;
uniform vec2 u_input_size;
uniform vec2 u_output_size;
uniform float SCANLINE_STRENGTH;
uniform float SCANLINE_SIZE;

void main() {
    vec4 color = texture2D(u_texture, v_texcoord);

    // Calculate scanline based on output pixel position
    float scanline_pos = v_texcoord.y * u_output_size.y / SCANLINE_SIZE;
    float scanline = sin(scanline_pos * 3.14159) * 0.5 + 0.5;

    // Apply scanline darkening
    color.rgb *= mix(1.0, scanline, SCANLINE_STRENGTH);

    gl_FragColor = color;
}
#endif
```

### Shader Loading

```c
// gl_shader.c

bool GLShader_loadFromFile(ShaderPass* pass, const char* path) {
    // 1. Read shader file
    char* source = readFileToString(path);

    // 2. Parse pragma parameters
    parsePragmaParameters(source, pass->params, &pass->num_params);

    // 3. Strip pragma lines (GL won't understand them)
    char* cleaned_source = removePragmaLines(source);

    // 4. Detect #version and convert if needed
    //    #version 110-450 → #version 300 es (GLES3)
    //    #version 100 → keep as is (GLES2)
    char* gles_source = transpileToGLES(cleaned_source);

    // 5. Compile vertex shader with #define VERTEX
    GLuint vert = compileShader(GL_VERTEX_SHADER, gles_source, "#define VERTEX\n");

    // 6. Compile fragment shader with #define FRAGMENT + precision
    const char* precision_header =
        "#ifdef GL_ES\n"
        "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
        "precision highp float;\n"
        "#else\n"
        "precision mediump float;\n"
        "#endif\n"
        "#endif\n";

    GLuint frag = compileShader(GL_FRAGMENT_SHADER, gles_source,
                                "#define FRAGMENT\n", precision_header);

    // 7. Link program
    pass->program = linkProgram(vert, frag);

    // 8. Cache uniform locations
    pass->u_mvp = glGetUniformLocation(pass->program, "u_mvp");
    pass->u_texture = glGetUniformLocation(pass->program, "u_texture");
    pass->u_input_size = glGetUniformLocation(pass->program, "u_input_size");
    pass->u_output_size = glGetUniformLocation(pass->program, "u_output_size");
    pass->u_texture_size = glGetUniformLocation(pass->program, "u_texture_size");
    pass->u_frame_count = glGetUniformLocation(pass->program, "u_frame_count");

    // Cache parameter uniform locations
    for (int i = 0; i < pass->num_params; i++) {
        pass->params[i].uniform_location =
            glGetUniformLocation(pass->program, pass->params[i].name);
    }

    free(source);
    free(cleaned_source);
    free(gles_source);

    return true;
}
```

### Binary Caching

```c
// gl_shader_cache.c

#define CACHE_DIR "/.shadercache"

bool GLShaderCache_load(ShaderPass* pass, const char* shader_path) {
    char cache_path[MAX_PATH];
    snprintf(cache_path, sizeof(cache_path), "%s%s/%s.bin",
             SDCARD_PATH, CACHE_DIR, basename(shader_path));

    // Check if cache exists and is newer than source
    if (!isCacheValid(cache_path, shader_path))
        return false;

    // Load binary
    FILE* f = fopen(cache_path, "rb");
    if (!f) return false;

    GLenum format;
    GLsizei length;
    fread(&format, sizeof(format), 1, f);
    fread(&length, sizeof(length), 1, f);

    void* binary = malloc(length);
    fread(binary, 1, length, f);
    fclose(f);

    // Load program from binary
    pass->program = glCreateProgram();
    glProgramBinary(pass->program, format, binary, length);

    GLint status;
    glGetProgramiv(pass->program, GL_LINK_STATUS, &status);

    free(binary);
    return (status == GL_TRUE);
}

void GLShaderCache_save(ShaderPass* pass, const char* shader_path) {
    // Get binary from GL
    GLsizei length;
    GLenum format;
    glGetProgramiv(pass->program, GL_PROGRAM_BINARY_LENGTH, &length);

    void* binary = malloc(length);
    glGetProgramBinary(pass->program, length, &length, &format, binary);

    // Save to cache
    char cache_path[MAX_PATH];
    snprintf(cache_path, sizeof(cache_path), "%s%s/%s.bin",
             SDCARD_PATH, CACHE_DIR, basename(shader_path));

    mkdirRecursive(dirname(cache_path));

    FILE* f = fopen(cache_path, "wb");
    fwrite(&format, sizeof(format), 1, f);
    fwrite(&length, sizeof(length), 1, f);
    fwrite(binary, 1, length, f);
    fclose(f);

    free(binary);
}
```

### Multi-Pass Rendering

```c
// gl_shader_pipeline.c

void GLShaderPipeline_render(ShaderPipeline* pipeline, GLuint input_texture,
                             unsigned input_width, unsigned input_height) {
    pipeline->frame_count++;

    GLuint current_texture = input_texture;
    unsigned current_width = input_width;
    unsigned current_height = input_height;

    for (int i = 0; i < pipeline->num_passes; i++) {
        ShaderPass* pass = &pipeline->passes[i];
        bool is_final = (i == pipeline->num_passes - 1);

        // Calculate output size based on scale_type
        unsigned output_width, output_height;
        calculateOutputSize(pass, current_width, current_height,
                          &output_width, &output_height);

        // Bind output (FBO or screen)
        if (is_final) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            // Use actual screen dimensions
            SDL_GL_GetDrawableSize(window, &output_width, &output_height);
        } else {
            // Ensure FBO exists with correct size
            ensureFBO(pass, output_width, output_height);
            glBindFramebuffer(GL_FRAMEBUFFER, pass->fbo);
        }

        // Set viewport
        glViewport(0, 0, output_width, output_height);
        glClear(GL_COLOR_BUFFER_BIT);

        // Activate shader
        glUseProgram(pass->program);

        // Bind input texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, current_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, pass->filter);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, pass->filter);
        glUniform1i(pass->u_texture, 0);

        // Set standard uniforms
        glUniformMatrix4fv(pass->u_mvp, 1, GL_FALSE, identity_mvp);
        glUniform2f(pass->u_input_size, current_width, current_height);
        glUniform2f(pass->u_output_size, output_width, output_height);
        glUniform2f(pass->u_texture_size, current_width, current_height);
        glUniform1i(pass->u_frame_count, pipeline->frame_count);

        // Set shader-specific parameters
        for (int p = 0; p < pass->num_params; p++) {
            glUniform1f(pass->params[p].uniform_location,
                       pass->params[p].value);
        }

        // Draw fullscreen quad
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        // Output becomes next input
        if (!is_final) {
            current_texture = pass->fbo_texture;
            current_width = output_width;
            current_height = output_height;
        }
    }
}
```

## Integration with GLVideo

```c
// workspace/all/common/gl_video.c

typedef struct GLVideoState {
    // ... existing fields ...

    // Shader pipeline (optional)
    ShaderPipeline* shader_pipeline;
    bool shaders_enabled;
} GLVideoState;

void GLVideo_present(void) {
    // Get input texture (SW or HW)
    GLuint input_texture = getInputTexture();
    unsigned width = getInputWidth();
    unsigned height = getInputHeight();

    if (state.shaders_enabled && state.shader_pipeline) {
        // Shader pipeline handles final presentation
        GLShaderPipeline_render(state.shader_pipeline,
                               input_texture, width, height);
    } else {
        // Simple passthrough (current implementation)
        presentSimple(input_texture, width, height);
    }

    SDL_GL_SwapWindow(state.window);
}
```

## Bundled Shaders

Minimal initial collection (can expand later):

```
skeleton/SYSTEM/common/shaders/
├── stock.glsl          # Passthrough (no effect)
├── scanlines.glsl      # CRT scanlines
├── lcd-grid.glsl       # LCD subpixel grid
├── sharp-bilinear.glsl # Sharp upscaling
└── crt-simple.glsl     # Basic CRT (scanlines + curvature)
```

## UI Integration

```c
// In-game menu additions:

Menu Item: "Shaders"
  ├─ "None" (stock.glsl)
  ├─ "Scanlines"
  ├─ "LCD Grid"
  ├─ "Sharp Upscale"
  └─ "CRT Effect"

When shader selected with parameters:
  ├─ "Shader Settings" submenu
  │   ├─ Parameter 1 (slider: min → max, step)
  │   ├─ Parameter 2 (slider)
  │   └─ ...
```

## Performance Considerations

1. **Binary caching**: First load compiles shader, saves .bin. Subsequent loads instant.
2. **Triple buffering**: Prevents stalls when shader reads previous frame texture.
3. **Uniform caching**: Get uniform locations once, reuse every frame.
4. **Minimal passes**: 3 max keeps frame time low even on weak GPUs.
5. **GLES2 baseline**: No advanced features, works on all Mali/PowerVR devices.

## Migration Path

**Phase 1: Foundation** (after unified GL refactor)

- Implement shader loading and compilation
- Add pragma parameter parsing
- Create stock.glsl (passthrough)
- Test with SW and HW cores

**Phase 2: Multi-pass**

- Implement FBO chain for intermediate passes
- Add scaling calculations
- Test with 2-3 pass shader presets

**Phase 3: UI Integration**

- Add shader selection to menu
- Per-game shader settings
- Parameter adjustment UI

**Phase 4: Polish**

- Binary caching for fast startup
- Bundled shader collection
- Performance profiling

## References

- **RetroArch**: ~/Code/RetroArch/gfx/drivers/gl2.c (multi-pass implementation)
- **NextUI**: ~/Code/NextUI (pragmatic 3-pass system, binary caching)
- **GLSL ES Spec**: OpenGL ES Shading Language 1.00 (GLES2 baseline)
