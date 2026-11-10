/*
**  GeneralsX Web Port — Minimal d3d8types.h stub for Emscripten
**
**  Provides D3D8 type definitions needed by SAGE engine math/rendering code.
**  On Linux, these come from DXVK. On Emscripten, we provide minimal stubs
**  since the actual rendering goes through our GLES3 wrapper.
**
**  GeneralsX @feature WebPort 09/03/2026
*/

#ifndef __EMSCRIPTEN_D3D8TYPES_H
#define __EMSCRIPTEN_D3D8TYPES_H

#include "windows_base.h"
#include <stdint.h>

/* Forward-declare DWORD if not already available */
#ifndef _DWORD_DEFINED
typedef unsigned long DWORD;
#define _DWORD_DEFINED
#endif

#ifndef FLOAT
typedef float FLOAT;
#endif

#ifndef UINT
typedef unsigned int UINT;
#endif

#ifndef _BYTE_DEFINED
typedef unsigned char BYTE;
#define _BYTE_DEFINED
#endif

#ifndef _WORD_DEFINED
typedef unsigned short WORD;
#define _WORD_DEFINED
#endif

#ifndef _BOOL_DEFINED
typedef int BOOL;
#define _BOOL_DEFINED
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* ===================================================================
 * D3DMATRIX — 4x4 float matrix (core type used by D3DXMATRIX)
 * =================================================================== */
typedef struct _D3DMATRIX {
  union {
    struct {
      float _11, _12, _13, _14;
      float _21, _22, _23, _24;
      float _31, _32, _33, _34;
      float _41, _42, _43, _44;
    };
    float m[4][4];
  };
} D3DMATRIX;

/* ===================================================================
 * D3DVECTOR — 3D float vector
 * =================================================================== */
typedef struct _D3DVECTOR {
  float x, y, z;
} D3DVECTOR;

/* ===================================================================
 * D3DCOLORVALUE — RGBA float color
 * =================================================================== */
typedef struct _D3DCOLORVALUE {
  float r, g, b, a;
} D3DCOLORVALUE;

/* ===================================================================
 * D3DRECT — Rectangle for clipping
 * =================================================================== */
typedef struct _D3DRECT {
  LONG x1, y1, x2, y2;
} D3DRECT;

/* ===================================================================
 * Basic D3D type aliases
 * =================================================================== */
typedef DWORD D3DCOLOR;
typedef DWORD D3DFORMAT;
typedef DWORD D3DRESOURCETYPE;
typedef DWORD D3DMULTISAMPLE_TYPE;
typedef DWORD D3DPOOL;
typedef float D3DVALUE;
typedef DWORD D3DBLEND;
typedef DWORD D3DTEXTUREOP;
typedef DWORD D3DZBUFFERTYPE;
typedef DWORD D3DCMPFUNC;
typedef DWORD D3DFOGMODE;
typedef DWORD D3DSTENCILOP;

/* ===================================================================
 * D3DFORMAT constants (texture/surface formats)
 * =================================================================== */
#define D3DFMT_UNKNOWN 0
#define D3DFMT_R8G8B8 20
#define D3DFMT_A8R8G8B8 21
#define D3DFMT_X8R8G8B8 22
#define D3DFMT_R5G6B5 23
#define D3DFMT_X1R5G5B5 24
#define D3DFMT_A1R5G5B5 25
#define D3DFMT_A4R4G4B4 26
#define D3DFMT_A8 28
#define D3DFMT_P8 41
#define D3DFMT_L8 50
#define D3DFMT_A8L8 51
#define D3DFMT_A8P8 40
#define D3DFMT_DXT1 0x31545844 /* "DXT1" */
#define D3DFMT_DXT2 0x32545844
#define D3DFMT_DXT3 0x33545844
#define D3DFMT_DXT4 0x34545844
#define D3DFMT_DXT5 0x35545844
#define D3DFMT_D16 80
#define D3DFMT_D24S8 75
#define D3DFMT_D24X8 77
#define D3DFMT_D32 71
#define D3DFMT_R3G3B2 27
#define D3DFMT_A8R3G3B2 29
#define D3DFMT_X4R4G4B4 30
#define D3DFMT_A2B10G10R10 31
#define D3DFMT_G16R16 34
#define D3DFMT_A4L4 52
#define D3DFMT_V8U8 60
#define D3DFMT_L6V5U5 61
#define D3DFMT_X8L8V8U8 62
#define D3DFMT_Q8W8V8U8 63
#define D3DFMT_V16U16 64
#define D3DFMT_W11V11U10 65
#define D3DFMT_A2W10V10U10 67
#define D3DFMT_UYVY 0x59565955 /* "UYVY" */
#define D3DFMT_YUY2 0x32595559 /* "YUY2" */
#define D3DFMT_D16_LOCKABLE 70
#define D3DFMT_D15S1 73
#define D3DFMT_D24X4S4 79
#define D3DFMT_VERTEXDATA 100
#define D3DFMT_R3G3B2 27
#define D3DFMT_A8R3G3B2 29
#define D3DFMT_X4R4G4B4 30
#define D3DFMT_A2B10G10R10 31
#define D3DFMT_G16R16 34
#define D3DFMT_A4L4 52
#define D3DFMT_V8U8 60
#define D3DFMT_L6V5U5 61
#define D3DFMT_X8L8V8U8 62
#define D3DFMT_Q8W8V8U8 63
#define D3DFMT_V16U16 64
#define D3DFMT_W11V11U10 65
#define D3DFMT_A2W10V10U10 67
#define D3DFMT_UYVY 0x59565955 /* "UYVY" */
#define D3DFMT_YUY2 0x32595559 /* "YUY2" */
#define D3DFMT_D16_LOCKABLE 70
#define D3DFMT_D15S1 73
#define D3DFMT_D24X4S4 79
#define D3DFMT_VERTEXDATA 100
#define D3DFMT_INDEX16 101
#define D3DFMT_INDEX32 102

