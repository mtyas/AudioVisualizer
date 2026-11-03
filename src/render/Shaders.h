#pragma once

// GLSL 1.50 (OpenGL 3.2 core) shader sources for multi-layer rendering & post-processing.
namespace shaders
{
inline constexpr const char* fullscreenVS = R"glsl(#version 150
out vec2 uv;
void main()
{
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

// ---- 1. 3D Scene: Instanced mesh with 5 layouts (Spiral, Grid, TorusRing, Helix, SphereShell) ----
inline constexpr const char* sceneVS = R"glsl(#version 150
in vec3 aPos;
in vec3 aNormal;
uniform mat4 uViewProj;
uniform float uTime;
uniform float uPulse;
uniform float uScale;
uniform vec3 uSpin;
uniform float uSpread;
uniform int uCount;
uniform int uLayout;
uniform float uBands[16];
out vec3 vN;
out vec3 vWorld;
out float vBand;
out float vFi;

float hash11(float n) { return fract(sin(n * 127.1) * 43758.5453); }

mat3 rotXYZ(vec3 a)
{
    float cx = cos(a.x), sx = sin(a.x);
    float cy = cos(a.y), sy = sin(a.y);
    float cz = cos(a.z), sz = sin(a.z);
    mat3 rx = mat3(1,0,0, 0,cx,sx, 0,-sx,cx);
    mat3 ry = mat3(cy,0,-sy, 0,1,0, sy,0,cy);
    mat3 rz = mat3(cz,sz,0, -sz,cz,0, 0,0,1);
    return rz * ry * rx;
}

void main()
{
    int id = gl_InstanceID;
    float fi = float(id) / float(max(uCount - 1, 1));
    float b = uBands[clamp(int(fi * 15.999), 0, 15)];
    vec3 center = vec3(0.0);

    if (uLayout == 0) // Phyllotaxis Spiral
    {
        float ang = float(id) * 2.399963;
        float r = uSpread * 6.0 * sqrt(fi);
        center = vec3(r * cos(ang), sin(r * 0.8 - uTime * 1.5) * 0.35 + b * uPulse * 1.6, r * sin(ang));
    }
    else if (uLayout == 1) // 3D Architectural Grid
    {
        int side = max(1, int(pow(float(uCount), 0.3334) + 0.5));
        int ix = id % side;
        int iy = (id / side) % side;
        int iz = id / (side * side);
        vec3 g = (vec3(ix, iy, iz) / float(max(side - 1, 1)) - 0.5) * 2.0;
        center = g * uSpread * 4.2;
        center.y += sin(uTime * 2.0 + g.x * 3.0 + g.z * 3.0) * 0.25 + (b - 0.3) * uPulse * 0.9;
    }
    else if (uLayout == 2) // Torus Vortex Ring
    {
        float u = fi * 6.2831853 * 8.0 + uTime * 0.4;
        float v = fi * 6.2831853;
        float R = 3.6 * uSpread;
        float r = (1.1 + b * uPulse * 0.9) * uSpread;
        center = vec3((R + r * cos(u)) * cos(v), r * sin(u), (R + r * cos(u)) * sin(v));
    }
    else if (uLayout == 3) // Double DNA Helix
    {
        float strand = float(id & 1) * 3.14159265;
        float t = (fi - 0.5) * 10.0 * uSpread;
        float ang = t * 1.6 + uTime + strand;
        float rad = (1.8 + b * uPulse * 1.1) * uSpread;
        center = vec3(rad * cos(ang), t * 0.65, rad * sin(ang));
    }
    else if (uLayout == 5) // Random Cloud
    {
        vec3 h = vec3(hash11(float(id) + 1.0), hash11(float(id) * 1.37 + 7.0), hash11(float(id) * 2.11 + 13.0));
        vec3 dir = normalize(h * 2.0 - 1.0 + 1e-4);
        float rr = pow(hash11(float(id) * 3.7 + 21.0), 0.4) * 4.8 * uSpread;
        center = dir * rr + 0.25 * sin(uTime * 0.6 + h * 6.2831) + vec3(0.0, b * uPulse * 1.2, 0.0);
    }
    else if (uLayout == 6) // Wave Plane
    {
        int side = max(2, int(sqrt(float(uCount)) + 0.5));
        vec2 g = vec2(float(id % side), float(id / side)) / float(side - 1) - 0.5;
        float wave = sin(g.x * 9.0 + uTime * 2.0) * cos(g.y * 9.0 + uTime * 1.5);
        center = vec3(g.x * 10.0 * uSpread, wave * 0.8 + b * uPulse * 1.3 - 0.6, g.y * 10.0 * uSpread);
    }
    else if (uLayout == 7) // Concentric Rings
    {
        int ring = id % 8;
        int per = max(uCount / 8, 1);
        float ang = float(id / 8) / float(per) * 6.2831853 + uTime * (0.25 + float(ring) * 0.06) * (ring % 2 == 0 ? 1.0 : -1.0);
        float rad = (0.8 + float(ring) * 0.62) * uSpread;
        center = vec3(rad * cos(ang), sin(uTime + float(ring)) * 0.35 + uBands[ring * 2] * uPulse * 1.2, rad * sin(ang));
    }
    else if (uLayout == 8) // Lissajous Knot
    {
        float t = fi * 6.2831853;
        center = vec3(sin(3.0 * t + uTime * 0.4), sin(4.0 * t), sin(5.0 * t + uTime * 0.3)) * 3.6 * uSpread;
        center.y += b * uPulse * 0.8;
    }
    else if (uLayout == 9) // Galaxy Arms
    {
        float arm = float(id % 3) * 2.0943951;
        float r = sqrt(fi) * 6.2 * uSpread;
        float ang = r * 0.85 + arm + uTime * 0.2;
        center = vec3(r * cos(ang), (hash11(float(id)) - 0.5) * 0.7 + b * uPulse * 0.8, r * sin(ang));
    }
    else if (uLayout == 10) // Tunnel
    {
        float zz = (fract(fi * 6.0 + uTime * 0.06) - 0.5) * 18.0;
        float ang = float(id) * 0.83;
        float rad = (2.4 + 0.5 * sin(zz * 0.5 + uTime)) * uSpread + b * uPulse * 0.8;
        center = vec3(rad * cos(ang), rad * sin(ang), zz);
    }
    else // 4: Geodesic Fibonacci Sphere Shell
    {
        float y = 1.0 - 2.0 * fi;
        float rad = sqrt(max(0.0, 1.0 - y * y));
        float theta = float(id) * 2.399963;
        vec3 dir = vec3(cos(theta) * rad, y, sin(theta) * rad);
        center = dir * (3.4 * uSpread + b * uPulse * 1.8);
    }

    float sc = (0.09 + 0.06 * (1.0 - fi * 0.5) + b * uPulse * 0.42) * uScale;
    mat3 R = rotXYZ(uSpin * uTime);
    mat3 localR = rotXYZ(vec3(uTime * 0.7 + fi * 6.0, uTime * 0.5 + float(id) * 0.1, 0.0));
    vec3 world = R * center + (R * localR) * (aPos * sc);
    vN = (R * localR) * aNormal;
    vWorld = world;
    vBand = b;
    vFi = fi;
    gl_Position = uViewProj * vec4(world, 1.0);
}
)glsl";

inline constexpr const char* sceneFS = R"glsl(#version 150
in vec3 vN;
in vec3 vWorld;
in float vBand;
in float vFi;
uniform vec3 uCam;
uniform float uHue;
uniform float uSat;
uniform float uMetal;
uniform float uRough;
uniform float uEmissive;
out vec4 o;
const float PI = 3.14159265;

vec3 hsv(float h, float s, float v)
{
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}
float distGGX(float NdH, float a){ float a2 = a*a; float d = NdH*NdH*(a2-1.0)+1.0; return a2/(PI*d*d); }
float geoSchlick(float NdX, float k){ return NdX/(NdX*(1.0-k)+k); }

vec3 light(vec3 L, vec3 radiance, vec3 N, vec3 V, vec3 albedo, float metal, float rough)
{
    vec3 H = normalize(V + L);
    float NdL = max(dot(N,L),0.0), NdV = max(dot(N,V),1e-3), NdH = max(dot(N,H),0.0), VdH = max(dot(V,H),0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metal);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdH, 5.0);
    float a = rough * rough, k = (rough + 1.0) * (rough + 1.0) / 8.0;
    vec3 spec = distGGX(NdH, a) * geoSchlick(NdL,k) * geoSchlick(NdV,k) * F / max(4.0*NdV*NdL, 1e-3);
    vec3 diff = (1.0 - F) * (1.0 - metal) * albedo / PI;
    return (diff + spec) * radiance * NdL;
}

