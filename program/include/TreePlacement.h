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

        using PathSegment = std::pair<glm::vec2, glm::vec2>;

        class TreePlacement
        {
        public:
            static std::vector<TreeInstance> GenerateInPark(
                const ScotlandYard::MapGen::Park& park,
                int targetCount,
                float minDistance,
                unsigned int seed,
                const std::vector<PathSegment>& obstacles = {}
            );

        private:
            static float RandRange(std::mt19937& rng, float a, float b);
            static float DistToSegmentSq(glm::vec2 p, glm::vec2 a, glm::vec2 b);
        };

    } // namespace Core
} // namespace ScotlandYard
