/*
**  GeneralsX Web Port — Minimal d3d8.h stub for Emscripten
**
**  Provides the D3D8 COM interfaces as empty stubs. The actual rendering
**  is handled by the GLES3 wrapper (gles3_wrapper.h/cpp).
**
**  GeneralsX @feature WebPort 09/03/2026
*/

#pragma once

#ifndef __EMSCRIPTEN_D3D8_H
#define __EMSCRIPTEN_D3D8_H

#include "d3d8caps.h"
#include "d3d8types.h"
#include "windows_base.h"
#include <vector>
#include <cstdint>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
// Forward declaration — defined in gles3_wrapper.cpp (which has SDL access)
void GLES3_Swap_Buffers();

// GeneralsX @feature WebPort 2026-05-05 — black-canvas triage counters.
// Defined in dx8wrapper.cpp; bumped from inline DrawIndexedPrimitive below
// and read from EmscriptenMain.cpp once a second to localise where draws
// are getting lost.
extern "C" {
extern int gx_d3d_dip_called;
extern int gx_d3d_dip_no_vb;
extern int gx_d3d_dip_empty_vb;
extern int gx_d3d_dip_no_ib;
extern int gx_d3d_dip_empty_ib;
extern int gx_d3d_dip_drew;
extern int gx_d3d_setstreamsource_calls;
extern int gx_d3d_setstreamsource_null;
extern int gx_d3d_setindices_calls;
extern int gx_d3d_setindices_null;
}

#define D3DCURSOR_IMMEDIATE_UPDATE 0x00000001L

#define D3DUSAGE_POINTS 0x00000040L

#define D3DVSD_END() 0xFFFFFFFF
#define D3DVSD_STREAM(streamNumber) (0x10000000 | ((streamNumber) << 24))
#define D3DVSD_REG(regNumber, dataType)                                        \
  (0x20000000 | ((dataType) << 24) | (regNumber))

enum {
  D3DVSDT_FLOAT1 = 0,
  D3DVSDT_FLOAT2 = 1,
  D3DVSDT_FLOAT3 = 2,
  D3DVSDT_FLOAT4 = 3,
  D3DVSDT_D3DCOLOR = 4,
};

/* Forward-declare D3DGAMMARAMP (used by IDirect3DDevice8) */
#ifndef WORD
typedef unsigned short WORD;
#endif
typedef struct _D3DGAMMARAMP {
  WORD red[256];
  WORD green[256];
  WORD blue[256];
} D3DGAMMARAMP;

/* IUnknown base — minimal COM interface for D3D8 objects */
#ifndef __IUnknown_FWD_DEFINED__
#define __IUnknown_FWD_DEFINED__
typedef struct IUnknown IUnknown;
#endif

/* REFIID / GUID stub */
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct _GUID {
  uint32_t Data1;
  uint16_t Data2;
  uint16_t Data3;
  uint8_t Data4[8];
} GUID;
typedef GUID IID;
typedef const GUID &REFIID;
#endif

/* IUnknown — base COM interface */
#ifndef __IUnknown_INTERFACE_DEFINED__
#define __IUnknown_INTERFACE_DEFINED__
struct IUnknown {
  virtual HRESULT QueryInterface(REFIID riid, void **ppvObject) = 0;
  virtual ULONG AddRef() = 0;
  virtual ULONG Release() = 0;
};
#endif

/* Forward declarations for D3D8 interfaces */
struct IDirect3D8;
struct IDirect3DDevice8;
struct IDirect3DTexture8;
struct IDirect3DSurface8;
struct IDirect3DVertexBuffer8;
struct IDirect3DIndexBuffer8;
struct IDirect3DBaseTexture8;

/* ===================================================================
 * IDirect3DSurface8
 * =================================================================== */
struct IDirect3DSurface8 : public IUnknown {
  virtual HRESULT GetDesc(D3DSURFACE_DESC *pDesc) = 0;
  virtual HRESULT LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect,
                           DWORD Flags) = 0;
  virtual HRESULT UnlockRect() = 0;
};

/* ===================================================================
 * IDirect3DBaseTexture8
 * =================================================================== */
struct IDirect3DBaseTexture8 : public IUnknown {
  virtual DWORD GetLevelCount() = 0;
  virtual DWORD GetPriority() = 0;
  virtual DWORD SetPriority(DWORD PriorityNew) = 0;
  virtual DWORD SetLOD(DWORD LODNew) = 0;
  virtual DWORD GetLOD() = 0;
};

/* ===================================================================
 * IDirect3DTexture8
 * =================================================================== */
struct IDirect3DTexture8 : public IDirect3DBaseTexture8 {
  virtual HRESULT GetSurfaceLevel(UINT Level,
                                  IDirect3DSurface8 **ppSurfaceLevel) = 0;
  virtual HRESULT LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect,
                           const RECT *pRect, DWORD Flags) = 0;
  virtual HRESULT UnlockRect(UINT Level) = 0;
  virtual HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) = 0;
};

/* ===================================================================
 * IDirect3DVertexBuffer8
 * =================================================================== */
struct IDirect3DVertexBuffer8 : public IUnknown {
  virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData,
                       DWORD Flags) = 0;
  virtual HRESULT Unlock() = 0;
};

/* ===================================================================
 * IDirect3DIndexBuffer8
 * =================================================================== */
struct IDirect3DIndexBuffer8 : public IUnknown {
  virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData,
                       DWORD Flags) = 0;
  virtual HRESULT Unlock() = 0;
};

/* ===================================================================
 * IDirect3DDevice8 — primary rendering interface (stub)
 * =================================================================== */
struct IDirect3DDevice8 : public IUnknown {
  virtual HRESULT GetDirect3D(IDirect3D8 **ppD3D8) = 0;
  virtual HRESULT TestCooperativeLevel() = 0;
  virtual HRESULT GetDeviceCaps(D3DCAPS8 *pCaps) = 0;
  virtual HRESULT GetDisplayMode(D3DDISPLAYMODE *pMode) = 0;
  virtual HRESULT Reset(D3DPRESENT_PARAMETERS *pParams) = 0;
  virtual HRESULT Present(const RECT *pSrc, const RECT *pDst, void *hDstWnd,
                          const RGNDATA *pDirtyRgn) = 0;
  virtual HRESULT Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags,
                        D3DCOLOR Color, float Z, DWORD Stencil) = 0;
  virtual HRESULT BeginScene() = 0;
  virtual HRESULT EndScene() = 0;
  virtual HRESULT SetTransform(D3DTRANSFORMSTATETYPE State,
                               const D3DMATRIX *pMatrix) = 0;
  virtual HRESULT GetTransform(D3DTRANSFORMSTATETYPE State,
                               D3DMATRIX *pMatrix) = 0;
  virtual HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) = 0;
  virtual HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) = 0;
  virtual HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture8 *pTexture) = 0;
  virtual HRESULT SetTextureStageState(DWORD Stage,
                                       D3DTEXTURESTAGESTATETYPE Type,
                                       DWORD Value) = 0;
  virtual HRESULT SetLight(DWORD Index, const D3DLIGHT8 *pLight) = 0;
  virtual HRESULT LightEnable(DWORD Index, BOOL Enable) = 0;
  virtual HRESULT SetMaterial(const D3DMATERIAL8 *pMaterial) = 0;
  virtual HRESULT SetViewport(const D3DVIEWPORT8 *pViewport) = 0;
  virtual HRESULT GetViewport(D3DVIEWPORT8 *pViewport) = 0;
  virtual HRESULT SetStreamSource(UINT StreamNumber,
                                  IDirect3DVertexBuffer8 *pStreamData,
                                  UINT Stride) = 0;
  virtual HRESULT SetIndices(IDirect3DIndexBuffer8 *pIndexData,
                             UINT BaseVertexIndex) = 0;
  virtual HRESULT SetVertexShader(DWORD Handle) = 0;
  virtual HRESULT SetPixelShader(DWORD Handle) = 0;
  virtual HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimType, UINT StartVertex,
                                UINT PrimCount) = 0;
  virtual HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimType, UINT PrimCount,
                                  const void *pVertexStreamZeroData,
                                  UINT VertexStreamZeroStride) = 0;
  virtual HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimType, UINT MinIndex,
                                       UINT NumVertices, UINT StartIndex,
                                       UINT PrimCount) = 0;
  virtual HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimType,
                                         UINT MinVertexIndex, UINT NumVertices,
                                         UINT PrimCount, const void *pIndexData,
                                         D3DFORMAT IndexDataFormat,
                                         const void *pVertexStreamZeroData,
                                         UINT VertexStreamZeroStride) = 0;
  virtual HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels,
                                DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                IDirect3DTexture8 **ppTexture) = 0;
  virtual HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF,
                                     D3DPOOL Pool,
                                     IDirect3DVertexBuffer8 **ppVB) = 0;
  virtual HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                                    D3DPOOL Pool,
                                    IDirect3DIndexBuffer8 **ppIB) = 0;
  virtual HRESULT GetBackBuffer(UINT BackBuffer, DWORD Type,
                                IDirect3DSurface8 **ppBackBuffer) = 0;
  virtual HRESULT GetRenderTarget(IDirect3DSurface8 **ppRenderTarget) = 0;
  virtual HRESULT SetRenderTarget(IDirect3DSurface8 *pRenderTarget,
                                  IDirect3DSurface8 *pDepthStencil) = 0;
  virtual HRESULT
  GetDepthStencilSurface(IDirect3DSurface8 **ppZStencilSurface) = 0;
  virtual HRESULT CopyRects(IDirect3DSurface8 *pSrc, const RECT *pSrcRectsArray,
                            UINT cRects, IDirect3DSurface8 *pDst,
                            const POINT *pDstPointsArray) = 0;
  virtual HRESULT SetVertexShaderConstant(DWORD Register,
                                          const void *pConstantData,
                                          DWORD ConstantCount) = 0;
  virtual HRESULT SetPixelShaderConstant(DWORD Register,
                                         const void *pConstantData,
                                         DWORD ConstantCount) = 0;
  virtual HRESULT SetClipPlane(DWORD Index, const float *pPlane) = 0;
  virtual HRESULT CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                     IDirect3DSurface8 **ppSurface) = 0;
  virtual HRESULT SetPaletteEntries(UINT PaletteNumber,
                                    const PALETTEENTRY *pEntries) = 0;
  virtual HRESULT SetCurrentTexturePalette(UINT PaletteNumber) = 0;
  virtual HRESULT GetGammaRamp(D3DGAMMARAMP *pRamp) = 0;
  virtual HRESULT SetGammaRamp(DWORD Flags, const D3DGAMMARAMP *pRamp) = 0;
  virtual HRESULT CreateVertexShader(const DWORD *pDeclaration,
                                     const DWORD *pFunction, DWORD *pHandle,
                                     DWORD Usage) = 0;
  virtual HRESULT DeleteVertexShader(DWORD Handle) = 0;
  virtual HRESULT CreatePixelShader(const DWORD *pFunction, DWORD *pHandle) = 0;
  virtual HRESULT DeletePixelShader(DWORD Handle) = 0;
  virtual HRESULT ValidateDevice(DWORD *pNumPasses) = 0;
  virtual HRESULT ResourceManagerDiscardBytes(DWORD Bytes) = 0;
  virtual HRESULT UpdateTexture(IDirect3DBaseTexture8 *pSourceTexture,
                                IDirect3DBaseTexture8 *pDestinationTexture) = 0;
  virtual HRESULT GetFrontBuffer(IDirect3DSurface8 *pDestSurface) = 0;
  virtual HRESULT
  CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters,
                            IDirect3DSwapChain8 **pSwapChain) = 0;
  virtual UINT GetAvailableTextureMem() = 0;
  virtual BOOL ShowCursor(BOOL bShow) = 0;
  virtual HRESULT SetCursorProperties(UINT XHotSpot, UINT YHotSpot,
                                      IDirect3DSurface8 *pCursorBitmap) = 0;
  virtual void SetCursorPosition(int X, int Y, DWORD Flags) = 0;
};

