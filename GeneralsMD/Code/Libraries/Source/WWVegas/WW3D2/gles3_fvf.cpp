/*
** GeneralsX Web Port — FVF (Flexible Vertex Format) Decoder Implementation
**
** Decodes DX8's Flexible Vertex Format bitmask into byte offsets,
** then translates those offsets to glVertexAttribPointer() calls.
**
** VERTEX DATA LAYOUT (DX8 specification, strict order):
**   [Position 3f] [BlendWeights 1-4f] [Normal 3f] [Diffuse 4ub] [Specular 4ub] [TexCoord0 2-4f] [TexCoord1 2-4f] ...
**
** DIFFUSE/SPECULAR COLOR HANDLING:
**   DX8 stores vertex colors as DWORD in BGRA byte order (B=byte0, G=byte1, R=byte2, A=byte3).
**   We pass them as GL_UNSIGNED_BYTE with GL_TRUE normalization, which maps 0-255 → 0.0-1.0.
**   The BGRA→RGBA swizzle is handled by using GL_BGRA_EXT format where available,
**   or by shader swizzle on platforms that don't support it.
**   WebGL2 NOTE: GL_BGRA is NOT a valid format. We handle the swizzle in the shader instead.
**
** LICENSE: GPL-3.0
*/

#if defined(__EMSCRIPTEN__)

#include "gles3_fvf.h"
#include "gles3_wrapper.h"
#include <cstring>
#include <cstdio>

// ============================================================================
// Get_FVF_Vertex_Size — Calculate total vertex size from FVF flags
// Mirrors D3DXGetFVFVertexSize() from the DX8 SDK
// ============================================================================
unsigned int GLES3_FVFDecoder::Get_FVF_Vertex_Size(unsigned int fvf) {
    unsigned int size = 0;

    // Position
    switch (fvf & D3DFVF_POSITION_MASK) {
        case D3DFVF_XYZ:    size += 3 * sizeof(float); break;    // 12 bytes
        case D3DFVF_XYZRHW: size += 4 * sizeof(float); break;    // 16 bytes (pre-transformed)
        case D3DFVF_XYZB1:  size += 4 * sizeof(float); break;    // 16 bytes (XYZ + 1 blend)
        case D3DFVF_XYZB2:  size += 5 * sizeof(float); break;    // 20 bytes
        case D3DFVF_XYZB3:  size += 6 * sizeof(float); break;    // 24 bytes
        case D3DFVF_XYZB4:
            if (fvf & D3DFVF_LASTBETA_UBYTE4)
                size += 6 * sizeof(float) + sizeof(unsigned int); // 3 blend floats + 1 DWORD index
            else
                size += 7 * sizeof(float); // 28 bytes
            break;
        case D3DFVF_XYZB5:  size += 8 * sizeof(float); break;
    }

    // Normal
    if (fvf & D3DFVF_NORMAL)
        size += 3 * sizeof(float);  // 12 bytes

    // Diffuse color
    if (fvf & D3DFVF_DIFFUSE)
        size += sizeof(unsigned int);  // 4 bytes (BGRA packed)

    // Specular color
    if (fvf & D3DFVF_SPECULAR)
        size += sizeof(unsigned int);  // 4 bytes (BGRA packed)

    // Texture coordinates
    unsigned int tex_count = Get_Tex_Count(fvf);
    for (unsigned int i = 0; i < tex_count; i++) {
        size += Get_Tex_Coord_Size(fvf, i) * sizeof(float);
    }

    return size;
}

