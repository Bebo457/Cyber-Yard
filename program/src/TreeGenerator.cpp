#include "TreeGenerator.h"
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <ctime>

namespace ScotlandYard {
    namespace Core {

        float TreeGenerator::RandRange(std::mt19937& rng, float a, float b)
        {
            std::uniform_real_distribution<float> dist(a, b);
            return dist(rng);
        }

        void TreeGenerator::AppendTriangle(TreeMesh& mesh, unsigned int a, unsigned int b, unsigned int c)
        {
            mesh.indices.push_back(a);
            mesh.indices.push_back(b);
            mesh.indices.push_back(c);
        }

        void TreeGenerator::EnsureArrays(TreeMesh& mesh)
        {
            if (mesh.normals.size() != mesh.vertices.size())
                mesh.normals.resize(mesh.vertices.size(), glm::vec3(0.0f));
            if (mesh.texCoords.size() != mesh.vertices.size())
                mesh.texCoords.resize(mesh.vertices.size(), glm::vec2(0.0f));
        }

        void TreeGenerator::AppendCylinder(
            TreeMesh& mesh,
            float radius,
            float height,
            int segments,
            bool caps,
            unsigned int& outFirstIndex,
            unsigned int& outIndexCount
        )
        {
            outFirstIndex = (unsigned int)mesh.indices.size();

            const unsigned int baseVertex = (unsigned int)mesh.vertices.size();

            // Side vertices 2 rings
            for (int i = 0; i <= segments; ++i)
            {
                float t = (float)i / (float)segments;
                float ang = t * glm::two_pi<float>();
                float x = std::cos(ang) * radius;
                float y = std::sin(ang) * radius;

                // bottom
                mesh.vertices.emplace_back(x, y, 0.0f);
                mesh.normals.emplace_back(glm::normalize(glm::vec3(x, y, 0.0f)));
                mesh.texCoords.emplace_back(t, 0.0f);

                // top
                mesh.vertices.emplace_back(x, y, height);
                mesh.normals.emplace_back(glm::normalize(glm::vec3(x, y, 0.0f)));
                mesh.texCoords.emplace_back(t, 1.0f);
            }

            // Side indices
            for (int i = 0; i < segments; ++i)
            {
                unsigned int i0 = baseVertex + (unsigned int)(2 * i + 0);
                unsigned int i1 = baseVertex + (unsigned int)(2 * i + 1);
                unsigned int i2 = baseVertex + (unsigned int)(2 * (i + 1) + 1);
                unsigned int i3 = baseVertex + (unsigned int)(2 * (i + 1) + 0);

                // two triangles
                AppendTriangle(mesh, i0, i1, i2);
                AppendTriangle(mesh, i0, i2, i3);
            }

            if (caps)
            {
                // bottom cap center
                unsigned int bottomCenter = (unsigned int)mesh.vertices.size();
                mesh.vertices.emplace_back(0.0f, 0.0f, 0.0f);
                mesh.normals.emplace_back(0.0f, 0.0f, -1.0f);
                mesh.texCoords.emplace_back(0.5f, 0.5f);

                // top cap center
                unsigned int topCenter = (unsigned int)mesh.vertices.size();
                mesh.vertices.emplace_back(0.0f, 0.0f, height);
                mesh.normals.emplace_back(0.0f, 0.0f, 1.0f);
                mesh.texCoords.emplace_back(0.5f, 0.5f);

                // cap rings
                unsigned int bottomRingStart = (unsigned int)mesh.vertices.size();
                for (int i = 0; i <= segments; ++i)
                {
                    float t = (float)i / (float)segments;
                    float ang = t * glm::two_pi<float>();
                    float x = std::cos(ang) * radius;
                    float y = std::sin(ang) * radius;

                    // bottom
                    mesh.vertices.emplace_back(x, y, 0.0f);
                    mesh.normals.emplace_back(0.0f, 0.0f, -1.0f);
                    mesh.texCoords.emplace_back(0.5f + x / (2.0f * radius), 0.5f + y / (2.0f * radius));

                    // top
                    mesh.vertices.emplace_back(x, y, height);
                    mesh.normals.emplace_back(0.0f, 0.0f, 1.0f);
                    mesh.texCoords.emplace_back(0.5f + x / (2.0f * radius), 0.5f + y / (2.0f * radius));
                }

                for (int i = 0; i < segments; ++i)
                {
                    unsigned int b0 = bottomRingStart + (unsigned int)(2 * i + 0);
                    unsigned int b1 = bottomRingStart + (unsigned int)(2 * (i + 1) + 0);

                    // bottom cap (clockwise for -Z normal)
                    AppendTriangle(mesh, bottomCenter, b1, b0);

                    unsigned int t0 = bottomRingStart + (unsigned int)(2 * i + 1);
                    unsigned int t1 = bottomRingStart + (unsigned int)(2 * (i + 1) + 1);

                    // top cap (counter-clockwise for +Z normal)
                    AppendTriangle(mesh, topCenter, t0, t1);
                }
            }

            outIndexCount = (unsigned int)mesh.indices.size() - outFirstIndex;
        }

