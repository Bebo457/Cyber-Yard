#include "RoadGenerator.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

ScotlandYard::Core::RoadMesh
ScotlandYard::Core::RoadGenerator::GenerateRoad(
    const std::vector<glm::vec2>& points,
    const std::vector<float>& roadWidths,   // width per point
    float textureRepeatMeters
) {
    RoadMesh mesh;

    if (points.size() < 2 || points.size() != roadWidths.size())
        return mesh;

    float accumulatedLength = 0.0f;

    // Reserve memory for efficiency
    mesh.vertices.reserve(points.size() * 2);
    mesh.normals.reserve(points.size() * 2);
    mesh.texCoords.reserve(points.size() * 2);
    mesh.indices.reserve((points.size() - 1) * 6);

    for (size_t i = 0; i < points.size(); ++i) {
        // --- Tangent ---
        glm::vec2 tangent;
        if (i == points.size() - 1)
            tangent = points[i] - points[i - 1];
        else
            tangent = points[i + 1] - points[i];
        tangent = glm::normalize(tangent);

        // --- Perpendicular (2D normal) ---
        glm::vec2 normal2D(-tangent.y, tangent.x);

        // --- Accumulate length ---
        if (i > 0) {
            accumulatedLength += glm::length(points[i] - points[i - 1]);
        }
        float vCoord = accumulatedLength / textureRepeatMeters;

        // --- Left & Right vertices using per-point width ---
        float halfWidth = roadWidths[i] * 0.5f;
        glm::vec2 left2D  = points[i] - normal2D * halfWidth;
        glm::vec2 right2D = points[i] + normal2D * halfWidth;

        mesh.vertices.emplace_back(left2D.x, 0.0f, left2D.y);
        mesh.vertices.emplace_back(right2D.x, 0.0f, right2D.y);

        // --- Normals (flat Y-up) ---
        mesh.normals.emplace_back(0.0f, 1.0f, 0.0f);
        mesh.normals.emplace_back(0.0f, 1.0f, 0.0f);

        // --- UVs ---
        mesh.texCoords.emplace_back(0.0f, vCoord); // left
        mesh.texCoords.emplace_back(1.0f, vCoord); // right

        // --- Indices ---
        if (i < points.size() - 1) {
            unsigned int base = static_cast<unsigned int>(i * 2);

            mesh.indices.push_back(base + 0);
            mesh.indices.push_back(base + 2);
            mesh.indices.push_back(base + 1);

            mesh.indices.push_back(base + 1);
            mesh.indices.push_back(base + 2);
            mesh.indices.push_back(base + 3);
        }
    }

    // --- Single material ---
    RoadMesh::RoadMaterial material;
    material.firstIndex = 0;
    material.indexCount = static_cast<unsigned int>(mesh.indices.size());
    material.color = glm::vec3(0.2f, 0.2f, 0.2f);

    mesh.materials.push_back(material);

    return mesh;
}
