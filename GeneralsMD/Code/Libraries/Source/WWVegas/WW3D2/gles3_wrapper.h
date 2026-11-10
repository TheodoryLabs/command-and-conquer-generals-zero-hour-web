/*
** GeneralsX Web Port — OpenGL ES 3.0 / WebGL 2.0 Graphics Backend
** Replaces dx8wrapper.h for Emscripten/WebAssembly builds
**
** This header provides a DX8-compatible interface implemented in OpenGL ES 3.0,
** which Emscripten automatically translates to WebGL 2.0.
**
** The original DX8Wrapper class has ~40 public static methods. This file
*provides
** a drop-in replacement that maps each DX8 concept to its OpenGL ES equivalent.
**
** ARCHITECTURE:
**   Game Code → DX8Wrapper API (unchanged) → gles3_wrapper.cpp → OpenGL ES 3.0
**   Emscripten compiles OpenGL ES 3.0 → WebGL 2.0 automatically.
**
** DX8 FIXED-FUNCTION PIPELINE EMULATION:
**   DX8's fixed-function pipeline (texture stages, material colors, vertex
*lighting,
**   fog) is emulated via a single GLSL vertex+fragment shader pair that
*replicates
**   the DX8 render state machine. See shaders/dx8_fixedfunction.vert/.frag
**
** LICENSE: GPL-3.0 (same as GeneralsX)
*/

#pragma once

// ============================================================================
// Platform detection — only compile this for Emscripten/WebGL builds
// ============================================================================
#if defined(__EMSCRIPTEN__)

#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <emscripten.h>
#include <emscripten/html5.h>

#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

// ============================================================================
// DX8 → OpenGL ES 3.0 Type Mappings
// These typedefs let existing game code compile without changes.
// ============================================================================

// --- Replacement types for D3D8 COM interfaces ---
// Instead of IDirect3DDevice8*, we use our own lightweight state tracker.

typedef unsigned long DWORD;
typedef long HRESULT;
#include "emscripten_compat/windows_base.h"
#define D3D_OK 0

// --- Transform types (D3DTRANSFORMSTATETYPE) ---
enum GLES3_TransformType {
  GLES3_TS_WORLD = 0,
  GLES3_TS_VIEW = 1,
  GLES3_TS_PROJECTION = 2,
  GLES3_TS_TEXTURE0 = 16,
  GLES3_TS_TEXTURE1 = 17,
  GLES3_TS_TEXTURE2 = 18,
  GLES3_TS_TEXTURE3 = 19,
  GLES3_TS_MAX = 32
};

// --- Render state types (D3DRENDERSTATETYPE subset used by Generals) ---
enum GLES3_RenderState {
  GLES3_RS_ZENABLE = 7,
  GLES3_RS_FILLMODE = 8,
  GLES3_RS_ZWRITEENABLE = 14,
  GLES3_RS_ALPHATESTENABLE = 15,
  GLES3_RS_SRCBLEND = 19,
  GLES3_RS_DESTBLEND = 20,
  GLES3_RS_CULLMODE = 22,
  GLES3_RS_ZFUNC = 23,
  GLES3_RS_ALPHAREF = 24,
  GLES3_RS_ALPHAFUNC = 25,
  GLES3_RS_ALPHABLENDENABLE = 27,
  GLES3_RS_FOGENABLE = 28,
  GLES3_RS_FOGCOLOR = 34,
  GLES3_RS_FOGTABLEMODE = 35,
  GLES3_RS_FOGSTART = 36,
  GLES3_RS_FOGEND = 37,
  GLES3_RS_FOGDENSITY = 38,
  GLES3_RS_LIGHTING = 137,
  GLES3_RS_AMBIENT = 139,
  GLES3_RS_COLORVERTEX = 141,
  GLES3_RS_DIFFUSEMATERIALSOURCE = 145,
  GLES3_RS_AMBIENTMATERIALSOURCE = 147,
  GLES3_RS_EMISSIVEMATERIALSOURCE = 148,
  GLES3_RS_NORMALIZENORMALS = 143,
  GLES3_RS_STENCILENABLE = 52,
  GLES3_RS_TEXTUREFACTOR = 60,
  GLES3_RS_ZBIAS = 47,
  GLES3_RS_MAX = 256
};