/* ===================================================================
 * D3DPRIMITIVETYPE — Drawing primitives
 * =================================================================== */
typedef enum _D3DPRIMITIVETYPE {
  D3DPT_POINTLIST = 1,
  D3DPT_LINELIST = 2,
  D3DPT_LINESTRIP = 3,
  D3DPT_TRIANGLELIST = 4,
  D3DPT_TRIANGLESTRIP = 5,
  D3DPT_TRIANGLEFAN = 6,
} D3DPRIMITIVETYPE;

/* ===================================================================
 * D3DTRANSFORMSTATETYPE — Transform types
 * =================================================================== */
typedef enum _D3DTRANSFORMSTATETYPE {
  D3DTS_VIEW = 2,
  D3DTS_PROJECTION = 3,
  D3DTS_WORLD = 256,
  D3DTS_TEXTURE0 = 16,
  D3DTS_TEXTURE1 = 17,
  D3DTS_TEXTURE2 = 18,
  D3DTS_TEXTURE3 = 19,
} D3DTRANSFORMSTATETYPE;

/* ===================================================================
 * D3DLIGHT8 — Light description
 * =================================================================== */
typedef enum _D3DLIGHTTYPE {
  D3DLIGHT_POINT = 1,
  D3DLIGHT_SPOT = 2,
  D3DLIGHT_DIRECTIONAL = 3,
} D3DLIGHTTYPE;

typedef struct _D3DLIGHT8 {
  D3DLIGHTTYPE Type;
  D3DCOLORVALUE Diffuse;
  D3DCOLORVALUE Specular;
  D3DCOLORVALUE Ambient;
  D3DVECTOR Position;
  D3DVECTOR Direction;
  float Range;
  float Falloff;
  float Attenuation0;
  float Attenuation1;
  float Attenuation2;
  float Theta;
  float Phi;
} D3DLIGHT8;

/* ===================================================================
 * D3DMATERIAL8 — Material description
 * =================================================================== */
typedef struct _D3DMATERIAL8 {
  D3DCOLORVALUE Diffuse;
  D3DCOLORVALUE Ambient;
  D3DCOLORVALUE Specular;
  D3DCOLORVALUE Emissive;
  float Power;
} D3DMATERIAL8;

/* ===================================================================
 * D3DVIEWPORT8 — Viewport
 * =================================================================== */
typedef struct _D3DVIEWPORT8 {
  DWORD X, Y;
  DWORD Width, Height;
  float MinZ, MaxZ;
} D3DVIEWPORT8;

/* ===================================================================
 * D3DLOCKED_RECT — Locked surface data
 * =================================================================== */
typedef struct _D3DLOCKED_RECT {
  int Pitch;
  void *pBits;
} D3DLOCKED_RECT;

/* ===================================================================
 * D3DLOCKED_BOX — Locked volume texture data
 * =================================================================== */
typedef struct _D3DLOCKED_BOX {
  int RowPitch;
  int SlicePitch;
  void *pBits;
} D3DLOCKED_BOX;

/* ===================================================================
 * D3DVOLUME_DESC — Volume texture level description
 * =================================================================== */
