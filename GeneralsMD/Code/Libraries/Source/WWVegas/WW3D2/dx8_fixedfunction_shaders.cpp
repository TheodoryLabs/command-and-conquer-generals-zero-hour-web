/*
** GeneralsX Web Port — Embedded Fixed-Function Emulation Shaders
**
** Provides the GLSL ES 3.0 shader sources that emulate DX8's fixed-function
** pipeline. The shader manager (GLES3_ShaderManager) calls these functions
** to get shader source for compilation.
**
** DESIGN:
** Rather than shipping separate .vert/.frag files (which would need to be
** loaded via Emscripten's filesystem), we embed the shader sources as C
** string literals. This eliminates async file loading and ensures shaders
** are always available.
**
** The pipeline config hash determines which features to enable:
**   bit 0: lighting enabled
**   bit 1: fog enabled
**   bit 2: alpha test enabled
**   bits 3-6: texture stage active (one bit per stage 0-3)
**
** For v1, we use a single universal shader that handles all configurations
** via uniform booleans. This is simpler and still performant since Generals
** only uses ~10-20 unique pipeline configurations per scene. GPU branch
** prediction handles the uniform-based branching efficiently on modern
** hardware (especially since entire draw calls share the same branch path).
**
** BGRA VERTEX COLOR SWIZZLE:
** DX8 stores vertex colors as BGRA (Blue in low byte). Since WebGL2 doesn't
** support GL_BGRA format in vertex attributes, we read the color as RGBA
** from the vertex buffer and swizzle .r↔.b in the vertex shader.
**
** LICENSE: GPL-3.0
*/

#if defined(__EMSCRIPTEN__)

// ============================================================================
// Vertex Shader — DX8 Fixed-Function Pipeline Emulation
// ============================================================================
static const char* g_VertexShaderSource = R"GLSL(#version 300 es
/*
** DX8 Fixed-Function Pipeline Emulation — Vertex Shader
** Handles: transform, per-vertex lighting, fog, tex coord passthrough
** BGRA vertex color swizzle (DX8 uses BGRA byte order)
*/
precision highp float;

// Vertex attributes (matching FVF decoder attribute locations)
layout(location = 0) in vec4 a_Position;  // xyz (w defaults to 1), or XYZRHW
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec4 a_Color;       // DX8 DIFFUSE (BGRA byte order in memory)
layout(location = 3) in vec2 a_TexCoord0;
layout(location = 4) in vec2 a_TexCoord1;
layout(location = 5) in vec2 a_TexCoord2;
layout(location = 6) in vec2 a_TexCoord3;
layout(location = 7) in mat4 a_InstanceWorld; // Instanced matrix layout locations 7, 8, 9, 10
layout(location = 11) in vec4 a_Specular;     // DX8 SPECULAR (BGRA), 0 when the FVF has none

// Transform matrices
uniform mat4 u_World;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform bool u_InstancingEnabled;

// Per-stage texture coordinates, as D3D8 fixed function builds them:
// D3DTSS_TEXCOORDINDEX (low bits: UV set, high 16 bits: D3DTSS_TCI_* generation mode),
// D3DTSS_TEXTURETRANSFORMFLAGS (D3DTTFF_COUNTn | D3DTTFF_PROJECTED) and D3DTS_TEXTUREn.
// The shroud, cloud shadows and projected decals use camera-space generation.
uniform int u_TexCoordIndex[4];
uniform int u_TexTransform[4];
uniform mat4 u_TexMatrix[4];

// Material
uniform vec4 u_MatDiffuse;
uniform vec4 u_MatAmbient;
uniform vec4 u_MatSpecular;
uniform vec4 u_MatEmissive;
uniform float u_MatPower;

// Lighting
uniform bool u_LightingEnabled;

struct Light {
    vec4 diffuse;
    vec3 position;
    vec3 direction;
    bool enabled;
    int type;          // 1=point, 2=spot, 3=directional
    float attenuation0;
    float attenuation1;
    float attenuation2;
    vec4 ambient;
    float range;       // point/spot: no light beyond this distance
    vec4 specular;
};
uniform Light u_Lights[4];

// D3DFVF_XYZRHW: positions are already in render-target pixels, with 1/w.
// D3D skips transform, lighting and vertex fog for them.
uniform bool u_PreTransformed;
uniform vec4 u_ViewportRect; // x, y, width, height in pixels