// --- Blend modes (D3DBLEND) ---
enum GLES3_Blend {
  GLES3_BLEND_ZERO = 1,
  GLES3_BLEND_ONE = 2,
  GLES3_BLEND_SRCCOLOR = 3,
  GLES3_BLEND_INVSRCCOLOR = 4,
  GLES3_BLEND_SRCALPHA = 5,
  GLES3_BLEND_INVSRCALPHA = 6,
  GLES3_BLEND_DESTALPHA = 7,
  GLES3_BLEND_INVDESTALPHA = 8,
  GLES3_BLEND_DESTCOLOR = 9,
  GLES3_BLEND_INVDESTCOLOR = 10,
  GLES3_BLEND_SRCALPHASAT = 11
};

// --- Compare functions (D3DCMPFUNC) ---
enum GLES3_CmpFunc {
  GLES3_CMP_NEVER = 1,
  GLES3_CMP_LESS = 2,
  GLES3_CMP_EQUAL = 3,
  GLES3_CMP_LESSEQUAL = 4,
  GLES3_CMP_GREATER = 5,
  GLES3_CMP_NOTEQUAL = 6,
  GLES3_CMP_GREATEREQUAL = 7,
  GLES3_CMP_ALWAYS = 8
};

// --- Texture stage states (D3DTEXTURESTAGESTATETYPE subset) ---
enum GLES3_TextureStageState {
  GLES3_TSS_COLOROP = 1,
  GLES3_TSS_COLORARG1 = 2,
  GLES3_TSS_COLORARG2 = 3,
  GLES3_TSS_ALPHAOP = 4,
  GLES3_TSS_ALPHAARG1 = 5,
  GLES3_TSS_ALPHAARG2 = 6,
  GLES3_TSS_TEXCOORDINDEX = 11,
  GLES3_TSS_ADDRESSU = 13,
  GLES3_TSS_ADDRESSV = 14,
  GLES3_TSS_MAGFILTER = 16,
  GLES3_TSS_MINFILTER = 17,
  GLES3_TSS_MIPFILTER = 18,
  GLES3_TSS_TEXTURETRANSFORMFLAGS = 24,
  GLES3_TSS_MAX = 32
};

// --- Primitive types ---
enum GLES3_PrimitiveType {
  GLES3_PT_TRIANGLELIST = 4,
  GLES3_PT_TRIANGLESTRIP = 5,
  GLES3_PT_TRIANGLEFAN = 6,
  GLES3_PT_LINELIST = 2,
  GLES3_PT_LINESTRIP = 3,
  GLES3_PT_POINTLIST = 1
};

// --- Light structure (mirrors D3DLIGHT8) ---
struct GLES3_Light {
  unsigned int type; // 1=point, 2=spot, 3=directional
  float diffuse[4];
  float specular[4];
  float ambient[4];
  float position[3];
  float direction[3];
  float range;
  float falloff;
  float attenuation0;
  float attenuation1;
  float attenuation2;
  float theta;
  float phi;
};

// --- Material structure (mirrors D3DMATERIAL8) ---
struct GLES3_Material {
  float diffuse[4];
  float ambient[4];
  float specular[4];
  float emissive[4];
  float power;
};

// --- Viewport structure ---
struct GLES3_Viewport {
  unsigned int x, y, width, height;
  float min_z, max_z;
};

// --- Texture handle ---
struct GLES3_TextureHandle {
  GLuint gl_texture;
  GLenum gl_target; // GL_TEXTURE_2D, GL_TEXTURE_CUBE_MAP, etc.
  unsigned int width, height;
  unsigned int mip_levels;
};

// ============================================================================
// GLES3 State Tracker
// Tracks the full DX8 fixed-function pipeline state to generate appropriate
// uniform values for the emulation shader each frame.
// ============================================================================
struct GLES3_PipelineState {
  // Transforms
  float world_matrix[16];
  float view_matrix[16];
  float projection_matrix[16];
  float texture_matrices[8][16];

  // Material
  GLES3_Material material;

  // Lights (DX8 supports up to 8, Generals uses up to 4)
  GLES3_Light lights[4];
  bool light_enabled[4];
  bool lighting_enabled;
  float global_ambient[4];

  // Fog
  bool fog_enabled;
  float fog_color[3];
  float fog_start;
  float fog_end;

  // Texture stages (DX8 has 8, Generals uses up to 4)
  unsigned int texture_stage_state[8][GLES3_TSS_MAX];
  GLuint bound_textures[8];

