/*
** GeneralsX Web Port — OpenGL ES 3.0 / WebGL 2.0 Graphics Backend
** Implementation of the DX8Wrapper-compatible API using OpenGL ES 3.0.
**
** This file replaces dx8wrapper.cpp for Emscripten builds.
** The #ifdef __EMSCRIPTEN__ guard ensures it only compiles for web targets.
**
** KEY MAPPING (DX8 → OpenGL ES 3.0):
**   IDirect3DDevice8::DrawIndexedPrimitive → glDrawElements
**   IDirect3DDevice8::SetRenderState       → glEnable/glDisable/glBlendFunc/etc
**   IDirect3DDevice8::SetTexture           → glActiveTexture + glBindTexture
**   IDirect3DDevice8::SetTransform         → uniform mat4 in shader
**   IDirect3DDevice8::CreateVertexBuffer   → glGenBuffers + glBufferData
**   IDirect3DDevice8::CreateTexture        → glGenTextures + glTexImage2D
**   IDirect3DDevice8::BeginScene/EndScene  → no-op (GL has no scene concept)
**   IDirect3DDevice8::Present              → emscripten_webgl_commit_frame
**
** LICENSE: GPL-3.0
*/

#if defined(__EMSCRIPTEN__)

#include "gles3_wrapper.h"
#include <cstring>
#include <cstdio>
#include <cmath>
#include <vector>

// Forward-declare SDL types so we don't need <SDL.h> at compile time.
// The linker resolves SDL_GL_SwapWindow via -sUSE_SDL=2 at link time.
// EmscriptenMain.cpp (which does include <SDL.h>) provides TheSDL3Window.
struct SDL_Window;
extern "C" void SDL_GL_SwapWindow(SDL_Window *window);

// The SDL window created in EmscriptenMain.cpp
extern SDL_Window *TheSDL3Window;

// ============================================================================
// Static state
// ============================================================================
#include <unordered_map>

static GLES3_PipelineState g_State;
static bool g_Initialized = false;
static std::unordered_map<GLuint, bool> g_TextureHasMipmaps;

void GLES3_Register_Texture(GLuint tex, bool has_mipmaps) {
    g_TextureHasMipmaps[tex] = has_mipmaps;
}

void GLES3_Unregister_Texture(GLuint tex) {
    g_TextureHasMipmaps.erase(tex);
}

bool GLES3_Texture_Has_Mipmaps(GLuint tex) {
    auto it = g_TextureHasMipmaps.find(tex);
    if (it != g_TextureHasMipmaps.end()) {
        return it->second;
    }
    return false;
}


// Shader manager statics
GLuint GLES3_ShaderManager::cached_programs[256] = {0};
GLES3_ProgramUniformLocations GLES3_ShaderManager::cached_locations[256];

struct GLES3_CachedUniformState {
    bool initialized;
    float world_matrix[16];
    float view_matrix[16];
    float projection_matrix[16];
    float material_diffuse[4];
    float material_ambient[4];
    float material_specular[4];
    float material_emissive[4];
    float material_power;
    int lighting_enabled;
    float global_ambient[4];
    struct {
        float diffuse[4];
        float position[3];
        float direction[3];
        int enabled;
        int type;
        float attenuation0;
        float attenuation1;
        float attenuation2;
        float ambient[4];
        float range;
    } lights[4];
    int fog_enabled;
    float fog_color[3];
    float fog_start;
    float fog_end;
    int alpha_test_enabled;
    float alpha_ref;
    int alpha_func;
    int stage_ops[8];
    int specular_enable;
    unsigned int pixel_shader;
    float ps_const[8][4];
    float light_specular[4][4];
    int color_vertex;
    int stage_args[24];
    int tex_bound;
    int pretransformed;
    float viewport_rect[4];
    float texture_factor[4];
    int tex_coord_index[4];
    int tex_transform[4];
    float tex_matrix[4][16];
};

static GLES3_CachedUniformState cached_uniform_states[256] = {};

// ============================================================================
// Helper: DX8 Blend Mode → GL Blend Factor
// ============================================================================
namespace GLES3_Helpers {

GLenum Blend_To_GL(unsigned int dx8_blend) {
    switch (dx8_blend) {
        case GLES3_BLEND_ZERO:         return GL_ZERO;
        case GLES3_BLEND_ONE:          return GL_ONE;
        case GLES3_BLEND_SRCCOLOR:     return GL_SRC_COLOR;
        case GLES3_BLEND_INVSRCCOLOR:  return GL_ONE_MINUS_SRC_COLOR;
        case GLES3_BLEND_SRCALPHA:     return GL_SRC_ALPHA;
        case GLES3_BLEND_INVSRCALPHA:  return GL_ONE_MINUS_SRC_ALPHA;
        case GLES3_BLEND_DESTALPHA:    return GL_DST_ALPHA;
        case GLES3_BLEND_INVDESTALPHA: return GL_ONE_MINUS_DST_ALPHA;
        case GLES3_BLEND_DESTCOLOR:    return GL_DST_COLOR;
        case GLES3_BLEND_INVDESTCOLOR: return GL_ONE_MINUS_DST_COLOR;
        case GLES3_BLEND_SRCALPHASAT:  return GL_SRC_ALPHA_SATURATE;
        default: return GL_ONE;
    }
}

GLenum StencilOp_To_GL(unsigned int dx8_op) {
    switch (dx8_op) {
        case 2: return GL_ZERO;
        case 3: return GL_REPLACE;
        case 4: return GL_INCR;       // D3DSTENCILOP_INCRSAT
        case 5: return GL_DECR;       // D3DSTENCILOP_DECRSAT
        case 6: return GL_INVERT;
        case 7: return GL_INCR_WRAP;  // D3DSTENCILOP_INCR
        case 8: return GL_DECR_WRAP;  // D3DSTENCILOP_DECR
        default: return GL_KEEP;      // 1 = KEEP
    }
}

GLenum CmpFunc_To_GL(unsigned int dx8_cmp) {
    switch (dx8_cmp) {
        case GLES3_CMP_NEVER:        return GL_NEVER;
        case GLES3_CMP_LESS:         return GL_LESS;
        case GLES3_CMP_EQUAL:        return GL_EQUAL;
        case GLES3_CMP_LESSEQUAL:    return GL_LEQUAL;
        case GLES3_CMP_GREATER:      return GL_GREATER;
        case GLES3_CMP_NOTEQUAL:     return GL_NOTEQUAL;
        case GLES3_CMP_GREATEREQUAL: return GL_GEQUAL;
        case GLES3_CMP_ALWAYS:       return GL_ALWAYS;
        default: return GL_LEQUAL;
    }
}

GLenum CullMode_To_GL(unsigned int dx8_cull) {
    // DX8: 1=NONE, 2=CW, 3=CCW.  glFrontFace(GL_CW) is set at init, so:
    //   CW-wound  polygons = GL "front faces"
    //   CCW-wound polygons = GL "back faces"
    //
    // D3DCULL_CW(2)  = cull CW-wound polygons  → GL_FRONT
    // D3DCULL_CCW(3) = cull CCW-wound polygons → GL_BACK  ← DX8 default
    // (previous mapping was reversed — caused inside-out rendering whenever
    //  the game explicitly set D3DRS_CULLMODE to D3DCULL_CCW)
    switch (dx8_cull) {
        case 1: return 0;           // NONE — caller disables GL_CULL_FACE
        case 2: return GL_FRONT;    // D3DCULL_CW  → cull CW-wound faces
        case 3: return GL_BACK;     // D3DCULL_CCW → cull CCW-wound faces
        default: return GL_BACK;
    }
}

GLenum PrimType_To_GL(unsigned int dx8_prim) {
    switch (dx8_prim) {
        case GLES3_PT_POINTLIST:     return GL_POINTS;
        case GLES3_PT_LINELIST:      return GL_LINES;
        case GLES3_PT_LINESTRIP:     return GL_LINE_STRIP;
        case GLES3_PT_TRIANGLELIST:  return GL_TRIANGLES;
        case GLES3_PT_TRIANGLESTRIP: return GL_TRIANGLE_STRIP;
        case GLES3_PT_TRIANGLEFAN:   return GL_TRIANGLE_FAN;
        default: return GL_TRIANGLES;
    }
}

GLenum AddressMode_To_GL(unsigned int dx8_addr) {
    // DX8: 1=WRAP, 2=MIRROR, 3=CLAMP
    switch (dx8_addr) {
        case 1: return GL_REPEAT;
        case 2: return GL_MIRRORED_REPEAT;
        case 3: return GL_CLAMP_TO_EDGE;
        default: return GL_REPEAT;
    }
}

GLenum FilterMode_To_GL(unsigned int dx8_filter) {
    // DX8: 1=POINT, 2=LINEAR, 3=ANISOTROPIC
    switch (dx8_filter) {
        case 1: return GL_NEAREST;
        case 2: return GL_LINEAR;
        case 3: return GL_LINEAR; // anisotropic handled via extension
        default: return GL_LINEAR;
    }
}

void Matrix4x4_To_GL(const float* src_row_major, float* dst_col_major) {
    // DX8 uses row-major matrices, OpenGL uses column-major.
    // Since we multiply matrices on the left of column vectors in the shader
    // (e.g., u_World * vec4), and DX8 multiplies row vectors on the left of row-major matrices,
    // the row-major memory layout read by GL as column-major is already the correct transpose (M^T).
    // Therefore, we do NOT transpose the memory in code. Doing so would warp translations and projection.
    memcpy(dst_col_major, src_row_major, 16 * sizeof(float));
}

} // namespace GLES3_Helpers