void main()
{
    vec3 N = normalize(vN);
    vec3 V = normalize(uCam - vWorld);
    vec3 albedo = hsv(fract(uHue + vBand * 0.28 + vFi * 0.15), clamp(uSat, 0.0, 1.0), 0.92);
    float rough = clamp(uRough, 0.04, 1.0);
    vec3 c = vec3(0.0);
    c += light(normalize(vec3( 0.6, 0.8, 0.5)), vec3(3.0, 2.9, 2.8), N, V, albedo, uMetal, rough);
    c += light(normalize(vec3(-0.7, 0.3,-0.4)), vec3(1.2, 1.6, 3.2), N, V, albedo, uMetal, rough);
    c += light(normalize(vec3( 0.0,-0.6, 0.8)), vec3(2.2, 0.8, 1.2), N, V, albedo, uMetal, rough);
    c += albedo * 0.06;
    c += hsv(fract(uHue + vBand * 0.3 + 0.08), clamp(uSat, 0.0, 1.0), 1.0) * uEmissive * (0.12 + vBand * vBand * 2.2);
    o = vec4(c, 1.0);
}
)glsl";

// ---- 2. Built-in 2D Procedural Shader Modes (0=Plasma, 1=SacredGeo, 2=Scope, 3=BioCells, 4=Starfield) ----
inline constexpr const char* proc2dFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform vec3 iResolution;
uniform float iTime;
uniform int uMode;
uniform float uSpeed, uScale, uComplexity, uColorShift, uParamA, uParamB;
uniform float uRms, uEnv, uTransient, uCentroid;
uniform float uBands[16];

vec3 pal(float t)
{
    return 0.5 + 0.5 * cos(6.28318 * (t + uColorShift + vec3(0.0, 0.33, 0.67)));
}

mat2 rot(float a){ float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }

float hash1(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float vnoise(vec2 x)
{
    vec2 i = floor(x), f = fract(x);
    f = f * f * (3.0 - 2.0 * f);
    return mix(mix(hash1(i), hash1(i + vec2(1, 0)), f.x), mix(hash1(i + vec2(0, 1)), hash1(i + vec2(1, 1)), f.x), f.y);
}
float fbm(vec2 x)
{
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; ++i) { v += a * vnoise(x); x = x * 2.02 + 13.1; a *= 0.5; }
    return v;
}

vec2 hash2(vec2 p)
{
    p = vec2(dot(p, vec2(127.1, 311.7)), dot(p, vec2(269.5, 183.3)));
    return fract(sin(p) * 43758.5453);
}