typedef struct _D3DVOLUME_DESC {
  D3DFORMAT Format;
  D3DRESOURCETYPE Type;
  DWORD Usage;
  D3DPOOL Pool;
  UINT Size;
  UINT Width;
  UINT Height;
  UINT Depth;
} D3DVOLUME_DESC;

/* ===================================================================
 * D3DCUBEMAP_FACES — Cube map face identifiers
 * =================================================================== */
typedef enum _D3DCUBEMAP_FACES {
  D3DCUBEMAP_FACE_POSITIVE_X = 0,
  D3DCUBEMAP_FACE_NEGATIVE_X = 1,
  D3DCUBEMAP_FACE_POSITIVE_Y = 2,
  D3DCUBEMAP_FACE_NEGATIVE_Y = 3,
  D3DCUBEMAP_FACE_POSITIVE_Z = 4,
  D3DCUBEMAP_FACE_NEGATIVE_Z = 5,
} D3DCUBEMAP_FACES;

/* ===================================================================
 * D3DSURFACE_DESC — Surface description
 * =================================================================== */
typedef struct _D3DSURFACE_DESC {
  D3DFORMAT Format;
  D3DRESOURCETYPE Type;
  DWORD Usage;
  D3DPOOL Pool;
  UINT Size;
  D3DMULTISAMPLE_TYPE MultiSampleType;
  UINT Width;
  UINT Height;
} D3DSURFACE_DESC;

/* ===================================================================
 * D3DCAPS8 — Device capabilities (minimal stub)
 * =================================================================== */
typedef struct _D3DCAPS8 {
  DWORD DeviceType;
  DWORD AdapterOrdinal;
  DWORD Caps;
  DWORD Caps2;
  DWORD Caps3;
  DWORD PresentationIntervals;
  DWORD DevCaps;
  DWORD PrimitiveMiscCaps;
  DWORD RasterCaps;
  DWORD ZCmpCaps;
  DWORD SrcBlendCaps;
  DWORD DestBlendCaps;
  DWORD AlphaCmpCaps;
  DWORD ShadeCaps;
  DWORD TextureCaps;
  DWORD TextureFilterCaps;
  DWORD CubeTextureFilterCaps;
  DWORD VolumeTextureFilterCaps;
  DWORD TextureAddressCaps;
  DWORD VolumeTextureAddressCaps;
  DWORD LineCaps;
  DWORD MaxTextureWidth;
  DWORD MaxTextureHeight;
  DWORD MaxVolumeExtent;
  DWORD MaxTextureRepeat;
  DWORD MaxTextureAspectRatio;
  DWORD MaxAnisotropy;
  float MaxVertexW;
  float GuardBandLeft;
  float GuardBandTop;
  float GuardBandRight;
  float GuardBandBottom;
  float ExtentsAdjust;
  DWORD StencilCaps;
  DWORD FVFCaps;
  DWORD TextureOpCaps;
  DWORD MaxTextureBlendStages;
  DWORD MaxSimultaneousTextures;
  DWORD VertexProcessingCaps;
  DWORD MaxActiveLights;
  DWORD MaxUserClipPlanes;
  DWORD MaxVertexBlendMatrices;
  DWORD MaxVertexBlendMatrixIndex;
  float MaxPointSize;
  DWORD MaxPrimitiveCount;
  DWORD MaxVertexIndex;
  DWORD MaxStreams;
  DWORD MaxStreamStride;
  DWORD VertexShaderVersion;
  DWORD MaxVertexShaderConst;
  DWORD PixelShaderVersion;
  float MaxPixelShaderValue;
} D3DCAPS8;

/* ===================================================================
 * D3DPRESENT_PARAMETERS — Presentation parameters
 * =================================================================== */
typedef struct _D3DPRESENT_PARAMETERS {
  UINT BackBufferWidth;
  UINT BackBufferHeight;
  D3DFORMAT BackBufferFormat;
  UINT BackBufferCount;
  D3DMULTISAMPLE_TYPE MultiSampleType;
  DWORD SwapEffect;
  void *hDeviceWindow;
  BOOL Windowed;
  BOOL EnableAutoDepthStencil;
  D3DFORMAT AutoDepthStencilFormat;
  DWORD Flags;
  UINT FullScreen_RefreshRateInHz;
  UINT FullScreen_PresentationInterval;
} D3DPRESENT_PARAMETERS;

/* ===================================================================
 * D3DADAPTER_IDENTIFIER8 — Adapter info
 * =================================================================== */