// Global ambient light (DX8 D3DRS_AMBIENT default)
uniform vec4 u_GlobalAmbient;

// Fog
uniform bool u_FogEnabled;
uniform float u_FogStart;
uniform float u_FogEnd;

// Which material colours come from the vertex colour (D3DRS_COLORVERTEX with
// D3DRS_*MATERIALSOURCE = D3DMCS_COLOR1 and a diffuse vertex colour):
// bit 0 ambient, bit 1 diffuse, bit 2 emissive. Otherwise the material is used.
uniform int u_ColorVertex;

// D3DRS_SPECULARENABLE: lit vertices get Blinn-Phong highlights (local viewer, D3D's
// default), unlit ones pass the vertex specular colour; the pixel stage adds it.
uniform bool u_SpecularEnable;

// Outputs to fragment shader
out vec4 v_Color;
out vec4 v_Specular;
out vec3 v_TexCoord0; // xy, and the divisor for D3DTTFF_PROJECTED (else 1)
out vec3 v_TexCoord1;
out vec3 v_TexCoord2;
out vec3 v_TexCoord3;
out float v_FogFactor;

vec3 stage_texcoord(int i, vec3 eyePos, vec3 eyeNormal) {
    int tci = u_TexCoordIndex[i];
    int mode = tci >> 16;
    vec4 c;
    if (mode == 1) c = vec4(eyeNormal, 1.0);                              // CAMERASPACENORMAL
    else if (mode == 2) c = vec4(eyePos, 1.0);                            // CAMERASPACEPOSITION
    else if (mode == 3) c = vec4(reflect(normalize(eyePos), eyeNormal), 1.0); // REFLECTIONVECTOR
    else {
        int set = tci & 3;
        vec2 uv = set == 0 ? a_TexCoord0 : set == 1 ? a_TexCoord1 : set == 2 ? a_TexCoord2 : a_TexCoord3;
        c = vec4(uv, 1.0, 0.0); // D3D puts the 2D translation in the matrix's third row
    }
    int count = u_TexTransform[i] & 0xff;
    if (count == 0) return vec3(c.xy, 1.0);
    vec4 t = u_TexMatrix[i] * c;
    float q = 1.0;
    if ((u_TexTransform[i] & 256) != 0) q = count == 4 ? t.w : count == 3 ? t.z : t.y;
    return vec3(t.xy, q);
}