/* ===================================================================
 * IDirect3D8 — factory interface (stub)
 * =================================================================== */
struct IDirect3D8 : public IUnknown {
  virtual UINT GetAdapterCount() = 0;
  virtual HRESULT GetAdapterIdentifier(UINT Adapter, DWORD Flags,
                                       D3DADAPTER_IDENTIFIER8 *pId) = 0;
  virtual UINT GetAdapterModeCount(UINT Adapter) = 0;
  virtual HRESULT EnumAdapterModes(UINT Adapter, UINT Mode,
                                   D3DDISPLAYMODE *pMode) = 0;
  virtual HRESULT GetAdapterDisplayMode(UINT Adapter,
                                        D3DDISPLAYMODE *pMode) = 0;
  virtual HRESULT CreateDevice(UINT Adapter, DWORD DeviceType,
                               void *hFocusWindow, DWORD BehaviorFlags,
                               D3DPRESENT_PARAMETERS *pParams,
                               IDirect3DDevice8 **ppDevice) = 0;
  virtual HRESULT GetDeviceCaps(UINT Adapter, DWORD DeviceType,
                                D3DCAPS8 *pCaps) = 0;
  virtual HRESULT CheckDeviceFormat(UINT Adapter, DWORD DeviceType,
                                    DWORD AdapterFormat, DWORD Usage,
                                    DWORD RType, DWORD CheckFormat) = 0;
  virtual HRESULT CheckDeviceType(UINT Adapter, DWORD DeviceType,
                                  DWORD DisplayFormat, DWORD BackBufferFormat,
                                  int Windowed) = 0;
  virtual HRESULT CheckDeviceMultiSampleType(UINT Adapter, DWORD DeviceType,
                                             DWORD SurfaceFormat, int Windowed,
                                             DWORD MultiSampleType) = 0;
  virtual HRESULT CheckDepthStencilMatch(UINT Adapter, DWORD DeviceType,
                                         DWORD AdapterFormat,
                                         DWORD RenderTargetFormat,
                                         DWORD DepthStencilFormat) = 0;
};

/* Direct3DCreate8 — entry point (our GLES3 wrapper implements this) */
#ifdef __cplusplus
extern "C" {
#endif
IDirect3D8 *Direct3DCreate8(UINT SDKVersion);
#ifdef __cplusplus
}
#endif

/* D3D clear flags */
#define D3DCLEAR_TARGET 0x00000001L
#define D3DCLEAR_ZBUFFER 0x00000002L
#define D3DCLEAR_STENCIL 0x00000004L

/* Device types */
#define D3DDEVTYPE_HAL 1
#define D3DDEVTYPE_REF 2

/* Create flags */
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING 0x00000020L
#define D3DCREATE_HARDWARE_VERTEXPROCESSING 0x00000040L
#define D3DCREATE_MIXED_VERTEXPROCESSING 0x00000080L

/* Adapter */
#define D3DADAPTER_DEFAULT 0

/* IDirect3DSwapChain8 — stub */
struct IDirect3DSwapChain8 : public IUnknown {
  virtual HRESULT Present(const RECT *pSrc, const RECT *pDst, void *hDstWnd,
                          const RGNDATA *pDirtyRgn) = 0;
  virtual HRESULT GetBackBuffer(UINT BackBuffer, DWORD Type,
                                IDirect3DSurface8 **ppBackBuffer) = 0;
};

/* LP pointer typedefs */
typedef IDirect3D8 *LPDIRECT3D8;
typedef IDirect3DDevice8 *LPDIRECT3DDEVICE8;
typedef IDirect3DTexture8 *LPDIRECT3DTEXTURE8;
typedef IDirect3DSurface8 *LPDIRECT3DSURFACE8;
typedef IDirect3DVertexBuffer8 *LPDIRECT3DVERTEXBUFFER8;
typedef IDirect3DIndexBuffer8 *LPDIRECT3DINDEXBUFFER8;
typedef IDirect3DBaseTexture8 *LPDIRECT3DBASETEXTURE8;
typedef IDirect3DSwapChain8 *LPDIRECT3DSWAPCHAIN8;

/* Cube and Volume texture stubs */
struct IDirect3DCubeTexture8 : public IDirect3DBaseTexture8 {
  virtual HRESULT LockRect(DWORD FaceType, UINT Level,
                           D3DLOCKED_RECT *pLockedRect, const RECT *pRect,
                           DWORD Flags) = 0;
  virtual HRESULT UnlockRect(DWORD FaceType, UINT Level) = 0;
  virtual HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) = 0;
  virtual HRESULT GetCubeMapSurface(DWORD FaceType, UINT Level,
                                    IDirect3DSurface8 **ppSurface) = 0;
};

struct IDirect3DVolumeTexture8 : public IDirect3DBaseTexture8 {
  virtual HRESULT GetLevelDesc(UINT Level, D3DVOLUME_DESC *pDesc) = 0;
  virtual HRESULT LockBox(UINT Level, D3DLOCKED_BOX *pLockedVolume,
                          const void *pBox, DWORD Flags) = 0;
  virtual HRESULT UnlockBox(UINT Level) = 0;
};

typedef IDirect3DCubeTexture8 *LPDIRECT3DCUBETEXTURE8;
typedef IDirect3DVolumeTexture8 *LPDIRECT3DVOLUMETEXTURE8;

/* Additional D3DERR codes */
#define D3DERR_DEVICELOST ((HRESULT)0x88760868L)
#define D3DERR_DEVICENOTRESET ((HRESULT)0x88760869L)
#define D3DERR_NOTAVAILABLE ((HRESULT)0x8876086AL)
#define D3DERR_INVALIDCALL ((HRESULT)0x8876086CL)

/* Presentation interval */
#define D3DPRESENT_INTERVAL_DEFAULT 0x00000000L
#define D3DPRESENT_INTERVAL_ONE 0x00000001L
#define D3DPRESENT_INTERVAL_IMMEDIATE 0x80000000L

/* Resource types */
#define D3DRTYPE_SURFACE 1
#define D3DRTYPE_TEXTURE 3
#define D3DRTYPE_CUBETEXTURE 5

/* Backbuffer types */
#define D3DBACKBUFFER_TYPE_MONO 0

/* Gamma Ramp flags */
#define D3DSGR_NO_CALIBRATION 0x00000000
#define D3DSGR_CALIBRATE 0x00000001


#include "d3dx8core.h"
#include "d3dx8math.h"
#ifdef __EMSCRIPTEN__

#include "../gles3_wrapper.h"
#include "../gles3_fvf.h"