typedef struct _GUID_D3D8 {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} GUID_D3D8;

typedef struct _D3DADAPTER_IDENTIFIER8 {
  char Driver[512];
  char Description[512];
  LARGE_INTEGER DriverVersion;
  DWORD VendorId;
  DWORD DeviceId;
  DWORD SubSysId;
  DWORD Revision;
  GUID_D3D8 DeviceIdentifier;
  DWORD WHQLLevel;
} D3DADAPTER_IDENTIFIER8;

/* ===================================================================
 * D3DDISPLAYMODE — Display mode info
 * =================================================================== */
typedef struct _D3DDISPLAYMODE {
  UINT Width;
  UINT Height;
  UINT RefreshRate;
  D3DFORMAT Format;
} D3DDISPLAYMODE;

/* ===================================================================
 * Render state / texture stage state enums (values only, not full enum)
 * =================================================================== */
typedef DWORD D3DRENDERSTATETYPE;
typedef DWORD D3DTEXTURESTAGESTATETYPE;

/* Common render states */
#define D3DRS_ZENABLE 7
#define D3DRS_FILLMODE 8
#define D3DRS_SHADEMODE 9
#define D3DRS_ZWRITEENABLE 14
#define D3DRS_ALPHATESTENABLE 15
#define D3DRS_SRCBLEND 19
#define D3DRS_DESTBLEND 20
#define D3DRS_CULLMODE 22
#define D3DRS_ZFUNC 23
#define D3DRS_ALPHAREF 24
#define D3DRS_ALPHAFUNC 25
#define D3DRS_DITHERENABLE 26
#define D3DRS_ALPHABLENDENABLE 27
#define D3DRS_FOGENABLE 28
#define D3DRS_SPECULARENABLE 29
#define D3DRS_FOGCOLOR 34
#define D3DRS_FOGTABLEMODE 35
#define D3DRS_FOGSTART 36
#define D3DRS_FOGEND 37
#define D3DRS_FOGDENSITY 38
#define D3DRS_LIGHTING 137
#define D3DRS_AMBIENT 139
#define D3DRS_COLORVERTEX 141
#define D3DRS_NORMALIZENORMALS 143
#define D3DRS_DIFFUSEMATERIALSOURCE 145
#define D3DRS_SPECULARMATERIALSOURCE 146
#define D3DRS_AMBIENTMATERIALSOURCE 147
#define D3DRS_EMISSIVEMATERIALSOURCE 148
#define D3DRS_POINTSIZE 154
#define D3DRS_POINTSIZE_MIN 155
#define D3DRS_POINTSPRITEENABLE 156
#define D3DRS_POINTSCALEENABLE 157
#define D3DRS_POINTSCALE_A 158
#define D3DRS_POINTSCALE_B 159
#define D3DRS_POINTSCALE_C 160
#define D3DRS_POINTSIZE_MAX 166
#define D3DRS_TEXTUREFACTOR 60
#define D3DRS_WRAP0 128
#define D3DRS_CLIPPING 136
#define D3DRS_STENCILENABLE 52
#define D3DRS_STENCILFAIL 53
#define D3DRS_STENCILZFAIL 54
#define D3DRS_STENCILPASS 55
#define D3DRS_STENCILFUNC 56
#define D3DRS_STENCILREF 57
#define D3DRS_STENCILMASK 58
#define D3DRS_STENCILWRITEMASK 59
#define D3DRS_COLORWRITEENABLE 168
#define D3DRS_VERTEXBLEND 151

/* Common texture stage states */
#define D3DTSS_COLOROP 1
#define D3DTSS_COLORARG1 2
#define D3DTSS_COLORARG2 3
#define D3DTSS_ALPHAOP 4
#define D3DTSS_ALPHAARG1 5
#define D3DTSS_ALPHAARG2 6
#define D3DTSS_TEXCOORDINDEX 11
#define D3DTSS_ADDRESSU 13
#define D3DTSS_ADDRESSV 14
/* D3D8 values: DX8Wrapper caches these in TextureStageStates[stage][32] and the
   GLES3 layer reads them by number (GLES3_TSS_*), so they must match d3d8types.h. */
#define D3DTSS_BUMPENVMAT00 7
#define D3DTSS_BUMPENVMAT01 8
#define D3DTSS_BUMPENVMAT10 9
#define D3DTSS_BUMPENVMAT11 10
#define D3DTSS_MAGFILTER 16
#define D3DTSS_MINFILTER 17
#define D3DTSS_MIPFILTER 18
#define D3DTSS_MIPMAPLODBIAS 19
#define D3DTSS_MAXMIPLEVEL 20
#define D3DTSS_TEXTURETRANSFORMFLAGS 24
#define D3DTSS_RESULTARG 28

