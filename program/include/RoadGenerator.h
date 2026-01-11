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
     * Generate a road mesh from a list of 2D points (width per point)
     */
    static RoadMesh GenerateRoad(
        const std::vector<glm::vec2>& points,
        const std::vector<float>& roadWidths,
        float textureRepeatMeters
    );

    /**
     * Generate a simple road segment between two points
     * @param accumulatedLength - pass by reference to accumulate V coordinate for UV
     */
    static RoadMesh GenerateRoadSegment(
        const glm::vec2& p0,
        const glm::vec2& p1,
        float halfWidth,
        float textureRepeatMeters,
        float& accumulatedLength
    );

    /**
     * Generate a round join (intersection) connecting multiple neighbor nodes
     * @param segmentsPerJoin - number of triangles per segment of the join
     */
    static RoadMesh GenerateRoundJoin(
        const glm::vec2& nodePos,
        const std::vector<glm::vec2>& neighborPositions,
        float halfWidth,
        int segmentsPerJoin = 6
    );
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_ROADGENERATOR_H