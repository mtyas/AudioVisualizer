#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MeshData
{
    std::vector<float> vertices;     // interleaved: position(3), normal(3)
    std::vector<uint32_t> indices;
};

// Loads all triangle primitives of a .gltf/.glb into one mesh, normalised to a unit sphere.
// Node transforms and textures are ignored for now.
bool loadGltf (const std::string& path, MeshData& out, std::string& error);