// Proper COM-style refcounting for heap-allocated D3D objects.
// AddRef increments, Release decrements and deletes at zero.
// Use DUMMY_IUNKNOWN_SINGLETON for objects that must never be deleted
// (e.g. static/stack-allocated objects — none exist here currently).
#define DUMMY_IUNKNOWN \
    ULONG m_comRefCount = 1; \
    virtual HRESULT QueryInterface(REFIID, void **) override { return 0; } \
    virtual ULONG AddRef() override { return ++m_comRefCount; } \
    virtual ULONG Release() override { \
        ULONG r = --m_comRefCount; \
        if (r == 0) { delete this; } \
        return r; \
    }

#define DUMMY_IBASETEXTURE8 \
    virtual DWORD GetLevelCount() override { return 1; } \
    virtual DWORD GetPriority() override { return 0; } \
    virtual DWORD SetPriority(DWORD PriorityNew) override { return 0; } \
    virtual DWORD SetLOD(DWORD LODNew) override { return 0; } \
    virtual DWORD GetLOD() override { return 0; }

// ---------------------------------------------------------------------------
// Bytes-per-pixel for each D3DFORMAT used by the game
// ---------------------------------------------------------------------------
static inline UINT D3DFmt_BPP(D3DFORMAT fmt) {
    switch (fmt) {
        case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: return 4;
        case D3DFMT_R8G8B8:   return 3;
        case D3DFMT_R5G6B5:   case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4:
        case D3DFMT_A8L8:     case D3DFMT_V8U8:     return 2;
        case D3DFMT_A8:       case D3DFMT_L8:       case D3DFMT_P8:        return 1;
        default:              return 4;  // safe default
    }
}

// Bytes to hold one w x h level. DXT data is 4x4 blocks (8 bytes DXT1, 16 DXT3/5), so a
// level below 4x4 still needs a whole block; w*h*BPP alone is less than that for 1x1/2x1.
static inline size_t D3DFmt_Level_Bytes(D3DFORMAT fmt, UINT w, UINT h) {
    size_t px = (size_t)w * h * D3DFmt_BPP(fmt);
    size_t bs = fmt == (D3DFORMAT)827611204 ? 8 : (fmt == (D3DFORMAT)861165636 || fmt == (D3DFORMAT)894720068) ? 16 : 0;
    size_t blocks = bs * ((w + 3) / 4) * ((h + 3) / 4);
    return px > blocks ? px : blocks;
}

// ---------------------------------------------------------------------------
// CPU-buffer-backed surface — returned by GetSurfaceLevel / CreateImageSurface.
// Release() deletes this object since it's always heap-allocated.
// ---------------------------------------------------------------------------
struct Emscripten_IDirect3DTexture8;

struct Emscripten_IDirect3DSurface8 : public IDirect3DSurface8 {
    UINT   Width;
    UINT   Height;
    UINT   BytesPerPixel;
    D3DFORMAT Format;
    std::vector<uint8_t> Pixels;
    Emscripten_IDirect3DTexture8 *ParentTexture = nullptr;
    UINT ParentLevel = 0;

    Emscripten_IDirect3DSurface8();
    Emscripten_IDirect3DSurface8(UINT w, UINT h, D3DFORMAT fmt);
    virtual ~Emscripten_IDirect3DSurface8();

    // Always heap-allocated — proper COM refcounting
    ULONG m_comRefCount = 1;
    virtual HRESULT QueryInterface(REFIID, void **) override { return 0; }
    virtual ULONG   AddRef()  override;
    virtual ULONG   Release() override;

    virtual HRESULT GetDesc(D3DSURFACE_DESC *pDesc) override {
        if (pDesc) {
            pDesc->Format = Format;
            pDesc->Width  = Width;
            pDesc->Height = Height;
        }
        return 0;
    }
    virtual HRESULT LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect,
                             DWORD Flags) override;
    virtual HRESULT UnlockRect() override;
};

// ---------------------------------------------------------------------------
// CPU-buffer-backed texture — provides real memory for LockRect/UnlockRect.
// GL upload happens lazily on first SetTexture call (or after dirty unlock).
// ---------------------------------------------------------------------------
struct Emscripten_IDirect3DTexture8 : public IDirect3DTexture8 {
    UINT       Width, Height, LevelCount, BPP;
    D3DFORMAT  Fmt;
    bool       dirty;
    GLuint     gl_tex_id;

    struct MipLevel { UINT w, h; std::vector<uint8_t> pixels; };
    std::vector<MipLevel> levels;

    // Default ctor — 1×1 placeholder
    Emscripten_IDirect3DTexture8()
        : Width(1), Height(1), LevelCount(1), BPP(4),
          Fmt(D3DFMT_A8R8G8B8), dirty(false), gl_tex_id(0)
    { _alloc_levels(); }

    // Dimensional ctor — called by CreateTexture / D3DXCreateTexture
    Emscripten_IDirect3DTexture8(UINT w, UINT h, UINT lvls, D3DFORMAT fmt)
        : Width(w ? w : 1), Height(h ? h : 1),
          LevelCount(lvls ? lvls : _count_mips(w ? w : 1, h ? h : 1)),
          BPP(D3DFmt_BPP(fmt)), Fmt(fmt), dirty(false), gl_tex_id(0)
    { _alloc_levels(); }

    ~Emscripten_IDirect3DTexture8() {
        if (gl_tex_id) {
            GLES3_Unregister_Texture(gl_tex_id);
            glDeleteTextures(1, &gl_tex_id);
            gl_tex_id = 0;
        }
    }


    void _alloc_levels() {
        levels.resize(LevelCount);
        UINT mw = Width, mh = Height;
        for (UINT i = 0; i < LevelCount; ++i) {
            levels[i].w = mw; levels[i].h = mh;
            levels[i].pixels.assign(D3DFmt_Level_Bytes(Fmt, mw, mh), 0);
            if (mw > 1) mw >>= 1;
            if (mh > 1) mh >>= 1;
        }
    }

    static UINT _count_mips(UINT w, UINT h) {
        UINT n = 1;
        while (w > 1 || h > 1) { if (w > 1) w >>= 1; if (h > 1) h >>= 1; ++n; }
        return n;
    }

    // Returns true if this texture format is DXT compressed.
    static bool _is_dxt(D3DFORMAT fmt) {
        // DXT1=827611204, DXT3=861165636, DXT5=894720068
        return (fmt == (D3DFORMAT)827611204 ||
                fmt == (D3DFORMAT)861165636 ||
                fmt == (D3DFORMAT)894720068);
    }

    // Upload CPU pixels to a new GL texture and return its id.
    //
    // For DXT (compressed) textures: all mip levels stored in levels[] are
    // uploaded individually with glCompressedTexImage2D so we don't rely on
    // glGenerateMipmap from a compressed base level (unreliable in WebGL2).
    //
    // For uncompressed textures: Create_Texture uploads level 0 and calls
    // glGenerateMipmap to auto-generate the rest.
    GLuint Upload_To_GL() {
        if (levels.empty() || levels[0].pixels.empty()) return 0;

        bool dxt = _is_dxt(Fmt);

        if (gl_tex_id && !dxt) {
            GLES3_TextureConverter::Update_Texture(
                gl_tex_id, Width, Height, (unsigned int)Fmt,
                (unsigned int)LevelCount, levels[0].pixels.data());
            dirty = false;
            return gl_tex_id;
        }

        if (gl_tex_id) {
            GLES3_Unregister_Texture(gl_tex_id);
            glDeleteTextures(1, &gl_tex_id);
            gl_tex_id = 0;
        }

        if (dxt && LevelCount > 1) {
            // Pass mip_levels=1 so Create_Texture sets up the texture object
            // without calling glGenerateMipmap (we upload each level below).
            gl_tex_id = GLES3_TextureConverter::Create_Texture(
                Width, Height, (unsigned int)Fmt,
                1, levels[0].pixels.data());
            GLES3_Register_Texture(gl_tex_id, true);

            // Ensure driver knows how many levels to expect
            glBindTexture(GL_TEXTURE_2D, gl_tex_id);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, (int)LevelCount - 1);

            GLenum internal_fmt = GLES3_TextureConverter::DX8_Format_To_GL_Internal(
                (unsigned int)Fmt);
            UINT mw = Width, mh = Height;
            // Level 0 already uploaded; upload levels 1..N
            for (UINT lvl = 1; lvl < LevelCount; ++lvl) {
                if (mw > 1) mw >>= 1;
                if (mh > 1) mh >>= 1;
                if (levels[lvl].pixels.empty()) break;
                UINT bsz = (internal_fmt ==
                    GL_COMPRESSED_RGB_S3TC_DXT1_EXT) ? 8 : 16;
                UINT data_size = ((mw + 3) / 4) * ((mh + 3) / 4) * bsz;
                glCompressedTexImage2D(GL_TEXTURE_2D, (GLint)lvl,
                    internal_fmt, (GLsizei)mw, (GLsizei)mh, 0,
                    (GLsizei)data_size, levels[lvl].pixels.data());
            }
            // Now set mip filter (Create_Texture set GL_LINEAR since we
            // passed mip_levels=1 — override it).
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                            GL_LINEAR_MIPMAP_LINEAR);
        } else {
            // Uncompressed (or single-mip DXT): let Create_Texture handle
            // glGenerateMipmap if LevelCount > 1.
            gl_tex_id = GLES3_TextureConverter::Create_Texture(
                Width, Height, (unsigned int)Fmt,
                (unsigned int)LevelCount, levels[0].pixels.data());
        }

        dirty = false;
        return gl_tex_id;
    }

    DUMMY_IUNKNOWN

    // Override GetLevelCount to return actual level count
    virtual DWORD GetLevelCount() override { return (DWORD)LevelCount; }
    virtual DWORD GetPriority()   override { return 0; }
    virtual DWORD SetPriority(DWORD) override { return 0; }
    virtual DWORD SetLOD(DWORD)   override { return 0; }
    virtual DWORD GetLOD()        override { return 0; }

    virtual HRESULT LockRect(UINT Level, D3DLOCKED_RECT *pLockedRect,
                             const RECT *pRect, DWORD) override {
        if (!pLockedRect) return 0;
        if (Level >= LevelCount || levels[Level].pixels.empty()) {
            pLockedRect->pBits = nullptr;
            pLockedRect->Pitch = 0;
            return 0;
        }
        pLockedRect->Pitch  = (int)(levels[Level].w * BPP);
        // D3D points pBits at the locked rect's first pixel, not the surface's.
        pLockedRect->pBits  = levels[Level].pixels.data() +
            (pRect ? pRect->top * pLockedRect->Pitch + pRect->left * BPP : 0);
        return 0;
    }

    virtual HRESULT UnlockRect(UINT) override { dirty = true; return 0; }

    virtual HRESULT GetSurfaceLevel(UINT Level, IDirect3DSurface8 **ppS) override {
        if (!ppS) return 0;
        UINT idx = (Level < LevelCount) ? Level : 0;
        Emscripten_IDirect3DSurface8 *surf =
            new Emscripten_IDirect3DSurface8(levels[idx].w, levels[idx].h, Fmt);
        surf->ParentTexture = this;
        surf->ParentLevel = idx;
        this->AddRef();
        surf->BytesPerPixel = BPP;
        *ppS = surf;
        return 0;
    }

    virtual HRESULT GetLevelDesc(UINT Level, D3DSURFACE_DESC *pDesc) override {
        if (pDesc && Level < LevelCount) {
            pDesc->Format = Fmt;
            pDesc->Width  = levels[Level].w;
            pDesc->Height = levels[Level].h;
        }
        return 0;
    }
};