void main()
{
    vec2 p = (uv - 0.5) * vec2(iResolution.x / max(iResolution.y, 1.0), 1.0) * (2.0 * uScale);
    float t = iTime * uSpeed;
    vec3 col = vec3(0.0);

    if (uMode == 0) // Domain-Warped Audio Plasma
    {
        vec2 q = p;
        for (int i = 1; i <= 4; ++i)
        {
            float fi = float(i);
            q += (0.25 + 0.45 * uComplexity) / fi * vec2(
                sin(fi * q.y + t + uEnv * 3.0 + uParamA * 4.0),
                cos(fi * q.x - t * 0.8 + uBands[i * 2] * 4.0 + uParamB * 4.0));
        }
        float v = 0.5 + 0.5 * sin(q.x * 2.5 + q.y * 2.5 + t);
        col = pal(v + length(q) * 0.15) * (0.45 + 0.85 * v + uTransient * 0.6);
    }
    else if (uMode == 1) // Sacred Geometry Mandala
    {
        vec2 q = p;
        float dAcc = 0.0;
        int iters = 3 + int(uComplexity * 4.0);
        for (int i = 0; i < 7; ++i)
        {
            if (i >= iters) break;
            q = abs(q) - (0.35 + 0.25 * uParamA + uEnv * 0.2);
            q *= rot(t * 0.25 + float(i) * 0.5 + uParamB * 2.0);
            float r = length(q);
            float ring = abs(sin(r * (6.0 + float(i) * 2.0) - t * 2.0)) / (r + 0.2);
            dAcc += 0.025 / max(ring, 0.008);
        }
        col = pal(dAcc * 0.12 + length(p) * 0.2) * dAcc * (0.45 + uTransient * 0.7);
    }
    else if (uMode == 2) // Studio Spectrum & Vector Oscilloscope
    {
        vec2 u = uv;
        int bi = clamp(int(u.x * 16.0), 0, 15);
        float bx = fract(u.x * 16.0);
        float bh = uBands[bi] * 0.75;
        float barMask = step(0.1, bx) * step(bx, 0.9) * step(u.y, bh);
        vec3 barCol = pal(float(bi) / 16.0) * (0.5 + 0.5 * (u.y / max(bh, 0.01)));

        // Waveform ribbon across center
        float wave = 0.0;
        for (int k = 0; k < 8; ++k)
            wave += sin(u.x * (6.0 + float(k) * 5.0) * uScale + t * (2.0 + float(k))) * uBands[k * 2] * 0.06;
        float dist = abs(u.y - 0.55 - wave);
        float glow = 0.008 / (dist + 0.004);
        col = barCol * barMask * 0.75 + pal(u.x + 0.25) * glow * (0.7 + uRms * 2.0);
    }
    else if (uMode == 3) // Reaction-Diffusion / Bio-Voronoi Cells
    {
        vec2 st = p * (1.8 + uComplexity * 2.5);
        vec2 i_st = floor(st);
        vec2 f_st = fract(st);
        float m1 = 8.0, m2 = 8.0;
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
            {
                vec2 nb = vec2(float(x), float(y));
                vec2 pt = hash2(i_st + nb);
                pt = 0.5 + 0.5 * sin(t * 1.4 + 6.2831 * pt + vec2(uParamA, uParamB) * 6.0);
                float d = length(nb + pt - f_st);
                if (d < m1) { m2 = m1; m1 = d; }
                else if (d < m2) { m2 = d; }
            }
        float edge = m2 - m1;
        float membrane = smoothstep(0.0, 0.09 + uEnv * 0.08, edge);
        col = pal(m1 * 0.8 + uCentroid) * (1.0 - membrane * 0.82) + pal(m1 + 0.4) * (0.02 / (edge + 0.015));
    }
    else if (uMode == 5) // Interference Wave Rings
    {
        vec2 s1 = vec2(sin(t * 0.7) * 0.8, cos(t * 0.5) * 0.6), s2 = vec2(cos(t * 0.6) * 0.9, sin(t * 0.8) * 0.5);
        float v = sin(length(p - s1) * (10.0 + uParamA * 20.0) - t * 3.0)
                + sin(length(p - s2) * (10.0 + uParamB * 20.0) - t * 2.4)
                + sin(length(p) * 8.0 + uEnv * 6.0 - t);
        v = v / 3.0 * 0.5 + 0.5;
        col = pal(v + uCentroid * 0.5) * (0.25 + 1.2 * smoothstep(0.35, 0.9, v)) * (0.7 + uTransient);
    }
    else if (uMode == 6) // Tunnel
    {
        float r = length(p), a = atan(p.y, p.x);
        float z = 1.0 / (r + 0.1);
        vec2 tu = vec2(a / 3.14159265 * (2.0 + floor(uComplexity * 6.0)), z * 0.6 + t * 1.2);
        float checker = abs(step(0.5, fract(tu.x)) - step(0.5, fract(tu.y)));
        col = pal(tu.y * 0.08 + uParamA) * (checker * 0.85 + 0.15) * min(1.0, r * 1.6) * (1.0 + uEnv * 1.5);
    }
    else if (uMode == 7) // Julia Fractal
    {
        vec2 z = p * 0.9;
        vec2 c = vec2(-0.75 + 0.15 * sin(t * 0.3) + uParamA * 0.3, 0.11 + 0.25 * cos(t * 0.27) + uParamB * 0.3);
        float it = 0.0;
        for (int i = 0; i < 48; ++i)
        {
            z = vec2(z.x * z.x - z.y * z.y, 2.0 * z.x * z.y) + c;
            if (dot(z, z) > 16.0) break;
            it += 1.0;
        }
        float f = it / 48.0;
        col = pal(f * 2.0 + uCentroid) * (f < 0.99 ? (0.25 + f * 1.6) : 0.03) * (1.0 + uEnv);
    }
    else if (uMode == 8) // Metaballs
    {
        float f = 0.0;
        for (int i = 0; i < 6; ++i)
        {
            float fi = float(i);
            vec2 c = vec2(sin(t * (0.5 + fi * 0.17) + fi * 2.0) * 1.1, cos(t * (0.43 + fi * 0.13) + fi * 1.3) * 0.7);
            float r = 0.18 + 0.3 * uBands[i * 2] + 0.08 * uEnv;
            f += r * r / dot(p - c, p - c);
        }
        float edge = smoothstep(0.9, 1.1, f);
        col = pal(f * 0.15 + uParamA) * edge * (0.6 + 0.4 * smoothstep(1.0, 3.0, f)) + pal(0.5 + uParamB) * 0.03 / (abs(f - 1.0) + 0.05);
    }
    else if (uMode == 9) // Neon Grid Horizon
    {
        float h = p.y + 0.15;
        if (h < 0.0)
        {
            float zz = 0.5 / abs(h);
            vec2 g = vec2(p.x * zz, zz + t * 2.0);
            vec2 gl = abs(fract(g * (1.0 + uComplexity * 1.5)) - 0.5);
            float line = smoothstep(0.05, 0.0, min(gl.x, gl.y));
            col = pal(0.6 + uParamA) * line * (1.0 / (1.0 + zz * 0.12)) * (1.0 + uEnv * 1.5);
        }
        else
        {
            col = pal(0.9 + uParamB) * 0.15 * exp(-h * 3.0) * (1.0 + uRms * 4.0);
            float sd = length(p - vec2(0.0, 0.5));
            col += pal(0.1) * smoothstep(0.5, 0.48, sd) * step(0.3, fract(p.y * 12.0 - t));
        }
    }
    else if (uMode == 10) // Aurora Clouds
    {
        vec2 q = p * 1.2 + vec2(0.0, t * 0.2);
        float n = fbm(q + fbm(q * 1.7 + t * 0.1));
        float band = smoothstep(0.3, 0.9, n);
        col = mix(pal(n + uParamA) * band, pal(n + 0.4 + uParamB), 0.3) * (0.6 + 0.8 * (1.0 - uv.y)) * (0.7 + uEnv);
    }
    else if (uMode == 11) // Kaleido Flower
    {
        float r = length(p), a = atan(p.y, p.x);
        float petals = 4.0 + floor(uComplexity * 8.0);
        float k = abs(cos(a * petals * 0.5 + t * 0.5 + uParamA * 3.0));
        float d = r - (0.3 + 0.45 * k + uEnv * 0.15);
        float line = 0.02 / (abs(d) + 0.01);
        col = pal(r * 0.8 + a * 0.1 + uParamB) * line * (0.5 + uTransient) + pal(r) * 0.12 * exp(-r * 2.0);
    }
    else if (uMode == 12) // Lissajous Spirograph
    {
        float acc = 0.0;
        for (int i = 0; i < 64; ++i)
        {
            float ti = float(i) / 64.0 * 6.2831853;
            vec2 c = vec2(sin(ti * (3.0 + floor(uParamA * 4.0)) + t), cos(ti * (2.0 + floor(uParamB * 4.0)) + t * 0.7)) * 0.8 * (1.0 + uBands[2]);
            acc += 0.0015 / (dot(p - c, p - c) + 0.0008);
        }
        col = pal(acc * 0.05 + length(p)) * acc * 0.12;
    }
    else // 4: Hyperspace Starfield & Synth Horizon
    {
        vec3 acc = vec3(0.0);
        for (int l = 1; l <= 4; ++l)
        {
            float fl = float(l);
            float z = fract(fl * 0.25 - t * 0.25 * (1.0 + uEnv));
            float fade = smoothstep(0.0, 0.2, z) * (1.0 - z);
            vec2 sp = p * (z * 6.0) + vec2(fl * 17.3, fl * 9.1);
            vec2 id = floor(sp);
            vec2 gv = fract(sp) - 0.5;
            vec2 rnd = hash2(id) - 0.5;
            float d = length(gv - rnd * 0.7);
            float spark = (0.004 * (1.0 + uBands[l * 3] * 3.0)) / (d * d + 0.001);
            acc += pal(rnd.x + fl * 0.2) * spark * fade;
        }
        col = acc;
    }

    o = vec4(col, 1.0);
}
)glsl";

