#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <random>

namespace ScotlandYard {
    namespace Core {

        struct TreeMesh
        {
            std::vector<glm::vec3> vertices;
            std::vector<unsigned int> indices;
            std::vector<glm::vec3> normals;
            std::vector<glm::vec2> texCoords;

            struct MaterialGroup {
                unsigned int firstIndex = 0;
                unsigned int indexCount = 0;
                glm::vec3 color = glm::vec3(1.0f);
                std::string name;
            };

            std::vector<MaterialGroup> materials;
        };

        struct TreeParams
        {
            // Trunk
            float trunkHeight = 2.5f;
            float trunkRadius = 0.18f;
            int   trunkSegments = 16;
            bool  trunkCaps = true;

            // Crown
            float crownRadius = 1.1f;
            int   crownSegments = 18;      
            int   crownSmallSpheres = 4;   // smaller ones
            float smallSphereRadiusMin = 0.25f; // relative to crownRadius
            float smallSphereRadiusMax = 0.45f; 
            float smallSphereOffset = 0.55f;

            // Random ranges
            float trunkHeightJitter = 0.25f;
            float trunkRadiusJitter = 0.20f;
            float crownRadiusJitter = 0.25f;

            // Crown shape variation
            float crownEllipsoidJitter = 0.25f;

            // Materials
            glm::vec3 trunkColor = glm::vec3(0.42f, 0.28f, 0.16f);
            glm::vec3 crownColor = glm::vec3(0.16f, 0.55f, 0.22f);
        };

        class TreeGenerator
        {
        public:
            // Deterministic with seed
            static TreeMesh GenerateTree(const TreeParams& baseParams, unsigned int seed = 0);

        private:
            static float RandRange(std::mt19937& rng, float a, float b);

            static void AppendCylinder(
                TreeMesh& mesh,
                float radius,
                float height,
                int segments,
                bool caps,
                unsigned int& outFirstIndex,
                unsigned int& outIndexCount
            );

            static void AppendSphere(
                TreeMesh& mesh,
                const glm::vec3& center,
                float radius,
                int segments,
                const glm::vec3& nonUniformScale, // (sx, sy, sz)
                unsigned int& outFirstIndex,
                unsigned int& outIndexCount
            );

            static void AppendTriangle(TreeMesh& mesh, unsigned int a, unsigned int b, unsigned int c);

            static void EnsureArrays(TreeMesh& mesh);
        };

    } // namespace Core
} // namespace ScotlandYard
