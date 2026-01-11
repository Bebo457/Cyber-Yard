#include "TreePlacement.h"
#include <cmath>

namespace ScotlandYard {
    namespace Core {

        float TreePlacement::RandRange(std::mt19937& rng, float a, float b)
        {
            std::uniform_real_distribution<float> dist(a, b);
            return dist(rng);
        }

        float TreePlacement::DistToSegmentSq(glm::vec2 p, glm::vec2 a, glm::vec2 b) {
            glm::vec2 ab = b - a;
            glm::vec2 ap = p - a;
            float t = glm::dot(ap, ab) / glm::dot(ab, ab);
            t = std::max(0.0f, std::min(1.0f, t));
            glm::vec2 closest = a + ab * t;
            glm::vec2 d = p - closest;
            return glm::dot(d, d);
        }

        std::vector<TreeInstance> TreePlacement::GenerateInPark(
            const ScotlandYard::MapGen::Park& park,
            int targetCount,
            float minDistance,
            unsigned int seed,
            const std::vector<PathSegment>& obstacles
        )
        {
            std::vector<TreeInstance> out;
            if (targetCount <= 0) return out;

            std::mt19937 rng(seed);

            // Bounding box around park polygon
            float minX = park.vec_Vertices[0].x, maxX = park.vec_Vertices[0].x;
            float minY = park.vec_Vertices[0].y, maxY = park.vec_Vertices[0].y;
            for (const auto& v : park.vec_Vertices) {
                minX = std::min(minX, v.x); maxX = std::max(maxX, v.x);
                minY = std::min(minY, v.y); maxY = std::max(maxY, v.y);
            }

            const int maxAttempts = targetCount * 100;
            const float minPathDistSq = 3.5f * 3.5f;

            for (int attempt = 0; attempt < maxAttempts && (int)out.size() < targetCount; ++attempt)
            {
                float x = RandRange(rng, minX, maxX);
                float y = RandRange(rng, minY, maxY);

                if (!park.ContainsPoint(x, y)) continue;

                // Keep away from borders
                float dx = x - park.center.x;
                float dy = y - park.center.y;
                float dCenter = std::sqrt(dx * dx + dy * dy);
                if (dCenter > park.f_BaseRadius * 0.95f) continue;

                // SPRAWDZANIE KOLIZJI ZE ŒCIE¯KAMI
                bool hitsPath = false;
                glm::vec2 p(x, y);
                for (const auto& seg : obstacles) {
                    if (DistToSegmentSq(p, seg.first, seg.second) < minPathDistSq) {
                        hitsPath = true;
                        break;
                    }
                }
                if (hitsPath) continue;

                // Sprawdzanie kolizji z innymi drzewami (bez zmian)
                bool ok = true;
                for (const auto& t : out)
                {
                    float ddx = x - t.position.x;
                    float ddy = y - t.position.y;
                    if ((ddx * ddx + ddy * ddy) < (minDistance * minDistance)) {
                        ok = false; break;
                    }
                }
                if (!ok) continue;

                TreeInstance inst;
                inst.position = glm::vec3(x, y, 0.0f);
                inst.scale = RandRange(rng, 0.85f, 1.25f);
                inst.seed = rng();
                out.push_back(inst);
            }

            return out;
        }

    } // namespace Core
} // namespace ScotlandYard