// ============================================================================
// DX8Wrapper API Implementation — Lifecycle
// ============================================================================

/*
** DX8Wrapper::Init — Create WebGL 2.0 context via Emscripten
**
** Original DX8 version: Loads d3d8.dll, creates IDirect3D8, enumerates
** adapters, creates IDirect3DDevice8 with present parameters.
**
** WebGL version: Creates an Emscripten WebGL2 context on the canvas element.
*/
// This would be called as: DX8Wrapper::Init(canvas_handle)
bool GLES3_Init(void* hwnd, bool lite) {
    if (g_Initialized) return true;

    // Create WebGL 2.0 context
    EmscriptenWebGLContextAttributes attrs;
    emscripten_webgl_init_context_attributes(&attrs);
    attrs.majorVersion = 2;  // WebGL 2.0 = OpenGL ES 3.0
    attrs.minorVersion = 0;
    attrs.alpha = false;
    attrs.depth = true;
    attrs.stencil = true;
    attrs.antialias = true;
    attrs.premultipliedAlpha = false;
    attrs.preserveDrawingBuffer = false;
    attrs.powerPreference = EM_WEBGL_POWER_PREFERENCE_HIGH_PERFORMANCE;

    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("#canvas", &attrs);
    if (ctx <= 0) {
        fprintf(stderr, "ERROR: Failed to create WebGL 2.0 context (code: %d)\n", ctx);
        return false;
    }
    emscripten_webgl_make_context_current(ctx);

    fprintf(stderr, "INFO: WebGL 2.0 context created successfully\n");
    fprintf(stderr, "INFO: GL_RENDERER: %s\n", glGetString(GL_RENDERER));
    fprintf(stderr, "INFO: GL_VERSION: %s\n", glGetString(GL_VERSION));
    fprintf(stderr, "INFO: GL_SHADING_LANGUAGE_VERSION: %s\n",
            glGetString(GL_SHADING_LANGUAGE_VERSION));

    // Initialize state
    memset(&g_State, 0, sizeof(g_State));
    g_State.texture_factor[0] = 1.0f;
    g_State.texture_factor[1] = 1.0f;
    g_State.texture_factor[2] = 1.0f;
    g_State.texture_factor[3] = 1.0f;
    for (int i = 0; i < 8; i++) {
        g_State.texture_stage_state[i][GLES3_TSS_ADDRESSU] = 1; // WRAP
        g_State.texture_stage_state[i][GLES3_TSS_ADDRESSV] = 1; // WRAP
        g_State.texture_stage_state[i][GLES3_TSS_MAGFILTER] = 2; // LINEAR
        g_State.texture_stage_state[i][GLES3_TSS_MINFILTER] = 2; // LINEAR
        g_State.texture_stage_state[i][GLES3_TSS_MIPFILTER] = 0; // NONE
        g_State.texture_stage_state[i][GLES3_TSS_TEXCOORDINDEX] = i; // D3D default: UV set = stage
        // D3D defaults: ARG1 = TEXTURE, ARG2 = CURRENT, ARG0 = CURRENT
        g_State.texture_stage_state[i][GLES3_TSS_COLORARG1] = 2;
        g_State.texture_stage_state[i][GLES3_TSS_COLORARG2] = 1;
        g_State.texture_stage_state[i][GLES3_TSS_ALPHAARG1] = 2;
        g_State.texture_stage_state[i][GLES3_TSS_ALPHAARG2] = 1;
        g_State.texture_stage_state[i][GLES3_TSS_COLORARG0] = 1;
        g_State.texture_stage_state[i][GLES3_TSS_ALPHAARG0] = 1;
        // Stage 0 modulates texture and diffuse; later stages start disabled.
        g_State.texture_stage_state[i][GLES3_TSS_COLOROP] = i == 0 ? 4 : 1;
        g_State.texture_stage_state[i][GLES3_TSS_ALPHAOP] = i == 0 ? 2 : 1;
    }
    // D3D stencil defaults, which are also GL's: func ALWAYS, ops KEEP, masks all ones.
    // GL is set up below with depth test and depth writes on; record that too.
    g_State.render_states[GLES3_RS_ZENABLE] = 1;
    g_State.render_states[GLES3_RS_ZWRITEENABLE] = 1;
    g_State.render_states[GLES3_RS_STENCILFUNC] = 8;
    g_State.render_states[GLES3_RS_STENCILFAIL] = 1;
    g_State.render_states[GLES3_RS_STENCILZFAIL] = 1;
    g_State.render_states[GLES3_RS_STENCILPASS] = 1;
    g_State.render_states[GLES3_RS_STENCILMASK] = 0xFFFFFFFFu;
    g_State.render_states[GLES3_RS_STENCILWRITEMASK] = 0xFFFFFFFFu;


    // Set identity matrices
    float identity[16] = {
        1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
    };
    memcpy(g_State.world_matrix, identity, sizeof(identity));
    memcpy(g_State.view_matrix, identity, sizeof(identity));
    memcpy(g_State.projection_matrix, identity, sizeof(identity));
    for (int i = 0; i < 8; i++)
        memcpy(g_State.texture_matrices[i], identity, sizeof(identity));

    // Default material (white)
    g_State.material.diffuse[0] = g_State.material.diffuse[1] =
    g_State.material.diffuse[2] = g_State.material.diffuse[3] = 1.0f;
    g_State.material.ambient[0] = g_State.material.ambient[1] =
    g_State.material.ambient[2] = 0.2f;
    g_State.material.ambient[3] = 1.0f;

    // Default global ambient (gray/dark gray fallback matching shader)
    g_State.global_ambient[0] = g_State.global_ambient[1] =
    g_State.global_ambient[2] = 0.2f;
    g_State.global_ambient[3] = 1.0f;

    // Create VAO (required in OpenGL ES 3.0 / WebGL 2.0)
    glGenVertexArrays(1, &g_State.vao);
    glBindVertexArray(g_State.vao);

    // Default GL state matching DX8 defaults
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);  // DX8 default winding

    // Initialize shader manager
    if (!GLES3_ShaderManager::Init()) {
        fprintf(stderr, "ERROR: Failed to initialize shader manager\n");
        return false;
    }

    g_Initialized = true;
    return true;
}

void GLES3_Shutdown() {
    if (!g_Initialized) return;

    GLES3_ShaderManager::Shutdown();

    if (g_State.vao) {
        glDeleteVertexArrays(1, &g_State.vao);
        g_State.vao = 0;
    }

    g_Initialized = false;
}


// ============================================================================
// DX8Wrapper API — Scene Management
// ============================================================================

/*
** BeginScene/EndScene — In DX8 these bracket a frame.
** In OpenGL there's no equivalent — these are essentially no-ops.
** We use them to reset per-frame statistics.
*/
void GLES3_Begin_Scene() {
    g_State.draw_calls = 0;
    g_State.triangles_drawn = 0;
}

void GLES3_End_Scene(bool flip_frame) {
    if (flip_frame && TheSDL3Window) {
        SDL_GL_SwapWindow(TheSDL3Window);
    }
}

void GLES3_Swap_Buffers() {
    if (TheSDL3Window) {
        SDL_GL_SwapWindow(TheSDL3Window);
    }
}