  // Render states
  unsigned int render_states[GLES3_RS_MAX];
  bool vertex_has_color; // current FVF carries D3DFVF_DIFFUSE

  // Alpha test (emulated in fragment shader since GLES3 removed it)
  bool alpha_test_enabled;
  unsigned int alpha_ref;  // 0-255
  unsigned int alpha_func; // GLES3_CmpFunc

  // Viewport
  GLES3_Viewport viewport;

  // Current shader program
  GLuint active_program;

  // Vertex array object
  GLuint vao;

  // RenderState TFACTOR
  float texture_factor[4];

  // Statistics
  unsigned int draw_calls;
  unsigned int triangles_drawn;
};

// ============================================================================
// Shader Uniform Cache
// ============================================================================
struct GLES3_ProgramUniformLocations {
  GLint u_World;
  GLint u_View;
  GLint u_Projection;
  GLint u_MatDiffuse;
  GLint u_MatAmbient;
  GLint u_MatSpecular;
  GLint u_MatEmissive;
  GLint u_MatPower;
  GLint u_LightingEnabled;
  GLint u_GlobalAmbient;
  struct {
    GLint diffuse;
    GLint position;
    GLint direction;
    GLint enabled;
    GLint type;
    GLint attenuation0;
    GLint attenuation1;
    GLint attenuation2;
  } u_Lights[4];
  GLint u_FogEnabled;
  GLint u_FogColor;
  GLint u_FogStart;
  GLint u_FogEnd;
  GLint u_AlphaTestEnabled;
  GLint u_AlphaRef;
  GLint u_AlphaFunc;
  GLint u_Texture[4];
  GLint u_TextureStageCount;
  GLint u_Stage0_ColorOp;
  GLint u_Stage1_ColorOp;
  GLint u_Stage0_AlphaOp;
  GLint u_Stage1_AlphaOp;
  GLint u_ColorVertex;
  GLint u_TextureFactor;
  GLint u_InstancingEnabled;
  GLint u_TexCoordIndex;
  GLint u_TexTransform;
  GLint u_TexMatrix;
};


// ============================================================================
// Fixed-Function Emulation Shader Manager
// Compiles and manages the GLSL shaders that emulate DX8's fixed-function
// pipeline (texture blending stages, vertex lighting, fog, alpha test).
// ============================================================================
class GLES3_ShaderManager {
public:
  static bool Init();
  static void Shutdown();

  // Get/compile the appropriate shader variant for current pipeline state
  static GLuint Get_Program(const GLES3_PipelineState &state);

  // Upload uniforms to match current DX8 state
  static void Apply_Uniforms(GLuint program, const GLES3_PipelineState &state);

  // Get cached instancing uniform location
  static GLint Get_Instancing_Uniform_Location(const GLES3_PipelineState &state);

private:
  static GLuint Compile_Shader(GLenum type, const char *source);
  static GLuint Link_Program(GLuint vert, GLuint frag);
  static void Populate_Locations(GLuint program, unsigned int hash);

  // Shader cache (keyed by texture stage configuration hash)
  // Avoids recompiling shaders every frame
  static GLuint cached_programs[256];
  static GLES3_ProgramUniformLocations cached_locations[256];
  static unsigned int Hash_Pipeline_Config(const GLES3_PipelineState &state);
};

// ============================================================================
// Texture Format Converter
// Converts DX8 texture formats (D3DFMT_*) to OpenGL ES 3.0 equivalents.
// Handles DDS, TGA, and the game's custom .big archive textures.
// ============================================================================
class GLES3_TextureConverter {
public:
  static GLenum DX8_Format_To_GL(unsigned int dx8_format);
  static GLenum DX8_Format_To_GL_Type(unsigned int dx8_format);
  static GLenum DX8_Format_To_GL_Internal(unsigned int dx8_format);

  // Create GL texture from raw pixel data (used by texture loader)
  static GLuint Create_Texture(unsigned int width, unsigned int height,
                               unsigned int dx8_format, unsigned int mip_levels,
                               const void *data);

  // Update GL texture in-place (used by dynamic textures)
  static void Update_Texture(GLuint gl_tex_id, unsigned int width, unsigned int height,
                             unsigned int dx8_format, unsigned int mip_levels,
                             const void *data);
};

