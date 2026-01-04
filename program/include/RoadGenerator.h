#ifndef SCOTLANDYARD_CORE_ROADGENERATOR_H
#define SCOTLANDYARD_CORE_ROADGENERATOR_H

#include <glm/glm.hpp>
#include <vector>

namespace ScotlandYard {
namespace Core {

/**
 * @struct RoadMesh
 * Contains vertices, indices, normals, UVs, and materials for a procedural road
 */
struct RoadMesh {
    std::vector<glm::vec3> vertices;       // Left/right vertices
    std::vector<unsigned int> indices;     // Triangle indices
    std::vector<glm::vec3> normals;        // Per-vertex normals
    std::vector<glm::vec2> texCoords;      // UV coordinates

    struct RoadMaterial {
        unsigned int firstIndex = 0;
        unsigned int indexCount = 0;
        glm::vec3 color = glm::vec3(0.5f);
    };
    std::vector<RoadMaterial> materials;
};

/**
 * @class RoadGenerator
 * Generates a road mesh from a sequence of points
 */
class RoadGenerator {
public:
    /**
     * Generate a road mesh from a list of 2D points
     * @param points          Centerline points of the road
     * @param roadWidth       Width of the road
     * @param textureRepeat   Number of meters per texture repeat
     * @return RoadMesh       The generated mesh
     */
    // static RoadMesh GenerateRoad(
    //     const std::vector<glm::vec2>& points,
    //     float roadWidth,
    //     float textureRepeatMeters
    // );

    //Road widths per point version
    static RoadMesh GenerateRoad(
        const std::vector<glm::vec2>& points,
        const std::vector<float>& roadWidths,
        float textureRepeatMeters
    );
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_ROADGENERATOR_H