inline Emscripten_IDirect3DSurface8::Emscripten_IDirect3DSurface8()
    : Width(0), Height(0), BytesPerPixel(4), Format(D3DFMT_A8R8G8B8) {}

inline Emscripten_IDirect3DSurface8::Emscripten_IDirect3DSurface8(UINT w, UINT h, D3DFORMAT fmt)
    : Width(w), Height(h), BytesPerPixel(D3DFmt_BPP(fmt)), Format(fmt)
{
    Pixels.assign(D3DFmt_Level_Bytes(fmt, w, h), 0);
}

inline Emscripten_IDirect3DSurface8::~Emscripten_IDirect3DSurface8() {
    if (ParentTexture) {
        ParentTexture->Release();
    }
}

inline ULONG Emscripten_IDirect3DSurface8::AddRef() {
    return ++m_comRefCount;
}

inline ULONG Emscripten_IDirect3DSurface8::Release() {
    ULONG r = --m_comRefCount;
    if (r == 0) { delete this; }
    return r;
}

inline HRESULT Emscripten_IDirect3DSurface8::LockRect(D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD) {
    if (pLockedRect) {
        if (ParentTexture) {
            if (ParentLevel < ParentTexture->LevelCount && !ParentTexture->levels[ParentLevel].pixels.empty()) {
                pLockedRect->Pitch = (int)(Width * BytesPerPixel);
                pLockedRect->pBits = ParentTexture->levels[ParentLevel].pixels.data();
            } else {
                pLockedRect->Pitch = 0;
                pLockedRect->pBits = nullptr;
            }
        } else if (!Pixels.empty()) {
            pLockedRect->Pitch = (int)(Width * BytesPerPixel);
            pLockedRect->pBits = Pixels.data();
        } else {
            pLockedRect->Pitch = 0;
            pLockedRect->pBits = nullptr;
        }
        // D3D points pBits at the locked rect's first pixel, not the surface's.
        if (pRect && pLockedRect->pBits)
            pLockedRect->pBits = (uint8_t *)pLockedRect->pBits +
                pRect->top * pLockedRect->Pitch + pRect->left * BytesPerPixel;
    }
    return 0;
}

inline HRESULT Emscripten_IDirect3DSurface8::UnlockRect() {
    if (ParentTexture) {
        ParentTexture->dirty = true;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Vertex buffer — CPU-side byte buffer + GL VBO uploaded on demand
// ---------------------------------------------------------------------------
struct Emscripten_IDirect3DVertexBuffer8 : public IDirect3DVertexBuffer8 {
    std::vector<uint8_t> data;
    UINT  stride;    // set by SetStreamSource
    GLuint gl_vbo;
    bool dirty;

    Emscripten_IDirect3DVertexBuffer8(UINT length = 0)
        : stride(0), gl_vbo(0), dirty(true) {
        data.resize(length, 0);
    }
    ~Emscripten_IDirect3DVertexBuffer8() {
        if (gl_vbo) { glDeleteBuffers(1, &gl_vbo); gl_vbo = 0; }
    }

    DUMMY_IUNKNOWN

    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData,
                         DWORD Flags) override {
        if (ppbData) *ppbData = data.data() + OffsetToLock;
        return 0;
    }
    virtual HRESULT Unlock() override { dirty = true; return 0; }

    // Upload CPU data to GL VBO and bind it
    GLuint Upload_And_Bind() {
        if (!gl_vbo) glGenBuffers(1, &gl_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
        if (dirty) {
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)data.size(),
                         data.data(), GL_DYNAMIC_DRAW);
            dirty = false;
        }
        return gl_vbo;
    }
};

// ---------------------------------------------------------------------------
// Index buffer — CPU-side byte buffer + GL IBO uploaded on demand
// ---------------------------------------------------------------------------
struct Emscripten_IDirect3DIndexBuffer8 : public IDirect3DIndexBuffer8 {
    std::vector<uint8_t> data;
    GLuint gl_ibo;
    bool dirty;

    Emscripten_IDirect3DIndexBuffer8(UINT length = 0)
        : gl_ibo(0), dirty(true) {
        data.resize(length, 0);
    }
    ~Emscripten_IDirect3DIndexBuffer8() {
        if (gl_ibo) { glDeleteBuffers(1, &gl_ibo); gl_ibo = 0; }
    }

    DUMMY_IUNKNOWN

    virtual HRESULT Lock(UINT OffsetToLock, UINT SizeToLock, BYTE **ppbData,
                         DWORD Flags) override {
        if (ppbData) *ppbData = data.data() + OffsetToLock;
        return 0;
    }
    virtual HRESULT Unlock() override { dirty = true; return 0; }

    GLuint Upload_And_Bind() {
        if (!gl_ibo) glGenBuffers(1, &gl_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gl_ibo);
        if (dirty) {
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)data.size(),
                         data.data(), GL_DYNAMIC_DRAW);
            dirty = false;
        }
        return gl_ibo;
    }
};

struct Emscripten_IDirect3DSwapChain8 : public IDirect3DSwapChain8 {
    DUMMY_IUNKNOWN
    virtual HRESULT Present(const RECT *pSrc, const RECT *pDst, void *hDstWnd,
                          const RGNDATA *pDirtyRgn) override { return 0; }
    virtual HRESULT GetBackBuffer(UINT BackBuffer, DWORD Type,
                                IDirect3DSurface8 **ppBackBuffer) override { return 0; }
};

struct Emscripten_IDirect3DDevice8 : public IDirect3DDevice8 {
    // ----- per-draw state -----
    Emscripten_IDirect3DVertexBuffer8 *current_vb     = nullptr;
    UINT                               current_stride  = 0;
    DWORD                              current_fvf     = 0;   // set by SetVertexShader (FVF handle)
    Emscripten_IDirect3DIndexBuffer8  *current_ib      = nullptr;
    UINT                               base_vtx_index  = 0;