void main() {
    vec4 vertexColor = a_Color.bgra;
    if (u_PreTransformed) {
        float w = a_Position.w > 0.0 ? 1.0 / a_Position.w : 1.0;
        // D3D pixel centres are at integer coordinates, GL's at +0.5.
        vec2 p = (a_Position.xy + 0.5 - u_ViewportRect.xy) / u_ViewportRect.zw;
        gl_Position = vec4(p.x * 2.0 - 1.0, 1.0 - p.y * 2.0, a_Position.z * 2.0 - 1.0, 1.0) * w;
        v_Color = vertexColor;
        v_Specular = a_Specular.bgra;
        v_TexCoord0 = vec3(a_TexCoord0, 1.0);
        v_TexCoord1 = vec3(a_TexCoord1, 1.0);
        v_TexCoord2 = vec3(a_TexCoord2, 1.0);
        v_TexCoord3 = vec3(a_TexCoord3, 1.0);
        v_FogFactor = 1.0;
        return;
    }
    mat4 worldMat = u_World;
    if (u_InstancingEnabled) {
        worldMat = a_InstanceWorld;
    }
    // Transform position through World * View * Projection
    vec4 worldPos = worldMat * vec4(a_Position.xyz, 1.0);
    vec4 viewPos = u_View * worldPos;
    gl_Position = u_Projection * viewPos;
    // Map D3D projection depth [0, w] to OpenGL ES clip space depth [-w, w]
    gl_Position.z = gl_Position.z * 2.0 - gl_Position.w;

    // BGRA → RGBA swizzle for vertex color
    // DX8 stores vertex colors as BGRA in memory. When read as GL_UNSIGNED_BYTE
    // with GL_RGBA format, the bytes map to: R=B, G=G, B=R, A=A
    // We need to swap R and B channels.

    vec3 eyeNormal = normalize(mat3(u_View) * (mat3(worldMat) * a_Normal));
    v_TexCoord0 = stage_texcoord(0, viewPos.xyz, eyeNormal);
    v_TexCoord1 = stage_texcoord(1, viewPos.xyz, eyeNormal);
    v_TexCoord2 = stage_texcoord(2, viewPos.xyz, eyeNormal);
    v_TexCoord3 = stage_texcoord(3, viewPos.xyz, eyeNormal);

    // Compute lighting
    if (u_LightingEnabled) {
        mat3 worldNormalMatrix = mat3(worldMat);
        vec3 worldNormal = normalize(worldNormalMatrix * a_Normal);

        // Start with emissive material color
        vec4 totalDiffuse = (u_ColorVertex & 4) != 0 ? vertexColor : u_MatEmissive;

        // Global ambient contribution
        vec4 matAmb = (u_ColorVertex & 1) != 0 ? vertexColor : u_MatAmbient;
        vec4 matDiff = (u_ColorVertex & 2) != 0 ? vertexColor : u_MatDiffuse;
        vec4 lightAmbient = u_GlobalAmbient; // D3D: global + sum of attenuated light ambients
        vec4 totalSpecular = vec4(0.0);
        // Camera position in world space, for the local-viewer half vector.
        vec3 camPos = -transpose(mat3(u_View)) * u_View[3].xyz;
        vec3 toEye = normalize(camPos - worldPos.xyz);

        // Per-light contribution
        for (int i = 0; i < 4; i++) {
            if (!u_Lights[i].enabled) continue;

            vec3 lightDir;
            float attenuation = 1.0;

            if (u_Lights[i].type == 3) {
                // Directional light — direction is towards the light source
                lightDir = normalize(-u_Lights[i].direction);
            } else if (u_Lights[i].type == 1) {
                // Point light — direction from vertex to light
                vec3 toLight = u_Lights[i].position - worldPos.xyz;
                float dist = length(toLight);
                if (dist > u_Lights[i].range) continue;
                lightDir = dist > 0.0 ? toLight / dist : vec3(0.0, 0.0, 1.0);
                attenuation = 1.0 / (u_Lights[i].attenuation0
                    + u_Lights[i].attenuation1 * dist
                    + u_Lights[i].attenuation2 * dist * dist);
            } else {
                // Spot light — simplified as point with direction check
                vec3 toLight = u_Lights[i].position - worldPos.xyz;
                float dist = length(toLight);
                if (dist > u_Lights[i].range) continue;
                lightDir = dist > 0.0 ? toLight / dist : vec3(0.0, 0.0, 1.0);
                attenuation = 1.0 / (u_Lights[i].attenuation0
                    + u_Lights[i].attenuation1 * dist
                    + u_Lights[i].attenuation2 * dist * dist);
                // Spot cone falloff
                float cosAngle = dot(-lightDir, normalize(u_Lights[i].direction));
                attenuation *= smoothstep(0.0, 1.0, cosAngle);
            }

            float NdotL = max(dot(worldNormal, lightDir), 0.0);

            totalDiffuse += matDiff * u_Lights[i].diffuse * NdotL * attenuation;
            lightAmbient += u_Lights[i].ambient * attenuation;
            if (u_SpecularEnable && NdotL > 0.0) {
                float NdotH = max(dot(worldNormal, normalize(lightDir + toEye)), 0.0);
                totalSpecular += u_Lights[i].specular * pow(NdotH, max(u_MatPower, 0.0001)) * attenuation;
            }
        }

        v_Color = clamp(totalDiffuse + matAmb * lightAmbient, 0.0, 1.0);
        v_Color.a = matDiff.a;
        v_Specular = clamp(u_MatSpecular * totalSpecular, 0.0, 1.0);
    } else {
        // No lighting — use vertex color directly (UI, 2D elements, terrain decals)
        v_Color = vertexColor;
        v_Specular = a_Specular.bgra;
    }

    // Fog factor (linear fog in eye space)
    if (u_FogEnabled && u_FogEnd > u_FogStart) {
        float eyeDist = abs(viewPos.z); // D3D vertex fog uses eye-space z (no RANGEFOGENABLE)
        v_FogFactor = clamp((u_FogEnd - eyeDist) / (u_FogEnd - u_FogStart), 0.0, 1.0);
    } else {
        v_FogFactor = 1.0;
    }
}
)GLSL";