        void TreeGenerator::AppendSphere(
            TreeMesh& mesh,
            const glm::vec3& center,
            float radius,
            int segments,
            const glm::vec3& nonUniformScale,
            unsigned int& outFirstIndex,
            unsigned int& outIndexCount
        )
        {
            outFirstIndex = (unsigned int)mesh.indices.size();
            const unsigned int baseVertex = (unsigned int)mesh.vertices.size();

            // segments
            int latSeg = std::max(6, segments / 2);
            int lonSeg = std::max(8, segments);

            for (int i = 0; i <= latSeg; ++i)
            {
                float v = (float)i / (float)latSeg;             
                float phi = v * glm::pi<float>();               
                float sinPhi = std::sin(phi);
                float cosPhi = std::cos(phi);

                for (int j = 0; j <= lonSeg; ++j)
                {
                    float u = (float)j / (float)lonSeg;         
                    float theta = u * glm::two_pi<float>();     
                    float sinTheta = std::sin(theta);
                    float cosTheta = std::cos(theta);

                    glm::vec3 unit(
                        cosTheta * sinPhi,
                        sinTheta * sinPhi,
                        cosPhi
                    );

                    
                    glm::vec3 scaled = unit * nonUniformScale;

                    glm::vec3 pos = center + scaled * radius;

                    
                    glm::vec3 n = glm::normalize(unit);

                    mesh.vertices.push_back(pos);
                    mesh.normals.push_back(n);

                    // Spherical UV
                    mesh.texCoords.emplace_back(u, 1.0f - v);
                }
            }

            // Indices
            for (int i = 0; i < latSeg; ++i)
            {
                for (int j = 0; j < lonSeg; ++j)
                {
                    unsigned int a = baseVertex + (unsigned int)(i * (lonSeg + 1) + j);
                    unsigned int b = baseVertex + (unsigned int)((i + 1) * (lonSeg + 1) + j);
                    unsigned int c = baseVertex + (unsigned int)((i + 1) * (lonSeg + 1) + (j + 1));
                    unsigned int d = baseVertex + (unsigned int)(i * (lonSeg + 1) + (j + 1));

                    AppendTriangle(mesh, a, b, c);
                    AppendTriangle(mesh, a, c, d);
                }
            }

            outIndexCount = (unsigned int)mesh.indices.size() - outFirstIndex;
        }

        TreeMesh TreeGenerator::GenerateTree(const TreeParams& baseParams, unsigned int seed)
        {
            TreeParams p = baseParams;

            if (seed == 0) seed = (unsigned int)std::time(nullptr);
            std::mt19937 rng(seed);

            // Apply jitter
            auto jitterMul = [&](float jitter) {
                return RandRange(rng, 1.0f - jitter, 1.0f + jitter);
                };

            p.trunkHeight *= jitterMul(p.trunkHeightJitter);
            p.trunkRadius *= jitterMul(p.trunkRadiusJitter);
            p.crownRadius *= jitterMul(p.crownRadiusJitter);

            // Crown ellipsoid scale variation
            glm::vec3 crownScale(
                jitterMul(p.crownEllipsoidJitter),
                jitterMul(p.crownEllipsoidJitter),
                jitterMul(p.crownEllipsoidJitter * 0.6f) // slightly less on Z
            );

            TreeMesh mesh;

            //  TRUNK 
            unsigned int trunkFirst = 0, trunkCount = 0;
            AppendCylinder(mesh, p.trunkRadius, p.trunkHeight, p.trunkSegments, p.trunkCaps, trunkFirst, trunkCount);

            mesh.materials.push_back(TreeMesh::MaterialGroup{ trunkFirst, trunkCount, p.trunkColor, "trunk" });

            //  CROWN 
            glm::vec3 crownCenter(0.0f, 0.0f, p.trunkHeight + p.crownRadius * 0.75f);

            unsigned int crownFirst = 0, crownCount = 0;
            AppendSphere(mesh, crownCenter, p.crownRadius, p.crownSegments, crownScale, crownFirst, crownCount);

            // Small spheres inside to create irregular crown
            unsigned int allCrownFirst = crownFirst;
            unsigned int allCrownCount = crownCount;

            for (int i = 0; i < p.crownSmallSpheres; ++i)
            {
                float z = RandRange(rng, -1.0f, 1.0f);
                float a = RandRange(rng, 0.0f, glm::two_pi<float>());
                float rxy = std::sqrt(std::max(0.0f, 1.0f - z * z));
                glm::vec3 dir(rxy * std::cos(a), rxy * std::sin(a), z);

                
                float rRel = RandRange(rng, p.smallSphereRadiusMin, p.smallSphereRadiusMax);
                float r = p.crownRadius * rRel;

                
                float k = RandRange(rng, 0.65f, 1.05f);

                
                dir.z = glm::clamp(dir.z + RandRange(rng, 0.15f, 0.55f), -1.0f, 1.0f);
                dir = glm::normalize(dir);

                
                glm::vec3 smallCenter = crownCenter + dir * (p.crownRadius * k);

                
                glm::vec3 sphScale(
                    jitterMul(p.crownEllipsoidJitter * 0.6f),
                    jitterMul(p.crownEllipsoidJitter * 0.6f),
                    jitterMul(p.crownEllipsoidJitter * 0.4f)
                );

                unsigned int sf = 0, sc = 0;
                AppendSphere(mesh, smallCenter, r, p.crownSegments, sphScale, sf, sc);

                allCrownCount += sc;
            }


            mesh.materials.push_back(TreeMesh::MaterialGroup{ allCrownFirst, allCrownCount, p.crownColor, "crown" });

            // arrays already populated along the way
            return mesh;
        }

    } // namespace Core
} // namespace ScotlandYard