    DUMMY_IUNKNOWN
    virtual HRESULT GetDirect3D(IDirect3D8 **ppD3D8) override { return 0; }
    virtual HRESULT TestCooperativeLevel() override { return 0; }
    virtual HRESULT GetDeviceCaps(D3DCAPS8 *pCaps) override {
        if (pCaps) {
            memset(pCaps, 0, sizeof(*pCaps));
            // WebGL/GLES3 safe capability values
            pCaps->DevCaps             = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_DRAWPRIMITIVES2;
            pCaps->MaxTextureWidth     = 4096;
            pCaps->MaxTextureHeight    = 4096;
            pCaps->MaxVolumeExtent     = 256;
            pCaps->MaxTextureRepeat    = 8192;
            pCaps->MaxTextureAspectRatio = 0;   // 0 = no restriction
            pCaps->MaxAnisotropy       = 4;
            pCaps->MaxPointSize        = 256.0f;
            pCaps->MaxPrimitiveCount   = 1048575;
            pCaps->MaxVertexIndex      = 1048575;
            pCaps->MaxStreams           = 16;
            pCaps->MaxStreamStride     = 255;
            pCaps->MaxSimultaneousTextures = 8;
            pCaps->MaxTextureBlendStages   = 8;
            pCaps->TextureOpCaps       = 0x03FFFFFF; // support all common tex ops
            pCaps->RasterCaps          = D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_FOGVERTEX |
                                         D3DPRASTERCAPS_WFOG;
            // GL has bilinear and mip filtering; without these caps TextureFilterClass
            // maps FILTER_TYPE_BEST (the default) to POINT, e.g. a blocky shroud.
            pCaps->TextureFilterCaps   = D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFLINEAR |
                                         D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MIPFPOINT;
            pCaps->SrcBlendCaps        = 0x00003FFF;
            pCaps->DestBlendCaps       = 0x00003FFF;
            pCaps->TextureCaps         = D3DPTEXTURECAPS_PROJECTED | D3DPTEXTURECAPS_CUBEMAP;
            pCaps->VertexShaderVersion = 0x0101;  // VS 1.1
            pCaps->MaxVertexShaderConst = 96;
            pCaps->PixelShaderVersion  = 0x0101;  // PS 1.1
            pCaps->MaxPixelShaderValue = 1.0f;
            pCaps->StencilCaps         = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_REPLACE |
                                         D3DSTENCILCAPS_INCRSAT | D3DSTENCILCAPS_DECRSAT |
                                         D3DSTENCILCAPS_INCR    | D3DSTENCILCAPS_DECR;
        }
        return 0;
    }
    virtual HRESULT GetDisplayMode(D3DDISPLAYMODE *pMode) override { return 0; }
    virtual HRESULT Reset(D3DPRESENT_PARAMETERS *pParams) override {
        Size_Canvas(pParams);
        return 0;
    }
    // The GL back buffer is the canvas backing store: give it the engine's
    // resolution, and let the page letterbox it at that aspect ratio.
    static void Size_Canvas(const D3DPRESENT_PARAMETERS *pParams) {
#ifdef __EMSCRIPTEN__
        if (!pParams || pParams->BackBufferWidth < 1 || pParams->BackBufferHeight < 1) return;
        EM_ASM({
            Module.gxCanvasW = $0;
            Module.gxCanvasH = $1;
            var c = document.getElementById('canvas');
            if (!c) return;
            c.width = $0;
            c.height = $1;
            c.style.aspectRatio = $0 + ' / ' + $1;
        }, (int)pParams->BackBufferWidth, (int)pParams->BackBufferHeight);
#endif
    }
    virtual HRESULT Present(const RECT *pSrc, const RECT *pDst, void *hDstWnd,
                          const RGNDATA *pDirtyRgn) override {
        GLES3_Swap_Buffers();
        return 0;
    }
    virtual HRESULT Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags,
                        D3DCOLOR Color, float Z, DWORD Stencil) override {
        float r = ((Color >> 16) & 0xFF) / 255.0f;
        float g = ((Color >>  8) & 0xFF) / 255.0f;
        float b = ((Color      ) & 0xFF) / 255.0f;
        float a = ((Color >> 24) & 0xFF) / 255.0f;
        bool cc  = (Flags & D3DCLEAR_TARGET)  != 0;
        bool czs = (Flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) != 0;
        GLES3_Clear(cc, czs, r, g, b, a, Z, (unsigned int)Stencil);
        return 0;
    }
    virtual HRESULT BeginScene() override { GLES3_Begin_Scene(); return 0; }
    virtual HRESULT EndScene()   override { GLES3_End_Scene(false); return 0; }

    // Map D3DTRANSFORMSTATETYPE → GLES3_TransformType
    static unsigned int _d3dts_to_gles3(D3DTRANSFORMSTATETYPE s) {
        switch ((unsigned int)s) {
            case 2:   return GLES3_TS_VIEW;
            case 3:   return GLES3_TS_PROJECTION;
            case 256: return GLES3_TS_WORLD;
            default:
                if ((unsigned int)s >= 16 && (unsigned int)s <= 19)
                    return GLES3_TS_TEXTURE0 + ((unsigned int)s - 16);
                return GLES3_TS_WORLD;
        }
    }
    virtual HRESULT SetTransform(D3DTRANSFORMSTATETYPE State,
                               const D3DMATRIX *pMatrix) override {
        if (pMatrix) GLES3_Set_Transform(_d3dts_to_gles3(State), &pMatrix->_11);
        return 0;
    }
    virtual HRESULT GetTransform(D3DTRANSFORMSTATETYPE State,
                               D3DMATRIX *pMatrix) override {
        if (pMatrix) GLES3_Get_Transform(_d3dts_to_gles3(State), &pMatrix->_11);
        return 0;
    }
    virtual HRESULT SetRenderState(D3DRENDERSTATETYPE State, DWORD Value) override {
        GLES3_Set_Render_State((unsigned int)State, (unsigned int)Value);
        return 0;
    }
    virtual HRESULT GetRenderState(D3DRENDERSTATETYPE State, DWORD *pValue) override { return 0; }
    virtual HRESULT SetTexture(DWORD Stage, IDirect3DBaseTexture8 *pTexture) override {
        Emscripten_IDirect3DTexture8 *t =
            static_cast<Emscripten_IDirect3DTexture8 *>(pTexture);
        if (t) {
            if (t->gl_tex_id == 0 || t->dirty) t->Upload_To_GL();
            GLES3_Set_Texture(Stage, t->gl_tex_id);
        } else {
            GLES3_Set_Texture(Stage, 0);
        }
        return 0;
    }
    virtual HRESULT SetTextureStageState(DWORD Stage,
                                       D3DTEXTURESTAGESTATETYPE Type,
                                       DWORD Value) override {
        GLES3_Set_Texture_Stage_State((unsigned int)Stage,
                                      (unsigned int)Type, (unsigned int)Value);
        return 0;
    }
    virtual HRESULT SetLight(DWORD Index, const D3DLIGHT8 *pLight) override {
        if (!pLight) return 0;
        GLES3_Light gl;
        gl.type          = pLight->Type;
        gl.diffuse[0]    = pLight->Diffuse.r;  gl.diffuse[1]  = pLight->Diffuse.g;
        gl.diffuse[2]    = pLight->Diffuse.b;  gl.diffuse[3]  = pLight->Diffuse.a;
        gl.specular[0]   = pLight->Specular.r; gl.specular[1] = pLight->Specular.g;
        gl.specular[2]   = pLight->Specular.b; gl.specular[3] = pLight->Specular.a;
        gl.ambient[0]    = pLight->Ambient.r;  gl.ambient[1]  = pLight->Ambient.g;
        gl.ambient[2]    = pLight->Ambient.b;  gl.ambient[3]  = pLight->Ambient.a;
        gl.position[0]   = pLight->Position.x; gl.position[1] = pLight->Position.y;
        gl.position[2]   = pLight->Position.z;
        gl.direction[0]  = pLight->Direction.x;gl.direction[1] = pLight->Direction.y;
        gl.direction[2]  = pLight->Direction.z;
        gl.range = pLight->Range; gl.falloff = pLight->Falloff;
        gl.attenuation0  = pLight->Attenuation0;
        gl.attenuation1  = pLight->Attenuation1;
        gl.attenuation2  = pLight->Attenuation2;
        gl.theta = pLight->Theta; gl.phi = pLight->Phi;
        GLES3_Set_Light((unsigned int)Index, &gl);
        return 0;
    }
    virtual HRESULT LightEnable(DWORD Index, BOOL Enable) override {
        GLES3_Enable_Light((unsigned int)Index, Enable != 0);
        return 0;
    }
    virtual HRESULT SetMaterial(const D3DMATERIAL8 *pMaterial) override {
        if (!pMaterial) return 0;
        GLES3_Material m;
        m.diffuse[0]  = pMaterial->Diffuse.r;  m.diffuse[1]  = pMaterial->Diffuse.g;
        m.diffuse[2]  = pMaterial->Diffuse.b;  m.diffuse[3]  = pMaterial->Diffuse.a;
        m.ambient[0]  = pMaterial->Ambient.r;  m.ambient[1]  = pMaterial->Ambient.g;
        m.ambient[2]  = pMaterial->Ambient.b;  m.ambient[3]  = pMaterial->Ambient.a;
        m.specular[0] = pMaterial->Specular.r; m.specular[1] = pMaterial->Specular.g;
        m.specular[2] = pMaterial->Specular.b; m.specular[3] = pMaterial->Specular.a;
        m.emissive[0] = pMaterial->Emissive.r; m.emissive[1] = pMaterial->Emissive.g;
        m.emissive[2] = pMaterial->Emissive.b; m.emissive[3] = pMaterial->Emissive.a;
        m.power = pMaterial->Power;
        GLES3_Set_Material(&m);
        return 0;
    }
    virtual HRESULT SetViewport(const D3DVIEWPORT8 *pViewport) override {
        if (pViewport)
            GLES3_Set_Viewport(pViewport->X, pViewport->Y,
                               pViewport->Width, pViewport->Height,
                               pViewport->MinZ, pViewport->MaxZ);
        return 0;
    }
    virtual HRESULT GetViewport(D3DVIEWPORT8 *pViewport) override { return 0; }
    virtual HRESULT SetStreamSource(UINT StreamNumber,
                                  IDirect3DVertexBuffer8 *pStreamData,
                                  UINT Stride) override {
        extern int gx_d3d_setstreamsource_calls;
        extern int gx_d3d_setstreamsource_null;
        ++gx_d3d_setstreamsource_calls;
        if (!pStreamData) ++gx_d3d_setstreamsource_null;
        // GeneralsX @feature WebPort 2026-05-05 — black-canvas root cause #4
        //
        // The W3D engine sets streams in a loop over MAX_VERTEX_STREAMS,
        // calling SetStreamSource(0, valid_vb, ...) for stream 0 and
        // SetStreamSource(i, nullptr, 0) for streams 1 .. MAX-1. This stub
        // only models a single bound stream (current_vb / current_stride),
        // so without filtering by stream number, the trailing null calls
        // for streams 1+ would clobber the just-set stream 0 — every
        // subsequent DrawIndexedPrimitive would short-circuit on
        // !current_vb. Filter to stream 0 only; we don't actually support
        // multi-stream geometry in the GLES3 path anyway.
        if (StreamNumber != 0)
            return 0;
        current_vb     = static_cast<Emscripten_IDirect3DVertexBuffer8*>(pStreamData);
        current_stride = Stride;
        return 0;
    }
    virtual HRESULT SetIndices(IDirect3DIndexBuffer8 *pIndexData,
                             UINT BaseVertexIndex) override {
        extern int gx_d3d_setindices_calls;
        extern int gx_d3d_setindices_null;
        ++gx_d3d_setindices_calls;
        if (!pIndexData) ++gx_d3d_setindices_null;
        current_ib       = static_cast<Emscripten_IDirect3DIndexBuffer8*>(pIndexData);
        base_vtx_index   = BaseVertexIndex;
        return 0;
    }
    virtual HRESULT SetVertexShader(DWORD Handle) override {
        // In D3D8 fixed-function mode the "handle" is actually the FVF bitmask.
        // Real programmable shader handles are >= 0xFFFE0000; ignore those for now.
        if (Handle < 0xFFFE0000u)
            current_fvf = Handle;
        return 0;
    }
    virtual HRESULT SetPixelShader(DWORD Handle) override { GLES3_Set_Pixel_Shader(Handle); return 0; }

    // Helper: decode current FVF and apply vertex attrib pointers (VBO must be bound).
    //
    // GeneralsX @feature WebPort 2026-05-05 — apply BaseVertexIndex offset.
    // The W3D engine's dynamic VB allocator returns a `VertexBufferOffset`
    // (in vertices) and passes it through `SetIndices(BaseVertexIndex=offset)`.
    // In real D3D8 the offset is added to every fetched index; in WebGL2/
    // GLES3 there's no `glDrawElementsBaseVertex` equivalent, so we slide
    // the per-attribute pointer base forward by `base_vtx_index * stride`
    // bytes. With this in place, the engine's indices [0,1,2,3] correctly
    // address THIS draw's vertices instead of vertices 0..3 of the shared
    // dynamic VBO (which would be whoever wrote first this frame).
    // D3D applies BaseVertexIndex (SetIndices) to DrawIndexedPrimitive only; the
    // non-indexed and UP draws pass use_base = false.
    void _setup_vertex_attribs(UINT stride_override, bool use_base) {
        if (!current_fvf) return;
        GLES3_FVFDecoder dec;
        dec.Decode(current_fvf);
        UINT stride = stride_override ? stride_override : current_stride;
        if (stride == 0) stride = dec.Get_FVF_Size();
        const UINT base_offset_bytes = use_base ? (UINT)base_vtx_index * stride : 0;
        dec.Apply_Vertex_Attribs(stride, base_offset_bytes);
    }

    virtual HRESULT DrawPrimitive(D3DPRIMITIVETYPE PrimType, UINT StartVertex,
                                UINT PrimCount) override {
        if (!current_vb || current_vb->data.empty()) return 0;
        current_vb->Upload_And_Bind();
        _setup_vertex_attribs(0, false);
        GLES3_Draw_Arrays((unsigned int)PrimType, StartVertex, PrimCount);
        return 0;
    }

    virtual HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE PrimType, UINT PrimCount,
                                  const void *pVertexStreamZeroData,
                                  UINT VertexStreamZeroStride) override {
        if (!pVertexStreamZeroData || !PrimCount) return 0;
        // Compute vertex count for this primitive type
        unsigned int vertex_count = 0;
        switch ((unsigned int)PrimType) {
            case 4: vertex_count = PrimCount * 3; break; // TRIANGLELIST
            case 5: vertex_count = PrimCount + 2; break; // TRIANGLESTRIP
            case 6: vertex_count = PrimCount + 2; break; // TRIANGLEFAN
            case 2: vertex_count = PrimCount * 2; break; // LINELIST
            case 3: vertex_count = PrimCount + 1; break; // LINESTRIP
            case 1: vertex_count = PrimCount;     break; // POINTLIST
            default: vertex_count = PrimCount * 3; break;
        }
        GLuint tmp_vbo = 0;
        glGenBuffers(1, &tmp_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, tmp_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(vertex_count * VertexStreamZeroStride),
                     pVertexStreamZeroData, GL_DYNAMIC_DRAW);
        _setup_vertex_attribs(VertexStreamZeroStride, false);
        GLES3_Draw_Arrays((unsigned int)PrimType, 0, PrimCount);
        glDeleteBuffers(1, &tmp_vbo);
        return 0;
    }

    virtual HRESULT DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimType, UINT MinIndex,
                                       UINT NumVertices, UINT StartIndex,
                                       UINT PrimCount) override {
        // GeneralsX @feature WebPort 2026-05-05 — black-canvas triage
        // Counters declared with C linkage at file scope; defined in
        // dx8wrapper.cpp.
        ++gx_d3d_dip_called;
        if (!current_vb) { ++gx_d3d_dip_no_vb; return 0; }
        if (current_vb->data.empty()) { ++gx_d3d_dip_empty_vb; return 0; }
        if (!current_ib) { ++gx_d3d_dip_no_ib; return 0; }
        if (current_ib->data.empty()) { ++gx_d3d_dip_empty_ib; return 0; }
        current_vb->Upload_And_Bind();
        current_ib->Upload_And_Bind();
        _setup_vertex_attribs(0, true);
        GLES3_Draw_Triangles((unsigned int)PrimType, StartIndex, PrimCount,
                             MinIndex, NumVertices);
        ++gx_d3d_dip_drew;
        return 0;
    }

    virtual HRESULT DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimType,
                                         UINT MinVertexIndex, UINT NumVertices,
                                         UINT PrimCount, const void *pIndexData,
                                         D3DFORMAT IndexDataFormat,
                                         const void *pVertexStreamZeroData,
                                         UINT VertexStreamZeroStride) override {
        if (!pVertexStreamZeroData || !pIndexData || !PrimCount) return 0;
        unsigned int index_count = 0;
        switch ((unsigned int)PrimType) {
            case 4: index_count = PrimCount * 3; break; // TRIANGLELIST
            case 5: index_count = PrimCount + 2; break; // TRIANGLESTRIP
            case 2: index_count = PrimCount * 2; break; // LINELIST
            case 3: index_count = PrimCount + 1; break; // LINESTRIP
            case 1: index_count = PrimCount;     break; // POINTLIST
            default: index_count = PrimCount * 3; break;
        }
        // Upload vertices
        GLuint tmp_vbo = 0;
        glGenBuffers(1, &tmp_vbo);
        glBindBuffer(GL_ARRAY_BUFFER, tmp_vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     (GLsizeiptr)(NumVertices * VertexStreamZeroStride),
                     pVertexStreamZeroData, GL_DYNAMIC_DRAW);
        _setup_vertex_attribs(VertexStreamZeroStride, false);
        // Upload indices (GLES3_Draw_Triangles expects GL_UNSIGNED_SHORT)
        UINT idx_bytes = (IndexDataFormat == D3DFMT_INDEX16) ? 2 : 4;
        GLuint tmp_ibo = 0;
        glGenBuffers(1, &tmp_ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tmp_ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     (GLsizeiptr)(index_count * idx_bytes),
                     pIndexData, GL_DYNAMIC_DRAW);
        GLES3_Draw_Triangles((unsigned int)PrimType, 0, PrimCount,
                             MinVertexIndex, NumVertices);
        glDeleteBuffers(1, &tmp_vbo);
        glDeleteBuffers(1, &tmp_ibo);
        return 0;
    }
    virtual HRESULT CreateTexture(UINT Width, UINT Height, UINT Levels,
                                DWORD Usage, D3DFORMAT Format, D3DPOOL Pool,
                                IDirect3DTexture8 **ppTexture) override {
        if (ppTexture)
            *ppTexture = new Emscripten_IDirect3DTexture8(Width, Height, Levels, Format);
        return 0;
    }
    virtual HRESULT CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF,
                                     D3DPOOL Pool,
                                     IDirect3DVertexBuffer8 **ppVB) override { 
        if (ppVB) *ppVB = new Emscripten_IDirect3DVertexBuffer8(Length);
        return 0; 
    }
    virtual HRESULT CreateIndexBuffer(UINT Length, DWORD Usage, D3DFORMAT Format,
                                    D3DPOOL Pool,
                                    IDirect3DIndexBuffer8 **ppIB) override { 
        if (ppIB) *ppIB = new Emscripten_IDirect3DIndexBuffer8(Length);
        return 0; 
    }
    virtual HRESULT GetBackBuffer(UINT BackBuffer, DWORD Type,
                                IDirect3DSurface8 **ppBackBuffer) override { 
        *ppBackBuffer = new Emscripten_IDirect3DSurface8();
        return 0; 
    }
    virtual HRESULT GetRenderTarget(IDirect3DSurface8 **ppRenderTarget) override { 
        *ppRenderTarget = new Emscripten_IDirect3DSurface8();
        return 0; 
    }
    virtual HRESULT SetRenderTarget(IDirect3DSurface8 *pRenderTarget,
                                  IDirect3DSurface8 *pDepthStencil) override { return 0; }
    virtual HRESULT
  GetDepthStencilSurface(IDirect3DSurface8 **ppZStencilSurface) override { 
        *ppZStencilSurface = new Emscripten_IDirect3DSurface8();
        return 0; 
    }
    virtual HRESULT CopyRects(IDirect3DSurface8 *pSrc, const RECT *pSrcRectsArray,
                            UINT cRects, IDirect3DSurface8 *pDst,
                            const POINT *pDstPointsArray) override {
        if (!pSrc || !pDst) return 0;
        Emscripten_IDirect3DSurface8 *src = static_cast<Emscripten_IDirect3DSurface8 *>(pSrc);
        Emscripten_IDirect3DSurface8 *dst = static_cast<Emscripten_IDirect3DSurface8 *>(pDst);

        D3DLOCKED_RECT srcLock, dstLock;
        if (src->LockRect(&srcLock, nullptr, 0) != 0) return 0;
        if (dst->LockRect(&dstLock, nullptr, 0) != 0) {
            src->UnlockRect();
            return 0;
        }

        uint8_t *srcBits = static_cast<uint8_t *>(srcLock.pBits);
        uint8_t *dstBits = static_cast<uint8_t *>(dstLock.pBits);

        if (srcBits && dstBits) {
            if (pSrcRectsArray && cRects > 0) {
                for (UINT i = 0; i < cRects; ++i) {
                    const RECT &srect = pSrcRectsArray[i];
                    POINT dpoint = { srect.left, srect.top };
                    if (pDstPointsArray) {
                        dpoint = pDstPointsArray[i];
                    }
                    
                    int width = srect.right - srect.left;
                    int height = srect.bottom - srect.top;
                    int bytesPerPixel = (int)src->BytesPerPixel;
                    
                    for (int y = 0; y < height; ++y) {
                        int srcY = srect.top + y;
                        int dstY = dpoint.y + y;
                        if (srcY >= 0 && srcY < (int)src->Height && dstY >= 0 && dstY < (int)dst->Height) {
                            uint8_t *srcRow = srcBits + srcY * srcLock.Pitch + srect.left * bytesPerPixel;
                            uint8_t *dstRow = dstBits + dstY * dstLock.Pitch + dpoint.x * (int)dst->BytesPerPixel;
                            int copyBytes = width * bytesPerPixel;
                            int dstXOffset = dpoint.x * (int)dst->BytesPerPixel;
                            if (dstXOffset + copyBytes > (int)(dst->Width * dst->BytesPerPixel)) {
                                copyBytes = (int)(dst->Width * dst->BytesPerPixel) - dstXOffset;
                            }
                            if (copyBytes > 0) {
                                memcpy(dstRow, srcRow, copyBytes);
                            }
                        }
                    }
                }
            } else {
                int copyWidth = (src->Width < dst->Width) ? src->Width : dst->Width;
                int copyHeight = (src->Height < dst->Height) ? src->Height : dst->Height;
                int bytesPerPixel = (int)src->BytesPerPixel;
                int rowSize = copyWidth * bytesPerPixel;

                for (int y = 0; y < copyHeight; ++y) {
                    uint8_t *srcRow = srcBits + y * srcLock.Pitch;
                    uint8_t *dstRow = dstBits + y * dstLock.Pitch;
                    memcpy(dstRow, srcRow, rowSize);
                }
            }
        }

        dst->UnlockRect();
        src->UnlockRect();
        return 0;
    }
    virtual HRESULT SetVertexShaderConstant(DWORD Register,
                                          const void *pConstantData,
                                          DWORD ConstantCount) override { return 0; }
    virtual HRESULT SetPixelShaderConstant(DWORD Register,
                                         const void *pConstantData,
                                         DWORD ConstantCount) override {
        GLES3_Set_Pixel_Shader_Constants(Register, (const float *)pConstantData, ConstantCount);
        return 0;
    }
    virtual HRESULT SetClipPlane(DWORD Index, const float *pPlane) override { return 0; }
    virtual HRESULT CreateImageSurface(UINT Width, UINT Height, D3DFORMAT Format,
                                     IDirect3DSurface8 **ppSurface) override {
        if (ppSurface)
            *ppSurface = new Emscripten_IDirect3DSurface8(Width, Height, Format);
        return 0;
    }
    virtual HRESULT SetPaletteEntries(UINT PaletteNumber,
                                    const PALETTEENTRY *pEntries) override { return 0; }
    virtual HRESULT SetCurrentTexturePalette(UINT PaletteNumber) override { return 0; }
    virtual HRESULT GetGammaRamp(D3DGAMMARAMP *pRamp) override { return 0; }
    virtual HRESULT SetGammaRamp(DWORD Flags, const D3DGAMMARAMP *pRamp) override { return 0; }
    virtual HRESULT CreateVertexShader(const DWORD *pDeclaration,
                                     const DWORD *pFunction, DWORD *pHandle,
                                     DWORD Usage) override { return 0; }
    virtual HRESULT DeleteVertexShader(DWORD Handle) override { return 0; }
    // A program the emulation can't run (texbem, ...) gets no handle, so the caller's handle
    // stays 0 and the fixed-function stages draw it, as before pixel shaders were emulated.
    virtual HRESULT CreatePixelShader(const DWORD *pFunction, DWORD *pHandle) override {
        unsigned int h = GLES3_Create_Pixel_Shader((const unsigned int *)pFunction);
        if (h && pHandle) *pHandle = h;
        return 0;
    }
    virtual HRESULT DeletePixelShader(DWORD Handle) override { return 0; }
    virtual HRESULT ValidateDevice(DWORD *pNumPasses) override { return 0; }
    virtual HRESULT ResourceManagerDiscardBytes(DWORD Bytes) override { return 0; }
    virtual HRESULT UpdateTexture(IDirect3DBaseTexture8 *pSourceTexture,
                                IDirect3DBaseTexture8 *pDestinationTexture) override {
        // For USE_MANAGED_TEXTURES builds this path isn't taken, but handle it anyway:
        // copy CPU pixel data from source to dest and mark dest dirty.
        Emscripten_IDirect3DTexture8 *src =
            static_cast<Emscripten_IDirect3DTexture8 *>(pSourceTexture);
        Emscripten_IDirect3DTexture8 *dst =
            static_cast<Emscripten_IDirect3DTexture8 *>(pDestinationTexture);
        if (src && dst && src->LevelCount == dst->LevelCount) {
            for (UINT i = 0; i < src->LevelCount; ++i)
                dst->levels[i].pixels = src->levels[i].pixels;
            dst->dirty = true;
        } else if (src) {
            // dst may be null — upload src directly
            src->Upload_To_GL();
        }
        return 0;
    }
    virtual HRESULT GetFrontBuffer(IDirect3DSurface8 *pDestSurface) override { return 0; }
    virtual HRESULT
  CreateAdditionalSwapChain(D3DPRESENT_PARAMETERS *pPresentationParameters,
                            IDirect3DSwapChain8 **pSwapChain) override { return 0; }
    virtual UINT GetAvailableTextureMem() override { return 0; }
    virtual BOOL ShowCursor(BOOL bShow) override { return 0; }
    virtual HRESULT SetCursorProperties(UINT XHotSpot, UINT YHotSpot,
                                      IDirect3DSurface8 *pCursorBitmap) override { return 0; }
    virtual void SetCursorPosition(int X, int Y, DWORD Flags) override {  }
};