// ============================================================================
// Fragment Shader — DX8 Fixed-Function Pipeline Emulation
// ============================================================================
static const char* g_FragmentShaderSource = R"GLSL(#version 300 es
/*
** DX8 Fixed-Function Pipeline Emulation — Fragment Shader
** Handles: multi-texture blending, alpha test, fog
*/
precision highp float;

// Inputs from vertex shader
in vec4 v_Color;
in vec3 v_TexCoord0;
in vec3 v_TexCoord1;
in vec3 v_TexCoord2;
in vec3 v_TexCoord3;
in float v_FogFactor;

// Texture samplers
uniform sampler2D u_Texture0;
uniform sampler2D u_Texture1;
uniform sampler2D u_Texture2;
uniform sampler2D u_Texture3;

// Fog
uniform bool u_FogEnabled;
uniform vec3 u_FogColor;

// Alpha test emulation (removed in GLES3, emulated here)
uniform bool u_AlphaTestEnabled;
uniform float u_AlphaRef;         // 0.0 - 1.0 (normalized from DX8's 0-255)
uniform int u_AlphaFunc;          // D3DCMPFUNC enum value

// D3DRS_TEXTUREFACTOR
uniform vec4 u_TextureFactor;

// Texture stages 0-3, as D3D8 fixed function runs them. u_StageOps: COLOROP, ALPHAOP per
// stage (D3DTEXTUREOP). u_StageArgs: COLORARG0/1/2, ALPHAARG0/1/2 per stage (D3DTA_*: low
// 4 bits select, 0x10 COMPLEMENT, 0x20 ALPHAREPLICATE). Bit n of u_TexBound: stage n has a
// texture (D3D reads an unbound stage's texture as opaque white).
uniform int u_StageOps[8];
uniform int u_StageArgs[24];
uniform int u_TexBound;

// D3DRS_SPECULARENABLE: add the specular colour after the stages.
uniform bool u_SpecularEnable;
in vec4 v_Specular;

// A ps.1.1 pixel shader set with SetPixelShader replaces the stages (u_PSCount > 0).
// Two ivec4 per instruction (encoded by GLES3_Create_Pixel_Shader):
//   [2i]   = (opcode | writemask << 8 | shift << 12 | saturate << 16, dst, src0, src1)
//   [2i+1] = (src2, 0, 0, 0)
// A register is 0-1 r0-r1, 2-5 t0-t3, 6-7 v0-v1, 8-15 c0-c7; a source adds swizzle << 5
// (1 = .a, 2 = .b replicate) and modifier << 8 (D3DSPSM_*).
uniform int u_PSCount;
uniform ivec4 u_PS[32];
uniform vec4 u_PSConst[8];

// Output
out vec4 fragColor;

// Alpha test comparison
bool alpha_test(float alpha) {
    if (!u_AlphaTestEnabled) return true;

    // D3DCMPFUNC values
    if (u_AlphaFunc == 1) return false;                             // NEVER
    if (u_AlphaFunc == 2) return alpha < u_AlphaRef;                // LESS
    if (u_AlphaFunc == 3) return abs(alpha - u_AlphaRef) < 0.004;   // EQUAL
    if (u_AlphaFunc == 4) return alpha <= u_AlphaRef;               // LESSEQUAL
    if (u_AlphaFunc == 5) return alpha > u_AlphaRef;                // GREATER
    if (u_AlphaFunc == 6) return abs(alpha - u_AlphaRef) >= 0.004;  // NOTEQUAL
    if (u_AlphaFunc == 7) return alpha >= u_AlphaRef;               // GREATEREQUAL
    if (u_AlphaFunc == 8) return true;                              // ALWAYS
    return true;
}

vec4 sample_stage(int s) {
    if ((u_TexBound & (1 << s)) == 0) return vec4(1.0);
    if (s == 0) return texture(u_Texture0, v_TexCoord0.xy / v_TexCoord0.z);
    if (s == 1) return texture(u_Texture1, v_TexCoord1.xy / v_TexCoord1.z);
    if (s == 2) return texture(u_Texture2, v_TexCoord2.xy / v_TexCoord2.z);
    return texture(u_Texture3, v_TexCoord3.xy / v_TexCoord3.z);
}

