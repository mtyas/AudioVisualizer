#include "Renderer.h"
#include "Shaders.h"
#include "IsfParser.h"
#include <cmath>

using namespace juce::gl;

namespace
{
struct Mat4 { float m[16] {}; };

Mat4 mul (const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
        {
            float s = 0;
            for (int k = 0; k < 4; ++k)
                s += a.m[k * 4 + row] * b.m[c * 4 + k];
            r.m[c * 4 + row] = s;
        }
    return r;
}

Mat4 perspective (float fovy, float aspect, float n, float f)
{
    Mat4 r;
    const float t = 1.0f / std::tan (fovy * 0.5f);
    r.m[0] = t / aspect;
    r.m[5] = t;
    r.m[10] = (f + n) / (n - f);
    r.m[11] = -1.0f;
    r.m[14] = 2.0f * f * n / (n - f);
    return r;
}

Mat4 lookAt (const float eye[3], const float ctr[3])
{
    float z[3] = { eye[0] - ctr[0], eye[1] - ctr[1], eye[2] - ctr[2] };
    auto norm = [] (float* v) { const float l = std::sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-8f; for (int i = 0; i < 3; ++i) v[i] /= l; };
    norm (z);
    const float up[3] = { 0, 1, 0 };
    float x[3] = { up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0] };
    norm (x);
    const float y[3] = { z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0] };
    Mat4 r;
    r.m[0] = x[0]; r.m[1] = y[0]; r.m[2] = z[0];
    r.m[4] = x[1]; r.m[5] = y[1]; r.m[6] = z[1];
    r.m[8] = x[2]; r.m[9] = y[2]; r.m[10] = z[2];
    r.m[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
    r.m[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
    r.m[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
    r.m[15] = 1.0f;
    return r;
}

MeshData makeSphere()
{
    MeshData m;
    const int stacks = 14, slices = 28;
    for (int i = 0; i <= stacks; ++i)
    {
        const float th = juce::MathConstants<float>::pi * (float) i / stacks;
        for (int j = 0; j <= slices; ++j)
        {
            const float ph = juce::MathConstants<float>::twoPi * (float) j / slices;
            const float x = std::sin (th) * std::cos (ph), y = std::cos (th), z = std::sin (th) * std::sin (ph);
            m.vertices.insert (m.vertices.end(), { x, y, z, x, y, z });
        }
    }
    for (int i = 0; i < stacks; ++i)
        for (int j = 0; j < slices; ++j)
        {
            const uint32_t a = (uint32_t) (i * (slices + 1) + j), b = a + (uint32_t) slices + 1;
            m.indices.insert (m.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
        }
    return m;
}

MeshData makeCube()
{
    MeshData m;
    const float n[6][3] = { {0,0,1}, {0,0,-1}, {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0} };
    const float u[6][3] = { {1,0,0}, {-1,0,0}, {0,0,-1}, {0,0,1}, {1,0,0}, {1,0,0} };
    const float v[6][3] = { {0,1,0}, {0,1,0}, {0,1,0}, {0,1,0}, {0,0,-1}, {0,0,1} };
    const float s = 0.72f;
    for (int f = 0; f < 6; ++f)
    {
        const uint32_t base = (uint32_t) (m.vertices.size() / 6);
        const float c[4][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,1} };
        for (int k = 0; k < 4; ++k)
        {
            const float px = (n[f][0] + c[k][0] * u[f][0] + c[k][1] * v[f][0]) * s;
            const float py = (n[f][1] + c[k][0] * u[f][1] + c[k][1] * v[f][1]) * s;
            const float pz = (n[f][2] + c[k][0] * u[f][2] + c[k][1] * v[f][2]) * s;
            m.vertices.insert (m.vertices.end(), { px, py, pz, n[f][0], n[f][1], n[f][2] });
        }
        m.indices.insert (m.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
    return m;
}

MeshData makeTorus()
{
    MeshData m;
    const int segU = 28, segV = 14;
    const float R = 0.75f, r = 0.30f;
    for (int i = 0; i <= segU; ++i)
    {
        const float u = juce::MathConstants<float>::twoPi * (float) i / segU;
        const float cu = std::cos (u), su = std::sin (u);
        for (int j = 0; j <= segV; ++j)
        {
            const float v = juce::MathConstants<float>::twoPi * (float) j / segV;
            const float cv = std::cos (v), sv = std::sin (v);
            const float px = (R + r * cv) * cu, py = r * sv, pz = (R + r * cv) * su;
            const float nx = cv * cu, ny = sv, nz = cv * su;
            m.vertices.insert (m.vertices.end(), { px, py, pz, nx, ny, nz });
        }
    }
    for (int i = 0; i < segU; ++i)
        for (int j = 0; j < segV; ++j)
        {
            const uint32_t a = (uint32_t) (i * (segV + 1) + j), b = a + (uint32_t) segV + 1;
            m.indices.insert (m.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
        }
    return m;
}

MeshData makeIcosahedron()
{
    MeshData m;
    const float t = (1.0f + std::sqrt (5.0f)) * 0.5f;
    const float rawV[12][3] = {
        {-1, t, 0}, { 1, t, 0}, {-1,-t, 0}, { 1,-t, 0},
        { 0,-1, t}, { 0, 1, t}, { 0,-1,-t}, { 0, 1,-t},
        { t, 0,-1}, { t, 0, 1}, {-t, 0,-1}, {-t, 0, 1}
    };
    const int tris[20][3] = {
        {0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}
    };
    for (int f = 0; f < 20; ++f)
    {
        float v[3][3];
        for (int k = 0; k < 3; ++k)
        {
            const float* src = rawV[tris[f][k]];
            const float l = std::sqrt (src[0] * src[0] + src[1] * src[1] + src[2] * src[2]);
            v[k][0] = src[0] / l; v[k][1] = src[1] / l; v[k][2] = src[2] / l;
        }
        const float e1[3] = { v[1][0] - v[0][0], v[1][1] - v[0][1], v[1][2] - v[0][2] };
        const float e2[3] = { v[2][0] - v[0][0], v[2][1] - v[0][1], v[2][2] - v[0][2] };
        float fn[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
        const float nl = std::sqrt (fn[0] * fn[0] + fn[1] * fn[1] + fn[2] * fn[2]) + 1e-8f;
        fn[0] /= nl; fn[1] /= nl; fn[2] /= nl;
        const uint32_t base = (uint32_t) (m.vertices.size() / 6);
        for (int k = 0; k < 3; ++k)
            m.vertices.insert (m.vertices.end(), { v[k][0], v[k][1], v[k][2], fn[0], fn[1], fn[2] });
        m.indices.insert (m.indices.end(), { base, base + 1, base + 2 });
    }
    return m;
}

// Flat-shaded triangle whose normal faces away from the origin (all these solids are convex around it).
void addTri (MeshData& m, const float a[3], const float b[3], const float c[3])
{
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    const float e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    float n[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
    const float l = std::sqrt (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]) + 1e-8f;
    for (auto& x : n) x /= l;
    const float centroid[3] = { (a[0] + b[0] + c[0]) / 3.0f, (a[1] + b[1] + c[1]) / 3.0f, (a[2] + b[2] + c[2]) / 3.0f };
    if (n[0] * centroid[0] + n[1] * centroid[1] + n[2] * centroid[2] < 0.0f)
        for (auto& x : n) x = -x;
    const uint32_t base = (uint32_t) (m.vertices.size() / 6);
    for (const float* v : { a, b, c })
        m.vertices.insert (m.vertices.end(), { v[0], v[1], v[2], n[0], n[1], n[2] });
    m.indices.insert (m.indices.end(), { base, base + 1, base + 2 });
}

MeshData makeCylinder (float radius, float halfH, int seg)
{
    MeshData m;
    const float twoPi = juce::MathConstants<float>::twoPi;
    for (int i = 0; i <= seg; ++i)
    {
        const float a = twoPi * (float) i / (float) seg, cx = std::cos (a), sz = std::sin (a);
        m.vertices.insert (m.vertices.end(), { radius * cx, -halfH, radius * sz, cx, 0.0f, sz });
        m.vertices.insert (m.vertices.end(), { radius * cx,  halfH, radius * sz, cx, 0.0f, sz });
    }
    for (int i = 0; i < seg; ++i)
    {
        const uint32_t b = (uint32_t) (i * 2);
        m.indices.insert (m.indices.end(), { b, b + 1, b + 2, b + 2, b + 1, b + 3 });
    }
    for (int cap = 0; cap < 2; ++cap) // flat end caps
    {
        const float y = cap == 0 ? -halfH : halfH, ny = cap == 0 ? -1.0f : 1.0f;
        const uint32_t centre = (uint32_t) (m.vertices.size() / 6);
        m.vertices.insert (m.vertices.end(), { 0.0f, y, 0.0f, 0.0f, ny, 0.0f });
        for (int i = 0; i <= seg; ++i)
        {
            const float a = twoPi * (float) i / (float) seg;
            m.vertices.insert (m.vertices.end(), { radius * std::cos (a), y, radius * std::sin (a), 0.0f, ny, 0.0f });
        }
        for (int i = 0; i < seg; ++i)
            m.indices.insert (m.indices.end(), { centre, centre + 1 + (uint32_t) i, centre + 2 + (uint32_t) i });
    }
    return m;
}

MeshData makeCone()
{
    MeshData m;
    const int seg = 28;
    const float r = 0.8f, apex[3] = { 0.0f, 0.85f, 0.0f }, yb = -0.8f;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float centre[3] = { 0.0f, yb, 0.0f };
    for (int i = 0; i < seg; ++i)
    {
        const float a0 = twoPi * (float) i / seg, a1 = twoPi * (float) (i + 1) / seg;
        const float p0[3] = { r * std::cos (a0), yb, r * std::sin (a0) }, p1[3] = { r * std::cos (a1), yb, r * std::sin (a1) };
        addTri (m, apex, p1, p0);
        addTri (m, centre, p0, p1);
    }
    return m;
}

MeshData makeOctahedron()
{
    MeshData m;
    const float s = 1.1f;
    const float v[6][3] = { { s, 0, 0 }, { -s, 0, 0 }, { 0, s, 0 }, { 0, -s, 0 }, { 0, 0, s }, { 0, 0, -s } };
    const int f[8][3] = { { 0, 2, 4 }, { 2, 1, 4 }, { 1, 3, 4 }, { 3, 0, 4 }, { 2, 0, 5 }, { 1, 2, 5 }, { 3, 1, 5 }, { 0, 3, 5 } };
    for (auto& t : f) addTri (m, v[t[0]], v[t[1]], v[t[2]]);
    return m;
}

MeshData makeTetrahedron()
{
    MeshData m;
    const float s = 0.75f;
    const float v[4][3] = { { s, s, s }, { s, -s, -s }, { -s, s, -s }, { -s, -s, s } };
    const int f[4][3] = { { 0, 1, 2 }, { 0, 3, 1 }, { 0, 2, 3 }, { 1, 3, 2 } };
    for (auto& t : f) addTri (m, v[t[0]], v[t[1]], v[t[2]]);
    return m;
}

MeshData makePyramid()
{
    MeshData m;
    const float b = 0.85f, yb = -0.75f;
    const float apex[3] = { 0.0f, 0.95f, 0.0f };
    const float c[4][3] = { { -b, yb, -b }, { b, yb, -b }, { b, yb, b }, { -b, yb, b } };
    for (int i = 0; i < 4; ++i)
        addTri (m, apex, c[i], c[(i + 1) % 4]);
    addTri (m, c[0], c[2], c[1]);
    addTri (m, c[0], c[3], c[2]);
    return m;
}

MeshData makeTorusKnot()
{
    MeshData m;
    const int U = 120, V = 12;
    const float tube = 0.14f, scale = 0.3f, twoPi = juce::MathConstants<float>::twoPi;
    auto curve = [&] (float u, float out[3])
    {
        const float r = 2.0f + std::cos (3.0f * u);
        out[0] = r * std::cos (2.0f * u) * scale;
        out[1] = r * std::sin (2.0f * u) * scale;
        out[2] = std::sin (3.0f * u) * scale * 1.4f;
    };
    for (int i = 0; i <= U; ++i)
    {
        const float u = twoPi * (float) i / (float) U, d = 0.01f;
        float p[3], pa[3], pb[3];
        curve (u, p); curve (u + d, pa); curve (u - d, pb);
        float T[3] = { pa[0] - pb[0], pa[1] - pb[1], pa[2] - pb[2] };
        float N[3] = { pa[0] - 2 * p[0] + pb[0], pa[1] - 2 * p[1] + pb[1], pa[2] - 2 * p[2] + pb[2] };
        auto norm = [] (float* v) { const float l = std::sqrt (v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-9f; for (int k = 0; k < 3; ++k) v[k] /= l; };
        norm (T); norm (N);
        float B[3] = { T[1] * N[2] - T[2] * N[1], T[2] * N[0] - T[0] * N[2], T[0] * N[1] - T[1] * N[0] };
        for (int j = 0; j <= V; ++j)
        {
            const float v = twoPi * (float) j / (float) V, cv = std::cos (v), sv = std::sin (v);
            const float nx = cv * N[0] + sv * B[0], ny = cv * N[1] + sv * B[1], nz = cv * N[2] + sv * B[2];
            m.vertices.insert (m.vertices.end(), { p[0] + tube * nx, p[1] + tube * ny, p[2] + tube * nz, nx, ny, nz });
        }
    }
    for (int i = 0; i < U; ++i)
        for (int j = 0; j < V; ++j)
        {
            const uint32_t a = (uint32_t) (i * (V + 1) + j), b = a + (uint32_t) V + 1;
            m.indices.insert (m.indices.end(), { a, b, a + 1, a + 1, b, b + 1 });
        }
    return m;
}

void bindTex (int unit, unsigned int tex)
{
    glActiveTexture (GL_TEXTURE0 + (GLenum) unit);
    glBindTexture (GL_TEXTURE_2D, tex);
}
} // namespace

// ---------------------------------------------------------------------------------------------

void Renderer::Fbo::create (int width, int height, bool withDepth)
{
    release();
    w = width;
    h = height;
    glGenTextures (1, &tex);
    glBindTexture (GL_TEXTURE_2D, tex);
    glTexImage2D (GL_TEXTURE_2D, 0, (GLint) GL_RGBA16F, w, h, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);

    glGenFramebuffers (1, &fbo);
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    if (withDepth)
    {
        glGenRenderbuffers (1, &depth);
        glBindRenderbuffer (GL_RENDERBUFFER, depth);
        glRenderbufferStorage (GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
        glFramebufferRenderbuffer (GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    }
    glClearColor (0, 0, 0, 1);
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::Fbo::bind() const
{
    glBindFramebuffer (GL_FRAMEBUFFER, fbo);
    glViewport (0, 0, w, h);
}

void Renderer::Fbo::release()
{
    if (fbo != 0) glDeleteFramebuffers (1, &fbo);
    if (tex != 0) glDeleteTextures (1, &tex);
    if (depth != 0) glDeleteRenderbuffers (1, &depth);
    fbo = tex = depth = 0;
    w = h = 0;
}

void Renderer::GpuMesh::upload (const MeshData& m)
{
    if (vbo == 0) glGenBuffers (1, &vbo);
    if (ebo == 0) glGenBuffers (1, &ebo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (m.vertices.size() * sizeof (float)), m.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (m.indices.size() * sizeof (uint32_t)), m.indices.data(), GL_STATIC_DRAW);
    indexCount = (int) m.indices.size();
}

void Renderer::GpuMesh::release()
{
    if (vbo != 0) glDeleteBuffers (1, &vbo);
    if (ebo != 0) glDeleteBuffers (1, &ebo);
    vbo = ebo = 0;
    indexCount = 0;
}

// ---------------------------------------------------------------------------------------------

Renderer::Renderer (Engine& e, juce::OpenGLContext& c) : engine (e), context (c)
{
    static const struct { const char* u; const char* s; } table[] = {
        { "u_rms", "audio.rms" }, { "u_peak", "audio.peak" }, { "u_env", "audio.env" },
        { "u_transient", "audio.transient" }, { "u_centroid", "audio.centroid" }, { "u_flux", "audio.flux" },
        { "u_mid", "audio.mid" }, { "u_side", "audio.side" }, { "u_width", "audio.width" },
        { "u_sc1", "audio.sc1" }, { "u_sc2", "audio.sc2" },
        { "u_bpm", "host.bpm" }, { "u_beat", "host.beat" }, { "u_playing", "host.playing" },
        { "u_midi_gate", "midi.gate" }, { "u_midi_note", "midi.note" }, { "u_midi_velocity", "midi.velocity" },
        { "u_midi_pitchbend", "midi.pitchbend" }, { "u_midi_trigger", "midi.trigger" },
        { "u_cam_active", "cam.active" }, { "u_cam_motion", "cam.motion" }, { "u_cam_x", "cam.x" },
        { "u_cam_y", "cam.y" }, { "u_cam_brightness", "cam.brightness" } };
    for (auto& t : table)
        floatUniforms.push_back ({ t.u, t.s, e.bus.find (t.s) });
    for (int i = 0; i < 16; ++i) bandIdx[i] = e.bus.find ("band." + juce::String (i));
    for (int i = 0; i < 128; ++i)
    {
        noteIdx[i] = e.bus.find ("note." + juce::String (i));
        ccIdx[i] = e.bus.find ("cc." + juce::String (i));
    }
    idxRms   = e.bus.find ("audio.rms");
    idxEnv   = e.bus.find ("audio.env");
    idxTrans = e.bus.find ("audio.transient");
    idxCent  = e.bus.find ("audio.centroid");
    idxBeat  = e.bus.find ("host.beat");
}

std::unique_ptr<juce::OpenGLShaderProgram> Renderer::build (const char* vs, const juce::String& fs, juce::String& err)
{
    auto p = std::make_unique<juce::OpenGLShaderProgram> (context);
    if (p->addVertexShader (juce::String (vs)) && p->addFragmentShader (fs) && p->link())
        return p;
    err = p->getLastError();
    return nullptr;
}

void Renderer::newOpenGLContextCreated()
{
    glGenVertexArrays (1, &vao);
    glBindVertexArray (vao);

    meshes[0].upload (makeSphere());
    meshes[1].upload (makeCube());
    meshes[2].upload (makeTorus());
    meshes[3].upload (makeIcosahedron());
    meshes[4].upload (makeCone());
    meshes[5].upload (makeCylinder (0.7f, 0.8f, 28));
    meshes[6].upload (makeOctahedron());
    meshes[7].upload (makeTetrahedron());
    meshes[8].upload (makePyramid());
    meshes[9].upload (makeTorusKnot());
    meshes[10].upload (makeCylinder (1.0f, 0.09f, 36));
    meshes[kGltfMesh].upload (makeTorus());

    juce::String err;
    sceneProg  = build (shaders::sceneVS,      shaders::sceneFS,       err);
    proc2dProg = build (shaders::fullscreenVS, shaders::proc2dFS,      err);
    videoProg  = build (shaders::fullscreenVS, shaders::videoLayerFS,  err);
    webcamProg = build (shaders::fullscreenVS, shaders::webcamLayerFS, err);
    textProg   = build (shaders::fullscreenVS, shaders::textLayerFS,   err);
    spectrumProg = build (shaders::fullscreenVS, shaders::spectrumFS,  err);
    fillProg   = build (shaders::fullscreenVS, shaders::fillFS,        err);
    blendProg  = build (shaders::fullscreenVS, shaders::blendFS,       err);
    postFxProg = build (shaders::fullscreenVS, shaders::postFxFS,      err);
    blitProg   = build (shaders::fullscreenVS, shaders::blitFS,        err);
    brightProg = build (shaders::fullscreenVS, shaders::brightFS,      err);
    blurProg   = build (shaders::fullscreenVS, shaders::blurFS,        err);
    compProg   = build (shaders::fullscreenVS, shaders::compositeFS,   err);

    const uint8_t black[4] = { 0, 0, 0, 255 };
    glGenTextures (1, &camTex);
    glBindTexture (GL_TEXTURE_2D, camTex);
    glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D (GL_TEXTURE_2D, 0, (GLint) GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
    camW = camH = 0;
    camVersion = 0;

    glGenTextures (1, &vidFileTex);
    glBindTexture (GL_TEXTURE_2D, vidFileTex);
    glTexImage2D (GL_TEXTURE_2D, 0, (GLint) GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
    vidW = vidH = 0;
    lastVidFrameIdx = -1;

    for (auto& tt : textTexs) { tt.tex = 0; tt.key = {}; } // old GL objects died with the previous context
    updateTextTextures();

    glGenTextures (1, &loopTex);
    glBindTexture (GL_TEXTURE_2D_ARRAY, loopTex);
    glTexImage3D (GL_TEXTURE_2D_ARRAY, 0, (GLint) GL_RGBA8, kLoopW, kLoopH, kLoopFrames, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
    glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
    glTexParameteri (GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
    glGenFramebuffers (1, &loopFbo);
    loopLen = loopWrite = 0;
    loopRecording = false;

    seenShaderVersion = -1;
    seenLayerRev = -1;
    seenInlineVersion = -1;
    rebuildLayer();
    loadModel();

    // Shader compile failures are otherwise silent (the layer just doesn't draw): surface them in the status bar.
    {
        juce::String failed;
        const std::pair<ShaderProg*, const char*> progs[] = {
            { &sceneProg, "3D scene" }, { &proc2dProg, "2D shader" }, { &videoProg, "video" }, { &webcamProg, "webcam" },
            { &textProg, "text" }, { &spectrumProg, "spectrum" }, { &fillProg, "fill" }, { &blendProg, "blend" },
            { &postFxProg, "post fx" }, { &blitProg, "blit" }, { &brightProg, "bloom" }, { &blurProg, "blur" }, { &compProg, "composite" } };
        for (auto& pr : progs)
            if (! (bool) *pr.first)
                failed += juce::String (pr.second) + " ";
        if (failed.isNotEmpty())
        {
            DBG ("Shader compile failure: " << failed << err);
            juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("AudioVisualizer_shader_errors.log")
                .replaceWithText ("Failed programs: " + failed + "\n" + err);
            engine.setStatus ("SHADER ERROR in: " + failed.trim() + " - " + err.replace ("\n", " ").substring (0, 160));
        }
    }
    startTime = lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void Renderer::openGLContextClosing()
{
    sceneProg.reset(); proc2dProg.reset(); customLayerProg.reset();
    videoProg.reset(); webcamProg.reset(); textProg.reset(); spectrumProg.reset(); fillProg.reset();
    blendProg.reset(); postFxProg.reset(); blitProg.reset();
    brightProg.reset(); blurProg.reset(); compProg.reset();

    if (camTex != 0) glDeleteTextures (1, &camTex);
    if (vidFileTex != 0) glDeleteTextures (1, &vidFileTex);
    for (auto& tt : textTexs) { if (tt.tex != 0) glDeleteTextures (1, &tt.tex); tt.tex = 0; tt.key = {}; }
    if (loopTex != 0) glDeleteTextures (1, &loopTex);
    if (loopFbo != 0) glDeleteFramebuffers (1, &loopFbo);
    camTex = vidFileTex = loopTex = loopFbo = 0;

    layerFbo.release();
    for (auto& f : canvas) f.release();
    for (auto& f : feedback) f.release();
    for (auto& f : bloom) f.release();
    for (auto& m : meshes) m.release();
    if (vao != 0) glDeleteVertexArrays (1, &vao);
    vao = 0;
    width = height = 0;
}

void Renderer::updateTextTextures()
{
    const auto stack = engine.visual.getLayerStack();
    for (int i = 0; i < stack.count; ++i)
    {
        const int id = stack.ids[(size_t) i];
        if (engine.visual.getLayerType (id) != LayerType::Text)
            continue;

        const auto prefix = engine.visual.getLayerPrefix (id);
        const bool bold = engine.visual.getBase (prefix + ".bold") >= 0.5f;
        const bool italic = engine.visual.getBase (prefix + ".italic") >= 0.5f;
        const auto txt = engine.visual.getLayerText (id);
        const auto fontName = engine.visual.getLayerFont (id);
        const auto key = txt + "\n" + fontName + (bold ? "|b" : "|") + (italic ? "i" : "");

        auto& tt = textTexs[(size_t) id];
        if (tt.tex != 0 && tt.key == key)
            continue;
        tt.key = key;

        if (tt.tex == 0)
        {
            glGenTextures (1, &tt.tex);
            glBindTexture (GL_TEXTURE_2D, tt.tex);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint) GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint) GL_LINEAR);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLint) GL_CLAMP_TO_EDGE);
            glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLint) GL_CLAMP_TO_EDGE);
        }

        const int tw = 1024, th = 256;
        juce::Image img (juce::Image::ARGB, tw, th, true);
        {
            juce::Graphics g (img);
            g.setColour (juce::Colours::white);
            const int style = (bold ? juce::Font::bold : 0) | (italic ? juce::Font::italic : 0);
            g.setFont (juce::Font (juce::FontOptions (fontName.isEmpty() ? juce::Font::getDefaultSansSerifFontName() : fontName,
                                                      92.0f, style)));
            g.drawFittedText (txt.isEmpty() ? juce::String ("TEXT") : txt, 20, 20, tw - 40, th - 40, juce::Justification::centred, 2);
        }

        std::vector<uint8_t> rgba ((size_t) tw * (size_t) th * 4);
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        const int ps = bd.pixelStride;
        for (int y = 0; y < th; ++y)
        {
            const auto* srcLine = bd.getLinePointer (y);
            auto* dstRow = rgba.data() + ((size_t) y * (size_t) tw) * 4;
            for (int x = 0; x < tw; ++x)
            {
                const auto* px = reinterpret_cast<const juce::PixelARGB*> (srcLine + x * ps);
                auto* p = dstRow + (size_t) x * 4;
                p[0] = px->getRed(); p[1] = px->getGreen(); p[2] = px->getBlue(); p[3] = px->getAlpha();
            }
        }

        glBindTexture (GL_TEXTURE_2D, tt.tex);
        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D (GL_TEXTURE_2D, 0, (GLint) GL_RGBA8, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    }
}

void Renderer::loadModel()
{
    seenModelVersion = engine.modelVersion.load();
    const auto path = engine.getModelPath();
    if (path.isEmpty())
        return;
    MeshData m;
    std::string err;
    if (loadGltf (path.toStdString(), m, err))
    {
        // Selecting the glTF geometry is done by the UI's "Load glTF Model" action, so it is
        // undoable and isn't forced again every time the output window's GL context is recreated.
        meshes[kGltfMesh].upload (m);
        engine.setStatus ("glTF loaded: " + juce::File (path).getFileName());
    }
    else
    {
        engine.setStatus ("glTF error: " + juce::String (err));
    }
}

void Renderer::rebuildLayer()
{
    seenShaderVersion = engine.shaderVersion.load();
    seenInlineVersion = engine.visual.inlineShaderVersion.load();
    const juce::File f (engine.getShaderPath());
    juce::String body = engine.visual.getCustomShaderCode();
    bool fromFile = false;
    if (f != juce::File() && engine.getShaderPath().isNotEmpty() && f.existsAsFile())
    {
        body = f.loadFileAsString();
        shaderMTime = f.getLastModificationTime();
        fromFile = true;
    }

    body = isf::translateShaderIfISF (body);
    juce::String err;
    auto p = build (shaders::fullscreenVS, juce::String (shaders::layerPrelude) + body + shaders::layerEpilogue, err);
    if (p)
    {
        customLayerProg = std::move (p);
        userFileShader = fromFile;
        engine.setStatus (fromFile ? ("Shader file OK: " + f.getFileName()) : "GLSL compiled OK");
    }
    else
    {
        engine.setStatus ("GLSL error: " + err.replace ("\n", " "));
    }
}

void Renderer::checkReload()
{
    bool need = (seenShaderVersion != engine.shaderVersion.load())
             || (seenInlineVersion != engine.visual.inlineShaderVersion.load());
    if (! need && userFileShader && frame % 20 == 0)
        need = juce::File (engine.getShaderPath()).getLastModificationTime() != shaderMTime;
    if (need)
        rebuildLayer();
    if (seenModelVersion != engine.modelVersion.load())
        loadModel();
    if (frame % 10 == 0)
        updateTextTextures();
}

void Renderer::resize (int w, int h)
{
    width = w;
    height = h;
    layerFbo.create (w, h, true);
    canvas[0].create (w, h, false);
    canvas[1].create (w, h, false);
    feedback[0].create (w, h, false);
    feedback[1].create (w, h, false);
    bloom[0].create (juce::jmax (1, w / 2), juce::jmax (1, h / 2), false);
    bloom[1].create (juce::jmax (1, w / 2), juce::jmax (1, h / 2), false);
    fbCur = 0;
}

void Renderer::updateCameraAndVideo (float dt, float speed, float inPt, float outPt, bool rev,
                                     bool tempoSync, float syncBeats, float stutter)
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    bool fresh = false;
    int w = 0, h = 0;
    if (engine.camera.fetchFrame (camVersion, camFrame, w, h))
    {
        glBindTexture (GL_TEXTURE_2D, camTex);
        glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
        if (w != camW || h != camH)
        {
            glTexImage2D (GL_TEXTURE_2D, 0, (GLint) GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, camFrame->data());
            camW = w;
            camH = h;
        }
        else
            glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, camFrame->data());
        fresh = true;
        if (lastCamTime > 0.0)
        {
            const float d = (float) (now - lastCamTime);
            if (d > 0.005f && d < 0.5f)
                camFps += (1.0f / d - camFps) * 0.1f;
        }
        lastCamTime = now;
    }

    const int cmd = engine.loopCommand.exchange (0);
    if (cmd == 1)
    {
        if (loopRecording)
        {
            loopRecording = false;
            loopLen = loopWrite;
            loopPos = 0.0;
        }
        else
        {
            loopRecording = true;
            loopWrite = 0;
            loopLen = 0;
        }
    }
    else if (cmd == 2)
    {
        loopRecording = false;
        loopLen = loopWrite = 0;
    }

    if (loopRecording && (fresh || frame % 2 == 0) && blitProg)
    {
        glBindFramebuffer (GL_FRAMEBUFFER, loopFbo);
        glFramebufferTextureLayer (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, loopTex, 0, loopWrite);
        glViewport (0, 0, kLoopW, kLoopH);
        blitProg->use();
        blitProg->setUniform ("uTex", 0);
        bindTex (0, camW > 0 ? camTex : feedback[1 - fbCur].tex);
        drawFullscreen();
        if (++loopWrite >= kLoopFrames)
        {
            loopRecording = false;
            loopLen = loopWrite;
            loopPos = 0.0;
        }
    }

    // Advance playback position (supports free speed OR Host Tempo Sync!)
    const float bpm = juce::jmax (30.0f, engine.bus.get (engine.sBpm) > 1.0f ? engine.bus.get (engine.sBpm) : 120.0f);
    const double beatsPerSec = bpm / 60.0;
    const double cycleHz = tempoSync ? (beatsPerSec / juce::jmax (1.0, (double) syncBeats)) : 0.0;
    const double dir = rev ? -1.0 : 1.0;

    if (! loopRecording && loopLen > 0)
    {
        const double lo = juce::jlimit (0.0, (double) (loopLen - 1), (double) inPt * loopLen);
        const double hi = juce::jlimit (lo + 1.0, (double) loopLen, (double) outPt * loopLen);
        const double span = juce::jmax (1.0, hi - lo);
        const double step = tempoSync ? (dt * cycleHz * span * speed * dir) : (dt * camFps * speed * dir);
        loopPos = lo + std::fmod (loopPos - lo + step + span * 16.0, span);
    }
    engine.bus.set (engine.sLoopPos, loopLen > 0 ? (float) (loopPos / loopLen) : 0.0f);
    engine.bus.set (engine.sLoopRec, loopRecording ? 1.0f : 0.0f);

    // Advance loaded video file frame if active
    if (engine.videoDecoder.isLoaded())
    {
        const double lo = juce::jlimit (0.0, 0.95, (double) inPt);
        const double hi = juce::jlimit (lo + 0.05, 1.0, (double) outPt);
        const double span = hi - lo;
        const double vidStep = tempoSync
            ? (dt * cycleHz * span * speed * dir)
            : (dt * (engine.videoDecoder.getVideoFps() / juce::jmax (1, engine.videoDecoder.getFrameCount())) * speed * dir);
        vidNormPos = lo + std::fmod (vidNormPos - lo + vidStep + span * 16.0, span);
        double fetchPos = vidNormPos;
        if (stutter > 0.05f)
        {
            const double q = span / juce::jmax (2.0, std::floor ((1.0 - stutter * 0.8) * 16.0));
            fetchPos = lo + std::floor ((vidNormPos - lo) / q) * q;
        }
        int vw = 0, vh = 0;
        if (engine.videoDecoder.fetchFrameAt (fetchPos, lastVidFrameIdx, vidFrame, vw, vh))
        {
            glBindTexture (GL_TEXTURE_2D, vidFileTex);
            glPixelStorei (GL_UNPACK_ALIGNMENT, 1);
            if (vw != vidW || vh != vidH)
            {
                glTexImage2D (GL_TEXTURE_2D, 0, (GLint) GL_RGBA8, vw, vh, 0, GL_RGBA, GL_UNSIGNED_BYTE, vidFrame->data());
                vidW = vw;
                vidH = vh;
            }
            else
                glTexSubImage2D (GL_TEXTURE_2D, 0, 0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, vidFrame->data());
        }
    }
}

void Renderer::drawFullscreen()
{
    glDrawArrays (GL_TRIANGLES, 0, 3);
}

void Renderer::renderOpenGL()
{
    GLint vp[4];
    glGetIntegerv (GL_VIEWPORT, vp);
    const int W = juce::jmax (1, vp[2]), H = juce::jmax (1, vp[3]);
    if (W != width || H != height)
        resize (W, H);

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0005, 0.1, now - lastTime);
    animTime += (double) dt * (double) juce::jmax (0.0f, P ("global.speed"));
    const float t = (float) animTime;
    lastTime = now;
    fpsSmooth += (1.0f / dt - fpsSmooth) * 0.08f;
    engine.measuredFps.store (fpsSmooth, std::memory_order_relaxed);

    checkReload();
    engine.modulation.tick (dt);

    // P(id) is a member that caches the param index per literal (see Renderer.h).

    glBindVertexArray (vao);
    glDisable (GL_BLEND);

    updateCameraAndVideo (dt, P ("video.speed"), P ("video.inPoint"), P ("video.outPoint"),
                          P ("video.reverse") > 0.5f, P ("video.tempoSync") > 0.5f,
                          P ("video.syncBeats"), P ("video.stutter"));

    float bands[16];
    for (int i = 0; i < 16; ++i)
        bands[i] = engine.bus.get (bandIdx[i]);
    const float vRms   = engine.bus.get (idxRms);
    const float vEnv   = engine.bus.get (idxEnv);
    const float vTrans = engine.bus.get (idxTrans);
    const float vCent  = engine.bus.get (idxCent);
    const float vBeat  = engine.bus.get (idxBeat);

    // Clear compositing canvas[0]
    int curCanvas = 0;
    canvas[0].bind();
    glClearColor (0.01f, 0.01f, 0.018f, 1.0f);
    glClear (GL_COLOR_BUFFER_BIT);

    // Refresh cached per-layer parameter prefixes when layers were added or removed
    const int layerRev = engine.visual.layerRevision.load();
    if (layerRev != seenLayerRev)
    {
        seenLayerRev = layerRev;
        for (int i = 0; i < VisualState::kMaxLayers; ++i)
        {
            layerPrefixes[(size_t) i] = engine.visual.getLayerPrefix (i);
            layerParamCache[(size_t) i].clear();
        }
    }

    const auto stack = engine.visual.getLayerStack();
    for (int slot = 0; slot < stack.count; ++slot)
    {
        const int lid = stack.ids[(size_t) slot];
        const auto ltype = engine.visual.getLayerType (lid);
        if (ltype == LayerType::Count)
            continue;
        curLayer = lid;
        const bool legacy = VisualState::isLegacyLayer (lid);

        const bool enabled = LP ("enabled") >= 0.5f;
        const float opacity = LP ("opacity");
        const int blendMode = (int) std::round (LP ("blend"));
        if (! enabled || opacity <= 0.001f)
            continue;

        // Render layer into layerFbo
        layerFbo.bind();
        glClearColor (0.0f, 0.0f, 0.0f, 0.0f);
        glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (ltype == LayerType::Scene3D && sceneProg)
        {
            const int geomIdx = juce::jlimit (0, kNumMeshes - 1, (int) std::round (LP ("geometry")));
            const auto& mesh = meshes[(size_t) geomIdx];
            if (mesh.indexCount > 0)
            {
                glEnable (GL_DEPTH_TEST);
                const bool wire = LP ("wireframe") >= 0.5f;
                if (wire) glPolygonMode (GL_FRONT_AND_BACK, GL_LINE);

                const float camDist = LP ("camdist");
                const float orbit   = LP ("camOrbit");
                const float heightY = LP ("camHeight");
                const float eye[3]  = { std::sin (orbit) * camDist, heightY, std::cos (orbit) * camDist };
                const float ctr[3]  = { 0, 0, 0 };
                const auto vpMat    = mul (perspective (0.9f, (float) W / (float) H, 0.1f, 100.0f), lookAt (eye, ctr));
                const int instCount = juce::jlimit (16, 1024, (int) std::round (LP ("instances")));

                sceneProg->use();
                sceneProg->setUniformMat4 ("uViewProj", vpMat.m, 1, GL_FALSE);
                sceneProg->setUniform ("uTime", t);
                sceneProg->setUniform ("uPulse", LP ("pulse"));
                sceneProg->setUniform ("uScale", LP ("scale"));
                sceneProg->setUniform ("uSpin", LP ("spinX"), LP ("spinY"), LP ("spinZ"));
                sceneProg->setUniform ("uSpread", LP ("spread"));
                sceneProg->setUniform ("uCount", instCount);
                sceneProg->setUniform ("uLayout", (GLint) std::round (LP ("layout")));
                sceneProg->setUniform ("uBands", bands, 16);
                sceneProg->setUniform ("uCam", eye[0], eye[1], eye[2]);
                sceneProg->setUniform ("uHue", LP ("hue"));
                sceneProg->setUniform ("uSat", LP ("saturation"));
                sceneProg->setUniform ("uMetal", LP ("metallic"));
                sceneProg->setUniform ("uRough", LP ("roughness"));
                sceneProg->setUniform ("uEmissive", LP ("emissive"));

                const GLint aPos = sceneProg.attrib ("aPos"), aNrm = sceneProg.attrib ("aNormal");
                glBindBuffer (GL_ARRAY_BUFFER, mesh.vbo);
                glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
                if (aPos >= 0)
                {
                    glEnableVertexAttribArray ((GLuint) aPos);
                    glVertexAttribPointer ((GLuint) aPos, 3, GL_FLOAT, GL_FALSE, 6 * sizeof (float), nullptr);
                }
                if (aNrm >= 0)
                {
                    glEnableVertexAttribArray ((GLuint) aNrm);
                    glVertexAttribPointer ((GLuint) aNrm, 3, GL_FLOAT, GL_FALSE, 6 * sizeof (float), (const void*) (3 * sizeof (float)));
                }
                glDrawElementsInstanced (GL_TRIANGLES, mesh.indexCount, GL_UNSIGNED_INT, nullptr, instCount);
                if (aPos >= 0) glDisableVertexAttribArray ((GLuint) aPos);
                if (aNrm >= 0) glDisableVertexAttribArray ((GLuint) aNrm);
                if (wire) glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
                glDisable (GL_DEPTH_TEST);
            }
        }
        else if (ltype == LayerType::Shader2D)
        {
            const int mode = (int) std::round (LP ("mode"));
            // Custom GLSL / shader files only exist on the original 2D layer; extra shader layers use the built-ins
            if (legacy && (mode == 5 || userFileShader) && customLayerProg)
            {
                customLayerProg->use();
                customLayerProg->setUniform ("iResolution", (float) W, (float) H, 1.0f);
                customLayerProg->setUniform ("iTime", t * LP ("speed"));
                customLayerProg->setUniform ("iTimeDelta", dt);
                customLayerProg->setUniform ("iFrame", (GLint) frame);
                customLayerProg->setUniform ("iMouse", 0.0f, 0.0f, 0.0f, 0.0f);
                customLayerProg->setUniform ("iChannel0", 0);
                customLayerProg->setUniform ("iChannel1", 1);
                customLayerProg->setUniform ("iCam", 2);
                customLayerProg->setUniform ("iLoop", 3);
                customLayerProg->setUniform ("u_loop_len", (float) loopLen);
                customLayerProg->setUniform ("u_loop_pos", (float) loopPos);
                customLayerProg->setUniform ("u_feedback", P ("post.feedback"));
                for (const auto& u : floatUniforms)
                    customLayerProg->setUniform (u.glName, engine.bus.get (u.index));
                customLayerProg->setUniform ("u_bands", bands, 16);

                bindTex (0, feedback[1 - fbCur].tex);
                bindTex (1, canvas[curCanvas].tex);
                bindTex (2, camTex);
                glActiveTexture (GL_TEXTURE3);
                glBindTexture (GL_TEXTURE_2D_ARRAY, loopTex);
                drawFullscreen();
            }
            else if (proc2dProg)
            {
                proc2dProg->use();
                proc2dProg->setUniform ("iResolution", (float) W, (float) H, 1.0f);
                proc2dProg->setUniform ("iTime", t);
                proc2dProg->setUniform ("uMode", (GLint) juce::jlimit (0, 12, mode));
                proc2dProg->setUniform ("uSpeed", LP ("speed"));
                proc2dProg->setUniform ("uScale", LP ("scale"));
                proc2dProg->setUniform ("uComplexity", LP ("complexity"));
                proc2dProg->setUniform ("uColorShift", LP ("colorShift"));
                proc2dProg->setUniform ("uParamA", LP ("paramA"));
                proc2dProg->setUniform ("uParamB", LP ("paramB"));
                proc2dProg->setUniform ("uRms", vRms);
                proc2dProg->setUniform ("uEnv", vEnv);
                proc2dProg->setUniform ("uTransient", vTrans);
                proc2dProg->setUniform ("uCentroid", vCent);
                proc2dProg->setUniform ("uBands", bands, 16);
                drawFullscreen();
            }
        }
        else if (ltype == LayerType::VideoLoop && videoProg)
        {
            videoProg->use();
            videoProg->setUniform ("uLoopTex", 0);
            videoProg->setUniform ("uVidFileTex", 1);
            videoProg->setUniform ("uCamTex", 2);
            videoProg->setUniform ("uLoopLen", (float) loopLen);
            videoProg->setUniform ("uLoopPos", (float) loopPos);
            videoProg->setUniform ("uSource", (GLint) std::round (LP ("source")));
            videoProg->setUniform ("uTime", t);
            videoProg->setUniform ("uZoom", LP ("zoom"));
            videoProg->setUniform ("uStutter", LP ("stutter"));
            videoProg->setUniform ("uTintHue", LP ("tintHue"));
            videoProg->setUniform ("uEnv", vEnv);
            videoProg->setUniform ("uBeat", vBeat);
            glActiveTexture (GL_TEXTURE0);
            glBindTexture (GL_TEXTURE_2D_ARRAY, loopTex);
            bindTex (1, vidFileTex);
            bindTex (2, camTex);
            drawFullscreen();
        }
        else if (ltype == LayerType::WebcamFX && webcamProg)
        {
            webcamProg->use();
            webcamProg->setUniform ("uCamTex", 0);
            webcamProg->setUniform ("uTexel", 1.0f / (float) juce::jmax (1, camW), 1.0f / (float) juce::jmax (1, camH));
            webcamProg->setUniform ("uTime", t);
            webcamProg->setUniform ("uEnv", vEnv);
            webcamProg->setUniform ("uMirror", LP ("mirror"));
            webcamProg->setUniform ("uEdge", LP ("edgeDetect"));
            webcamProg->setUniform ("uPosterize", LP ("posterize"));
            webcamProg->setUniform ("uChromaKey", LP ("chromaKey"));
            webcamProg->setUniform ("uHueShift", LP ("hueShift"));
            webcamProg->setUniform ("uDisplace", LP ("uvDisplace"));
            webcamProg->setUniform ("uScale", LP ("scale"));
            bindTex (0, camTex);
            drawFullscreen();
        }
        else if (ltype == LayerType::Text && textProg)
        {
            textProg->use();
            textProg->setUniform ("uTextTex", 0);
            textProg->setUniform ("uTime", t);
            textProg->setUniform ("uEnv", vEnv);
            textProg->setUniform ("uTransient", vTrans);
            textProg->setUniform ("uSize", LP ("size"));
            textProg->setUniform ("uPosX", LP ("posX"));
            textProg->setUniform ("uPosY", LP ("posY"));
            textProg->setUniform ("uRotation", LP ("rotation"));
            textProg->setUniform ("uExtrude", LP ("extrude"));
            textProg->setUniform ("uGlitch", LP ("glitch"));
            textProg->setUniform ("uHue", LP ("hue"));
            textProg->setUniform ("uBounce", LP ("audioBounce"));
            bindTex (0, textTexs[(size_t) lid].tex);
            drawFullscreen();
        }
        else if (ltype == LayerType::Spectrum && spectrumProg)
        {
            spectrumProg->use();
            spectrumProg->setUniform ("uBands", bands, 16);
            spectrumProg->setUniform ("uTime", t);
            spectrumProg->setUniform ("uRms", vRms);
            spectrumProg->setUniform ("uEnv", vEnv);
            spectrumProg->setUniform ("uAspect", (float) W / (float) H);
            spectrumProg->setUniform ("uStyle", (GLint) std::round (LP ("style")));
            spectrumProg->setUniform ("uHue", LP ("hue"));
            spectrumProg->setUniform ("uHueSpread", LP ("hueSpread"));
            spectrumProg->setUniform ("uGain", LP ("gain"));
            spectrumProg->setUniform ("uThickness", LP ("thickness"));
            spectrumProg->setUniform ("uSize", LP ("size"));
            spectrumProg->setUniform ("uPosY", LP ("posY"));
            spectrumProg->setUniform ("uRot", LP ("rotation"));
            spectrumProg->setUniform ("uGlow", LP ("glow"));
            drawFullscreen();
        }
        else if (ltype == LayerType::Fill && fillProg)
        {
            fillProg->use();
            fillProg->setUniform ("uTime", t);
            fillProg->setUniform ("uEnv", vEnv);
            fillProg->setUniform ("uRms", vRms);
            fillProg->setUniform ("uAspect", (float) W / (float) H);
            fillProg->setUniform ("uStyle", (GLint) std::round (LP ("style")));
            fillProg->setUniform ("uHueA", LP ("hueA"));
            fillProg->setUniform ("uHueB", LP ("hueB"));
            fillProg->setUniform ("uSat", LP ("saturation"));
            fillProg->setUniform ("uBri", LP ("brightness"));
            fillProg->setUniform ("uAngle", LP ("angle"));
            fillProg->setUniform ("uScale", LP ("scale"));
            fillProg->setUniform ("uSpeed", LP ("speed"));
            fillProg->setUniform ("uAudio", LP ("audio"));
            drawFullscreen();
        }

        // Blend layerFbo onto canvas
        if (blendProg)
        {
            const int nextCanvas = 1 - curCanvas;
            canvas[nextCanvas].bind();
            blendProg->use();
            blendProg->setUniform ("uBase", 0);
            blendProg->setUniform ("uTop", 1);
            blendProg->setUniform ("uBlendMode", (GLint) juce::jlimit (0, 5, blendMode));
            blendProg->setUniform ("uOpacity", opacity);
            bindTex (0, canvas[curCanvas].tex);
            bindTex (1, layerFbo.tex);
            drawFullscreen();
            curCanvas = nextCanvas;
        }
    }

    // ---- Post-FX + Recursive Feedback Pass ----
    Fbo& fbDst  = feedback[fbCur];
    Fbo& fbPrev = feedback[1 - fbCur];
    fbDst.bind();
    if (postFxProg)
    {
        postFxProg->use();
        postFxProg->setUniform ("uCanvas", 0);
        postFxProg->setUniform ("uPrev", 1);
        postFxProg->setUniform ("uAspect", (float) W / (float) H, 1.0f);
        postFxProg->setUniform ("uTime", t);
        postFxProg->setUniform ("uFeedback", P ("post.feedback"));
        postFxProg->setUniform ("uFbZoom", P ("post.feedbackZoom"));
        postFxProg->setUniform ("uFbRot", P ("post.feedbackRotate"));
        postFxProg->setUniform ("uKaleido", P ("post.kaleidoscope"));
        postFxProg->setUniform ("uMirrorQuad", P ("post.mirrorQuad"));
        postFxProg->setUniform ("uRgbSplit", P ("post.rgbSplit"));
        postFxProg->setUniform ("uGlitch", P ("post.glitch"));
        postFxProg->setUniform ("uPixelate", P ("post.pixelate"));
        postFxProg->setUniform ("uTwirl", P ("post.twirl"));
        postFxProg->setUniform ("uRipple", P ("post.ripple"));
        postFxProg->setUniform ("uZoomBlur", P ("post.zoomBlur"));
        bindTex (0, canvas[curCanvas].tex);
        bindTex (1, fbPrev.tex);
        drawFullscreen();
    }

    // ---- HDR Bloom ----
    const float bloomAmt = P ("post.bloom");
    if (bloomAmt > 0.001f && brightProg && blurProg)
    {
        bloom[0].bind();
        brightProg->use();
        brightProg->setUniform ("uTex", 0);
        brightProg->setUniform ("uThreshold", P ("post.threshold"));
        bindTex (0, fbDst.tex);
        drawFullscreen();

        blurProg->use();
        blurProg->setUniform ("uTex", 0);
        for (int i = 0; i < 2; ++i)
        {
            bloom[1].bind();
            blurProg->setUniform ("uDir", 1.0f / (float) bloom[0].w, 0.0f);
            bindTex (0, bloom[0].tex);
            drawFullscreen();
            bloom[0].bind();
            blurProg->setUniform ("uDir", 0.0f, 1.0f / (float) bloom[0].h);
            bindTex (0, bloom[1].tex);
            drawFullscreen();
        }
    }

    // ---- Final Master Grading & Composite ----
    glBindFramebuffer (GL_FRAMEBUFFER, (GLuint) context.getFrameBufferID());
    glViewport (0, 0, W, H);
    if (compProg)
    {
        compProg->use();
        compProg->setUniform ("uScene", 0);
        compProg->setUniform ("uBloom", 1);
        compProg->setUniform ("uBloomAmt", bloomAmt);
        compProg->setUniform ("uScanlines", P ("post.scanlines"));
        compProg->setUniform ("uVignette", P ("post.vignette"));
        compProg->setUniform ("uHueShift", P ("post.hueShift"));
        compProg->setUniform ("uSaturation", P ("post.saturation"));
        compProg->setUniform ("uContrast", P ("post.contrast"));
        compProg->setUniform ("uStrobe", P ("post.strobe"));
        compProg->setUniform ("uDimmer", P ("post.masterDimmer"));
        compProg->setUniform ("uTonemap", P ("post.tonemap") >= 0.5f ? 1 : 0);
        compProg->setUniform ("uTime", t);
        compProg->setUniform ("uGrain", P ("post.grain"));
        compProg->setUniform ("uPosterize", P ("post.posterize"));
        compProg->setUniform ("uDuotone", P ("post.duotone"));
        compProg->setUniform ("uDuoA", P ("post.duoHueA"));
        compProg->setUniform ("uDuoB", P ("post.duoHueB"));
        bindTex (0, fbDst.tex);
        bindTex (1, bloom[0].tex);
        drawFullscreen();
    }
    glActiveTexture (GL_TEXTURE0);

    fbCur = 1 - fbCur;
    ++frame;
}