struct Emscripten_IDirect3DDevice8; // Forward declaration

struct Emscripten_IDirect3D8 : public IDirect3D8 {
    DUMMY_IUNKNOWN
    virtual UINT GetAdapterCount() override { return 1; }
    virtual HRESULT GetAdapterIdentifier(UINT Adapter, DWORD Flags,
                                       D3DADAPTER_IDENTIFIER8 *pId) override { return 0; }
    virtual UINT GetAdapterModeCount(UINT Adapter) override { return 0; }
    virtual HRESULT EnumAdapterModes(UINT Adapter, UINT Mode,
                                   D3DDISPLAYMODE *pMode) override { return 0; }
    virtual HRESULT GetAdapterDisplayMode(UINT Adapter,
                                        D3DDISPLAYMODE *pMode) override { return 0; }
    virtual HRESULT CreateDevice(UINT Adapter, DWORD DeviceType,
                               void *hFocusWindow, DWORD BehaviorFlags,
                               D3DPRESENT_PARAMETERS *pParams,
                               IDirect3DDevice8 **ppDevice) override { 
        *ppDevice = new Emscripten_IDirect3DDevice8();
        Emscripten_IDirect3DDevice8::Size_Canvas(pParams);
        return 0; 
    }
    virtual HRESULT GetDeviceCaps(UINT Adapter, DWORD DeviceType,
                                D3DCAPS8 *pCaps) override {
        if (pCaps) {
            memset(pCaps, 0, sizeof(*pCaps));
            pCaps->DevCaps             = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_DRAWPRIMITIVES2;
            pCaps->MaxTextureWidth     = 4096;
            pCaps->MaxTextureHeight    = 4096;
            pCaps->MaxVolumeExtent     = 256;
            pCaps->MaxTextureRepeat    = 8192;
            pCaps->MaxTextureAspectRatio = 0;
            pCaps->MaxAnisotropy       = 4;
            pCaps->MaxPointSize        = 256.0f;
            pCaps->MaxPrimitiveCount   = 1048575;
            pCaps->MaxVertexIndex      = 1048575;
            pCaps->MaxStreams           = 16;
            pCaps->MaxStreamStride     = 255;
            pCaps->MaxSimultaneousTextures = 8;
            pCaps->MaxTextureBlendStages   = 8;
            pCaps->TextureOpCaps       = 0x03FFFFFF;
            pCaps->RasterCaps          = D3DPRASTERCAPS_FOGTABLE | D3DPRASTERCAPS_FOGVERTEX |
                                         D3DPRASTERCAPS_WFOG;
            // GL has bilinear and mip filtering; without these caps TextureFilterClass
            // maps FILTER_TYPE_BEST (the default) to POINT, e.g. a blocky shroud.
            pCaps->TextureFilterCaps   = D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MAGFLINEAR |
                                         D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MIPFPOINT;
            pCaps->SrcBlendCaps        = 0x00003FFF;
            pCaps->DestBlendCaps       = 0x00003FFF;
            pCaps->TextureCaps         = D3DPTEXTURECAPS_PROJECTED | D3DPTEXTURECAPS_CUBEMAP;
            pCaps->VertexShaderVersion = 0x0101;
            pCaps->MaxVertexShaderConst = 96;
            pCaps->PixelShaderVersion  = 0x0101;
            pCaps->MaxPixelShaderValue = 1.0f;
            pCaps->StencilCaps         = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_REPLACE |
                                         D3DSTENCILCAPS_INCRSAT | D3DSTENCILCAPS_DECRSAT |
                                         D3DSTENCILCAPS_INCR    | D3DSTENCILCAPS_DECR;
        }
        return 0;
    }
    virtual HRESULT CheckDeviceFormat(UINT Adapter, DWORD DeviceType,
                                    DWORD AdapterFormat, DWORD Usage,
                                    DWORD RType, DWORD CheckFormat) override { return 0; }
    virtual HRESULT CheckDeviceType(UINT Adapter, DWORD DeviceType,
                                  DWORD DisplayFormat, DWORD BackBufferFormat,
                                  int Windowed) override { return 0; }
    virtual HRESULT CheckDeviceMultiSampleType(UINT Adapter, DWORD DeviceType,
                                             DWORD SurfaceFormat, int Windowed,
                                             DWORD MultiSampleType) override { return 0; }
    virtual HRESULT CheckDepthStencilMatch(UINT Adapter, DWORD DeviceType,
                                         DWORD AdapterFormat,
                                         DWORD RenderTargetFormat,
                                         DWORD DepthStencilFormat) override { return 0; }
};


