#include "BuildingGenerator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ScotlandYard {
namespace Core {

BuildingMesh BuildingGenerator::GenerateBuilding(
    const std::vector<glm::vec2>& vec_BasePoints,
    float f_Height,
    bool b_Daszek,
    float f_RoofHeight,
    const glm::vec3& wallColor,
    const glm::vec3& roofColor
) {
    if (vec_BasePoints.size() != 4) {
        throw std::invalid_argument("BuildingGenerator requires exactly 4 base points");
    }

    BuildingMesh mesh;

    std::vector<glm::vec3> vec_Base3D;
    for (const auto& point : vec_BasePoints) {
        vec_Base3D.push_back(glm::vec3(point.x, point.y, 0.0f));
    }

    std::vector<glm::vec3> vec_Top3D;
    for (const auto& point : vec_Base3D) {
        vec_Top3D.push_back(glm::vec3(point.x, point.y, f_Height));
    }
    
    // ===== BUILD THE WALLS =====
    unsigned int i_WallIndexStart = mesh.indices.size();

    for (int i = 0; i < 4; ++i) {
        int i_Next = (i + 1) % 4;
        AddQuad(mesh.vertices, mesh.indices,
                vec_Base3D[i], vec_Base3D[i_Next], vec_Top3D[i_Next], vec_Top3D[i]);
    }
    
    unsigned int i_WallIndexCount = mesh.indices.size() - i_WallIndexStart;
    
    // ===== BUILD THE FLOOR =====
    unsigned int i_FloorIndexStart = mesh.indices.size();
    
    AddQuad(mesh.vertices, mesh.indices,
            vec_Base3D[0], vec_Base3D[3], vec_Base3D[2], vec_Base3D[1]);
    
    unsigned int i_FloorIndexCount = mesh.indices.size() - i_FloorIndexStart;
    
    // ===== BUILD THE ROOF =====
    unsigned int i_RoofIndexStart = mesh.indices.size();
    unsigned int i_GableIndexCount = 0;
    unsigned int i_RoofIndexCount = 0;
    
    if (!b_Daszek) {
        // FLAT ROOF: Simply add the top face
        AddQuad(mesh.vertices, mesh.indices,
                vec_Top3D[0], vec_Top3D[1], vec_Top3D[2], vec_Top3D[3]);
        i_RoofIndexCount = mesh.indices.size() - i_RoofIndexStart;
    } else {
        // GABLED ROOF (TRIANGULAR PRISM)

        float f_Len01 = glm::length(vec_Top3D[1] - vec_Top3D[0]);
        float f_Len12 = glm::length(vec_Top3D[2] - vec_Top3D[1]);
        float f_Len23 = glm::length(vec_Top3D[3] - vec_Top3D[2]);
        float f_Len30 = glm::length(vec_Top3D[0] - vec_Top3D[3]);
        
        float f_AvgLen_01_23 = (f_Len01 + f_Len23) / 2.0f;
        float f_AvgLen_12_30 = (f_Len12 + f_Len30) / 2.0f;
        
        bool b_EdgesVertical = (f_AvgLen_01_23 > f_AvgLen_12_30);
        
        glm::vec3 ridgeStart, ridgeEnd;
        
        unsigned int roofStartLocal = mesh.indices.size();

        if (b_EdgesVertical) {

            ridgeStart = (vec_Top3D[3] + vec_Top3D[0]) / 2.0f;
            ridgeEnd = (vec_Top3D[1] + vec_Top3D[2]) / 2.0f;
            
            ridgeStart.z = f_Height + f_RoofHeight;
            ridgeEnd.z = f_Height + f_RoofHeight;
            
            unsigned int triStart1 = mesh.indices.size();
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[3], vec_Top3D[0], ridgeStart);
            unsigned int triStart2 = mesh.indices.size();
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[1], vec_Top3D[2], ridgeEnd);

            i_GableIndexCount = (mesh.indices.size() - triStart1);
            
            unsigned int quadStart1 = mesh.indices.size();
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[0], vec_Top3D[1], ridgeEnd, ridgeStart);
            unsigned int quadStart2 = mesh.indices.size();
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[2], vec_Top3D[3], ridgeStart, ridgeEnd);
            i_RoofIndexCount = mesh.indices.size() - quadStart1;
            i_RoofIndexStart = quadStart1;
        } else {
            
            ridgeStart = (vec_Top3D[0] + vec_Top3D[1]) / 2.0f;
            ridgeEnd = (vec_Top3D[2] + vec_Top3D[3]) / 2.0f;
            
            ridgeStart.z = f_Height + f_RoofHeight;
            ridgeEnd.z = f_Height + f_RoofHeight;
            
            unsigned int triStart1 = mesh.indices.size();
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[0], vec_Top3D[1], ridgeStart);
            unsigned int triStart2 = mesh.indices.size();
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[2], vec_Top3D[3], ridgeEnd);
            i_GableIndexCount = (mesh.indices.size() - triStart1);
            
            unsigned int quadStart1 = mesh.indices.size();
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[1], vec_Top3D[2], ridgeEnd, ridgeStart);
            unsigned int quadStart2 = mesh.indices.size();
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[3], vec_Top3D[0], ridgeStart, ridgeEnd);
            i_RoofIndexCount = mesh.indices.size() - quadStart1;
            i_RoofIndexStart = quadStart1;
        }
    }
    
    // ===== CREATE MATERIAL GROUPS =====
    mesh.materials.push_back(BuildingMesh::MaterialGroup(
        i_WallIndexStart, i_WallIndexCount, wallColor, "walls"));
    
    mesh.materials.push_back(BuildingMesh::MaterialGroup(
        i_FloorIndexStart, i_FloorIndexCount, glm::vec3(0.6f, 0.55f, 0.4f), "floor"));
    
    if (i_GableIndexCount > 0) {
        mesh.materials.push_back(BuildingMesh::MaterialGroup(
            i_RoofIndexStart - i_GableIndexCount, i_GableIndexCount, wallColor, "gable"));
    }
    mesh.materials.push_back(BuildingMesh::MaterialGroup(
        i_RoofIndexStart, i_RoofIndexCount, roofColor, "roof"));
    
    // ===== CALCULATE NORMALS =====
    CalculateNormals(mesh);
    
    // ===== GENERATE TEXTURE COORDINATES =====
    GenerateTexCoords(mesh);
    
    // ===== GENERATE WINDOWS =====
    GenerateWindows(mesh, vec_BasePoints, f_Height);

    // ===== GENERATE DOORS =====
    GenerateDoors(mesh, vec_BasePoints, f_Height);
    
    return mesh;
}