// ---- Custom Fullscreen Shader Layer (Shadertoy-compatible `mainImage`) ----
inline constexpr const char* layerPrelude = R"glsl(#version 150
uniform vec3 iResolution;
uniform float iTime;
uniform float iTimeDelta;
uniform int iFrame;
uniform vec4 iMouse;
uniform sampler2D iChannel0; // previous frame (feedback)
uniform sampler2D iChannel1; // composited canvas so far
uniform sampler2D iCam;      // webcam
uniform sampler2DArray iLoop; // live loop frames
uniform float u_cam_active, u_cam_motion, u_cam_x, u_cam_y, u_cam_brightness;
uniform float u_loop_len, u_loop_pos;
uniform float u_bands[16];
uniform float u_notes[128];
uniform float u_cc[128];
uniform float u_rms, u_peak, u_env, u_transient, u_centroid, u_flux, u_mid, u_side, u_width;
uniform float u_sc1, u_sc2, u_bpm, u_beat, u_playing;
uniform float u_midi_gate, u_midi_note, u_midi_velocity, u_midi_pitchbend, u_midi_trigger;
uniform float u_feedback;
vec3 loopFrame(vec2 uv)
{
    if (u_loop_len < 1.0) return vec3(0.0);
    float f0 = floor(u_loop_pos);
    float f1 = mod(f0 + 1.0, u_loop_len);
    return mix(texture(iLoop, vec3(uv, f0)).rgb, texture(iLoop, vec3(uv, f1)).rgb, u_loop_pos - f0);
}
out vec4 outColor;
#line 1
)glsl";

inline constexpr const char* layerEpilogue = R"glsl(
void main()
{
    vec4 c = vec4(0.0, 0.0, 0.0, 1.0);
    mainImage(c, gl_FragCoord.xy);
    outColor = c;
}
)glsl";

// ---- 3. Video / Live Loop Sampler Layer ----
inline constexpr const char* videoLayerFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform sampler2DArray uLoopTex;
uniform sampler2D uVidFileTex;
uniform sampler2D uCamTex;
uniform float uLoopLen, uLoopPos;
uniform int uSource;
uniform float uTime, uZoom, uStutter, uTintHue, uEnv, uBeat;