void GLES3_Clear(bool clear_color, bool clear_z_stencil,
                  float r, float g, float b, float a,
                  float z, unsigned int stencil) {
    GLbitfield mask = 0;
    if (clear_color) {
        glClearColor(r, g, b, a);
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (clear_z_stencil) {
        glClearDepthf(z);
        glClearStencil(stencil);
        mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }
    if (!mask) return;
    // D3D's Clear ignores the render states; GL's obeys its write masks and scissor.
    const unsigned int* rs = g_State.render_states;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glStencilMask(0xFFFFFFFFu);
    glClear(mask);
    glDepthMask(rs[GLES3_RS_ZWRITEENABLE] ? GL_TRUE : GL_FALSE);
    glStencilMask(rs[GLES3_RS_STENCILWRITEMASK]);
}


// ============================================================================
// DX8Wrapper API — Render State
// ============================================================================

/*
** Set_DX8_Render_State — Maps D3DRENDERSTATETYPE values to GL calls.
**
** The original dx8wrapper caches these to avoid redundant D3D calls.
** We do the same, plus translate to GL state.
*/
void GLES3_Set_Render_State(unsigned int state, unsigned int value) {
    if (state >= GLES3_RS_MAX) return;
    // No redundant-state skip: DX8Wrapper already drops repeats against its own cache,
    // which starts at 0x12345678 while this one starts at 0, so a skip here lost the
    // engine's first "set to 0" (TEXTUREFACTOR, AMBIENT) and kept our start values.
    g_State.render_states[state] = value;

    switch (state) {
        case GLES3_RS_ZENABLE:
            if (value) glEnable(GL_DEPTH_TEST);
            else       glDisable(GL_DEPTH_TEST);
            break;

        case GLES3_RS_ZWRITEENABLE:
            glDepthMask(value ? GL_TRUE : GL_FALSE);
            break;

        case GLES3_RS_ZFUNC:
            glDepthFunc(GLES3_Helpers::CmpFunc_To_GL(value));
            break;

        case GLES3_RS_ALPHABLENDENABLE:
            if (value) glEnable(GL_BLEND);
            else       glDisable(GL_BLEND);
            break;

        case GLES3_RS_SRCBLEND:
            glBlendFunc(
                GLES3_Helpers::Blend_To_GL(value),
                GLES3_Helpers::Blend_To_GL(
                    g_State.render_states[GLES3_RS_DESTBLEND]));
            break;

        case GLES3_RS_DESTBLEND:
            glBlendFunc(
                GLES3_Helpers::Blend_To_GL(
                    g_State.render_states[GLES3_RS_SRCBLEND]),
                GLES3_Helpers::Blend_To_GL(value));
            break;

        case GLES3_RS_CULLMODE:
            if (value == 1) { // D3DCULL_NONE
                glDisable(GL_CULL_FACE);
            } else {
                glEnable(GL_CULL_FACE);
                glCullFace(GLES3_Helpers::CullMode_To_GL(value));
            }
            break;

        case GLES3_RS_ALPHATESTENABLE:
            // OpenGL ES 3.0 removed alpha test — emulated in fragment shader
            g_State.alpha_test_enabled = (value != 0);
            break;

        case GLES3_RS_ALPHAREF:
            g_State.alpha_ref = value;
            break;

        case GLES3_RS_ALPHAFUNC:
            g_State.alpha_func = value;
            break;

        case GLES3_RS_FOGENABLE:
            g_State.fog_enabled = (value != 0);
            break;

        case GLES3_RS_FOGCOLOR: {
            // DX8 packs as 0xAARRGGBB
            g_State.fog_color[0] = ((value >> 16) & 0xFF) / 255.0f;
            g_State.fog_color[1] = ((value >>  8) & 0xFF) / 255.0f;
            g_State.fog_color[2] = ((value      ) & 0xFF) / 255.0f;
            break;
        }

        case GLES3_RS_LIGHTING:
            g_State.lighting_enabled = (value != 0);
            break;

        case GLES3_RS_AMBIENT: {
            g_State.global_ambient[0] = ((value >> 16) & 0xFF) / 255.0f;
            g_State.global_ambient[1] = ((value >>  8) & 0xFF) / 255.0f;
            g_State.global_ambient[2] = ((value      ) & 0xFF) / 255.0f;
            g_State.global_ambient[3] = ((value >> 24) & 0xFF) / 255.0f;
            break;
        }

        case GLES3_RS_STENCILENABLE:
            if (value) glEnable(GL_STENCIL_TEST);
            else       glDisable(GL_STENCIL_TEST);
            break;

        case GLES3_RS_STENCILFUNC:
        case GLES3_RS_STENCILREF:
        case GLES3_RS_STENCILMASK: {
            const unsigned int* rs = g_State.render_states;
            glStencilFunc(GLES3_Helpers::CmpFunc_To_GL(rs[GLES3_RS_STENCILFUNC]),
                          (GLint)rs[GLES3_RS_STENCILREF], rs[GLES3_RS_STENCILMASK]);
            break;
        }

        case GLES3_RS_STENCILFAIL:
        case GLES3_RS_STENCILZFAIL:
        case GLES3_RS_STENCILPASS: {
            const unsigned int* rs = g_State.render_states;
            glStencilOp(GLES3_Helpers::StencilOp_To_GL(rs[GLES3_RS_STENCILFAIL]),
                        GLES3_Helpers::StencilOp_To_GL(rs[GLES3_RS_STENCILZFAIL]),
                        GLES3_Helpers::StencilOp_To_GL(rs[GLES3_RS_STENCILPASS]));
            break;
        }

        case GLES3_RS_STENCILWRITEMASK:
            glStencilMask(value);
            break;

        // D3D passes these floats as their bit patterns (DX8Wrapper::Set_Fog).
        case GLES3_RS_FOGSTART:
            memcpy(&g_State.fog_start, &value, sizeof(float));
            break;
        case GLES3_RS_FOGEND:
            memcpy(&g_State.fog_end, &value, sizeof(float));
            break;

        // States handled in shader uniforms (no direct GL equivalent):
        case GLES3_RS_FOGDENSITY:
        case GLES3_RS_COLORVERTEX:
        case GLES3_RS_NORMALIZENORMALS:
            // Stored in g_State, applied via shader uniforms
            break;

        case GLES3_RS_TEXTUREFACTOR: {
            // Unpack 0xAARRGGBB
            g_State.texture_factor[0] = ((value >> 16) & 0xFF) / 255.0f; // Red
            g_State.texture_factor[1] = ((value >>  8) & 0xFF) / 255.0f; // Green
            g_State.texture_factor[2] = ((value      ) & 0xFF) / 255.0f; // Blue
            g_State.texture_factor[3] = ((value >> 24) & 0xFF) / 255.0f; // Alpha
            break;
        }

        case GLES3_RS_ZBIAS:
            if (value > 0) {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(-1.0f, -((float)value) * 2.0f);
            } else {
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
            break;

        default:
            // Unhandled state — log for debugging
            // fprintf(stderr, "WARN: Unhandled render state %d = %u\n", state, value);
            break;
    }
}


// ============================================================================
// DX8Wrapper API — Transforms
// ============================================================================

void GLES3_Set_Transform(unsigned int type, const float* matrix_row_major) {
    float* target = nullptr;
    switch (type) {
        case GLES3_TS_WORLD:      target = g_State.world_matrix; break;
        case GLES3_TS_VIEW:       target = g_State.view_matrix; break;
        case GLES3_TS_PROJECTION: target = g_State.projection_matrix; break;
        default:
            if (type >= GLES3_TS_TEXTURE0 && type <= GLES3_TS_TEXTURE3)
                target = g_State.texture_matrices[type - GLES3_TS_TEXTURE0];
            break;
    }
    if (target) {
        memcpy(target, matrix_row_major, 16 * sizeof(float));
    }
}

void GLES3_Get_Transform(unsigned int type, float* matrix_out) {
    const float* src = nullptr;
    switch (type) {
        case GLES3_TS_WORLD:      src = g_State.world_matrix; break;
        case GLES3_TS_VIEW:       src = g_State.view_matrix; break;
        case GLES3_TS_PROJECTION: src = g_State.projection_matrix; break;
        default:
            if (type >= GLES3_TS_TEXTURE0 && type <= GLES3_TS_TEXTURE3)
                src = g_State.texture_matrices[type - GLES3_TS_TEXTURE0];
            break;
    }
    if (src) memcpy(matrix_out, src, 16 * sizeof(float));
}


// ============================================================================
// DX8Wrapper API — Viewport
// ============================================================================

void GLES3_Set_Viewport(unsigned int x, unsigned int y,
                         unsigned int w, unsigned int h,
                         float min_z, float max_z) {
    g_State.viewport = { x, y, w, h, min_z, max_z };
    
    int canvas_w = 800;
    int canvas_h = 600;
    emscripten_get_canvas_element_size("#canvas", &canvas_w, &canvas_h);
    
    // Direct3D viewport Y coordinate starts at top-left.
    // OpenGL viewport Y coordinate starts at bottom-left.
    int gl_y = canvas_h - (int)y - (int)h;
    if (gl_y < 0) gl_y = 0;
    
    glViewport(x, (unsigned int)gl_y, w, h);
    glDepthRangef(min_z, max_z);
}


// ============================================================================
// DX8Wrapper API — Texture Binding
// ============================================================================

void GLES3_Apply_Texture_Stage_Parameters(unsigned int stage) {
    GLuint tex = g_State.bound_textures[stage];
    if (tex == 0) return;

    glActiveTexture(GL_TEXTURE0 + stage);

    // Apply wrap mode U (S)
    unsigned int u = g_State.texture_stage_state[stage][GLES3_TSS_ADDRESSU];
    if (u != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GLES3_Helpers::AddressMode_To_GL(u));
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    }

    // Apply wrap mode V (T)
    unsigned int v = g_State.texture_stage_state[stage][GLES3_TSS_ADDRESSV];
    if (v != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GLES3_Helpers::AddressMode_To_GL(v));
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    }

    // Apply filter modes
    unsigned int mag = g_State.texture_stage_state[stage][GLES3_TSS_MAGFILTER];
    if (mag != 0) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GLES3_Helpers::FilterMode_To_GL(mag));
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    unsigned int min = g_State.texture_stage_state[stage][GLES3_TSS_MINFILTER];
    unsigned int mip = g_State.texture_stage_state[stage][GLES3_TSS_MIPFILTER];

    // Determine if texture has mipmaps
    bool has_mipmaps = GLES3_Texture_Has_Mipmaps(tex);

    if (min == 0) min = 1; // Default D3D8: POINT (1)

    GLenum gl_min_filter = GL_LINEAR;
    if (!has_mipmaps || mip == 0) {
        // No mipmapping
        gl_min_filter = GLES3_Helpers::FilterMode_To_GL(min);
    } else if (mip == 1) {
        // D3DTEXF_POINT: Nearest mipmap
        gl_min_filter = (min == 1) ? GL_NEAREST_MIPMAP_NEAREST : GL_LINEAR_MIPMAP_NEAREST;
    } else {
        // D3DTEXF_LINEAR: Linear mipmap interpolation
        gl_min_filter = (min == 1) ? GL_NEAREST_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_LINEAR;
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_min_filter);
}