vec4 stage_arg(int a, vec4 diffuse, vec4 current, vec4 tex) {
    int sel = a & 15;
    vec4 v = sel == 0 ? diffuse : sel == 2 ? tex : sel == 3 ? u_TextureFactor
           : sel == 4 ? v_Specular : current; // 1 CURRENT; 5 TEMP as CURRENT
    if ((a & 16) != 0) v = vec4(1.0) - v;
    if ((a & 32) != 0) v = vec4(v.a);
    return v;
}

// One D3DTEXTUREOP on all four channels; the caller keeps .rgb (colour) or .a (alpha).
vec4 tex_op(int op, vec4 a1, vec4 a2, vec4 a0, vec4 diffuse, vec4 current, vec4 tex) {
    vec4 r;
    if (op == 2) r = a1;                                   // SELECTARG1
    else if (op == 3) r = a2;                              // SELECTARG2
    else if (op == 5) r = a1 * a2 * 2.0;                   // MODULATE2X
    else if (op == 6) r = a1 * a2 * 4.0;                   // MODULATE4X
    else if (op == 7) r = a1 + a2;                         // ADD
    else if (op == 8) r = a1 + a2 - 0.5;                   // ADDSIGNED
    else if (op == 9) r = (a1 + a2 - 0.5) * 2.0;           // ADDSIGNED2X
    else if (op == 10) r = a1 - a2;                        // SUBTRACT
    else if (op == 11) r = a1 + a2 - a1 * a2;              // ADDSMOOTH
    else if (op == 12) r = mix(a2, a1, diffuse.a);         // BLENDDIFFUSEALPHA
    else if (op == 13) r = mix(a2, a1, tex.a);             // BLENDTEXTUREALPHA
    else if (op == 14) r = mix(a2, a1, u_TextureFactor.a); // BLENDFACTORALPHA
    else if (op == 15) r = a1 + a2 * (1.0 - tex.a);        // BLENDTEXTUREALPHAPM
    else if (op == 16) r = mix(a2, a1, current.a);         // BLENDCURRENTALPHA
    else if (op == 18) r = a1 + a1.a * a2;                 // MODULATEALPHA_ADDCOLOR
    else if (op == 19) r = a1 * a2 + a1.a;                 // MODULATECOLOR_ADDALPHA
    else if (op == 20) r = (1.0 - a1.a) * a2 + a1;         // MODULATEINVALPHA_ADDCOLOR
    else if (op == 21) r = (1.0 - a1) * a2 + a1.a;         // MODULATEINVCOLOR_ADDALPHA
    else if (op == 22 || op == 23) r = a2;                 // BUMPENVMAP*: no bump, pass on
    else if (op == 24) r = vec4(4.0 * dot(a1.rgb - 0.5, a2.rgb - 0.5)); // DOTPRODUCT3
    else if (op == 25) r = a0 + a1 * a2;                   // MULTIPLYADD
    else if (op == 26) r = mix(a2, a1, a0);                // LERP
    else r = a1 * a2;                                      // MODULATE, PREMODULATE
    return clamp(r, 0.0, 1.0);                             // D3D saturates every stage
}

vec4 ps_src(vec4 R[16], int s) {
    vec4 v = R[s & 15];
    int swz = (s >> 5) & 3;
    if (swz == 1) v = vec4(v.a); else if (swz == 2) v = vec4(v.b);
    int m = (s >> 8) & 15;
    if (m == 1) v = -v;                          // NEG
    else if (m == 2) v = v - 0.5;                // BIAS
    else if (m == 3) v = 0.5 - v;                // BIASNEG
    else if (m == 4) v = v * 2.0 - 1.0;          // SIGN
    else if (m == 5) v = 1.0 - v * 2.0;          // SIGNNEG
    else if (m == 6) v = 1.0 - v;                // COMP
    else if (m == 7) v = v * 2.0;                // X2
    else if (m == 8) v = -v * 2.0;               // X2NEG
    return v;
}