vec3 hsv2rgb(vec3 c)
{
    vec3 k = clamp(abs(mod(c.x * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return c.z * mix(vec3(1.0), k, c.y);
}

void main()
{
    vec2 p = (uv - 0.5) / max(uZoom, 0.1) + 0.5;
    vec3 col = vec3(0.0);
    if (uSource == 0 && uLoopLen >= 1.0)
    {
        float pos = uLoopPos;
        if (uStutter > 0.05)
        {
            float q = max(1.0, floor((1.0 - uStutter * 0.85) * 8.0));
            pos = floor(pos / q) * q;
        }
        float f0 = mod(floor(pos), uLoopLen);
        float f1 = mod(f0 + 1.0, uLoopLen);
        col = mix(texture(uLoopTex, vec3(clamp(p, 0.0, 1.0), f0)).rgb,
                  texture(uLoopTex, vec3(clamp(p, 0.0, 1.0), f1)).rgb, fract(pos));
    }
    else if (uSource == 3)
    {
        col = texture(uVidFileTex, clamp(p, 0.0, 1.0)).rgb;
    }
    else if (uSource == 4)
    {
        col = texture(uCamTex, clamp(p, 0.0, 1.0)).rgb; // live webcam, no recording needed
    }
    else
    {
        // Procedural Synth Video Deck (1 = Synthwave Grid, 2 = Cyber Matrix Deck)
        vec2 q = p * 2.0 - 1.0;
        if (uSource == 2)
        {
            vec2 g = fract(q * 6.0 + vec2(0.0, uTime * 2.0)) - 0.5;
            float line = 0.02 / (min(abs(g.x), abs(g.y)) + 0.01);
            col = hsv2rgb(vec3(fract(0.52 + uTintHue + length(q) * 0.2), 0.85, 1.0)) * line * (0.4 + uEnv * 1.4);
        }
        else
        {
            float horizon = -0.1;
            if (q.y < horizon)
            {
                float z = 0.4 / abs(q.y - horizon);
                float x = q.x * z;
                float gx = abs(fract(x * 1.5) - 0.5);
                float gz = abs(fract(z * 1.5 - uTime * 3.0) - 0.5);
                float line = smoothstep(0.06, 0.0, min(gx, gz));
                col = hsv2rgb(vec3(fract(0.83 + uTintHue), 0.85, 1.0)) * line * (1.0 / (1.0 + z * 0.15));
            }
            else
            {
                float sun = length(q - vec2(0.0, 0.25));
                float stripes = step(0.25, fract(q.y * 18.0 - uTime));
                float mask = smoothstep(0.38, 0.35, sun) * stripes;
                col = mix(vec3(0.06, 0.01, 0.12), hsv2rgb(vec3(fract(0.08 + uTintHue), 0.9, 1.0)), mask);
            }
        }
    }
    if (uTintHue > 0.01)
        col = mix(col, col * hsv2rgb(vec3(uTintHue, 0.7, 1.2)), 0.5);
    o = vec4(col, 1.0);
}
)glsl";

// ---- 4. Webcam FX Layer ----
inline constexpr const char* webcamLayerFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform sampler2D uCamTex;
uniform vec2 uTexel;
uniform float uTime, uEnv;
uniform float uMirror, uEdge, uPosterize, uChromaKey, uHueShift, uDisplace, uScale;

vec3 hsv(float h, float s, float v)
{
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}

void main()
{
    vec2 p = (uv - 0.5) / max(uScale, 0.15) + 0.5;
    if (uMirror < 0.5) p.x = 1.0 - p.x;
    if (uDisplace > 0.001)
    {
        float w = uDisplace * (0.015 + uEnv * 0.06);
        p += vec2(sin(p.y * 22.0 + uTime * 5.0), cos(p.x * 22.0 - uTime * 4.0)) * w;
    }
    p = clamp(p, 0.0, 1.0);
    vec3 c = texture(uCamTex, p).rgb;

    if (uPosterize > 0.05)
    {
        float levels = mix(16.0, 2.5, uPosterize);
        c = floor(c * levels + 0.5) / levels;
    }

    if (uEdge > 0.01)
    {
        vec3 cx = texture(uCamTex, p + vec2(uTexel.x * 2.0, 0.0)).rgb - texture(uCamTex, p - vec2(uTexel.x * 2.0, 0.0)).rgb;
        vec3 cy = texture(uCamTex, p + vec2(0.0, uTexel.y * 2.0)).rgb - texture(uCamTex, p - vec2(0.0, uTexel.y * 2.0)).rgb;
        float e = length(cx) + length(cy);
        vec3 neon = hsv(fract(uHueShift + p.x * 0.3 + uTime * 0.15), 0.85, 1.0) * smoothstep(0.08, 0.45, e) * 2.2;
        c = mix(c, neon, uEdge);
    }

    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    float alpha = smoothstep(uChromaKey * 0.6, uChromaKey * 0.6 + 0.08, luma);
    o = vec4(c * alpha, alpha);
}
)glsl";

// ---- 5. Kinetic 3D Text Overlay Layer ----
inline constexpr const char* textLayerFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform sampler2D uTextTex;
uniform float uTime, uEnv, uTransient;
uniform float uSize, uPosX, uPosY, uRotation, uExtrude, uGlitch, uHue, uBounce;

vec3 hsv(float h, float s, float v)
{
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}

void main()
{
    vec2 p = uv - 0.5 - vec2(uPosX * 0.4, uPosY * 0.4);
    float scale = max(0.15, uSize * (1.0 + uEnv * uBounce * 0.45));
    p /= scale;
    float c = cos(uRotation), s = sin(uRotation);
    p = mat2(c, -s, s, c) * p;

    if (uGlitch > 0.01)
    {
        float band = floor((p.y + 0.5) * 18.0);
        float shift = sin(band * 13.7 + floor(uTime * 16.0)) * uGlitch * (0.04 + uTransient * 0.08);
        p.x += shift;
    }

    vec2 tuv = p + 0.5;
    tuv.y = 1.0 - tuv.y; // JUCE image top-down

    // 3D Extrusion trail towards center
    vec3 col = vec3(0.0);
    float alphaAcc = 0.0;
    int steps = 8;
    vec2 dir = (p) * uExtrude * 0.12;
    for (int i = steps; i >= 1; --i)
    {
        float fi = float(i) / float(steps);
        vec2 suv = tuv + dir * fi;
        if (suv.x >= 0.0 && suv.x <= 1.0 && suv.y >= 0.0 && suv.y <= 1.0)
        {
            float a = texture(uTextTex, suv).a;
            vec3 sideCol = hsv(fract(uHue + fi * 0.25), 0.85, (1.0 - fi * 0.7) * 0.7);
            col = mix(col, sideCol, a);
            alphaAcc = max(alphaAcc, a * 0.85);
        }
    }

    if (tuv.x >= 0.0 && tuv.x <= 1.0 && tuv.y >= 0.0 && tuv.y <= 1.0)
    {
        float front = texture(uTextTex, tuv).a;
        vec3 frontCol = hsv(uHue, 0.55, 1.25);
        col = mix(col, frontCol, front);
        alphaAcc = max(alphaAcc, front);
    }

    o = vec4(col, alphaAcc);
}
)glsl";