// ============================================================================
// Get_Tex_Count — Extract texture coordinate count from FVF
// ============================================================================
unsigned int GLES3_FVFDecoder::Get_Tex_Count(unsigned int fvf) {
    return (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
}

// ============================================================================
// Get_Tex_Coord_Size — Get dimension (1-4 floats) of a texture coord set
// Bits 16-31 encode the sizes: 2 bits per set
//   00 = 2 floats (default, UV)
//   01 = 3 floats (UVW)
//   10 = 4 floats (UVWQ)
//   11 = 1 float  (U only)
// ============================================================================
unsigned int GLES3_FVFDecoder::Get_Tex_Coord_Size(unsigned int fvf,
                                                    unsigned int index) {
    unsigned int bits = (fvf >> (index * 2 + 16)) & 0x3;
    switch (bits) {
        case D3DFVF_TEXTUREFORMAT1: return 1;   // 1D
        case D3DFVF_TEXTUREFORMAT2: return 2;   // 2D (default)
        case D3DFVF_TEXTUREFORMAT3: return 3;   // 3D
        case D3DFVF_TEXTUREFORMAT4: return 4;   // 4D
        default: return 2;
    }
}

// ============================================================================
// Decode — Parse FVF bitmask and compute byte offsets for each component
// Mirrors the FVFInfoClass constructor from the original dx8fvf.cpp
// ============================================================================
void GLES3_FVFDecoder::Decode(unsigned int new_fvf) {
    fvf = new_fvf;
    fvf_size = Get_FVF_Vertex_Size(fvf);

    unsigned int offset = 0;

    // --- Position ---
    location_offset = offset;
    switch (fvf & D3DFVF_POSITION_MASK) {
        case D3DFVF_XYZ:    offset += 3 * sizeof(float); break;
        case D3DFVF_XYZRHW: offset += 4 * sizeof(float); break;
        case D3DFVF_XYZB1:  offset += 4 * sizeof(float); break;
        case D3DFVF_XYZB2:  offset += 5 * sizeof(float); break;
        case D3DFVF_XYZB3:  offset += 6 * sizeof(float); break;
        case D3DFVF_XYZB4:
            if (fvf & D3DFVF_LASTBETA_UBYTE4)
                offset += 6 * sizeof(float) + sizeof(unsigned int);
            else
                offset += 7 * sizeof(float);
            break;
        case D3DFVF_XYZB5:  offset += 8 * sizeof(float); break;
    }
    blend_offset = location_offset + 3 * sizeof(float);  // blend starts after XYZ

    // --- Normal ---
    normal_offset = offset;
    if (fvf & D3DFVF_NORMAL)
        offset += 3 * sizeof(float);

    // --- Diffuse color ---
    diffuse_offset = offset;
    if (fvf & D3DFVF_DIFFUSE)
        offset += sizeof(unsigned int);

    // --- Specular color ---
    specular_offset = offset;
    if (fvf & D3DFVF_SPECULAR)
        offset += sizeof(unsigned int);

    // --- Texture coordinates ---
    unsigned int tex_count = Get_Tex_Count(fvf);
    for (unsigned int i = 0; i < D3DDP_MAXTEXCOORD; i++) {
        texcoord_offset[i] = offset;
        if (i < tex_count) {
            offset += Get_Tex_Coord_Size(fvf, i) * sizeof(float);
        }
    }
}

// ============================================================================
// Get_Tex_Offset — Get byte offset of texture coordinate set N
// ============================================================================
unsigned int GLES3_FVFDecoder::Get_Tex_Offset(unsigned int n) const {
    if (n >= D3DDP_MAXTEXCOORD) return 0;
    return texcoord_offset[n];
}

// ============================================================================
// Apply_Vertex_Attribs — Set up glVertexAttribPointer for decoded FVF
//
// This is the core bridge between DX8's FVF system and OpenGL ES 3.0's
// explicit vertex attribute specification.
//
// Must be called while the correct VBO is bound to GL_ARRAY_BUFFER.
// The 'stride' parameter is the vertex size in bytes (should match fvf_size).
// ============================================================================
void GLES3_FVFDecoder::Apply_Vertex_Attribs(unsigned int stride,
                                             unsigned int base_offset_bytes) const {
    if (stride == 0) stride = fvf_size;

    // --- Position (always present) ---
    if (fvf & D3DFVF_POSITION_MASK) {
        bool is_xyzrhw = (fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW;
        int pos_components = is_xyzrhw ? 4 : 3;

        glEnableVertexAttribArray(ATTR_POSITION);
        glVertexAttribPointer(
            ATTR_POSITION,
            pos_components,
            GL_FLOAT,
            GL_FALSE,
            stride,
            (const void*)(uintptr_t)(base_offset_bytes + location_offset)
        );
    } else {
        glDisableVertexAttribArray(ATTR_POSITION);
    }

    // --- Normal ---
    if (fvf & D3DFVF_NORMAL) {
        glEnableVertexAttribArray(ATTR_NORMAL);
        glVertexAttribPointer(
            ATTR_NORMAL,
            3,
            GL_FLOAT,
            GL_FALSE,
            stride,
            (const void*)(uintptr_t)(base_offset_bytes + normal_offset)
        );
    } else {
        glDisableVertexAttribArray(ATTR_NORMAL);
        // Default normal (0, 0, 1) for vertices without normals
        glVertexAttrib3f(ATTR_NORMAL, 0.0f, 0.0f, 1.0f);
    }

    // --- Diffuse color ---
    // DX8 stores as BGRA packed DWORD. In the shader, we'll swizzle B↔R.
    // GL_UNSIGNED_BYTE with GL_TRUE normalization maps 0-255 → 0.0-1.0.
    // WebGL2 does NOT support GL_BGRA, so we pass as GL_RGBA and swizzle
    // in the vertex shader (swap .r and .b).
    GLES3_Set_Vertex_Has_Color((fvf & D3DFVF_DIFFUSE) != 0);
    GLES3_Set_Pretransformed((fvf & D3DFVF_POSITION_MASK) == D3DFVF_XYZRHW);
    if (fvf & D3DFVF_DIFFUSE) {
        glEnableVertexAttribArray(ATTR_COLOR);
        glVertexAttribPointer(
            ATTR_COLOR,
            4,
            GL_UNSIGNED_BYTE,
            GL_TRUE,    // normalize 0-255 → 0.0-1.0
            stride,
            (const void*)(uintptr_t)(base_offset_bytes + diffuse_offset)
        );
    } else {
        glDisableVertexAttribArray(ATTR_COLOR);
        // Default: opaque white (matches DX8 default when no vertex color)
        glVertexAttrib4f(ATTR_COLOR, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    // --- Texture coordinates ---
    unsigned int tex_count = Get_Tex_Count(fvf);
    for (unsigned int i = 0; i < 4; i++) {  // shader supports up to 4
        int attr_loc = ATTR_TEXCOORD0 + i;
        if (i < tex_count) {
            unsigned int dim = Get_Tex_Coord_Size(fvf, i);
            glEnableVertexAttribArray(attr_loc);
            glVertexAttribPointer(
                attr_loc,
                dim,
                GL_FLOAT,
                GL_FALSE,
                stride,
                (const void*)(uintptr_t)(base_offset_bytes + texcoord_offset[i])
            );
        } else {
            glDisableVertexAttribArray(attr_loc);
            glVertexAttrib2f(attr_loc, 0.0f, 0.0f);
        }
    }
}

// ============================================================================
// Disable_All_Attribs — Disable all vertex attribute arrays
// ============================================================================
void GLES3_FVFDecoder::Disable_All_Attribs() {
    for (int i = 0; i <= ATTR_TEXCOORD3; i++) {
        glDisableVertexAttribArray(i);
    }
}

#endif // __EMSCRIPTEN__