/* Texture op values (full D3D8 set) */
#define D3DTOP_DISABLE 1
#define D3DTOP_SELECTARG1 2
#define D3DTOP_SELECTARG2 3
#define D3DTOP_MODULATE 4
#define D3DTOP_MODULATE2X 5
#define D3DTOP_MODULATE4X 6
#define D3DTOP_ADD 7
#define D3DTOP_ADDSIGNED 8
#define D3DTOP_ADDSIGNED2X 9
#define D3DTOP_SUBTRACT 10
#define D3DTOP_ADDSMOOTH 11
#define D3DTOP_BLENDDIFFUSEALPHA 12
#define D3DTOP_BLENDTEXTUREALPHA 13
#define D3DTOP_BLENDFACTORALPHA 14
#define D3DTOP_BLENDTEXTUREALPHAPM 15
#define D3DTOP_BLENDCURRENTALPHA 16
#define D3DTOP_PREMODULATE 17
#define D3DTOP_MODULATEALPHA_ADDCOLOR 18
#define D3DTOP_MODULATECOLOR_ADDALPHA 19
#define D3DTOP_MODULATEINVALPHA_ADDCOLOR 20
#define D3DTOP_MODULATEINVCOLOR_ADDALPHA 21
#define D3DTOP_BUMPENVMAP 22
#define D3DTOP_BUMPENVMAPLUMINANCE 23
#define D3DTOP_DOTPRODUCT3 24
#define D3DTOP_MULTIPLYADD 25
#define D3DTOP_LERP 26

/* Texture argument values (full D3D8 set) */
#define D3DTA_SELECTMASK 0x0000000f
#define D3DTA_DIFFUSE 0x00000000
#define D3DTA_CURRENT 0x00000001
#define D3DTA_TEXTURE 0x00000002
#define D3DTA_TFACTOR 0x00000003
#define D3DTA_SPECULAR 0x00000004
#define D3DTA_TEMP 0x00000005
#define D3DTA_COMPLEMENT 0x00000010
#define D3DTA_ALPHAREPLICATE 0x00000020

/* Blend modes (full D3D8 set) */
#define D3DBLEND_ZERO 1
#define D3DBLEND_ONE 2
#define D3DBLEND_SRCCOLOR 3
#define D3DBLEND_INVSRCCOLOR 4
#define D3DBLEND_SRCALPHA 5
#define D3DBLEND_INVSRCALPHA 6
#define D3DBLEND_DESTALPHA 7
#define D3DBLEND_INVDESTALPHA 8
#define D3DBLEND_DESTCOLOR 9
#define D3DBLEND_INVDESTCOLOR 10
#define D3DBLEND_SRCALPHASAT 11
#define D3DBLEND_BOTHSRCALPHA 12
#define D3DBLEND_BOTHINVSRCALPHA 13

/* Compare functions */
#define D3DCMP_NEVER 1
#define D3DCMP_LESS 2
#define D3DCMP_EQUAL 3
#define D3DCMP_LESSEQUAL 4
#define D3DCMP_GREATER 5
#define D3DCMP_NOTEQUAL 6
#define D3DCMP_GREATEREQUAL 7
#define D3DCMP_ALWAYS 8

/* Cull modes */
#define D3DCULL_NONE 1
#define D3DCULL_CW 2
#define D3DCULL_CCW 3

/* Fill modes */
#define D3DFILL_POINT 1
#define D3DFILL_WIREFRAME 2
#define D3DFILL_SOLID 3

/* Filter types (full D3D8 set) */
#define D3DTEXF_NONE 0
#define D3DTEXF_POINT 1
#define D3DTEXF_LINEAR 2
#define D3DTEXF_ANISOTROPIC 3
#define D3DTEXF_FLATCUBIC 4
#define D3DTEXF_GAUSSIANCUBIC 5

/* Address modes (full D3D8 set) */
#define D3DTADDRESS_WRAP 1
#define D3DTADDRESS_MIRROR 2
#define D3DTADDRESS_CLAMP 3
#define D3DTADDRESS_BORDER 4
#define D3DTADDRESS_MIRRORONCE 5

/* Usage flags */
#define D3DUSAGE_RENDERTARGET 0x00000001
#define D3DUSAGE_DEPTHSTENCIL 0x00000002
#define D3DUSAGE_WRITEONLY 0x00000008
#define D3DUSAGE_SOFTWAREPROCESSING 0x00000010
#define D3DUSAGE_DYNAMIC 0x00000200