void GLES3_Set_Texture(unsigned int stage, GLuint gl_texture) {
    if (stage >= 8) return;
    g_State.bound_textures[stage] = gl_texture;
    glActiveTexture(GL_TEXTURE0 + stage);
    if (gl_texture) {
        glBindTexture(GL_TEXTURE_2D, gl_texture);
        GLES3_Apply_Texture_Stage_Parameters(stage);
    } else {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}

void GLES3_Set_Texture_Stage_State(unsigned int stage, unsigned int state,
                                    unsigned int value) {
    if (stage >= 8 || state >= GLES3_TSS_MAX) return;
    g_State.texture_stage_state[stage][state] = value;

    if (g_State.bound_textures[stage] != 0) {
        GLES3_Apply_Texture_Stage_Parameters(stage);
    }
}



// ============================================================================
// DX8Wrapper API — Lighting
// ============================================================================

void GLES3_Set_Light(unsigned int index, const GLES3_Light* light) {
    if (index >= 4) return;
    memcpy(&g_State.lights[index], light, sizeof(GLES3_Light));
}

void GLES3_Set_Vertex_Has_Color(bool has_color) {
    g_State.vertex_has_color = has_color;
}

void GLES3_Set_Pretransformed(bool pretransformed) {
    g_State.pretransformed = pretransformed;
}

// ---------------------------------------------------------------------------
// ps.1.1 pixel shaders. The game's are short (terrain blend, flat terrain, roads,
// monochrome filter): tex, mov, add, sub, mul, mad, dp3, dp4, lrp with the usual source
// modifiers. Each is encoded for the fragment shader's interpreter (see u_PS there).
// ---------------------------------------------------------------------------
struct GLES3_PSProgram { int count; int data[16 * 8]; };
static std::vector<GLES3_PSProgram> g_PixelShaders;

// Register file index: r0-r1 0-1, t0-t3 2-5, v0-v1 6-7, c0-c7 8-15.
static int PS_Reg(unsigned int tok, bool& ok) {
    unsigned int type = (tok >> 28) & 7, num = tok & 0x7FF;
    if (type == 0 && num < 2) return (int)num;       // D3DSPR_TEMP
    if (type == 3 && num < 4) return 2 + (int)num;   // D3DSPR_TEXTURE
    if (type == 1 && num < 2) return 6 + (int)num;   // D3DSPR_INPUT
    if (type == 2 && num < 8) return 8 + (int)num;   // D3DSPR_CONST
    ok = false;
    return 0;
}

static int PS_Src(unsigned int tok, bool& ok) {
    int r = PS_Reg(tok, ok);
    unsigned int swz = (tok >> 16) & 0xFF, mod = (tok >> 24) & 0xF;
    int sw = swz == 0xE4 ? 0 : swz == 0xFF ? 1 : swz == 0xAA ? 2 : -1;
    if (sw < 0 || mod > 8) ok = false;
    return r | ((sw < 0 ? 0 : sw) << 5) | ((int)mod << 8);
}

unsigned int GLES3_Create_Pixel_Shader(const unsigned int* t) {
    if (!t || (t[0] & 0xFFFF0000u) != 0xFFFF0000u || (t[0] & 0xFFFF) > 0x0104) return 0;
    GLES3_PSProgram p = {};
    bool ok = true;
    for (unsigned int i = 1; ok && i < 512;) {
        unsigned int ins = t[i++], op = ins & 0xFFFF;
        if (op == 0xFFFF) break;                                     // end
        if (op == 0xFFFE) { i += (ins >> 16) & 0x7FFF; continue; }   // comment
        if (op == 0) continue;                                       // nop
        int nsrc;
        switch (op) {
            case 66: nsrc = 0; break;                                // tex
            case 1: nsrc = 1; break;                                 // mov
            case 2: case 3: case 5: case 8: case 9: nsrc = 2; break; // add sub mul dp3 dp4
            case 4: case 18: nsrc = 3; break;                        // mad lrp
            default: nsrc = -1; break;                               // texbem, def, cnd, ...
        }
        if (nsrc < 0 || p.count >= 16) { ok = false; break; }
        unsigned int dst = t[i++];
        int* d = &p.data[p.count++ * 8];
        if (op == 66) { d[0] = 66; continue; } // the stage samples are loaded up front
        d[0] = (int)op | (int)((dst >> 16) & 0xF) << 8 | (int)((dst >> 24) & 0xF) << 12
             | (int)((dst >> 20) & 1) << 16; // write mask, result shift, saturate
        d[1] = PS_Reg(dst, ok);
        for (int k = 0; k < nsrc; k++) d[2 + k] = PS_Src(t[i++], ok);
    }
    if (!ok || p.count == 0) return 0;
    g_PixelShaders.push_back(p);
    return (unsigned int)g_PixelShaders.size();
}

void GLES3_Set_Pixel_Shader(unsigned int handle) {
    g_State.pixel_shader = handle <= g_PixelShaders.size() ? handle : 0;
}

void GLES3_Set_Pixel_Shader_Constants(unsigned int first, const float* data, unsigned int count) {
    for (unsigned int i = 0; data && i < count && first + i < 8; i++)
        memcpy(g_State.ps_const[first + i], data + i * 4, sizeof(float) * 4);
}

static void GLES3_Upload_Pixel_Shader(const GLES3_ProgramUniformLocations& locs, unsigned int handle) {
    const GLES3_PSProgram* p = handle ? &g_PixelShaders[handle - 1] : nullptr;
    if (locs.u_PSCount != -1) glUniform1i(locs.u_PSCount, p ? p->count : 0);
    if (p && locs.u_PS != -1) glUniform4iv(locs.u_PS, p->count * 2, p->data);
}

void GLES3_Enable_Light(unsigned int index, bool enable) {
    if (index >= 4) return;
    g_State.light_enabled[index] = enable;
}

void GLES3_Set_Material(const GLES3_Material* mat) {
    memcpy(&g_State.material, mat, sizeof(GLES3_Material));
}


// ============================================================================
// DX8Wrapper API — Fog
// ============================================================================

void GLES3_Set_Fog(bool enable, float r, float g, float b,
                    float start, float end) {
    g_State.fog_enabled = enable;
    g_State.fog_color[0] = r;
    g_State.fog_color[1] = g;
    g_State.fog_color[2] = b;
    g_State.fog_start = start;
    g_State.fog_end = end;
}


// ============================================================================
// DX8Wrapper API — Drawing
// ============================================================================

/*
** Apply_Pipeline_State — Called before every draw call.
** Uploads the current DX8 state to the emulation shader as uniforms.
** This is where the DX8 fixed-function pipeline gets translated to GLSL.
*/
static void Apply_Pipeline_State() {
    GLuint program = GLES3_ShaderManager::Get_Program(g_State);
    if (program != g_State.active_program) {
        glUseProgram(program);
        g_State.active_program = program;
    }
    GLES3_ShaderManager::Apply_Uniforms(program, g_State);
}

/*
** Draw_Triangles — The main draw call. Maps to glDrawElements.
**
** Original DX8: IDirect3DDevice8::DrawIndexedPrimitive(
**   D3DPT_TRIANGLELIST, min_vertex, num_vertices,
**   start_index, primitive_count)
**
** OpenGL ES 3.0: glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, offset)
*/
void GLES3_Draw_Triangles(unsigned int prim_type,
                           unsigned int start_index,
                           unsigned int prim_count,
                           unsigned int min_vertex,
                           unsigned int num_vertices) {
    Apply_Pipeline_State();

    GLenum gl_prim = GLES3_Helpers::PrimType_To_GL(prim_type);

    unsigned int index_count = 0;
    switch (prim_type) {
        case GLES3_PT_TRIANGLELIST:  index_count = prim_count * 3; break;
        case GLES3_PT_TRIANGLESTRIP: index_count = prim_count + 2; break;
        case GLES3_PT_TRIANGLEFAN:   index_count = prim_count + 2; break;
        case GLES3_PT_LINELIST:      index_count = prim_count * 2; break;
        case GLES3_PT_LINESTRIP:     index_count = prim_count + 1; break;
        case GLES3_PT_POINTLIST:     index_count = prim_count; break;
    }

    glDrawElements(gl_prim, index_count, GL_UNSIGNED_SHORT,
                   (const void*)(start_index * sizeof(unsigned short)));

    g_State.draw_calls++;
    g_State.triangles_drawn += prim_count;
}

void GLES3_Draw_Triangles_Instanced(unsigned int prim_type,
                                    unsigned int start_index,
                                    unsigned int prim_count,
                                    unsigned int min_vertex,
                                    unsigned int num_vertices,
                                    const float* world_matrices,
                                    unsigned int instance_count) {
    if (instance_count == 0) return;
    Apply_Pipeline_State();

    GLenum gl_prim = GLES3_Helpers::PrimType_To_GL(prim_type);

    unsigned int index_count = 0;
    switch (prim_type) {
        case GLES3_PT_TRIANGLELIST:  index_count = prim_count * 3; break;
        case GLES3_PT_TRIANGLESTRIP: index_count = prim_count + 2; break;
        case GLES3_PT_TRIANGLEFAN:   index_count = prim_count + 2; break;
        case GLES3_PT_LINELIST:      index_count = prim_count * 2; break;
        case GLES3_PT_LINESTRIP:     index_count = prim_count + 1; break;
        case GLES3_PT_POINTLIST:     index_count = prim_count; break;
    }

    // Transpose row-major matrices in world_matrices to column-major for WebGL
    float* col_major_matrices = (float*)malloc(instance_count * 16 * sizeof(float));
    for (unsigned int inst = 0; inst < instance_count; ++inst) {
        GLES3_Helpers::Matrix4x4_To_GL(world_matrices + inst * 16, col_major_matrices + inst * 16);
    }

    static GLuint g_InstanceVBO = 0;
    if (g_InstanceVBO == 0) {
        glGenBuffers(1, &g_InstanceVBO);
    }
    glBindBuffer(GL_ARRAY_BUFFER, g_InstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instance_count * 16 * sizeof(float), col_major_matrices, GL_STREAM_DRAW);
    free(col_major_matrices);

    // Setup instanced matrix attributes at layout locations 7, 8, 9, 10
    for (int i = 0; i < 4; ++i) {
        glEnableVertexAttribArray(7 + i);
        glVertexAttribPointer(7 + i, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float), (const void*)(uintptr_t)(i * 4 * sizeof(float)));
        glVertexAttribDivisor(7 + i, 1);
    }

    // Get cached instancing uniform location
    GLint loc = GLES3_ShaderManager::Get_Instancing_Uniform_Location(g_State);
    if (loc != -1) {
        glUniform1i(loc, 1);
    }

    glDrawElementsInstanced(gl_prim, index_count, GL_UNSIGNED_SHORT,
                           (const void*)(start_index * sizeof(unsigned short)), instance_count);

    if (loc != -1) {
        glUniform1i(loc, 0);
    }
    for (int i = 0; i < 4; ++i) {
        glDisableVertexAttribArray(7 + i);
        glVertexAttribDivisor(7 + i, 0);
    }

    // Re-bind the zero VBO so we don't pollute subsequent draws
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_State.draw_calls++;
    g_State.triangles_drawn += prim_count * instance_count;
}


