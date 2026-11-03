#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>

#include "GltfLoader.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
const unsigned char* accessorPtr (const tinygltf::Model& m, const tinygltf::Accessor& a, int& stride)
{
    if (a.bufferView < 0 || a.bufferView >= (int) m.bufferViews.size())
        return nullptr;
    const auto& bv = m.bufferViews[(size_t) a.bufferView];
    stride = a.ByteStride (bv);
    if (stride <= 0)
        return nullptr;
    return m.buffers[(size_t) bv.buffer].data.data() + bv.byteOffset + a.byteOffset;
}
} // namespace

bool loadGltf (const std::string& path, MeshData& out, std::string& error)
{
    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string warn, err;

    // Textures are ignored: accept images without decoding.
    loader.SetImageLoader ([] (tinygltf::Image*, const int, std::string*, std::string*, int, int,
                               const unsigned char*, int, void*) { return true; }, nullptr);

    std::string lower = path;
    std::transform (lower.begin(), lower.end(), lower.begin(), [] (unsigned char c) { return (char) std::tolower (c); });
    const bool isBinary = lower.size() > 4 && lower.compare (lower.size() - 4, 4, ".glb") == 0;
    const bool ok = isBinary ? loader.LoadBinaryFromFile (&model, &err, &warn, path)
                             : loader.LoadASCIIFromFile (&model, &err, &warn, path);
    if (! ok)
    {
        error = err.empty() ? "Failed to load glTF" : err;
        return false;
    }

    MeshData mesh;
    for (const auto& m : model.meshes)
    {
        for (const auto& prim : m.primitives)
        {
            if (prim.mode != TINYGLTF_MODE_TRIANGLES && prim.mode != -1)
                continue;
            const auto posIt = prim.attributes.find ("POSITION");
            if (posIt == prim.attributes.end())
                continue;

            const auto& posAcc = model.accessors[(size_t) posIt->second];
            int posStride = 0;
            const unsigned char* posData = accessorPtr (model, posAcc, posStride);
            if (posData == nullptr || posAcc.type != TINYGLTF_TYPE_VEC3)
                continue;

            const uint32_t base = (uint32_t) (mesh.vertices.size() / 6);
            const size_t count = posAcc.count;

            const unsigned char* nrmData = nullptr;
            int nrmStride = 0;
            const auto nrmIt = prim.attributes.find ("NORMAL");
            if (nrmIt != prim.attributes.end())
                nrmData = accessorPtr (model, model.accessors[(size_t) nrmIt->second], nrmStride);

            for (size_t i = 0; i < count; ++i)
            {
                float p[3], n[3] = { 0, 0, 0 };
                std::memcpy (p, posData + i * (size_t) posStride, sizeof (p));
                if (nrmData != nullptr)
                    std::memcpy (n, nrmData + i * (size_t) nrmStride, sizeof (n));
                mesh.vertices.insert (mesh.vertices.end(), { p[0], p[1], p[2], n[0], n[1], n[2] });
            }

            const size_t idxStart = mesh.indices.size();
            if (prim.indices >= 0)
            {
                const auto& ia = model.accessors[(size_t) prim.indices];
                int is = 0;
                const unsigned char* idata = accessorPtr (model, ia, is);
                if (idata == nullptr)
                    continue;
                for (size_t i = 0; i < ia.count; ++i)
                {
                    const unsigned char* p = idata + i * (size_t) is;
                    uint32_t v = 0;
                    switch (ia.componentType)
                    {
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:  v = *p; break;
                        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: { uint16_t s; std::memcpy (&s, p, 2); v = s; break; }
                        default:                                     std::memcpy (&v, p, 4); break;
                    }
                    mesh.indices.push_back (base + v);
                }
            }
            else
            {
                for (size_t i = 0; i < count; ++i)
                    mesh.indices.push_back (base + (uint32_t) i);
            }

            // Smooth normals if the file has none
            if (nrmData == nullptr)
            {
                for (size_t t = idxStart; t + 2 < mesh.indices.size(); t += 3)
                {
                    float* v0 = &mesh.vertices[(size_t) mesh.indices[t] * 6];
                    float* v1 = &mesh.vertices[(size_t) mesh.indices[t + 1] * 6];
                    float* v2 = &mesh.vertices[(size_t) mesh.indices[t + 2] * 6];
                    const float e1[3] = { v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2] };
                    const float e2[3] = { v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2] };
                    const float fn[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
                    for (float* v : { v0, v1, v2 })
                        for (int k = 0; k < 3; ++k)
                            v[3 + k] += fn[k];
                }
            }
        }
    }

    if (mesh.vertices.empty() || mesh.indices.empty())
    {
        error = "No triangle geometry found";
        return false;
    }

    // Normalise normals, centre and scale to unit radius
    const size_t vc = mesh.vertices.size() / 6;
    float mn[3] = { 1e30f, 1e30f, 1e30f }, mx[3] = { -1e30f, -1e30f, -1e30f };
    for (size_t i = 0; i < vc; ++i)
    {
        float* v = &mesh.vertices[i * 6];
        for (int k = 0; k < 3; ++k) { mn[k] = std::min (mn[k], v[k]); mx[k] = std::max (mx[k], v[k]); }
        const float len = std::sqrt (v[3] * v[3] + v[4] * v[4] + v[5] * v[5]);
        if (len > 1e-8f) { v[3] /= len; v[4] /= len; v[5] /= len; }
        else { v[3] = 0; v[4] = 1; v[5] = 0; }
    }
    const float c[3] = { (mn[0] + mx[0]) * 0.5f, (mn[1] + mx[1]) * 0.5f, (mn[2] + mx[2]) * 0.5f };
    float radius = 1e-6f;
    for (size_t i = 0; i < vc; ++i)
    {
        const float* v = &mesh.vertices[i * 6];
        radius = std::max (radius, std::sqrt ((v[0] - c[0]) * (v[0] - c[0]) + (v[1] - c[1]) * (v[1] - c[1]) + (v[2] - c[2]) * (v[2] - c[2])));
    }
    for (size_t i = 0; i < vc; ++i)
    {
        float* v = &mesh.vertices[i * 6];
        for (int k = 0; k < 3; ++k)
            v[k] = (v[k] - c[k]) / radius;
    }

    out = std::move (mesh);
    return true;
}