// ===== HELPER FUNCTIONS =====

void BuildingGenerator::AddQuad(
    std::vector<glm::vec3>& vec_Vertices,
    std::vector<unsigned int>& vec_Indices,
    const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2, const glm::vec3& p3
) {
    unsigned int i_BaseIdx = vec_Vertices.size();
    vec_Vertices.push_back(p0);
    vec_Vertices.push_back(p1);
    vec_Vertices.push_back(p2);
    vec_Vertices.push_back(p3);

    vec_Indices.push_back(i_BaseIdx + 0);
    vec_Indices.push_back(i_BaseIdx + 1);
    vec_Indices.push_back(i_BaseIdx + 2);
    
    vec_Indices.push_back(i_BaseIdx + 0);
    vec_Indices.push_back(i_BaseIdx + 2);
    vec_Indices.push_back(i_BaseIdx + 3);
}

void BuildingGenerator::AddTriangle(
    std::vector<glm::vec3>& vec_Vertices,
    std::vector<unsigned int>& vec_Indices,
    const glm::vec3& p0, const glm::vec3& p1,
    const glm::vec3& p2
) {
    unsigned int i_BaseIdx = vec_Vertices.size();
    vec_Vertices.push_back(p0);
    vec_Vertices.push_back(p1);
    vec_Vertices.push_back(p2);
    
    vec_Indices.push_back(i_BaseIdx + 0);
    vec_Indices.push_back(i_BaseIdx + 1);
    vec_Indices.push_back(i_BaseIdx + 2);
}