/*
** Draw (non-indexed) — Maps to glDrawArrays.
*/
void GLES3_Draw_Arrays(unsigned int prim_type,
                        unsigned int start_vertex,
                        unsigned int prim_count) {
    Apply_Pipeline_State();

    GLenum gl_prim = GLES3_Helpers::PrimType_To_GL(prim_type);
    unsigned int vertex_count = 0;
    switch (prim_type) {
        case GLES3_PT_TRIANGLELIST:  vertex_count = prim_count * 3; break;
        case GLES3_PT_TRIANGLESTRIP: vertex_count = prim_count + 2; break;
        default: vertex_count = prim_count; break;
    }

    glDrawArrays(gl_prim, start_vertex, vertex_count);

    g_State.draw_calls++;
    g_State.triangles_drawn += prim_count;
}


// ============================================================================
// DX8Wrapper API — Vertex/Index Buffer Management
// ============================================================================

GLES3_VertexBuffer* GLES3_VertexBuffer::Create(unsigned int size,
                                                 unsigned int stride,
                                                 unsigned int fvf,
                                                 bool dynamic) {
    auto* vb = new GLES3_VertexBuffer();
    vb->size = size;
    vb->stride = stride;
    vb->fvf = fvf;
    vb->is_dynamic = dynamic;
    vb->lock_ptr = nullptr;

    glGenBuffers(1, &vb->gl_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vb->gl_buffer);
    glBufferData(GL_ARRAY_BUFFER, size,
                 nullptr, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    return vb;
}

void GLES3_VertexBuffer::Lock(void** data, unsigned int offset,
                               unsigned int lock_size) {
    if (!lock_ptr) {
        lock_ptr = malloc(size);
    }
    *data = (char*)lock_ptr + offset;
}

void GLES3_VertexBuffer::Unlock() {
    if (lock_ptr) {
        glBindBuffer(GL_ARRAY_BUFFER, gl_buffer);
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, lock_ptr);
    }
}

void GLES3_VertexBuffer::Destroy() {
    if (gl_buffer) {
        glDeleteBuffers(1, &gl_buffer);
        gl_buffer = 0;
    }
    if (lock_ptr) {
        free(lock_ptr);
        lock_ptr = nullptr;
    }
    delete this;
}


GLES3_IndexBuffer* GLES3_IndexBuffer::Create(unsigned int size, bool dynamic) {
    auto* ib = new GLES3_IndexBuffer();
    ib->size = size;
    ib->index_count = size / sizeof(unsigned short);
    ib->is_dynamic = dynamic;
    ib->lock_ptr = nullptr;

    glGenBuffers(1, &ib->gl_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ib->gl_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, size,
                 nullptr, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    return ib;
}

void GLES3_IndexBuffer::Lock(void** data, unsigned int offset,
                              unsigned int lock_size) {
    if (!lock_ptr) {
        lock_ptr = malloc(size);
    }
    *data = (char*)lock_ptr + offset;
}

void GLES3_IndexBuffer::Unlock() {
    if (lock_ptr) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_buffer);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, size, lock_ptr);
    }
}

void GLES3_IndexBuffer::Destroy() {
    if (gl_buffer) {
        glDeleteBuffers(1, &gl_buffer);
        gl_buffer = 0;
    }
    if (lock_ptr) {
        free(lock_ptr);
        lock_ptr = nullptr;
    }
    delete this;
}


// ============================================================================
// Texture Format Conversion
// ============================================================================

GLenum GLES3_TextureConverter::DX8_Format_To_GL_Internal(unsigned int dx8_fmt) {
    // Map common DX8 formats (D3DFMT_*) to GL internal formats
    // These are the formats actually used by Generals/Zero Hour
    switch (dx8_fmt) {
        case 21: // D3DFMT_A8R8G8B8
            return GL_RGBA8;
        case 22: // D3DFMT_X8R8G8B8
            return GL_RGBA8; // treat as RGBA, ignore alpha
        case 23: // D3DFMT_R5G6B5
            return GL_RGB565;
        case 25: // D3DFMT_A1R5G5B5
            return GL_RGB5_A1;
        case 26: // D3DFMT_A4R4G4B4
            return GL_RGBA4;
        case 20: // D3DFMT_R8G8B8 (expanded to RGBA by To_GL_Pixels)
        case 50: // D3DFMT_L8
        case 51: // D3DFMT_A8L8
        case 28: // D3DFMT_A8
            return GL_RGBA8;
        case 827611204: // D3DFMT_DXT1 (FOURCC)
            return GL_COMPRESSED_RGB_S3TC_DXT1_EXT; // if available
        case 861165636: // D3DFMT_DXT3
            return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        case 894720068: // D3DFMT_DXT5
            return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        default:
            fprintf(stderr, "WARN: Unknown DX8 texture format %u, defaulting to RGBA8\n",
                    dx8_fmt);
            return GL_RGBA8;
    }
}

// ---------------------------------------------------------------------------
// To_GL_Pixels — D3D8 pixel memory to a GL source format + type.
//
// WebGL2 has no GL_TEXTURE_SWIZZLE_* (texParameteri fails with INVALID_ENUM), so
// channel orders GL can't read directly are converted on the CPU: BGRA/BGRX/BGR
// to RGBA, L8 / A8L8 / A8 to RGBA, and the 16-bit ARGB layouts to GL's RGBA ones.
// Returns `data` itself when no conversion is needed, else `tmp`'s storage.
// ---------------------------------------------------------------------------
static const void* To_GL_Pixels(unsigned int dx8_format, unsigned int width,
                                unsigned int height, const void* data,
                                std::vector<unsigned char>& tmp,
                                GLenum& src_fmt, GLenum& src_type) {
    const unsigned int n = width * height;
    const unsigned char* in = (const unsigned char*)data;
    src_fmt = GL_RGBA;
    src_type = GL_UNSIGNED_BYTE;
    switch (dx8_format) {
        case 23: // D3DFMT_R5G6B5 — same bit layout as GL_UNSIGNED_SHORT_5_6_5
            src_fmt = GL_RGB; src_type = GL_UNSIGNED_SHORT_5_6_5;
            return data;
        case 25: // D3DFMT_A1R5G5B5 → GL 5551 (alpha in the low bit)
        case 26: { // D3DFMT_A4R4G4B4 → GL 4444 (alpha in the low nibble)
            const bool a1 = dx8_format == 25;
            src_type = a1 ? GL_UNSIGNED_SHORT_5_5_5_1 : GL_UNSIGNED_SHORT_4_4_4_4;
            if (!data) return nullptr;
            tmp.resize(n * 2);
            const unsigned short* src = (const unsigned short*)data;
            unsigned short* dst = (unsigned short*)tmp.data();
            for (unsigned int i = 0; i < n; i++)
                dst[i] = a1 ? (unsigned short)((src[i] << 1) | (src[i] >> 15))
                            : (unsigned short)((src[i] << 4) | (src[i] >> 12));
            return tmp.data();
        }
        default: break;
    }
    // Everything else becomes RGBA8 bytes.
    if (!data) return nullptr;
    tmp.resize(n * 4);
    unsigned char* o = tmp.data();
    for (unsigned int i = 0; i < n; i++, o += 4) {
        switch (dx8_format) {
            case 20: { const unsigned char* p = in + i * 3;     // R8G8B8: B G R
                o[0] = p[2]; o[1] = p[1]; o[2] = p[0]; o[3] = 255; break; }
            case 22: { const unsigned char* p = in + i * 4;     // X8R8G8B8: B G R X
                o[0] = p[2]; o[1] = p[1]; o[2] = p[0]; o[3] = 255; break; }
            case 50: o[0] = o[1] = o[2] = in[i]; o[3] = 255; break;            // L8
            case 51: o[0] = o[1] = o[2] = in[i * 2]; o[3] = in[i * 2 + 1]; break; // A8L8: L A
            case 28: o[0] = o[1] = o[2] = 0; o[3] = in[i]; break;               // A8
            default: { const unsigned char* p = in + i * 4;     // A8R8G8B8 (and unknown): B G R A
                o[0] = p[2]; o[1] = p[1]; o[2] = p[0]; o[3] = p[3]; break; }
        }
    }
    return tmp.data();
}