// ---- 6. Spectrum Analyzer layer (bars / mirror / radial / line / dots) ----
inline constexpr const char* spectrumFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform float uBands[16];
uniform float uTime, uRms, uEnv, uAspect;
uniform int uStyle;
uniform float uHue, uHueSpread, uGain, uThickness, uSize, uPosY, uRot, uGlow;

float band(float x)
{
    float f = clamp(x, 0.0, 1.0) * 15.0;
    int i0 = int(floor(f));
    int i1 = min(i0 + 1, 15);
    return mix(uBands[i0], uBands[i1], smoothstep(0.0, 1.0, f - float(i0)));
}

vec3 hsv(float h, float s, float v)
{
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}

void main()
{
    vec2 p = (uv - 0.5) * vec2(uAspect, 1.0);
    float cs = cos(uRot), sn = sin(uRot);
    p = mat2(cs, -sn, sn, cs) * p;
    float base = uPosY * 0.5;
    vec3 col = vec3(0.0);
    float a = 0.0;

    if (uStyle <= 1) // bars / mirrored bars
    {
        float w = uAspect * 0.92 * uSize;
        float x = p.x / w + 0.5;
        if (x >= 0.0 && x <= 1.0)
        {
            const float N = 32.0;
            float bi = floor(x * N), bx = fract(x * N);
            float v = band((bi + 0.5) / N) * uGain;
            float h = v * 0.5 * uSize;
            float y0 = p.y - base;
            float g = (1.0 - uThickness) * 0.5;
            float mask = smoothstep(g, g + 0.04, bx) * smoothstep(1.0 - g, 1.0 - g - 0.04, bx);
            float inside = (uStyle == 0) ? step(0.0, y0) * step(y0, h) : step(abs(y0), h * 0.5);
            float tip = (uStyle == 0) ? abs(y0 - h) : abs(abs(y0) - h * 0.5);
            col = hsv(uHue + x * uHueSpread, 0.8, 0.6 + 0.4 * min(1.0, v));
            a = mask * inside;
            col += hsv(uHue + x * uHueSpread, 0.4, 1.0) * uGlow * 0.04 * exp(-tip * 28.0) * mask * min(1.0, v + 0.2);
            a = max(a, min(1.0, uGlow * 0.5 * exp(-tip * 28.0) * mask * min(1.0, v + 0.2)));
        }
    }
    else if (uStyle == 2) // radial
    {
        float r = length(p), ang = atan(p.y, p.x);
        float x = abs(ang / 3.14159265);
        const float N = 48.0;
        float bi = floor(x * N), bx = fract(x * N);
        float v = band((bi + 0.5) / N) * uGain;
        float r0 = 0.14 * uSize, len = r0 + v * 0.34 * uSize;
        float g = (1.0 - uThickness) * 0.5;
        float mask = smoothstep(g, g + 0.05, bx) * smoothstep(1.0 - g, 1.0 - g - 0.05, bx);
        a = mask * step(r0, r) * step(r, len);
        col = hsv(uHue + x * uHueSpread, 0.8, 0.6 + 0.4 * min(1.0, v));
        col += hsv(uHue + x * uHueSpread, 0.4, 1.0) * uGlow * 0.04 * exp(-abs(r - len) * 28.0) * mask;
        a = max(a, min(1.0, uGlow * 0.5 * exp(-abs(r - len) * 28.0) * mask));
    }
    else if (uStyle == 3) // smooth line
    {
        float w = uAspect * 0.92 * uSize;
        float x = p.x / w + 0.5;
        if (x >= 0.0 && x <= 1.0)
        {
            float y = base + (band(x) * uGain - 0.25) * 0.5 * uSize;
            float d = abs(p.y - y);
            float th = 0.003 + 0.02 * uThickness;
            a = smoothstep(th, th * 0.3, d);
            col = hsv(uHue + x * uHueSpread, 0.8, 1.0);
            col += col * uGlow * 0.03 / (d + 0.01);
            a = max(a, min(1.0, uGlow * 0.04 / (d + 0.02)));
        }
    }
    else // dots matrix
    {
        float w = uAspect * 0.92 * uSize;
        float x = p.x / w + 0.5;
        float yy = (p.y - base) / (0.5 * uSize);
        if (x >= 0.0 && x <= 1.0 && yy >= 0.0 && yy <= 1.0)
        {
            const float N = 32.0, R = 10.0;
            float ci = floor(x * N), fx = fract(x * N);
            float v = band((ci + 0.5) / N) * uGain;
            float row = floor(yy * R), fy = fract(yy * R);
            float lit = step((row + 0.5) / R, v);
            float d = length(vec2(fx, fy) - 0.5);
            a = lit * smoothstep(0.18 + 0.3 * uThickness, 0.1 + 0.3 * uThickness, d);
            col = hsv(uHue + x * uHueSpread, 0.8, 0.7 + 0.3 * (row / R));
        }
    }
    o = vec4(col * a, clamp(a, 0.0, 1.0));
}
)glsl";