void BuildingGenerator::CalculateNormals(BuildingMesh& mesh) {
    mesh.normals.resize(mesh.vertices.size(), glm::vec3(0.0f));
    
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        unsigned int i_Idx0 = mesh.indices[i];
        unsigned int i_Idx1 = mesh.indices[i + 1];
        unsigned int i_Idx2 = mesh.indices[i + 2];
        
        const glm::vec3& v0 = mesh.vertices[i_Idx0];
        const glm::vec3& v1 = mesh.vertices[i_Idx1];
        const glm::vec3& v2 = mesh.vertices[i_Idx2];
        
        glm::vec3 edge1 = v1 - v0;
        glm::vec3 edge2 = v2 - v0;
        glm::vec3 normal = glm::cross(edge1, edge2);
        
        mesh.normals[i_Idx0] += normal;
        mesh.normals[i_Idx1] += normal;
        mesh.normals[i_Idx2] += normal;
    }

    for (auto& normal : mesh.normals) {
        float f_Len = glm::length(normal);
        if (f_Len > 0.0001f) {
            normal = glm::normalize(normal);
        } else {
            normal = glm::vec3(0.0f, 0.0f, 1.0f); 
        }
    }
}

void BuildingGenerator::GenerateTexCoords(BuildingMesh& mesh) {
    mesh.texCoords.resize(mesh.vertices.size(), glm::vec2(0.0f));
    
    float textureScale = 0.25f; 
    
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        const glm::vec3& vertex = mesh.vertices[i];
        const glm::vec3& normal = mesh.normals[i];
        
        float absNormalX = std::abs(normal.x);
        float absNormalY = std::abs(normal.y);
        float absNormalZ = std::abs(normal.z);
        
        if (absNormalZ > absNormalX && absNormalZ > absNormalY) {
            mesh.texCoords[i] = glm::vec2(vertex.x * textureScale, vertex.y * textureScale);
        } else if (absNormalX > absNormalY) {
            mesh.texCoords[i] = glm::vec2(vertex.y * textureScale, vertex.z * textureScale);
        } else {
            mesh.texCoords[i] = glm::vec2(vertex.x * textureScale, vertex.z * textureScale);
        }
    }
}