vec4 run_pixel_shader() {
    vec4 R[16];
    R[0] = vec4(0.0); R[1] = vec4(0.0);
    for (int i = 0; i < 4; i++) R[2 + i] = sample_stage(i);
    R[6] = v_Color; R[7] = v_Specular;
    for (int i = 0; i < 8; i++) R[8 + i] = u_PSConst[i];
    for (int i = 0; i < 16; i++) {
        if (i >= u_PSCount) break;
        ivec4 a = u_PS[2 * i];
        int op = a.x & 255;
        if (op == 66) continue; // tex: the stage samples are already in t0-t3
        vec4 s0 = ps_src(R, a.z), s1 = ps_src(R, a.w), s2 = ps_src(R, u_PS[2 * i + 1].x);
        vec4 r;
        if (op == 1) r = s0;                          // mov
        else if (op == 2) r = s0 + s1;                // add
        else if (op == 3) r = s0 - s1;                // sub
        else if (op == 4) r = s0 * s1 + s2;           // mad
        else if (op == 8) r = vec4(dot(s0.rgb, s1.rgb)); // dp3
        else if (op == 9) r = vec4(dot(s0, s1));      // dp4
        else if (op == 18) r = mix(s2, s1, s0);       // lrp
        else r = s0 * s1;                             // mul
        int shift = (a.x >> 12) & 15;
        if (shift == 1) r *= 2.0; else if (shift == 2) r *= 4.0; else if (shift == 15) r *= 0.5;
        r = ((a.x >> 16) & 1) != 0 ? clamp(r, 0.0, 1.0) : clamp(r, -1.0, 1.0); // ps.1.1 range
        int mask = (a.x >> 8) & 15;
        R[a.y] = mix(R[a.y], r, bvec4((mask & 1) != 0, (mask & 2) != 0, (mask & 4) != 0, (mask & 8) != 0));
    }
    return clamp(R[0], 0.0, 1.0);
}

void main() {
    vec4 diffuse = v_Color;
    vec4 color = v_Color; // CURRENT: the diffuse colour at stage 0

    if (u_PSCount > 0) {
        color = run_pixel_shader();
    } else {
        for (int s = 0; s < 4; s++) {
            int cop = u_StageOps[s * 2], aop = u_StageOps[s * 2 + 1];
            if (cop <= 1) break; // D3DTOP_DISABLE ends the chain here
            vec4 tex = sample_stage(s);
            int b = s * 6;
            vec4 c0 = stage_arg(u_StageArgs[b + 0], diffuse, color, tex);
            vec4 c1 = stage_arg(u_StageArgs[b + 1], diffuse, color, tex);
            vec4 c2 = stage_arg(u_StageArgs[b + 2], diffuse, color, tex);
            vec3 rgb = tex_op(cop, c1, c2, c0, diffuse, color, tex).rgb;
            if (aop > 1) {
                vec4 x0 = vec4(stage_arg(u_StageArgs[b + 3], diffuse, color, tex).a);
                vec4 x1 = vec4(stage_arg(u_StageArgs[b + 4], diffuse, color, tex).a);
                vec4 x2 = vec4(stage_arg(u_StageArgs[b + 5], diffuse, color, tex).a);
                color.a = tex_op(aop, x1, x2, x0, diffuse, color, tex).a;
            }
            color.rgb = rgb;
        }
        if (u_SpecularEnable) color.rgb = clamp(color.rgb + v_Specular.rgb, 0.0, 1.0);
    }

    // --- Alpha test ---
    if (!alpha_test(color.a)) {
        discard;
    }

    // --- Fog blending ---
    if (u_FogEnabled) {
        color.rgb = mix(u_FogColor, color.rgb, v_FogFactor);
    }

    fragColor = color;
}
)GLSL";


// ============================================================================
// Public API — called by GLES3_ShaderManager::Get_Program()
// ============================================================================

/*
** For v1 we return the same universal shader for all configurations.
** The shader uses uniform booleans to handle different pipeline states,
** which is efficient because the GPU evaluates all fragments in a draw call
** with the same branch path (uniform-based branching is essentially free).
**
** If profiling shows this is a bottleneck, v2 can generate specialized
** shader variants based on the config hash (e.g., separate shaders for
** lit vs unlit, fogged vs unfogged, 1-texture vs 2-texture).
*/

extern "C" {

const char* GLES3_Get_Vertex_Shader_Source(unsigned int config_hash) {
    (void)config_hash;  // All configs use the same shader in v1
    return g_VertexShaderSource;
}

const char* GLES3_Get_Fragment_Shader_Source(unsigned int config_hash) {
    (void)config_hash;
    return g_FragmentShaderSource;
}

} // extern "C"

#endif // __EMSCRIPTEN__
