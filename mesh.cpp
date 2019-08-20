#include "mesh.hpp"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

auto constexpr ppFlags = aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_PreTransformVertices;

std::optional<Mesh> load_mesh(const std::filesystem::__cxx11::path &path)
{
    Assimp::Importer importer;
    auto const srcmodel = importer.ReadFile(path.c_str(), ppFlags);
    if(srcmodel == nullptr)
        return std::nullopt;
    Mesh mesh;
    for(unsigned int meshIdx = 0; meshIdx < srcmodel->mNumMeshes; meshIdx++)
    {
        auto const srcmesh = srcmodel->mMeshes[meshIdx];
        for(unsigned int faceIdx = 0; faceIdx < srcmesh->mNumFaces; faceIdx++)
        {
            auto const & srcface = srcmesh->mFaces[faceIdx];
            if(srcface.mNumIndices != 3)
                continue;

            auto & face = mesh.faces.emplace_back();
            for(unsigned int i = 0; i < srcface.mNumIndices; i++)
            {
                auto const & vtx = srcmesh->mVertices[srcface.mIndices[i]];
                face.corners.at(i).position = glm::vec3(vtx.x, vtx.y, vtx.z);
            }
        }
    }
    return mesh;
}