void BuildingGenerator::GenerateWindows(BuildingMesh& mesh, const std::vector<glm::vec2>& vec_BasePoints, float f_Height) {
    float f_WindowWidth = 3.0f;
    float f_WindowHeight = 2.0f;
    float f_WindowOffset = 0.05f; 
    
    std::vector<glm::vec3> vec_Base3D;
    for (const auto& point : vec_BasePoints) {
        vec_Base3D.push_back(glm::vec3(point.x, point.y, 0.0f));
    }
    
    glm::vec3 vec_BuildingCenter = (vec_Base3D[0] + vec_Base3D[1] + vec_Base3D[2] + vec_Base3D[3]) * 0.25f + glm::vec3(0.0f, 0.0f, f_Height * 0.5f);
    
    for (int i = 0; i < 4; ++i) {
        int i_Next = (i + 1) % 4;
        glm::vec3 vec_P0 = vec_Base3D[i];
        glm::vec3 vec_P1 = vec_Base3D[i_Next];
        
        glm::vec3 vec_WallDir = glm::normalize(vec_P1 - vec_P0);
        glm::vec3 vec_WallNormal = glm::vec3(-vec_WallDir.y, vec_WallDir.x, 0.0f);
        glm::vec3 vec_Up = glm::vec3(0.0f, 0.0f, 1.0f);
        
        float f_WallLength = glm::length(vec_P1 - vec_P0);
        glm::vec3 vec_WallCenter = (vec_P0 + vec_P1) * 0.5f + vec_Up * (f_Height * 0.5f);
        
        glm::vec3 vec_ToWall = vec_WallCenter - vec_BuildingCenter;
        if (glm::dot(vec_WallNormal, vec_ToWall) < 0.0f) {
            vec_WallNormal = -vec_WallNormal;
        }
        
        int i_NumWindows = 1;
        if (f_WallLength < 6.0f) {
            i_NumWindows = 1;
        } else if (f_WallLength < 12.0f) {
            i_NumWindows = 2;
        } else {
            i_NumWindows = 3;
        }

        if (i == 0) {
            i_NumWindows = 2;
        }
        
        if (f_WallLength < f_WindowWidth * i_NumWindows * 1.5f) continue;
        
        BuildingMesh::WindowWall windowWall;
        windowWall.wallNormal = vec_WallNormal;
        windowWall.wallCenter = vec_WallCenter;
        
        float f_BaseHeight = 3.0f; 
        float f_Spacing = f_WallLength / (i_NumWindows + 1);

        std::vector<float> vec_WindowOffsets;
        if (i == 0 && i_NumWindows == 2) {
            float f_EdgeMargin = std::max(f_WindowWidth * 0.65f, f_WallLength * 0.22f);
            vec_WindowOffsets.push_back(f_EdgeMargin);
            vec_WindowOffsets.push_back(f_WallLength - f_EdgeMargin);
        }
        if (vec_WindowOffsets.empty()) {
            for (int i_Window = 0; i_Window < i_NumWindows; ++i_Window) {
                vec_WindowOffsets.push_back((i_Window + 1) * f_Spacing);
            }
        }
        
        for (int i_Window = 0; i_Window < i_NumWindows; ++i_Window) {
            float f_T = vec_WindowOffsets[i_Window];
            glm::vec3 vec_WallPos = vec_P0 + vec_WallDir * f_T;
            
            if (f_Height > 3.0f) {
                glm::vec3 vec_CenterLower = vec_WallPos + vec_Up * f_BaseHeight;
                unsigned int i_BaseIdx = windowWall.vertices.size();
                
                glm::vec3 vec_HalfRight = vec_WallDir * (f_WindowWidth * 0.5f);
                glm::vec3 vec_HalfUp = vec_Up * (f_WindowHeight * 0.5f);
                glm::vec3 vec_Offset = vec_WallNormal * f_WindowOffset;
                
                windowWall.vertices.push_back(vec_CenterLower - vec_HalfRight - vec_HalfUp + vec_Offset);
                windowWall.vertices.push_back(vec_CenterLower + vec_HalfRight - vec_HalfUp + vec_Offset);
                windowWall.vertices.push_back(vec_CenterLower + vec_HalfRight + vec_HalfUp + vec_Offset);
                windowWall.vertices.push_back(vec_CenterLower - vec_HalfRight + vec_HalfUp + vec_Offset);
                
                for (int j = 0; j < 4; ++j) {
                    windowWall.normals.push_back(vec_WallNormal);
                }
                
                windowWall.texCoords.push_back(glm::vec2(0.0f, 0.0f));
                windowWall.texCoords.push_back(glm::vec2(1.0f, 0.0f));
                windowWall.texCoords.push_back(glm::vec2(1.0f, 1.0f));
                windowWall.texCoords.push_back(glm::vec2(0.0f, 1.0f));
                
                windowWall.indices.push_back(i_BaseIdx + 0);
                windowWall.indices.push_back(i_BaseIdx + 1);
                windowWall.indices.push_back(i_BaseIdx + 2);
                windowWall.indices.push_back(i_BaseIdx + 0);
                windowWall.indices.push_back(i_BaseIdx + 2);
                windowWall.indices.push_back(i_BaseIdx + 3);
            }
            
            if (f_Height > 7.0f) {
                float f_UpperHeight = f_BaseHeight + (f_Height - f_BaseHeight) * 0.6f;
                glm::vec3 vec_CenterUpper = vec_WallPos + vec_Up * f_UpperHeight;
                unsigned int i_BaseIdx = windowWall.vertices.size();
                
                glm::vec3 vec_HalfRight = vec_WallDir * (f_WindowWidth * 0.5f);
                glm::vec3 vec_HalfUp = vec_Up * (f_WindowHeight * 0.5f);
                glm::vec3 vec_Offset = vec_WallNormal * f_WindowOffset;
                
                windowWall.vertices.push_back(vec_CenterUpper - vec_HalfRight - vec_HalfUp + vec_Offset);
                windowWall.vertices.push_back(vec_CenterUpper + vec_HalfRight - vec_HalfUp + vec_Offset);
                windowWall.vertices.push_back(vec_CenterUpper + vec_HalfRight + vec_HalfUp + vec_Offset);
                windowWall.vertices.push_back(vec_CenterUpper - vec_HalfRight + vec_HalfUp + vec_Offset);
                
                for (int j = 0; j < 4; ++j) {
                    windowWall.normals.push_back(vec_WallNormal);
                }
                
                windowWall.texCoords.push_back(glm::vec2(0.0f, 0.0f));
                windowWall.texCoords.push_back(glm::vec2(1.0f, 0.0f));
                windowWall.texCoords.push_back(glm::vec2(1.0f, 1.0f));
                windowWall.texCoords.push_back(glm::vec2(0.0f, 1.0f));
                
                windowWall.indices.push_back(i_BaseIdx + 0);
                windowWall.indices.push_back(i_BaseIdx + 1);
                windowWall.indices.push_back(i_BaseIdx + 2);
                windowWall.indices.push_back(i_BaseIdx + 0);
                windowWall.indices.push_back(i_BaseIdx + 2);
                windowWall.indices.push_back(i_BaseIdx + 3);
            }
        }
        
        if (!windowWall.vertices.empty()) {
            mesh.windowWalls.push_back(windowWall);
        }
    }
}