// ---- 7. Gradient / Fill layer ----
inline constexpr const char* fillFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform float uTime, uEnv, uRms, uAspect;
uniform int uStyle;
uniform float uHueA, uHueB, uSat, uBri, uAngle, uScale, uSpeed, uAudio;

vec3 hsv(float h, float s, float v)
{
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}

void main()
{
    vec2 p = (uv - 0.5) * vec2(uAspect, 1.0);
    float cs = cos(uAngle), sn = sin(uAngle);
    p = mat2(cs, -sn, sn, cs) * p;
    float t = uTime * uSpeed;
    float lum = uBri * (1.0 + uEnv * uAudio);
    vec3 A = hsv(uHueA, uSat, lum), B = hsv(uHueB, uSat, lum);
    float m = 0.0;

    if (uStyle == 1)      m = abs(fract(p.x * uScale * 0.5 + t * 0.2) * 2.0 - 1.0);
    else if (uStyle == 2) m = abs(fract(length(p) * uScale * 0.5 - t * 0.2) * 2.0 - 1.0);
    else if (uStyle == 3) m = abs(fract(atan(p.y, p.x) / 6.2831853 * floor(max(uScale, 1.0)) + t * 0.1) * 2.0 - 1.0);
    else if (uStyle == 4) m = smoothstep(0.45, 0.55, fract(p.x * uScale * 2.0 + t * 0.3));
    else if (uStyle == 5) m = mod(floor(p.x * uScale * 3.0 + t) + floor(p.y * uScale * 3.0), 2.0);
    else if (uStyle == 6) m = smoothstep(0.35, 0.5, abs(fract(length(p) * uScale * 3.0 - t * 0.5) - 0.5));

    o = vec4(mix(A, B, m), 1.0);
}
)glsl";

// ---- Layer Blend Compositor (0=Add, 1=Screen, 2=Multiply, 3=Overlay, 4=Alpha, 5=Difference) ----
inline constexpr const char* blendFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform sampler2D uBase;
uniform sampler2D uTop;
uniform int uBlendMode;
uniform float uOpacity;

void main()
{
    vec4 b = texture(uBase, uv);
    vec4 t = texture(uTop, uv);
    float op = clamp(uOpacity, 0.0, 1.0);
    vec3 tc = t.rgb;
    vec3 res = b.rgb;

    if (uBlendMode == 0)      res = b.rgb + tc * op;
    else if (uBlendMode == 1) res = 1.0 - (1.0 - b.rgb) * (1.0 - tc * op);
    else if (uBlendMode == 2) res = mix(b.rgb, b.rgb * tc, op);
    else if (uBlendMode == 3)
    {
        vec3 ov = mix(2.0 * b.rgb * tc, 1.0 - 2.0 * (1.0 - b.rgb) * (1.0 - tc), step(0.5, b.rgb));
        res = mix(b.rgb, ov, op);
    }
    else if (uBlendMode == 4) res = mix(b.rgb, tc, clamp(t.a * op, 0.0, 1.0));
    else                      res = mix(b.rgb, abs(b.rgb - tc), op);

    o = vec4(res, 1.0);
}
)glsl";

// ---- Feedback + Post-FX Pass (Feedback Zoom/Spin, Kaleidoscope, Quad Mirror, Pixelate, Glitch, RGB Split) ----
inline constexpr const char* postFxFS = R"glsl(#version 150
in vec2 uv;
out vec4 o;
uniform sampler2D uCanvas;
uniform sampler2D uPrev;
uniform vec2 uAspect;
uniform float uTime, uFeedback, uFbZoom, uFbRot;
uniform float uKaleido, uMirrorQuad, uRgbSplit, uGlitch, uPixelate, uTwirl, uRipple, uZoomBlur;

mat2 rot(float a){ float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }

vec2 transformUV(vec2 p)
{
    if (uRipple > 0.01)
        p += vec2(sin(p.y * 30.0 + uTime * 3.0), cos(p.x * 30.0 + uTime * 2.5)) * 0.012 * uRipple;
    if (abs(uTwirl) > 0.01)
    {
        vec2 c = (p - 0.5) * uAspect;
        float r = length(c);
        float ang = uTwirl * (1.0 - smoothstep(0.0, 0.7, r)) * 6.0;
        float cs = cos(ang), sn = sin(ang);
        c = mat2(cs, -sn, sn, cs) * c;
        p = clamp(c / uAspect + 0.5, 0.0, 1.0);
    }
    if (uPixelate > 0.02)
    {
        float cells = mix(240.0, 18.0, uPixelate);
        p = (floor(p * cells) + 0.5) / cells;
    }
    if (uMirrorQuad > 0.5)
        p = abs(p * 2.0 - 1.0);
    if (uKaleido >= 1.5)
    {
        vec2 c = (p - 0.5) * uAspect;
        float r = length(c);
        float a = atan(c.y, c.x);
        float seg = 6.2831853 / floor(uKaleido);
        a = mod(a, seg);
        a = abs(a - seg * 0.5);
        c = vec2(cos(a), sin(a)) * r / uAspect;
        p = clamp(c + 0.5, 0.0, 1.0);
    }
    if (uGlitch > 0.01)
    {
        float slice = floor(p.y * 24.0);
        float n = fract(sin(slice * 91.7 + floor(uTime * 18.0) * 17.3) * 43758.5);
        if (n < uGlitch * 0.45)
            p.x = fract(p.x + (n - 0.2) * 0.25 * uGlitch);
    }
    return clamp(p, 0.0, 1.0);
}