// ============================================================================
// Vertex Buffer Abstraction
// Maps DX8 vertex buffers (IDirect3DVertexBuffer8) to OpenGL VBOs.
// ============================================================================
class GLES3_VertexBuffer {
public:
  GLuint gl_buffer;
  unsigned int stride;
  unsigned int size;
  unsigned int fvf; // DX8 Flexible Vertex Format flags
  bool is_dynamic;

  static GLES3_VertexBuffer *Create(unsigned int size, unsigned int stride,
                                    unsigned int fvf, bool dynamic);
  void Lock(void **data, unsigned int offset, unsigned int size);
  void Unlock();
  void Destroy();

private:
  void *lock_ptr;
};

// ============================================================================
// Index Buffer Abstraction
// Maps DX8 index buffers (IDirect3DIndexBuffer8) to OpenGL IBOs.
// ============================================================================
class GLES3_IndexBuffer {
public:
  GLuint gl_buffer;
  unsigned int size;
  unsigned int index_count;
  bool is_dynamic;

  static GLES3_IndexBuffer *Create(unsigned int size, bool dynamic);
  void Lock(void **data, unsigned int offset, unsigned int size);
  void Unlock();
  void Destroy();

private:
  void *lock_ptr;
};

// ============================================================================
// DX8 → OpenGL ES Helper Functions
// Convert DX8 enum values to their GL equivalents at runtime.
// ============================================================================
namespace GLES3_Helpers {
GLenum Blend_To_GL(unsigned int dx8_blend);
GLenum CmpFunc_To_GL(unsigned int dx8_cmp);
GLenum CullMode_To_GL(unsigned int dx8_cull);
GLenum PrimType_To_GL(unsigned int dx8_prim);
GLenum AddressMode_To_GL(unsigned int dx8_addr);
GLenum FilterMode_To_GL(unsigned int dx8_filter);
void Matrix4x4_To_GL(const float *src_row_major, float *dst_col_major);
} // namespace GLES3_Helpers

// Free functions — all called from Emscripten_IDirect3DDevice8 in d3d8.h

// Scene management
void GLES3_Begin_Scene();
void GLES3_End_Scene(bool flip_frame);
void GLES3_Swap_Buffers();
void GLES3_Clear(bool clear_color, bool clear_z_stencil,
                  float r, float g, float b, float a,
                  float z, unsigned int stencil);

// Render states
void GLES3_Set_Render_State(unsigned int state, unsigned int value);

// Transforms
void GLES3_Set_Transform(unsigned int type, const float* matrix_row_major);
void GLES3_Get_Transform(unsigned int type, float* matrix_out);

// Viewport
void GLES3_Set_Viewport(unsigned int x, unsigned int y,
                         unsigned int w, unsigned int h,
                         float min_z, float max_z);

// Texture binding / stage state
void GLES3_Set_Texture(unsigned int stage, GLuint gl_texture);
void GLES3_Set_Texture_Stage_State(unsigned int stage, unsigned int state,
                                    unsigned int value);

// Texture mipmap tracking
void GLES3_Register_Texture(GLuint tex, bool has_mipmaps);
void GLES3_Unregister_Texture(GLuint tex);
bool GLES3_Texture_Has_Mipmaps(GLuint tex);


// Lighting & material
void GLES3_Set_Light(unsigned int index, const GLES3_Light* light);
void GLES3_Enable_Light(unsigned int index, bool enable);
void GLES3_Set_Vertex_Has_Color(bool has_color);
void GLES3_Set_Material(const GLES3_Material* mat);
void GLES3_Set_Fog(bool enable, float r, float g, float b,
                    float start, float end);

// Draw calls — VBO/IBO must be bound and vertex attribs configured first
void GLES3_Draw_Triangles(unsigned int prim_type,
                           unsigned int start_index,
                           unsigned int prim_count,
                           unsigned int min_vertex,
                           unsigned int num_vertices);
void GLES3_Draw_Triangles_Instanced(unsigned int prim_type,
                                    unsigned int start_index,
                                    unsigned int prim_count,
                                    unsigned int min_vertex,
                                    unsigned int num_vertices,
                                    const float* world_matrices,
                                    unsigned int instance_count);
void GLES3_Draw_Arrays(unsigned int prim_type,
                        unsigned int start_vertex,
                        unsigned int prim_count);


#endif // __EMSCRIPTEN__