void BuildingGenerator::GenerateDoors(BuildingMesh& mesh, const std::vector<glm::vec2>& vec_BasePoints, float f_Height) {
    float f_DoorWidth = 2.2f;
    float f_DoorHeight = 3.4f;
    float f_DoorOffset = 0.05f;

    std::vector<glm::vec3> vec_Base3D;
    for (const auto& point : vec_BasePoints) {
        vec_Base3D.push_back(glm::vec3(point.x, point.y, 0.0f));
    }

    glm::vec3 vec_BuildingCenter = (vec_Base3D[0] + vec_Base3D[1] + vec_Base3D[2] + vec_Base3D[3]) * 0.25f + glm::vec3(0.0f, 0.0f, f_Height * 0.5f);

    int i_WallIndex = 0;
    int i_Next = (i_WallIndex + 1) % 4;
    glm::vec3 vec_P0 = vec_Base3D[i_WallIndex];
    glm::vec3 vec_P1 = vec_Base3D[i_Next];

    glm::vec3 vec_WallDir = glm::normalize(vec_P1 - vec_P0);
    glm::vec3 vec_WallNormal = glm::vec3(-vec_WallDir.y, vec_WallDir.x, 0.0f);
    glm::vec3 vec_Up = glm::vec3(0.0f, 0.0f, 1.0f);

    float f_WallLength = glm::length(vec_P1 - vec_P0);
    glm::vec3 vec_WallCenter = (vec_P0 + vec_P1) * 0.5f + vec_Up * (f_Height * 0.5f);

    glm::vec3 vec_ToWall = vec_WallCenter - vec_BuildingCenter;
    if (glm::dot(vec_WallNormal, vec_ToWall) < 0.0f) {
        vec_WallNormal = -vec_WallNormal;
    }

    if (f_WallLength < f_DoorWidth * 1.5f) return;

    BuildingMesh::Door door;
    door.wallNormal = vec_WallNormal;
    door.wallCenter = vec_WallCenter;

    glm::vec3 vec_Center = (vec_P0 + vec_P1) * 0.5f + vec_Up * (f_DoorHeight * 0.5f);
    glm::vec3 vec_HalfRight = vec_WallDir * (f_DoorWidth * 0.5f);
    glm::vec3 vec_HalfUp = vec_Up * (f_DoorHeight * 0.5f);
    glm::vec3 vec_Offset = vec_WallNormal * f_DoorOffset;

    door.vertices.push_back(vec_Center - vec_HalfRight - vec_HalfUp + vec_Offset);
    door.vertices.push_back(vec_Center + vec_HalfRight - vec_HalfUp + vec_Offset);
    door.vertices.push_back(vec_Center + vec_HalfRight + vec_HalfUp + vec_Offset);
    door.vertices.push_back(vec_Center - vec_HalfRight + vec_HalfUp + vec_Offset);

    for (int j = 0; j < 4; ++j) {
        door.normals.push_back(vec_WallNormal);
    }


    door.texCoords.push_back(glm::vec2(0.0f, 1.0f));
    door.texCoords.push_back(glm::vec2(1.0f, 1.0f));
    door.texCoords.push_back(glm::vec2(1.0f, 0.0f));
    door.texCoords.push_back(glm::vec2(0.0f, 0.0f));

    door.indices.push_back(0);
    door.indices.push_back(1);
    door.indices.push_back(2);
    door.indices.push_back(0);
    door.indices.push_back(2);
    door.indices.push_back(3);

    mesh.doors.push_back(door);
}

} // namespace Core
} // namespace ScotlandYard