extern "C" IDirect3D8 *Direct3DCreate8(UINT SDKVersion);

#endif

#endif /* __EMSCRIPTEN_D3D8_H */

#define D3DX_FILTER_NONE (1 << 0)
#define D3DX_FILTER_POINT (2 << 0)
#define D3DX_FILTER_LINEAR (3 << 0)
#define D3DX_FILTER_TRIANGLE (4 << 0)
#define D3DX_FILTER_BOX (5 << 0)

inline HRESULT D3DXCreateTexture(struct IDirect3DDevice8 *pDevice, UINT Width,
                                 UINT Height, UINT MipLevels, DWORD Usage,
                                 D3DFORMAT Format, DWORD Pool,
                                 struct IDirect3DTexture8 **ppTexture) {
  if (ppTexture)
    *ppTexture = new Emscripten_IDirect3DTexture8(Width, Height, MipLevels, Format);
  return 0;
}

inline HRESULT D3DXCreateTextureFromFileExA(
    struct IDirect3DDevice8 *pDevice, const char *pSrcFile, UINT Width,
    UINT Height, UINT MipLevels, DWORD Usage, D3DFORMAT Format, DWORD Pool,
    DWORD Filter, DWORD MipFilter, D3DCOLOR ColorKey, void *pSrcInfo,
    void *pPalette, struct IDirect3DTexture8 **ppTexture) {
  if (ppTexture)
    *ppTexture = new Emscripten_IDirect3DTexture8(Width, Height, MipLevels, Format);
  return 0;
}

