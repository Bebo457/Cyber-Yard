#ifndef SCOTLANDYARD_CORE_BUILDINGGENERATOR_H
#define SCOTLANDYARD_CORE_BUILDINGGENERATOR_H

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace ScotlandYard {
namespace Core {

/**
 * @struct BuildingMesh
 * Structure containing geometric data for a building
 */
struct BuildingMesh {
    std::vector<glm::vec3> vertices;   
    std::vector<unsigned int> indices; 
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texCoords;
    
    struct WindowWall {
        std::vector<glm::vec3> vertices;
        std::vector<unsigned int> indices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        glm::vec3 wallNormal;  // Normal of the wall these windows are on
        glm::vec3 wallCenter;  // Center point of the wall
    };
    
    std::vector<WindowWall> windowWalls;  // Windows grouped by wall

    struct Door {
        std::vector<glm::vec3> vertices;
        std::vector<unsigned int> indices;
        std::vector<glm::vec3> normals;
        std::vector<glm::vec2> texCoords;
        glm::vec3 wallNormal;
        glm::vec3 wallCenter;
    };

    std::vector<Door> doors;  // Doors grouped by wall (currently one front door)
    
    struct MaterialGroup {
        unsigned int firstIndex = 0;    
        unsigned int indexCount = 0;   
        glm::vec3 color = glm::vec3(0.5f);
        std::string name = "";         
        
        MaterialGroup() = default;
        MaterialGroup(unsigned int idx, unsigned int count, const glm::vec3& col, const std::string& n)
            : firstIndex(idx), indexCount(count), color(col), name(n) {}
    };
    
    std::vector<MaterialGroup> materials;
};

/**
 * @class BuildingGenerator
 * Class for generating 3D building models with roofs
 */
class BuildingGenerator {
public:
    /**
     * Generates a 3D building model with the following specifications:
     * 
     * @param vec_BasePoints 
     * @param f_Height 
     * @param b_Daszek 
     * @param f_RoofHeight 
     * @param wallColor 
     * @param roofColor 
     * 
     * @return BuildingMesh Structure containing vertices, indices, normals, and material groups
     */
    static BuildingMesh GenerateBuilding(
        const std::vector<glm::vec2>& vec_BasePoints,
        float f_Height,
        bool b_Daszek = false,
        float f_RoofHeight = 0.0f,
        const glm::vec3& wallColor = glm::vec3(0.9f, 0.85f, 0.7f),
        const glm::vec3& roofColor = glm::vec3(0.8f, 0.2f, 0.2f)
    );

private:

    static void CalculateNormals(BuildingMesh& mesh);
    
    static void GenerateTexCoords(BuildingMesh& mesh);
    
    static void GenerateWindows(BuildingMesh& mesh, const std::vector<glm::vec2>& vec_BasePoints, float f_Height);

    static void GenerateDoors(BuildingMesh& mesh, const std::vector<glm::vec2>& vec_BasePoints, float f_Height);
    
    static void AddQuad(
        std::vector<glm::vec3>& vec_Vertices,
        std::vector<unsigned int>& vec_Indices,
        const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2, const glm::vec3& p3
    );
    

    static void AddTriangle(
        std::vector<glm::vec3>& vec_Vertices,
        std::vector<unsigned int>& vec_Indices,
        const glm::vec3& p0, const glm::vec3& p1,
        const glm::vec3& p2
    );
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_BUILDINGGENERATOR_H
