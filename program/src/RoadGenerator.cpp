#include "RoadGenerator.h"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

using namespace ScotlandYard::Core;

RoadMesh RoadGenerator::GenerateRoad(
    const std::vector<glm::vec2>& points,
    const std::vector<float>& roadWidths,
    float textureRepeatMeters
) {
    RoadMesh mesh;

    if (points.size() < 2 || points.size() != roadWidths.size())
        return mesh;

    float accumulatedLength = 0.0f;

    mesh.vertices.reserve(points.size() * 2);
    mesh.normals.reserve(points.size() * 2);
    mesh.texCoords.reserve(points.size() * 2);
    mesh.indices.reserve((points.size() - 1) * 6);

    for (size_t i = 0; i < points.size(); ++i) {
        glm::vec2 tangent;
        if (i == points.size() - 1)
            tangent = points[i] - points[i - 1];
        else
            tangent = points[i + 1] - points[i];
        tangent = glm::normalize(tangent);

        glm::vec2 normal2D(-tangent.y, tangent.x);

        if (i > 0)
            accumulatedLength += glm::length(points[i] - points[i - 1]);

        float vCoord = accumulatedLength / textureRepeatMeters;

        float halfWidth = roadWidths[i] * 0.5f;
        glm::vec2 left2D  = points[i] - normal2D * halfWidth;
        glm::vec2 right2D = points[i] + normal2D * halfWidth;

        mesh.vertices.emplace_back(left2D.x, 0.0f, left2D.y);
        mesh.vertices.emplace_back(right2D.x, 0.0f, right2D.y);

        mesh.normals.emplace_back(0.0f, 1.0f, 0.0f);
        mesh.normals.emplace_back(0.0f, 1.0f, 0.0f);

        mesh.texCoords.emplace_back(0.0f, vCoord);
        mesh.texCoords.emplace_back(2.0f, vCoord); // podwójne UV na dwa pasy

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

    RoadMesh::RoadMaterial material;
    material.firstIndex = 0;
    material.indexCount = static_cast<unsigned int>(mesh.indices.size());
    material.color = glm::vec3(0.2f, 0.2f, 0.2f);
    mesh.materials.push_back(material);

    return mesh;
}

// ---------------------------
// Simple road segment
// ---------------------------
RoadMesh RoadGenerator::GenerateRoadSegment(
    const glm::vec2& p0,
    const glm::vec2& p1,
    float halfWidth,
    float textureRepeatMeters,
    float& accumulatedLength
) {
    RoadMesh mesh;

    glm::vec2 dir = p1 - p0;
    float len = glm::length(dir);

    // --- zabezpieczenie przed zerową długością ---
    if (len < 1e-6f) {
        // segment o zerowej długości → zwróć pusty mesh
        return mesh;
    }

    dir /= len; // bezpieczna normalizacja
    glm::vec2 normal(-dir.y, dir.x);

    glm::vec2 left0  = p0 - normal * halfWidth;
    glm::vec2 right0 = p0 + normal * halfWidth;
    glm::vec2 left1  = p1 - normal * halfWidth;
    glm::vec2 right1 = p1 + normal * halfWidth;

    mesh.vertices.emplace_back(left0.x, 0.0f, left0.y);
    mesh.vertices.emplace_back(right0.x, 0.0f, right0.y);
    mesh.vertices.emplace_back(left1.x, 0.0f, left1.y);
    mesh.vertices.emplace_back(right1.x, 0.0f, right1.y);

    for (int i = 0; i < 4; i++)
        mesh.normals.emplace_back(0.0f, 1.0f, 0.0f);

    // --- bezpieczne UV ---
    float v0 = accumulatedLength / (textureRepeatMeters > 0.0f ? textureRepeatMeters : 1.0f);
    accumulatedLength += len;
    float v1 = accumulatedLength / (textureRepeatMeters > 0.0f ? textureRepeatMeters : 1.0f);

    mesh.texCoords.emplace_back(0.0f, v0);
    mesh.texCoords.emplace_back(1.0f, v0);
    mesh.texCoords.emplace_back(0.0f, v1);
    mesh.texCoords.emplace_back(1.0f, v1);

    mesh.indices = {0, 2, 1, 1, 2, 3};

    return mesh;
}


// ---------------------------
// Round join / intersection
// ---------------------------
RoadMesh RoadGenerator::GenerateRoundJoin(
    const glm::vec2& nodePos,
    const std::vector<glm::vec2>& neighborPositions,
    float halfWidth,
    int segmentsPerJoin
) {
    RoadMesh mesh;

    if (neighborPositions.size() < 2)
        return mesh;

    std::vector<glm::vec2> dirs;
    for (auto& nb : neighborPositions)
        dirs.push_back(glm::normalize(nb - nodePos));

    std::sort(dirs.begin(), dirs.end(), [](const glm::vec2& a, const glm::vec2& b){
        return atan2(a.y, a.x) < atan2(b.y, b.x);
    });

    for (size_t i=0;i<dirs.size();i++) {
        glm::vec2 dirA = dirs[i];
        glm::vec2 dirB = dirs[(i+1)%dirs.size()];

        float angleA = atan2(dirA.y, dirA.x);
        float angleB = atan2(dirB.y, dirB.x);
        if (angleB < angleA) angleB += glm::two_pi<float>();
        float delta = (angleB - angleA) / segmentsPerJoin;

        int startIdx = static_cast<int>(mesh.vertices.size());
        mesh.vertices.emplace_back(nodePos.x,0.0f,nodePos.y);
        mesh.normals.emplace_back(0.0f,1.0f,0.0f);
        mesh.texCoords.emplace_back(0.5f,0.5f);

        for (int s=0;s<=segmentsPerJoin;s++) {
            float a = angleA + delta*s;
            glm::vec2 p = nodePos + glm::vec2(cos(a), sin(a)) * halfWidth;
            mesh.vertices.emplace_back(p.x,0.0f,p.y);
            mesh.normals.emplace_back(0.0f,1.0f,0.0f);
            mesh.texCoords.emplace_back(0.5f + 0.5f*cos(a), 0.5f + 0.5f*sin(a));
        }

        for (int s=0;s<segmentsPerJoin;s++) {
            mesh.indices.push_back(startIdx);
            mesh.indices.push_back(startIdx + 1 + s);
            mesh.indices.push_back(startIdx + 2 + s);
        }
    }

    return mesh;
}
