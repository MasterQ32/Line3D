#ifndef MESH_HPP
#define MESH_HPP

#include <glm/glm.hpp>
#include <vector>
#include <array>
#include <filesystem>
#include <optional>

struct Vertex
{
    glm::vec3 position;
};

struct Triangle
{
    std::array<Vertex, 3> corners;
};

struct Mesh
{
    std::vector<Triangle> faces;
};

std::optional<Mesh> load_mesh(std::filesystem::path const & path);

#endif // MESH_HPP