/* Pool types */
#define D3DPOOL_DEFAULT 0
#define D3DPOOL_MANAGED 1
#define D3DPOOL_SYSTEMMEM 2
#define D3DPOOL_SCRATCH 3

/* Lock flags */
#define D3DLOCK_READONLY 0x00000010
#define D3DLOCK_DISCARD 0x00002000
#define D3DLOCK_NOOVERWRITE 0x00001000
#define D3DLOCK_NOSYSLOCK 0x00000800
#define D3DLOCK_NO_DIRTY_UPDATE 0x00008000

/* FVF flags */
#define D3DFVF_XYZ 0x002
#define D3DFVF_XYZRHW 0x004
#define D3DFVF_NORMAL 0x010
#define D3DFVF_DIFFUSE 0x040
#define D3DFVF_SPECULAR 0x080
#define D3DFVF_TEX0 0x000
#define D3DFVF_TEX1 0x100
#define D3DFVF_TEX2 0x200
#define D3DFVF_TEX3 0x300
#define D3DFVF_TEX4 0x400
#define D3DFVF_TEXCOUNT_MASK 0xf00
#define D3DFVF_TEXCOUNT_SHIFT 8

/* Color helpers */
#define D3DCOLOR_ARGB(a, r, g, b)                                              \
  ((D3DCOLOR)((((a) & 0xff) << 24) | (((r) & 0xff) << 16) |                    \
              (((g) & 0xff) << 8) | ((b) & 0xff)))
#define D3DCOLOR_RGBA(r, g, b, a) D3DCOLOR_ARGB(a, r, g, b)
#define D3DCOLOR_XRGB(r, g, b) D3DCOLOR_ARGB(0xff, r, g, b)

/* Stencil operations */
#define D3DSTENCILOP_KEEP 1
#define D3DSTENCILOP_ZERO 2
#define D3DSTENCILOP_REPLACE 3
#define D3DSTENCILOP_INCRSAT 4
#define D3DSTENCILOP_DECRSAT 5
#define D3DSTENCILOP_INVERT 6
#define D3DSTENCILOP_INCR 7
#define D3DSTENCILOP_DECR 8

/* Fog modes */
#define D3DFOG_NONE 0
#define D3DFOG_EXP 1
#define D3DFOG_EXP2 2
#define D3DFOG_LINEAR 3

/* Texture transform flags */
#define D3DTTFF_DISABLE 0
#define D3DTTFF_COUNT1 1
#define D3DTTFF_COUNT2 2
#define D3DTTFF_COUNT3 3
#define D3DTTFF_COUNT4 4
#define D3DTTFF_PROJECTED 256

/* Vertex blend (full D3D8 set) */
#define D3DVBF_DISABLE 0
#define D3DVBF_1WEIGHTS 1
#define D3DVBF_2WEIGHTS 2
#define D3DVBF_3WEIGHTS 3
#define D3DVBF_TWEENING 255
#define D3DVBF_0WEIGHTS 256

/* Patch edge style (D3DPATCHEDGESTYLE) */
#define D3DPATCHEDGE_DISCRETE 0
#define D3DPATCHEDGE_CONTINUOUS 1

/* Debug monitor token (D3DDEBUMONITORTOKENS) */
#define D3DDMT_ENABLE 0
#define D3DDMT_DISABLE 1

/* Shade modes (full D3D8 set) */
#define D3DSHADE_FLAT 1
#define D3DSHADE_GOURAUD 2
#define D3DSHADE_PHONG 3

/* Z-buffer enable types (D3DZBUFFERTYPE) — values for D3DRS_ZENABLE */
#define D3DZB_FALSE 0
#define D3DZB_TRUE 1
#define D3DZB_USEW 2

/* Texture coordinate index flags */
#define D3DTSS_TCI_PASSTHRU 0x00000000
#define D3DTSS_TCI_CAMERASPACENORMAL 0x00010000
#define D3DTSS_TCI_CAMERASPACEPOSITION 0x00020000
#define D3DTSS_TCI_CAMERASPACEREFLECTIONVECTOR 0x00030000

/* Material source */
#define D3DMCS_MATERIAL 0
#define D3DMCS_COLOR1 1
#define D3DMCS_COLOR2 2

/* Swap effect */
#define D3DSWAPEFFECT_DISCARD 1
#define D3DSWAPEFFECT_FLIP 2
#define D3DSWAPEFFECT_COPY 3

