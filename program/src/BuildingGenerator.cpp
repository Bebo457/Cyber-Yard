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
    
    if (!b_Daszek) {
        // FLAT ROOF: Simply add the top face
        AddQuad(mesh.vertices, mesh.indices,
                vec_Top3D[0], vec_Top3D[1], vec_Top3D[2], vec_Top3D[3]);
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
        
        if (b_EdgesVertical) {

            ridgeStart = (vec_Top3D[3] + vec_Top3D[0]) / 2.0f;
            ridgeEnd = (vec_Top3D[1] + vec_Top3D[2]) / 2.0f;
            
            ridgeStart.z = f_Height + f_RoofHeight;
            ridgeEnd.z = f_Height + f_RoofHeight;
            
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[3], vec_Top3D[0], ridgeStart);
            
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[1], vec_Top3D[2], ridgeEnd);
            
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[0], vec_Top3D[1], ridgeEnd, ridgeStart);
            
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[2], vec_Top3D[3], ridgeStart, ridgeEnd);
            
        } else {
            
            ridgeStart = (vec_Top3D[0] + vec_Top3D[1]) / 2.0f;
            ridgeEnd = (vec_Top3D[2] + vec_Top3D[3]) / 2.0f;
            
            ridgeStart.z = f_Height + f_RoofHeight;
            ridgeEnd.z = f_Height + f_RoofHeight;
            
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[0], vec_Top3D[1], ridgeStart);
            
            AddTriangle(mesh.vertices, mesh.indices,
                       vec_Top3D[2], vec_Top3D[3], ridgeEnd);
            
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[1], vec_Top3D[2], ridgeEnd, ridgeStart);
            
            AddQuad(mesh.vertices, mesh.indices,
                   vec_Top3D[3], vec_Top3D[0], ridgeStart, ridgeEnd);
        }
    }
    
    unsigned int i_RoofIndexCount = mesh.indices.size() - i_RoofIndexStart;
    
    // ===== CREATE MATERIAL GROUPS =====
    mesh.materials.push_back(BuildingMesh::MaterialGroup(
        i_WallIndexStart, i_WallIndexCount, wallColor, "walls"));
    
    mesh.materials.push_back(BuildingMesh::MaterialGroup(
        i_FloorIndexStart, i_FloorIndexCount, glm::vec3(0.6f, 0.55f, 0.4f), "floor"));
    
    mesh.materials.push_back(BuildingMesh::MaterialGroup(
        i_RoofIndexStart, i_RoofIndexCount, roofColor, "roof"));
    
    // ===== CALCULATE NORMALS =====
    CalculateNormals(mesh);
    
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
            normal = glm::vec3(0.0f, 0.0f, 1.0f); // Default up
        }
    }
}

} // namespace Core
} // namespace ScotlandYard