void main()
{
    vec2 p = transformUV(uv);
    float split = uRgbSplit * 0.028;
    vec2 dir = (p - 0.5) * split;
    vec3 cur;
    cur.r = texture(uCanvas, clamp(p + dir, 0.0, 1.0)).r;
    cur.g = texture(uCanvas, p).g;
    cur.b = texture(uCanvas, clamp(p - dir, 0.0, 1.0)).b;
    if (uZoomBlur > 0.01)
    {
        vec3 acc = vec3(0.0);
        for (int i = 0; i < 8; ++i)
        {
            float k = float(i) / 8.0 * uZoomBlur * 0.12;
            acc += texture(uCanvas, clamp((p - 0.5) * (1.0 - k) + 0.5, 0.0, 1.0)).rgb;
        }
        cur = mix(cur, acc / 8.0, min(1.0, uZoomBlur * 3.0));
    }

    // Feedback sample with zoom & rotation
    vec2 fp = (uv - 0.5) * (1.0 - uFbZoom);
    fp = rot(uFbRot) * fp + 0.5;
    vec3 prev = texture(uPrev, clamp(fp, 0.0, 1.0)).rgb * clamp(uFeedback, 0.0, 0.98);

    o = vec4(max(cur, prev * 0.985), 1.0);
}
)glsl";

inline constexpr const char* blitFS = R"glsl(#version 150
in vec2 uv; out vec4 o;
uniform sampler2D uTex;
void main(){ o = vec4(texture(uTex, uv).rgb, 1.0); }
)glsl";

inline constexpr const char* brightFS = R"glsl(#version 150
in vec2 uv; out vec4 o;
uniform sampler2D uTex; uniform float uThreshold;
void main(){
    vec3 c = texture(uTex, uv).rgb;
    float l = max(c.r, max(c.g, c.b));
    float k = max(l - uThreshold, 0.0) / max(l, 1e-4);
    o = vec4(c * k, 1.0);
}
)glsl";

inline constexpr const char* blurFS = R"glsl(#version 150
in vec2 uv; out vec4 o;
uniform sampler2D uTex; uniform vec2 uDir;
void main(){
    float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec3 c = texture(uTex, uv).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(uTex, uv + uDir * float(i)).rgb * w[i];
        c += texture(uTex, uv - uDir * float(i)).rgb * w[i];
    }
    o = vec4(c, 1.0);
}
)glsl";

// ---- Final Master Grading & Composite ----
inline constexpr const char* compositeFS = R"glsl(#version 150
in vec2 uv; out vec4 o;
uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uBloomAmt, uScanlines, uVignette, uHueShift, uSaturation, uContrast, uStrobe, uDimmer;
uniform float uGrain, uPosterize, uDuotone, uDuoA, uDuoB;
uniform int uTonemap;
uniform float uTime;

vec3 hsvc(float h, float s, float v)
{
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.,4.,2.), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return v * mix(vec3(1.0), k, s);
}

vec3 aces(vec3 x){ return clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14), 0.0, 1.0); }

vec3 hueRotate(vec3 c, float a)
{
    float cosA = cos(a * 6.2831853), sinA = sin(a * 6.2831853);
    mat3 m = mat3(
        0.299 + 0.701*cosA + 0.168*sinA, 0.587 - 0.587*cosA + 0.330*sinA, 0.114 - 0.114*cosA - 0.497*sinA,
        0.299 - 0.299*cosA - 0.328*sinA, 0.587 + 0.413*cosA + 0.035*sinA, 0.114 - 0.114*cosA + 0.292*sinA,
        0.299 - 0.300*cosA + 1.250*sinA, 0.587 - 0.588*cosA - 1.050*sinA, 0.114 + 0.886*cosA - 0.203*sinA);
    return m * c;
}

void main()
{
    vec3 c = texture(uScene, uv).rgb + texture(uBloom, uv).rgb * uBloomAmt;
    if (abs(uHueShift) > 0.005)
        c = hueRotate(c, uHueShift);
    float luma = dot(c, vec3(0.299, 0.587, 0.114));
    c = mix(vec3(luma), c, uSaturation);
    c = (c - 0.5) * uContrast + 0.5;
    c = max(c, vec3(0.0));

    if (uTonemap == 1)
        c = pow(aces(c), vec3(1.0 / 2.2));
    else
        c = clamp(c, 0.0, 1.0);

    if (uDuotone > 0.01)
    {
        float l = dot(c, vec3(0.299, 0.587, 0.114));
        c = mix(c, mix(hsvc(uDuoA, 0.8, 0.35), hsvc(uDuoB, 0.6, 1.0), smoothstep(0.0, 1.0, l)), uDuotone);
    }
    if (uPosterize > 0.02)
    {
        float lv = mix(24.0, 3.0, uPosterize);
        c = floor(c * lv + 0.5) / lv;
    }
    if (uGrain > 0.01)
    {
        float n = fract(sin(dot(uv * vec2(1234.5, 789.1) + fract(uTime) * 10.0, vec2(12.9898, 78.233))) * 43758.5453);
        c += (n - 0.5) * uGrain * 0.25;
    }
    if (uScanlines > 0.01)
    {
        float s = 0.5 + 0.5 * sin(uv.y * 540.0 + uTime * 8.0);
        c *= 1.0 - uScanlines * 0.45 * s;
    }
    if (uVignette > 0.01)
    {
        float d = length(uv - 0.5) * 1.45;
        c *= clamp(1.0 - d * d * uVignette, 0.0, 1.0);
    }
    if (uStrobe > 0.05 && fract(uTime * 12.0) < uStrobe * 0.5)
        c = 1.0 - c;

    o = vec4(c * uDimmer, 1.0);
}
)glsl";
} // namespace shaders