/* Color write enable */
#define D3DCOLORWRITEENABLE_RED (1L << 0)
#define D3DCOLORWRITEENABLE_GREEN (1L << 1)
#define D3DCOLORWRITEENABLE_BLUE (1L << 2)
#define D3DCOLORWRITEENABLE_ALPHA (1L << 3)

/* HRESULT success/failure */
#ifndef S_OK
#define S_OK ((HRESULT)0L)
#endif
#ifndef E_FAIL
#define E_FAIL ((HRESULT)0x80004005L)
#endif
#ifndef SUCCEEDED
#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#endif
#ifndef FAILED
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#endif

/* D3DERR values */
#define D3D_OK S_OK
#define D3DERR_NOTFOUND ((HRESULT)0x88760866L)

/* FVF texture coordinate size macros */
#define D3DFVF_TEXTUREFORMAT1 3
#define D3DFVF_TEXTUREFORMAT2 0
#define D3DFVF_TEXTUREFORMAT3 1
#define D3DFVF_TEXTUREFORMAT4 2
#define D3DFVF_TEXCOORDSIZE1(n) (D3DFVF_TEXTUREFORMAT1 << (2 * (n) + 16))
#define D3DFVF_TEXCOORDSIZE2(n) (D3DFVF_TEXTUREFORMAT2 << (2 * (n) + 16))
#define D3DFVF_TEXCOORDSIZE3(n) (D3DFVF_TEXTUREFORMAT3 << (2 * (n) + 16))
#define D3DFVF_TEXCOORDSIZE4(n) (D3DFVF_TEXTUREFORMAT4 << (2 * (n) + 16))

/* D3DDP maximum texture coordinates */
#define D3DDP_MAXTEXCOORD 8

/* Additional FVF flags */
#define D3DFVF_PSIZE 0x020
#define D3DFVF_XYZB1 0x006
#define D3DFVF_XYZB2 0x008
#define D3DFVF_XYZB3 0x00a
#define D3DFVF_XYZB4 0x00c
#define D3DFVF_XYZB5 0x00e
#define D3DFVF_TEX5 0x500
#define D3DFVF_TEX6 0x600
#define D3DFVF_TEX7 0x700
#define D3DFVF_TEX8 0x800
#define D3DFVF_LASTBETA_UBYTE4 0x1000
#define D3DFVF_RESERVED2 0x6000

/* D3DRS_ZBIAS (deprecated in D3D9, used in D3D8) */
#define D3DRS_ZBIAS 47

/* D3D8-specific / deprecated render states */
#define D3DRS_LINEPATTERN 10
#define D3DRS_LASTPIXEL 16
#define D3DRS_ZVISIBLE 30
#define D3DRS_EDGEANTIALIAS 40
#define D3DRS_LOCALVIEWER 142
#define D3DRS_CLIPPLANEENABLE 152
#define D3DRS_MULTISAMPLEANTIALIAS 161
#define D3DRS_MULTISAMPLEMASK 162
#define D3DRS_PATCHEDGESTYLE 163
#define D3DRS_PATCHSEGMENTS 164
#define D3DRS_DEBUGMONITORTOKEN 165
#define D3DRS_INDEXEDVERTEXBLENDENABLE 167
#define D3DRS_TWEENFACTOR 170
#define D3DRS_BLENDOP 171
#define D3DRS_WRAP1 129
#define D3DRS_WRAP2 130
#define D3DRS_WRAP3 131
#define D3DRS_WRAP4 132
#define D3DRS_WRAP5 133
#define D3DRS_WRAP6 134
#define D3DRS_WRAP7 135

/* Blend operation values (D3DBLENDOP) */
#define D3DBLENDOP_ADD 1
#define D3DBLENDOP_SUBTRACT 2
#define D3DBLENDOP_REVSUBTRACT 3
#define D3DBLENDOP_MIN 4
#define D3DBLENDOP_MAX 5

/* D3DWRAP texture-wrap flags */
#define D3DWRAP_U 1
#define D3DWRAP_V 2
#define D3DWRAP_W 4

/* D3D8 swap effect: COPY with v-sync (removed in D3D9) */
#define D3DSWAPEFFECT_COPY_VSYNC 4

/* D3DTSS additions */
#define D3DTSS_BORDERCOLOR 15
#define D3DTSS_ADDRESSW 25
#define D3DTSS_MAXANISOTROPY 21
#define D3DTSS_COLORARG0 26
#define D3DTSS_ALPHAARG0 27