// Pixel <-> A8R8G8B8 for the uncompressed formats surfaces use.
static inline bool D3DX_Read_ARGB(D3DFORMAT f, const uint8_t *p, uint32_t &c) {
  uint32_t v;
  switch (f) {
  case D3DFMT_A8R8G8B8: memcpy(&c, p, 4); return true;
  case D3DFMT_X8R8G8B8: memcpy(&c, p, 4); c |= 0xff000000u; return true;
  case D3DFMT_R8G8B8: c = 0xff000000u | (p[2] << 16) | (p[1] << 8) | p[0]; return true;
  case D3DFMT_R5G6B5: v = p[0] | (p[1] << 8);
    c = 0xff000000u | (((v >> 11) & 31) * 255 / 31) << 16 | (((v >> 5) & 63) * 255 / 63) << 8 | ((v & 31) * 255 / 31); return true;
  case D3DFMT_A1R5G5B5: v = p[0] | (p[1] << 8);
    c = ((v & 0x8000) ? 0xff000000u : 0) | (((v >> 10) & 31) * 255 / 31) << 16 | (((v >> 5) & 31) * 255 / 31) << 8 | ((v & 31) * 255 / 31); return true;
  case D3DFMT_A4R4G4B4: v = p[0] | (p[1] << 8);
    c = ((v >> 12) & 15) * 17u << 24 | ((v >> 8) & 15) * 17u << 16 | ((v >> 4) & 15) * 17u << 8 | (v & 15) * 17u; return true;
  default: return false;
  }
}
static inline bool D3DX_Write_ARGB(D3DFORMAT f, uint8_t *p, uint32_t c) {
  const uint32_t a = c >> 24, r = (c >> 16) & 255, g = (c >> 8) & 255, b = c & 255;
  uint32_t v;
  switch (f) {
  case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: memcpy(p, &c, 4); return true;
  case D3DFMT_R8G8B8: p[0] = b; p[1] = g; p[2] = r; return true;
  case D3DFMT_R5G6B5: v = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3); break;
  case D3DFMT_A1R5G5B5: v = (a >= 128 ? 0x8000 : 0) | (r >> 3) << 10 | (g >> 3) << 5 | (b >> 3); break;
  case D3DFMT_A4R4G4B4: v = (a >> 4) << 12 | (r >> 4) << 8 | (g >> 4) << 4 | (b >> 4); break;
  default: return false;
  }
  p[0] = v & 255; p[1] = v >> 8;
  return true;
}

// Copies (and point-samples, if the rects differ in size) a surface into another.
// W3D builds textures from surfaces this way, e.g. the team-coloured copies of
// infantry textures; a no-op left them all black.
inline HRESULT
D3DXLoadSurfaceFromSurface(void *pDestSurface, const void *pDestPalette,
                           const RECT *pDestRect, void *pSrcSurface,
                           const void *pSrcPalette, const RECT *pSrcRect,
                           DWORD Filter, D3DCOLOR ColorKey) {
  auto *dst = static_cast<Emscripten_IDirect3DSurface8 *>(static_cast<IDirect3DSurface8 *>(pDestSurface));
  auto *src = static_cast<Emscripten_IDirect3DSurface8 *>(static_cast<IDirect3DSurface8 *>(pSrcSurface));
  if (!dst || !src) return 0;
  const RECT sr = pSrcRect ? *pSrcRect : RECT{0, 0, (LONG)src->Width, (LONG)src->Height};
  const RECT dr = pDestRect ? *pDestRect : RECT{0, 0, (LONG)dst->Width, (LONG)dst->Height};
  const int sw = sr.right - sr.left, sh = sr.bottom - sr.top;
  const int dw = dr.right - dr.left, dh = dr.bottom - dr.top;
  if (sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return 0;
  D3DLOCKED_RECT sl, dl;
  src->LockRect(&sl, nullptr, 0);
  dst->LockRect(&dl, nullptr, 0);
  const uint8_t *sp = (const uint8_t *)sl.pBits;
  uint8_t *dp = (uint8_t *)dl.pBits;
  const UINT sb = src->BytesPerPixel, db = dst->BytesPerPixel;
  const bool same = src->Format == dst->Format;
  if (sp && dp) {
    for (int y = 0; y < dh; ++y) {
      const uint8_t *srow = sp + (size_t)(sr.top + y * sh / dh) * sl.Pitch;
      uint8_t *drow = dp + (size_t)(dr.top + y) * dl.Pitch;
      for (int x = 0; x < dw; ++x) {
        const uint8_t *s = srow + (size_t)(sr.left + x * sw / dw) * sb;
        uint8_t *d = drow + (size_t)(dr.left + x) * db;
        uint32_t c;
        if (same) memcpy(d, s, db);
        else if (D3DX_Read_ARGB(src->Format, s, c)) D3DX_Write_ARGB(dst->Format, d, c);
      }
    }
  }
  src->UnlockRect();
  dst->UnlockRect();
  return 0;
}

inline HRESULT D3DXFilterTexture(void *pBaseTexture, const void *pPalette,
                                 UINT SrcLevel, DWORD Filter) {
  return 0;
}

inline HRESULT
D3DXCreateCubeTexture(struct IDirect3DDevice8 *pDevice, UINT EdgeLength,
                      UINT MipLevels, DWORD Usage, D3DFORMAT Format, DWORD Pool,
                      struct IDirect3DCubeTexture8 **ppCubeTexture) {
  return 0;
}

inline HRESULT
D3DXCreateVolumeTexture(struct IDirect3DDevice8 *pDevice, UINT Width,
                        UINT Height, UINT Depth, UINT MipLevels, DWORD Usage,
                        D3DFORMAT Format, DWORD Pool,
                        struct IDirect3DVolumeTexture8 **ppVolumeTexture) {
  return 0;
}