// ---------------------------------------------------------------------------
// Create_Texture — Upload one mip level (level 0) of a D3D8 texture to GL.
//
// Uncompressed pixels go through To_GL_Pixels (channel order converted on the CPU).
//
// For DXT compressed textures glGenerateMipmap on a compressed base is
// unreliable; Upload_To_GL (d3d8.h) uploads each mip level separately and
// passes mip_levels=1 here to suppress the auto-generate call.
// ---------------------------------------------------------------------------
GLuint GLES3_TextureConverter::Create_Texture(unsigned int width,
                                               unsigned int height,
                                               unsigned int dx8_format,
                                               unsigned int mip_levels,
                                               const void* data) {
    // Uploads happen on whichever unit is active; put back what that unit had,
    // or another stage keeps sampling this texture.
    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum internal_fmt = DX8_Format_To_GL_Internal(dx8_format);

    bool is_compressed = (internal_fmt == GL_COMPRESSED_RGB_S3TC_DXT1_EXT  ||
                          internal_fmt == GL_COMPRESSED_RGBA_S3TC_DXT3_EXT ||
                          internal_fmt == GL_COMPRESSED_RGBA_S3TC_DXT5_EXT);

    if (is_compressed) {
        // WebGL2: WEBGL_compressed_texture_s3tc extension required.
        // The caller is responsible for uploading all mip levels if mip_levels > 1;
        // we only upload the base level here.
        unsigned int block_size = (internal_fmt == GL_COMPRESSED_RGB_S3TC_DXT1_EXT) ? 8 : 16;
        unsigned int data_size = ((width + 3) / 4) * ((height + 3) / 4) * block_size;
        // Set mip level storage range so the driver knows more levels will arrive
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (int)mip_levels - 1);
        glCompressedTexImage2D(GL_TEXTURE_2D, 0, internal_fmt,
                               (GLsizei)width, (GLsizei)height, 0,
                               (GLsizei)data_size, data);
    } else {
        GLenum src_fmt, src_type;
        std::vector<unsigned char> temp_buffer;
        const void* upload_data = To_GL_Pixels(dx8_format, width, height, data,
                                               temp_buffer, src_fmt, src_type);
        glTexImage2D(GL_TEXTURE_2D, 0, internal_fmt,
                     (GLsizei)width, (GLsizei)height, 0,
                     src_fmt, src_type, upload_data);

        // Generate mipmaps from the uploaded base level (uncompressed only)
        if (mip_levels != 1) {
            glGenerateMipmap(GL_TEXTURE_2D);
        }
    }

    if (mip_levels != 1) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    GLES3_Register_Texture(tex, mip_levels > 1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
    return tex;
}


// ---------------------------------------------------------------------------
// Update_Texture — Update one mip level (level 0) of an existing D3D8 texture in GL.
// ---------------------------------------------------------------------------
void GLES3_TextureConverter::Update_Texture(GLuint gl_tex_id,
                                            unsigned int width,
                                            unsigned int height,
                                            unsigned int dx8_format,
                                            unsigned int mip_levels,
                                            const void* data) {
    if (!gl_tex_id || !data) return;
    GLint prev_tex = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
    glBindTexture(GL_TEXTURE_2D, gl_tex_id);

    GLenum src_fmt, src_type;
    std::vector<unsigned char> temp_buffer;
    const void* upload_data = To_GL_Pixels(dx8_format, width, height, data,
                                           temp_buffer, src_fmt, src_type);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    (GLsizei)width, (GLsizei)height,
                    src_fmt, src_type, upload_data);

    if (mip_levels > 1) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
    glBindTexture(GL_TEXTURE_2D, (GLuint)prev_tex);
}



// ============================================================================
// Shader Manager Implementation
// ============================================================================

bool GLES3_ShaderManager::Init() {
    memset(cached_programs, 0, sizeof(cached_programs));
    memset(cached_locations, -1, sizeof(cached_locations));
    // The fixed-function emulation shaders are compiled on first use
    // via Get_Program(). This avoids compiling unused variants.
    return true;
}

void GLES3_ShaderManager::Shutdown() {
    for (int i = 0; i < 256; i++) {
        if (cached_programs[i]) {
            glDeleteProgram(cached_programs[i]);
            cached_programs[i] = 0;
        }
    }
}

GLuint GLES3_ShaderManager::Compile_Shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        fprintf(stderr, "ERROR: Shader compilation failed:\n%s\n", log);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint GLES3_ShaderManager::Link_Program(GLuint vert, GLuint frag) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vert);
    glAttachShader(program, frag);
    glLinkProgram(program);

    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char log[1024];
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        fprintf(stderr, "ERROR: Shader program link failed:\n%s\n", log);
        glDeleteProgram(program);
        return 0;
    }

    // Shaders can be detached after linking
    glDetachShader(program, vert);
    glDetachShader(program, frag);
    return program;
}

unsigned int GLES3_ShaderManager::Hash_Pipeline_Config(
        const GLES3_PipelineState& state) {
    // Create a simple hash from the active texture stage ops + lighting/fog state
    // This determines which shader variant to use
    unsigned int hash = 0;
    hash |= (state.lighting_enabled ? 1 : 0);
    hash |= (state.fog_enabled ? 2 : 0);
    hash |= (state.alpha_test_enabled ? 4 : 0);

    // Texture stage count (how many stages are active)
    for (int i = 0; i < 4; i++) {
        if (state.bound_textures[i] != 0) {
            hash |= (8 << i);
        }
    }
    return hash & 0xFF; // 256 variants max
}

GLint GLES3_ShaderManager::Get_Instancing_Uniform_Location(const GLES3_PipelineState &state) {
    unsigned int hash = Hash_Pipeline_Config(state);
    return cached_locations[hash].u_InstancingEnabled;
}

// Forward declaration — defined with extern "C" in dx8_fixedfunction_shaders.cpp
extern "C" const char* GLES3_Get_Vertex_Shader_Source(unsigned int config_hash);
extern "C" const char* GLES3_Get_Fragment_Shader_Source(unsigned int config_hash);

void GLES3_ShaderManager::Populate_Locations(GLuint program, unsigned int hash) {
    GLES3_ProgramUniformLocations& locs = cached_locations[hash];
    locs.u_World = glGetUniformLocation(program, "u_World");
    locs.u_View = glGetUniformLocation(program, "u_View");
    locs.u_Projection = glGetUniformLocation(program, "u_Projection");
    locs.u_MatDiffuse = glGetUniformLocation(program, "u_MatDiffuse");
    locs.u_MatAmbient = glGetUniformLocation(program, "u_MatAmbient");
    locs.u_MatSpecular = glGetUniformLocation(program, "u_MatSpecular");
    locs.u_MatEmissive = glGetUniformLocation(program, "u_MatEmissive");
    locs.u_MatPower = glGetUniformLocation(program, "u_MatPower");
    locs.u_LightingEnabled = glGetUniformLocation(program, "u_LightingEnabled");
    locs.u_GlobalAmbient = glGetUniformLocation(program, "u_GlobalAmbient");

    for (int i = 0; i < 4; i++) {
        char name[64];
        snprintf(name, sizeof(name), "u_Lights[%d].diffuse", i);
        locs.u_Lights[i].diffuse = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].position", i);
        locs.u_Lights[i].position = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].direction", i);
        locs.u_Lights[i].direction = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].enabled", i);
        locs.u_Lights[i].enabled = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].type", i);
        locs.u_Lights[i].type = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].attenuation0", i);
        locs.u_Lights[i].attenuation0 = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].attenuation1", i);
        locs.u_Lights[i].attenuation1 = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].attenuation2", i);
        locs.u_Lights[i].attenuation2 = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].ambient", i);
        locs.u_Lights[i].ambient = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].range", i);
        locs.u_Lights[i].range = glGetUniformLocation(program, name);
        snprintf(name, sizeof(name), "u_Lights[%d].specular", i);
        locs.u_Lights[i].specular = glGetUniformLocation(program, name);
    }

    locs.u_FogEnabled = glGetUniformLocation(program, "u_FogEnabled");
    locs.u_FogColor = glGetUniformLocation(program, "u_FogColor");
    locs.u_FogStart = glGetUniformLocation(program, "u_FogStart");
    locs.u_FogEnd = glGetUniformLocation(program, "u_FogEnd");
    locs.u_AlphaTestEnabled = glGetUniformLocation(program, "u_AlphaTestEnabled");
    locs.u_AlphaRef = glGetUniformLocation(program, "u_AlphaRef");
    locs.u_AlphaFunc = glGetUniformLocation(program, "u_AlphaFunc");

    for (int i = 0; i < 4; i++) {
        char name[32];
        snprintf(name, sizeof(name), "u_Texture%d", i);
        locs.u_Texture[i] = glGetUniformLocation(program, name);
    }

    locs.u_StageOps = glGetUniformLocation(program, "u_StageOps");
    locs.u_SpecularEnable = glGetUniformLocation(program, "u_SpecularEnable");
    locs.u_PSCount = glGetUniformLocation(program, "u_PSCount");
    locs.u_PS = glGetUniformLocation(program, "u_PS");
    locs.u_PSConst = glGetUniformLocation(program, "u_PSConst");
    locs.u_ColorVertex = glGetUniformLocation(program, "u_ColorVertex");
    locs.u_StageArgs = glGetUniformLocation(program, "u_StageArgs");
    locs.u_TexBound = glGetUniformLocation(program, "u_TexBound");
    locs.u_PreTransformed = glGetUniformLocation(program, "u_PreTransformed");
    locs.u_ViewportRect = glGetUniformLocation(program, "u_ViewportRect");
    locs.u_TextureFactor = glGetUniformLocation(program, "u_TextureFactor");
    locs.u_InstancingEnabled = glGetUniformLocation(program, "u_InstancingEnabled");
    locs.u_TexCoordIndex = glGetUniformLocation(program, "u_TexCoordIndex");
    locs.u_TexTransform = glGetUniformLocation(program, "u_TexTransform");
    locs.u_TexMatrix = glGetUniformLocation(program, "u_TexMatrix");
}