/* D3DUSAGE_NPATCHES — high-order patch tessellation usage flag */
#ifndef D3DUSAGE_NPATCHES
#define D3DUSAGE_NPATCHES 0x00000100L
#endif

/* Pointer typedefs for D3D8 interfaces are in d3d8.h (after struct definitions)
 */

/* Additional types */
struct IDirect3DSwapChain8;
typedef struct IDirect3DSwapChain8 *LPDIRECT3DSWAPCHAIN8;

/* Sample status values (used by Miles but also referenced in D3D context) */
#ifndef SMP_DONE
#define SMP_DONE 2
#define SMP_PLAYING 4
#endif

#define D3D_SDK_VERSION 220
#define D3DRS_RANGEFOGENABLE 48
#define D3DRS_FOGVERTEXMODE 140
#define D3DTSS_BUMPENVLSCALE 22
#define D3DTSS_BUMPENVLOFFSET 23
#define D3DENUM_NO_WHQL_LEVEL 0x00000002L
#define D3DCREATE_FPU_PRESERVE 0x00000002L

#define D3DMULTISAMPLE_NONE 0
#define D3DPRESENT_RATE_DEFAULT 0
#define D3DPRESENT_INTERVAL_TWO 2
#define D3DPRESENT_INTERVAL_THREE 3

#define D3DERR_CONFLICTINGTEXTUREFILTER ((HRESULT)0x88760879L)
#define D3DERR_CONFLICTINGTEXTUREPALETTE ((HRESULT)0x8876087AL)
#define D3DERR_TOOMANYOPERATIONS ((HRESULT)0x8876087BL)
#define D3DERR_UNSUPPORTEDALPHAARG ((HRESULT)0x8876087CL)
#define D3DERR_UNSUPPORTEDALPHAARG ((HRESULT)0x8876087CL)
#define D3DERR_UNSUPPORTEDALPHAOPERATION ((HRESULT)0x8876087DL)

#define D3DERR_UNSUPPORTEDCOLORARG ((HRESULT)0x88760808L)
#define D3DERR_UNSUPPORTEDCOLOROPERATION ((HRESULT)0x88760809L)
#define D3DERR_UNSUPPORTEDFACTORVALUE ((HRESULT)0x8876080AL)
#define D3DERR_UNSUPPORTEDTEXTUREFILTER ((HRESULT)0x8876080BL)
#define D3DERR_WRONGTEXTUREFORMAT ((HRESULT)0x88760818L)
#define D3DERR_OUTOFVIDEOMEMORY ((HRESULT)0x8876017CL)

#define D3DX_DEFAULT ((UINT) - 1)
#define D3DX_FILTER_BOX (0x00000002)

#define D3DTEXOPCAPS_DISABLE 0x00000001
#define D3DTEXOPCAPS_SELECTARG1 0x00000002
#define D3DTEXOPCAPS_SELECTARG2 0x00000004
#define D3DTEXOPCAPS_MODULATE 0x00000008
#define D3DTEXOPCAPS_MODULATE2X 0x00000010
#define D3DTEXOPCAPS_MODULATE4X 0x00000020
#define D3DTEXOPCAPS_ADD 0x00000040
#define D3DTEXOPCAPS_ADDSIGNED 0x00000080
#define D3DTEXOPCAPS_ADDSIGNED2X 0x00000100
#define D3DTEXOPCAPS_SUBTRACT 0x00000200
#define D3DTEXOPCAPS_ADDSMOOTH 0x00000400
#define D3DTEXOPCAPS_BLENDDIFFUSEALPHA 0x00000800
#define D3DTEXOPCAPS_BLENDTEXTUREALPHA 0x00001000
#define D3DTEXOPCAPS_BLENDFACTORALPHA 0x00002000
#define D3DTEXOPCAPS_BLENDTEXTUREALPHAPM 0x00004000
#define D3DTEXOPCAPS_BLENDCURRENTALPHA 0x00008000
#define D3DTEXOPCAPS_PREMODULATE 0x00010000
#define D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR 0x00020000
#define D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA 0x00040000
#define D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR 0x00080000
#define D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA 0x00100000
#define D3DTEXOPCAPS_BUMPENVMAP 0x00200000
#define D3DTEXOPCAPS_BUMPENVMAPLUMINANCE 0x00400000
#define D3DTEXOPCAPS_DOTPRODUCT3 0x00800000
#define D3DTEXOPCAPS_MULTIPLYADD 0x01000000
#define D3DTEXOPCAPS_LERP 0x02000000

#endif /* __EMSCRIPTEN_D3D8TYPES_H */
