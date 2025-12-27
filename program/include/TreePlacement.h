#pragma once
#include <vector>
#include <random>
#include <glm/glm.hpp>
#include "MapGenerator.h"

namespace ScotlandYard {
    namespace Core {

        struct TreeInstance
        {
            glm::vec3 position;   // world/map coordinates
            float scale = 1.0f;
            unsigned int seed = 0; // per-tree seed for TreeGenerator
        };

        class TreePlacement
        {
        public:
            // density trees per ~
            static std::vector<TreeInstance> GenerateInPark(
                const ScotlandYard::MapGen::Park& park,
                int targetCount,
                float minDistance,
                unsigned int seed
            );

        private:
            static float RandRange(std::mt19937& rng, float a, float b);
        };

    } // namespace Core
} // namespace ScotlandYard