GLuint GLES3_ShaderManager::Get_Program(const GLES3_PipelineState& state) {
    unsigned int hash = Hash_Pipeline_Config(state);

    if (cached_programs[hash] != 0) {
        return cached_programs[hash];
    }

    // Compile shader variant for this pipeline configuration
    const char* vs_src = GLES3_Get_Vertex_Shader_Source(hash);
    const char* fs_src = GLES3_Get_Fragment_Shader_Source(hash);

    GLuint vs = Compile_Shader(GL_VERTEX_SHADER, vs_src);
    GLuint fs = Compile_Shader(GL_FRAGMENT_SHADER, fs_src);

    if (vs && fs) {
        cached_programs[hash] = Link_Program(vs, fs);
        if (cached_programs[hash] != 0) {
            Populate_Locations(cached_programs[hash], hash);
        }
    }

    if (vs) glDeleteShader(vs);
    if (fs) glDeleteShader(fs);

    return cached_programs[hash];
}

void GLES3_ShaderManager::Apply_Uniforms(GLuint program,
                                          const GLES3_PipelineState& state) {
    unsigned int hash = Hash_Pipeline_Config(state);
    const GLES3_ProgramUniformLocations& locs = cached_locations[hash];
    GLES3_CachedUniformState& cache = cached_uniform_states[hash];

    // Upload matrices (transposed for GL column-major order)
    float gl_matrix[16];

    GLES3_Helpers::Matrix4x4_To_GL(state.world_matrix, gl_matrix);
    if (locs.u_World != -1 && (!cache.initialized || memcmp(cache.world_matrix, gl_matrix, sizeof(gl_matrix)) != 0)) {
        glUniformMatrix4fv(locs.u_World, 1, GL_FALSE, gl_matrix);
        memcpy(cache.world_matrix, gl_matrix, sizeof(gl_matrix));
    }

    GLES3_Helpers::Matrix4x4_To_GL(state.view_matrix, gl_matrix);
    if (locs.u_View != -1 && (!cache.initialized || memcmp(cache.view_matrix, gl_matrix, sizeof(gl_matrix)) != 0)) {
        glUniformMatrix4fv(locs.u_View, 1, GL_FALSE, gl_matrix);
        memcpy(cache.view_matrix, gl_matrix, sizeof(gl_matrix));
    }

    GLES3_Helpers::Matrix4x4_To_GL(state.projection_matrix, gl_matrix);
    if (locs.u_Projection != -1 && (!cache.initialized || memcmp(cache.projection_matrix, gl_matrix, sizeof(gl_matrix)) != 0)) {
        glUniformMatrix4fv(locs.u_Projection, 1, GL_FALSE, gl_matrix);
        memcpy(cache.projection_matrix, gl_matrix, sizeof(gl_matrix));
    }

    // Material
    if (locs.u_MatDiffuse != -1 && (!cache.initialized || memcmp(cache.material_diffuse, state.material.diffuse, sizeof(state.material.diffuse)) != 0)) {
        glUniform4fv(locs.u_MatDiffuse, 1, state.material.diffuse);
        memcpy(cache.material_diffuse, state.material.diffuse, sizeof(state.material.diffuse));
    }
    if (locs.u_MatAmbient != -1 && (!cache.initialized || memcmp(cache.material_ambient, state.material.ambient, sizeof(state.material.ambient)) != 0)) {
        glUniform4fv(locs.u_MatAmbient, 1, state.material.ambient);
        memcpy(cache.material_ambient, state.material.ambient, sizeof(state.material.ambient));
    }
    if (locs.u_MatSpecular != -1 && (!cache.initialized || memcmp(cache.material_specular, state.material.specular, sizeof(state.material.specular)) != 0)) {
        glUniform4fv(locs.u_MatSpecular, 1, state.material.specular);
        memcpy(cache.material_specular, state.material.specular, sizeof(state.material.specular));
    }
    if (locs.u_MatEmissive != -1 && (!cache.initialized || memcmp(cache.material_emissive, state.material.emissive, sizeof(state.material.emissive)) != 0)) {
        glUniform4fv(locs.u_MatEmissive, 1, state.material.emissive);
        memcpy(cache.material_emissive, state.material.emissive, sizeof(state.material.emissive));
    }
    if (locs.u_MatPower != -1 && (!cache.initialized || cache.material_power != state.material.power)) {
        glUniform1f(locs.u_MatPower, state.material.power);
        cache.material_power = state.material.power;
    }

    // Lighting
    int l_enabled = state.lighting_enabled ? 1 : 0;
    if (locs.u_LightingEnabled != -1 && (!cache.initialized || cache.lighting_enabled != l_enabled)) {
        glUniform1i(locs.u_LightingEnabled, l_enabled);
        cache.lighting_enabled = l_enabled;
    }
    if (locs.u_GlobalAmbient != -1 && (!cache.initialized || memcmp(cache.global_ambient, state.global_ambient, sizeof(state.global_ambient)) != 0)) {
        glUniform4fv(locs.u_GlobalAmbient, 1, state.global_ambient);
        memcpy(cache.global_ambient, state.global_ambient, sizeof(state.global_ambient));
    }
    for (int i = 0; i < 4; i++) {
        if (state.light_enabled[i]) {
            if (locs.u_Lights[i].diffuse != -1 && (!cache.initialized || memcmp(cache.lights[i].diffuse, state.lights[i].diffuse, sizeof(state.lights[i].diffuse)) != 0)) {
                glUniform4fv(locs.u_Lights[i].diffuse, 1, state.lights[i].diffuse);
                memcpy(cache.lights[i].diffuse, state.lights[i].diffuse, sizeof(state.lights[i].diffuse));
            }
            if (locs.u_Lights[i].position != -1 && (!cache.initialized || memcmp(cache.lights[i].position, state.lights[i].position, sizeof(state.lights[i].position)) != 0)) {
                glUniform3fv(locs.u_Lights[i].position, 1, state.lights[i].position);
                memcpy(cache.lights[i].position, state.lights[i].position, sizeof(state.lights[i].position));
            }
            if (locs.u_Lights[i].direction != -1 && (!cache.initialized || memcmp(cache.lights[i].direction, state.lights[i].direction, sizeof(state.lights[i].direction)) != 0)) {
                glUniform3fv(locs.u_Lights[i].direction, 1, state.lights[i].direction);
                memcpy(cache.lights[i].direction, state.lights[i].direction, sizeof(state.lights[i].direction));
            }
            if (locs.u_Lights[i].enabled != -1 && (!cache.initialized || cache.lights[i].enabled != 1)) {
                glUniform1i(locs.u_Lights[i].enabled, 1);
                cache.lights[i].enabled = 1;
            }
            if (locs.u_Lights[i].type != -1 && (!cache.initialized || cache.lights[i].type != (int)state.lights[i].type)) {
                glUniform1i(locs.u_Lights[i].type, (int)state.lights[i].type);
                cache.lights[i].type = (int)state.lights[i].type;
            }
            if (locs.u_Lights[i].attenuation0 != -1 && (!cache.initialized || cache.lights[i].attenuation0 != state.lights[i].attenuation0)) {
                glUniform1f(locs.u_Lights[i].attenuation0, state.lights[i].attenuation0);
                cache.lights[i].attenuation0 = state.lights[i].attenuation0;
            }
            if (locs.u_Lights[i].attenuation1 != -1 && (!cache.initialized || cache.lights[i].attenuation1 != state.lights[i].attenuation1)) {
                glUniform1f(locs.u_Lights[i].attenuation1, state.lights[i].attenuation1);
                cache.lights[i].attenuation1 = state.lights[i].attenuation1;
            }
            if (locs.u_Lights[i].attenuation2 != -1 && (!cache.initialized || cache.lights[i].attenuation2 != state.lights[i].attenuation2)) {
                glUniform1f(locs.u_Lights[i].attenuation2, state.lights[i].attenuation2);
                cache.lights[i].attenuation2 = state.lights[i].attenuation2;
            }
            if (locs.u_Lights[i].ambient != -1 && (!cache.initialized || memcmp(cache.lights[i].ambient, state.lights[i].ambient, sizeof(state.lights[i].ambient)) != 0)) {
                glUniform4fv(locs.u_Lights[i].ambient, 1, state.lights[i].ambient);
                memcpy(cache.lights[i].ambient, state.lights[i].ambient, sizeof(state.lights[i].ambient));
            }
            if (locs.u_Lights[i].range != -1 && (!cache.initialized || cache.lights[i].range != state.lights[i].range)) {
                glUniform1f(locs.u_Lights[i].range, state.lights[i].range);
                cache.lights[i].range = state.lights[i].range;
            }
            if (locs.u_Lights[i].specular != -1 && (!cache.initialized || memcmp(cache.light_specular[i], state.lights[i].specular, sizeof(state.lights[i].specular)) != 0)) {
                glUniform4fv(locs.u_Lights[i].specular, 1, state.lights[i].specular);
                memcpy(cache.light_specular[i], state.lights[i].specular, sizeof(state.lights[i].specular));
            }
        } else {
            if (locs.u_Lights[i].enabled != -1 && (!cache.initialized || cache.lights[i].enabled != 0)) {
                glUniform1i(locs.u_Lights[i].enabled, 0);
                cache.lights[i].enabled = 0;
            }
        }
    }

    // Fog
    int f_enabled = state.fog_enabled ? 1 : 0;
    if (locs.u_FogEnabled != -1 && (!cache.initialized || cache.fog_enabled != f_enabled)) {
        glUniform1i(locs.u_FogEnabled, f_enabled);
        cache.fog_enabled = f_enabled;
    }
    if (state.fog_enabled) {
        if (locs.u_FogColor != -1 && (!cache.initialized || memcmp(cache.fog_color, state.fog_color, sizeof(state.fog_color)) != 0)) {
            glUniform3fv(locs.u_FogColor, 1, state.fog_color);
            memcpy(cache.fog_color, state.fog_color, sizeof(state.fog_color));
        }
        if (locs.u_FogStart != -1 && (!cache.initialized || cache.fog_start != state.fog_start)) {
            glUniform1f(locs.u_FogStart, state.fog_start);
            cache.fog_start = state.fog_start;
        }
        if (locs.u_FogEnd != -1 && (!cache.initialized || cache.fog_end != state.fog_end)) {
            glUniform1f(locs.u_FogEnd, state.fog_end);
            cache.fog_end = state.fog_end;
        }
    }

    // Alpha test (emulated in shader)
    int at_enabled = state.alpha_test_enabled ? 1 : 0;
    if (locs.u_AlphaTestEnabled != -1 && (!cache.initialized || cache.alpha_test_enabled != at_enabled)) {
        glUniform1i(locs.u_AlphaTestEnabled, at_enabled);
        cache.alpha_test_enabled = at_enabled;
    }
    float at_ref = state.alpha_ref / 255.0f;
    if (locs.u_AlphaRef != -1 && (!cache.initialized || cache.alpha_ref != at_ref)) {
        glUniform1f(locs.u_AlphaRef, at_ref);
        cache.alpha_ref = at_ref;
    }
    if (locs.u_AlphaFunc != -1 && (!cache.initialized || cache.alpha_func != (int)state.alpha_func)) {
        glUniform1i(locs.u_AlphaFunc, state.alpha_func);
        cache.alpha_func = state.alpha_func;
    }

    // Texture samplers
    for (int i = 0; i < 4; i++) {
        if (locs.u_Texture[i] != -1 && (!cache.initialized)) {
            glUniform1i(locs.u_Texture[i], i);
        }
    }

    // Texture stage ops (stages 0-3), D3DRS_SPECULARENABLE and the pixel shader.
    {
        int ops[8];
        for (int i = 0; i < 4; i++) {
            ops[i * 2] = (int)state.texture_stage_state[i][GLES3_TSS_COLOROP];
            ops[i * 2 + 1] = (int)state.texture_stage_state[i][GLES3_TSS_ALPHAOP];
        }
        if (locs.u_StageOps != -1 && (!cache.initialized || memcmp(cache.stage_ops, ops, sizeof(ops)) != 0)) {
            glUniform1iv(locs.u_StageOps, 8, ops);
            memcpy(cache.stage_ops, ops, sizeof(ops));
        }
        int spec = state.render_states[GLES3_RS_SPECULARENABLE] ? 1 : 0;
        if (locs.u_SpecularEnable != -1 && (!cache.initialized || cache.specular_enable != spec)) {
            glUniform1i(locs.u_SpecularEnable, spec);
            cache.specular_enable = spec;
        }
        if (!cache.initialized || cache.pixel_shader != state.pixel_shader) {
            GLES3_Upload_Pixel_Shader(locs, state.pixel_shader);
            cache.pixel_shader = state.pixel_shader;
        }
        if (locs.u_PSConst != -1 && (!cache.initialized || memcmp(cache.ps_const, state.ps_const, sizeof(state.ps_const)) != 0)) {
            glUniform4fv(locs.u_PSConst, 8, &state.ps_const[0][0]);
            memcpy(cache.ps_const, state.ps_const, sizeof(state.ps_const));
        }
    }

    // Color vertex mode
    int c_vertex = 0;
    if (state.render_states[GLES3_RS_COLORVERTEX] && state.vertex_has_color) {
        if (state.render_states[GLES3_RS_AMBIENTMATERIALSOURCE] == 1) c_vertex |= 1;
        if (state.render_states[GLES3_RS_DIFFUSEMATERIALSOURCE] == 1) c_vertex |= 2;
        if (state.render_states[GLES3_RS_EMISSIVEMATERIALSOURCE] == 1) c_vertex |= 4;
    }
    if (locs.u_ColorVertex != -1 && (!cache.initialized || cache.color_vertex != c_vertex)) {
        glUniform1i(locs.u_ColorVertex, c_vertex);
        cache.color_vertex = c_vertex;
    }

    {
        // Stages 0-3: COLORARG0/1/2 then ALPHAARG0/1/2 (D3DTA_* incl. modifier bits).
        int args[24];
        for (int st = 0; st < 4; st++) {
            const unsigned int* t = state.texture_stage_state[st];
            args[st * 6 + 0] = (int)t[GLES3_TSS_COLORARG0];
            args[st * 6 + 1] = (int)t[GLES3_TSS_COLORARG1];
            args[st * 6 + 2] = (int)t[GLES3_TSS_COLORARG2];
            args[st * 6 + 3] = (int)t[GLES3_TSS_ALPHAARG0];
            args[st * 6 + 4] = (int)t[GLES3_TSS_ALPHAARG1];
            args[st * 6 + 5] = (int)t[GLES3_TSS_ALPHAARG2];
        }
        if (locs.u_StageArgs != -1 && (!cache.initialized || memcmp(cache.stage_args, args, sizeof(args)) != 0)) {
            glUniform1iv(locs.u_StageArgs, 24, args);
            memcpy(cache.stage_args, args, sizeof(args));
        }
        int bound = 0;
        for (int st = 0; st < 4; st++) if (state.bound_textures[st]) bound |= 1 << st;
        if (locs.u_TexBound != -1 && (!cache.initialized || cache.tex_bound != bound)) {
            glUniform1i(locs.u_TexBound, bound);
            cache.tex_bound = bound;
        }
        int pre = state.pretransformed ? 1 : 0;
        if (locs.u_PreTransformed != -1 && (!cache.initialized || cache.pretransformed != pre)) {
            glUniform1i(locs.u_PreTransformed, pre);
            cache.pretransformed = pre;
        }
        float vp[4] = { (float)state.viewport.x, (float)state.viewport.y,
                        (float)(state.viewport.width ? state.viewport.width : 1),
                        (float)(state.viewport.height ? state.viewport.height : 1) };
        if (locs.u_ViewportRect != -1 && (!cache.initialized || memcmp(cache.viewport_rect, vp, sizeof(vp)) != 0)) {
            glUniform4fv(locs.u_ViewportRect, 1, vp);
            memcpy(cache.viewport_rect, vp, sizeof(vp));
        }
    }

    if (locs.u_TextureFactor != -1 && (!cache.initialized || memcmp(cache.texture_factor, state.texture_factor, sizeof(state.texture_factor)) != 0)) {
        glUniform4fv(locs.u_TextureFactor, 1, state.texture_factor);
        memcpy(cache.texture_factor, state.texture_factor, sizeof(state.texture_factor));
    }
    if (locs.u_InstancingEnabled != -1) {
        glUniform1i(locs.u_InstancingEnabled, 0);
    }

    // Texture coordinate generation and texture transforms (stages 0-3)
    {
        int tci[4], ttf[4];
        float tm[4][16];
        for (int i = 0; i < 4; i++) {
            tci[i] = (int)state.texture_stage_state[i][GLES3_TSS_TEXCOORDINDEX];
            ttf[i] = (int)state.texture_stage_state[i][GLES3_TSS_TEXTURETRANSFORMFLAGS];
            GLES3_Helpers::Matrix4x4_To_GL(state.texture_matrices[i], tm[i]);
        }
        if (locs.u_TexCoordIndex != -1 && (!cache.initialized || memcmp(cache.tex_coord_index, tci, sizeof(tci)) != 0)) {
            glUniform1iv(locs.u_TexCoordIndex, 4, tci);
            memcpy(cache.tex_coord_index, tci, sizeof(tci));
        }
        if (locs.u_TexTransform != -1 && (!cache.initialized || memcmp(cache.tex_transform, ttf, sizeof(ttf)) != 0)) {
            glUniform1iv(locs.u_TexTransform, 4, ttf);
            memcpy(cache.tex_transform, ttf, sizeof(ttf));
        }
        if (locs.u_TexMatrix != -1 && (!cache.initialized || memcmp(cache.tex_matrix, tm, sizeof(tm)) != 0)) {
            glUniformMatrix4fv(locs.u_TexMatrix, 4, GL_FALSE, &tm[0][0]);
            memcpy(cache.tex_matrix, tm, sizeof(tm));
        }
    }

    cache.initialized = true;
}



// ============================================================================
// Access to pipeline state (for external code that needs it)
// ============================================================================
GLES3_PipelineState* GLES3_Get_State() {
    return &g_State;
}

bool GLES3_Is_Initialized() {
    return g_Initialized;
}

#endif // __EMSCRIPTEN__
