#include "MapGenerator.h"
#include <iostream>
#include <algorithm>

// Generator based on article:
// https://dl.acm.org/doi/abs/10.1145/383259.383292

namespace CityGen {

MapGenerator::MapGenerator(int width, int height)
    : m_Width(width)
    , m_Height(height)
{
    std::cout << "[MapGenerator] Created for map size: " 
              << width << "x" << height << std::endl;
    
    // Inicjalizacja mapy gęstości (wypełnij zerami)
    m_PopulationDensity.resize(height);
    for (int y = 0; y < height; ++y) {
        m_PopulationDensity[y].resize(width, 0.0f);
    }
}

MapGenerator::~MapGenerator() {
    std::cout << "[MapGenerator] Destroyed" << std::endl;
}

void MapGenerator::Generate() {
    std::cout << "[MapGenerator] Starting generation..." << std::endl;
    
    m_Roads.clear();
    
    // NOWE: Najpierw wygeneruj mapę gęstości
    GeneratePopulationDensity();
    
    // Generuj highways
    GenerateHighways();
    
    // TODO: Street generation (później)
    // GenerateStreets();
    
    std::cout << "[MapGenerator] Generation complete" << std::endl;
}

void MapGenerator::GeneratePopulationDensity() {
    std::cout << "[MapGenerator] Generating population density map..." << std::endl;
    
    // Wyczyść mapę
    for (int y = 0; y < m_Height; ++y) {
        for (int x = 0; x < m_Width; ++x) {
            m_PopulationDensity[y][x] = 0.0f;
        }
    }
    
    // Definiujemy 4 centra populacji (koła)
    struct PopulationCenter {
        float x, y;      // Pozycja środka
        float radius;    // Promień wpływu
        float intensity; // Maksymalna gęstość w centrum
    };
    
    std::vector<PopulationCenter> centers = {
        {m_Width * 0.30f, m_Height * 0.45f, 200.0f, 1.0f},  // Lewy górny
        {m_Width * 0.65f, m_Height * 0.3f, 300.0f, 0.9f},  // Prawy górny
        {m_Width * 0.35f, m_Height * 0.65f, 100.0f, 0.85f}, // Lewy dolny
        {m_Width * 0.6f, m_Height * 0.65f, 250.0f, 0.95f}  // Prawy dolny
    };
    
    // Dla każdego piksela, oblicz wpływ wszystkich centrów
    for (int y = 0; y < m_Height; ++y) {
        for (int x = 0; x < m_Width; ++x) {
            float totalDensity = 0.0f;
            
            // Sumuj wpływ każdego centrum
            for (const auto& center : centers) {
                // Odległość od centrum
                float dx = x - center.x;
                float dy = y - center.y;
                float distance = std::sqrt(dx * dx + dy * dy);
                
                // Gęstość maleje z odległością (falloff)
                if (distance < center.radius) {
                    // Normalizowana odległość (0.0 w centrum, 1.0 na brzegu)
                    float normalizedDist = distance / center.radius;
                    
                    // Smooth falloff (1.0 w centrum → 0.0 na brzegu)
                    float falloff = 1.0f - normalizedDist;
                    falloff = falloff * falloff; // Kwadratowy falloff dla gładszego przejścia
                    
                    // Dodaj do całkowitej gęstości
                    totalDensity += center.intensity * falloff;
                }
            }
            
            // Ogranicz do zakresu [0.0, 1.0]
            m_PopulationDensity[y][x] = std::min(1.0f, totalDensity);
        }
    }
    
    std::cout << "[MapGenerator] Population density map generated with " 
              << centers.size() << " centers" << std::endl;
}

float MapGenerator::GetDensityAt(int x, int y) const {
    if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) {
        return 0.0f;
    }
    return m_PopulationDensity[y][x];
}

void MapGenerator::GenerateHighways() {
    std::cout << "[MapGen] ====== HIGHWAY GENERATION START ======" << std::endl;
    std::cout << "[MapGen] Configuration:" << std::endl;
    std::cout << "[MapGen]   - Map size: " << m_Width << "x" << m_Height << std::endl;
    std::cout << "[MapGen]   - Segment length: " << HIGHWAY_SEGMENT_LENGTH << std::endl;
    std::cout << "[MapGen]   - Max iterations per highway: " << HIGHWAY_MAX_ITERATIONS << std::endl;
    std::cout << "[MapGen]   - Branch angle: " << (BRANCH_ANGLE * 180.0f / 3.14159f) << " degrees" << std::endl;
    std::cout << "[MapGen]   - FOV: " << (HIGHWAY_FOV * 180.0f / 3.14159f) << " degrees" << std::endl;
    std::cout << "[MapGen] ========================================" << std::endl;
    
    Point startPos;
    float maxDensity = 0.0f;
    
    for (int y = 0; y < m_Height; y += 10) {
        for (int x = 0; x < m_Width; x += 10) {
            float density = GetDensityAt(x, y);
            if (density > maxDensity) {
                maxDensity = density;
                startPos = Point(static_cast<float>(x), static_cast<float>(y));
            }
        }
    }
    
    std::cout << "[Highway] Starting at: (" << startPos.x << ", " << startPos.y 
              << ") with density: " << maxDensity << std::endl;
    
    int startNodeIdx = CreateOrGetNode(startPos, false);
    
    float initialAngle = std::atan2(
        m_Height / 2.0f - startPos.y,
        m_Width / 2.0f - startPos.x
    );
    Point initialDirection(std::cos(initialAngle), std::sin(initialAngle));
    
    std::cout << "[Highway] Initial direction angle: " 
              << (initialAngle * 180.0f / 3.14159f) << " degrees" << std::endl;
    
    HighwayEnd firstEnd(startNodeIdx, initialDirection, HIGHWAY_MAX_ITERATIONS);
    m_ActiveEnds.push_back(firstEnd);
    
    int iteration = 0;
    int totalSegmentsGrown = 0;
    int totalBranchesCreated = 0;
    int totalBranchesActivated = 0;
    
    while (!m_ActiveEnds.empty() || !m_SleepingBranches.empty()) {
        
        std::cout << "\n[Iteration " << iteration << "]" << std::endl;
        std::cout << "  Active ends: " << m_ActiveEnds.size() 
                  << ", Sleeping branches: " << m_SleepingBranches.size() 
                  << ", Total nodes: " << m_RoadNodes.size()
                  << ", Total roads: " << m_Roads.size() << std::endl;
        
        int segmentsGrownThisIteration = 0;
        int endsStoppedThisIteration = 0;
        
        for (int i = m_ActiveEnds.size() - 1; i >= 0; --i) {
            HighwayEnd& current = m_ActiveEnds[i];
            
            std::cout << "  [End " << i << "] Node: " << current.currentNodeIdx 
                      << ", Iterations left: " << current.iterationsLeft
                      << ", Distance since branch: " << current.distanceSinceLastBranch << std::endl;
            
            bool canGrow = GrowHighwayOneStep(current);
            
            if (canGrow) {
                segmentsGrownThisIteration++;
                totalSegmentsGrown++;
                std::cout << "    -> Grew successfully" << std::endl;
            } else {
                std::cout << "    -> Cannot grow (stopped)" << std::endl;
            }
            
            if (!canGrow || current.iterationsLeft <= 0) {
                std::cout << "    -> Removing from active ends" << std::endl;
                
                // NOWE: Jeśli highway się kończy (nie trafiając na skrzyżowanie), utwórz Highway
                if (!current.roadsSinceLastIntersection.empty()) {
                    std::cout << "    -> Highway ended without hitting intersection, creating final Highway" << std::endl;
                    CreateHighway(current.lastIntersectionIdx, current.currentNodeIdx, current.roadsSinceLastIntersection);
                }
                
                m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                endsStoppedThisIteration++;
                continue;
            }
            
            size_t branchesBeforeCreate = m_SleepingBranches.size();
            size_t activeEndsBeforeCreate = m_ActiveEnds.size();
            
            CreateBranchCandidates(current);
            
            size_t branchesAfterCreate = m_SleepingBranches.size();
            size_t activeEndsAfterCreate = m_ActiveEnds.size();
            
            int branchesCreatedNow = branchesAfterCreate - branchesBeforeCreate;
            bool newEndCreated = (activeEndsAfterCreate > activeEndsBeforeCreate);
            
            if (branchesCreatedNow > 0) {
                totalBranchesCreated += branchesCreatedNow;
                std::cout << "    -> Created " << branchesCreatedNow << " branch candidates" << std::endl;
                
                current.distanceSinceLastBranch = 0.0f;
                std::cout << "    -> Reset distance counter to 0" << std::endl;
                
                // NOWE: Jeśli utworzono nowy HighwayEnd (kontynuację), usuń aktualny end
                if (newEndCreated) {
                    std::cout << "    -> New continuation HighwayEnd created, removing current end" << std::endl;
                    m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                    continue;  // Pomiń dalsze przetwarzanie tego end'a
                }
            }
        }
        
        std::cout << "  Summary: " << segmentsGrownThisIteration << " segments grown, "
                  << endsStoppedThisIteration << " ends stopped" << std::endl;
        
        std::cout << "  Updating sleeping branches..." << std::endl;
        int branchesActivatedNow = 0;
        
        for (int i = m_SleepingBranches.size() - 1; i >= 0; --i) {
            Branch& branch = m_SleepingBranches[i];
            
            branch.delay--;
            
            if (branch.delay == 0) {
                std::cout << "    -> Activating branch from node " << branch.parentNodeIdx << std::endl;
                
                if (branch.parentNodeIdx >= 0 && branch.parentNodeIdx < (int)m_RoadNodes.size()) {
                    bool wasIntersection = m_RoadNodes[branch.parentNodeIdx].isIntersection;
                    m_RoadNodes[branch.parentNodeIdx].isIntersection = true;
                    
                    // NOWE: Jeśli węzeł nie był wcześniej skrzyżowaniem, podziel highways
                    if (!wasIntersection) {
                        SplitHighwaysAtIntersection(branch.parentNodeIdx);
                    }
                }
                
                HighwayEnd newEnd(
                    branch.parentNodeIdx,
                    branch.direction,
                    branch.iterationsLeft
                );
                
                m_ActiveEnds.push_back(newEnd);
                m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
                branchesActivatedNow++;
                totalBranchesActivated++;
            }
            else if (branch.delay < 0) {
                std::cout << "    -> Removing rejected branch from node " << branch.parentNodeIdx << std::endl;
                m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
            }
        }
        
        if (branchesActivatedNow > 0) {
            std::cout << "  Activated " << branchesActivatedNow << " branches" << std::endl;
        }
        
        iteration++;
        
        if (iteration > 1000) {
            std::cout << "[Highway] Max iterations reached" << std::endl;
            break;
        }
    }
    
    std::cout << "\n[MapGen] ====== HIGHWAY GENERATION COMPLETE ======" << std::endl;
    std::cout << "[MapGen] Statistics:" << std::endl;
    std::cout << "[MapGen]   - Total iterations: " << iteration << std::endl;
    std::cout << "[MapGen]   - Total segments grown: " << totalSegmentsGrown << std::endl;
    std::cout << "[MapGen]   - Total branches created: " << totalBranchesCreated << std::endl;
    std::cout << "[MapGen]   - Total branches activated: " << totalBranchesActivated << std::endl;
    std::cout << "[MapGen]   - Final roads: " << m_Roads.size() << std::endl;
    std::cout << "[MapGen]   - Final nodes: " << m_RoadNodes.size() << std::endl;
    std::cout << "[MapGen] ===========================================" << std::endl;

    // Post-processing: sprawdź wszystkie intersections
    std::cout << "\n[MapGen] ====== POST-PROCESSING INTERSECTIONS ======" << std::endl;
    PostProcessIntersections();
    std::cout << "[MapGen] =========================================" << std::endl;
    
    // Sprawdź redundancję po podziale highways
    std::cout << "\n[MapGen] ====== CHECKING REDUNDANT HIGHWAYS AFTER SPLIT ======" << std::endl;
    int removedCount = 0;
    for (int i = m_Highways.size() - 1; i >= 0; --i) {
        const Highway& hw = m_Highways[i];
        
        // Sprawdź czy istnieje inny highway między tymi samymi węzłami
        for (int j = i - 1; j >= 0; --j) {
            const Highway& other = m_Highways[j];
            
            bool sameDirection = (hw.startIntersectionIdx == other.startIntersectionIdx &&
                                 hw.endIntersectionIdx == other.endIntersectionIdx);
            
            bool oppositeDirection = (hw.startIntersectionIdx == other.endIntersectionIdx &&
                                     hw.endIntersectionIdx == other.startIntersectionIdx);
            
            if (sameDirection || oppositeDirection) {
                std::cout << "[PostProcess] Found duplicate highways between " 
                          << hw.startIntersectionIdx << " and " << hw.endIntersectionIdx << std::endl;
                std::cout << "  Highway " << i << " length: " << hw.totalLength << std::endl;
                std::cout << "  Highway " << j << " length: " << other.totalLength << std::endl;
                
                // Usuń dłuższy
                if (hw.totalLength > other.totalLength) {
                    std::cout << "  Removing highway " << i << " (longer)" << std::endl;
                    RemoveHighway(i);
                } else {
                    std::cout << "  Removing highway " << j << " (longer)" << std::endl;
                    RemoveHighway(j);
                }
                removedCount++;
                break; // Przejdź do następnego highway
            }
        }
    }
    std::cout << "[PostProcess] Removed " << removedCount << " redundant highways" << std::endl;
    std::cout << "[MapGen] =============================================" << std::endl;

    std::cout << "\n[MapGen] ====== FINAL HIGHWAYS LIST ======" << std::endl;
    std::cout << "[MapGen] Total highways: " << m_Highways.size() << std::endl;
    for (size_t i = 0; i < m_Highways.size(); ++i) {
        const Highway& hw = m_Highways[i];
        
        bool startIsIntersection = m_RoadNodes[hw.startIntersectionIdx].isIntersection;
        bool endIsIntersection = m_RoadNodes[hw.endIntersectionIdx].isIntersection;
        
        std::cout << "[Highway " << i << "] " 
                << hw.startIntersectionIdx << " -> " << hw.endIntersectionIdx
                << " | Start=" << (startIsIntersection ? "INTERSECTION" : "REGULAR")
                << " | End=" << (endIsIntersection ? "INTERSECTION" : "REGULAR")
                << " | Roads=" << hw.roadIndices.size()
                << " | Length=" << hw.totalLength << std::endl;
    }
    std::cout << "[MapGen] =================================" << std::endl;
}

float MapGenerator::ShootRay(Point pos, float angle) {
    float totalScore = 0.0f;
    
    // Próbkuj wzdłuż promienia
    for (int i = 1; i <= HIGHWAY_SAMPLES_PER_RAY; ++i) {
        float distance = (HIGHWAY_SAMPLE_RADIUS / HIGHWAY_SAMPLES_PER_RAY) * i;
        
        // Pozycja próbki
        Point samplePos(
            pos.x + std::cos(angle) * distance,
            pos.y + std::sin(angle) * distance
        );
        
        // Pobierz gęstość (GetDensityAt sprawdza granice)
        float density = GetDensityAt(
            static_cast<int>(samplePos.x), 
            static_cast<int>(samplePos.y)
        );
        
        // Waga odwrotna do odległości
        float weight = density / distance;
        
        totalScore += weight;
    }
    
    return totalScore;
}

Point MapGenerator::FindBestDirection(Point pos, Point prevDirection) {
    float bestScore = -1.0f;
    float bestAngle = 0.0f;
    
    const float PI = 3.14159265359f;
    
    // Oblicz kąt poprzedniego kierunku (środek FOV)
    float centerAngle = 0.0f;
    if (prevDirection.x != 0.0f || prevDirection.y != 0.0f) {
        centerAngle = std::atan2(prevDirection.y, prevDirection.x);
    }
    
    // Strzel rayami TYLKO w zakresie FOV wokół poprzedniego kierunku
    for (int i = 0; i < HIGHWAY_NUM_RAYS; ++i) {
        // ZMIENIONE: Kąt od -FOV/2 do +FOV/2 względem centerAngle
        float angleOffset = (HIGHWAY_FOV * i) / (HIGHWAY_NUM_RAYS - 1) - HIGHWAY_FOV / 2.0f;
        float angle = centerAngle + angleOffset;
        
        float score = ShootRay(pos, angle);
        
        if (score > bestScore) {
            bestScore = score;
            bestAngle = angle;
        }
    }
    
    // Jeśli score jest zbyt niski, zatrzymaj się
    const float MIN_SCORE_THRESHOLD = 0.01f;
    if (bestScore < MIN_SCORE_THRESHOLD) {
        return Point(0.0f, 0.0f);
    }
    
    return Point(std::cos(bestAngle), std::sin(bestAngle));
}

int MapGenerator::CreateOrGetNode(const Point& pos, bool isIntersection) {
    // Sprawdź czy już istnieje bardzo bliski punkt (w promieniu 5px)
    int existingIdx = FindNearestNode(pos, 5.0f);
    
    if (existingIdx != -1) {
        // Znaleziono istniejący punkt - ewentualnie uaktualnij flagę
        if (isIntersection) {
            m_RoadNodes[existingIdx].isIntersection = true;
        }
        return existingIdx;
    }
    
    // Nie znaleziono - utwórz nowy punkt
    Point newNode = pos;
    newNode.isIntersection = isIntersection;
    m_RoadNodes.push_back(newNode);
    
    return m_RoadNodes.size() - 1;
}

int MapGenerator::FindNearestNode(const Point& pos, float maxDist, bool intersectionsOnly) {
    int nearestIdx = -1;
    float minDistSq = maxDist * maxDist;
    
    for (size_t i = 0; i < m_RoadNodes.size(); ++i) {
        if (intersectionsOnly && !m_RoadNodes[i].isIntersection) {
            continue;
        }
        
        float dx = pos.x - m_RoadNodes[i].x;
        float dy = pos.y - m_RoadNodes[i].y;
        float distSq = dx*dx + dy*dy;
        
        if (distSq < minDistSq) {
            minDistSq = distSq;
            nearestIdx = static_cast<int>(i);
        }
    }
    
    return nearestIdx;
}

std::vector<int> MapGenerator::FindNearbyRoadIndices(const Point& pos, float radius) {
    std::vector<int> nearbyIndices;
    float radiusSq = radius * radius;
    
    for (size_t i = 0; i < m_Roads.size(); ++i) {
        const Road& road = m_Roads[i];
        const Point& start = m_RoadNodes[road.startNodeIdx];
        const Point& end = m_RoadNodes[road.endNodeIdx];
        
        // Sprawdź odległość od obu końców drogi
        float dx1 = pos.x - start.x;
        float dy1 = pos.y - start.y;
        float dist1Sq = dx1*dx1 + dy1*dy1;
        
        float dx2 = pos.x - end.x;
        float dy2 = pos.y - end.y;
        float dist2Sq = dx2*dx2 + dy2*dy2;
        
        if (dist1Sq < radiusSq || dist2Sq < radiusSq) {
            nearbyIndices.push_back(i);
        }
    }
    
    return nearbyIndices;
}

bool MapGenerator::DoSegmentsIntersect(const Point& a1, const Point& a2,
                                       const Point& b1, const Point& b2,
                                       Point& intersection) {
    // Algorytm przecięcia dwóch odcinków (line segment intersection)
    float x1 = a1.x, y1 = a1.y;
    float x2 = a2.x, y2 = a2.y;
    float x3 = b1.x, y3 = b1.y;
    float x4 = b2.x, y4 = b2.y;
    
    float denom = (x1-x2)*(y3-y4) - (y1-y2)*(x3-x4);
    
    // Linie równoległe lub pokrywające się
    if (std::abs(denom) < 1e-6f) {
        return false;
    }
    
    float t = ((x1-x3)*(y3-y4) - (y1-y3)*(x3-x4)) / denom;
    float u = -((x1-x2)*(y1-y3) - (y1-y2)*(x1-x3)) / denom;
    
    // Przecięcie musi być wewnątrz obu odcinków (0 <= t,u <= 1)
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        intersection.x = x1 + t * (x2 - x1);
        intersection.y = y1 + t * (y2 - y1);
        return true;
    }
    
    return false;
}

bool MapGenerator::CheckParallelDuringGrowth(const Point& start, const Point& end, int startNodeIdx) {
    const float PARALLEL_CHECK_RADIUS = PARALLEL_GROWTH_CHECK_RADIUS;
    const float PARALLEL_ANGLE_THRESHOLD = PARALLEL_GROWTH_ANGLE_THRESHOLD;
    
    
    // Kierunek nowego segmentu
    Point newDir;
    newDir.x = end.x - start.x;
    newDir.y = end.y - start.y;
    float newLen = std::sqrt(newDir.x * newDir.x + newDir.y * newDir.y);
    if (newLen < 0.01f) {
        // std::cout << "      [ParallelCheck] Segment too short, skipping" << std::endl;
        return true;
    }
    newDir.x /= newLen;
    newDir.y /= newLen;
    
    // Środek nowego segmentu
    Point newSegmentMid;
    newSegmentMid.x = (start.x + end.x) / 2.0f;
    newSegmentMid.y = (start.y + end.y) / 2.0f;
    
    // Znajdź pobliskie drogi
    std::vector<int> nearbyRoads = FindNearbyRoadIndices(newSegmentMid, PARALLEL_CHECK_RADIUS);
    
    // std::cout << "      [ParallelCheck] Found " << nearbyRoads.size() << " nearby roads" << std::endl;
    
    for (int roadIdx : nearbyRoads) {
        if (roadIdx < 0 || roadIdx >= (int)m_Roads.size())
            continue;
            
        const Road& otherRoad = m_Roads[roadIdx];
        
        // Ignoruj drogi łączące się z naszym węzłem startowym
        if (otherRoad.startNodeIdx == startNodeIdx || otherRoad.endNodeIdx == startNodeIdx)
            continue;
        
        const Point& roadStart = m_RoadNodes[otherRoad.startNodeIdx];
        const Point& roadEnd = m_RoadNodes[otherRoad.endNodeIdx];
        
        // Oblicz odległość od środka naszego segmentu do tej drogi
        Point closest;
        float distToRoad = DistancePointToSegment(newSegmentMid, roadStart, roadEnd, closest);
        
        if (distToRoad > PARALLEL_CHECK_RADIUS) {
            continue; // Za daleko
        }
        
        // Kierunek istniejącej drogi
        Point roadDir;
        roadDir.x = roadEnd.x - roadStart.x;
        roadDir.y = roadEnd.y - roadStart.y;
        float roadLen = std::sqrt(roadDir.x * roadDir.x + roadDir.y * roadDir.y);
        if (roadLen < 0.01f) continue;
        roadDir.x /= roadLen;
        roadDir.y /= roadLen;
        
        // Kąt między segmentami
        float dotProduct = newDir.x * roadDir.x + newDir.y * roadDir.y;
        dotProduct = std::max(-1.0f, std::min(1.0f, dotProduct));
        float angle = std::acos(dotProduct);
        
        // Uwzględnij symetrię (180° to też równoległość)
        if (angle > 3.14159f / 2.0f) {
            angle = 3.14159f - angle;
        }
        
        // std::cout << "      [ParallelCheck] Road " << roadIdx 
        //           << ": distance=" << distToRoad 
        //           << ", angle=" << (angle * 180.0f / 3.14159f) << "°" << std::endl;
        
        if (angle < PARALLEL_ANGLE_THRESHOLD) {
            // std::cout << "      [ParallelCheck] ✗ REJECTED: Too parallel to existing road (angle < " 
            //           << (PARALLEL_ANGLE_THRESHOLD * 180.0f / 3.14159f) << "°)" << std::endl;
            return false;
        }
    }
    
    // std::cout << "      [ParallelCheck] ✓ PASSED: Not parallel to any nearby road" << std::endl;
    return true;
}

bool MapGenerator::CheckLocalConstraints(const Point& start, const Point& end, int startNodeIdx, int& endNodeIdx)
{
    const float SEARCH_RADIUS = HIGHWAY_SEGMENT_LENGTH * 1.5f;
    const float HIT_DISTANCE  = HIGHWAY_SEGMENT_LENGTH * 0.5f;

    std::cout << "    [LocalConstraints] Checking segment from node " << startNodeIdx << std::endl;

    // ===== NOWY CHECK: Równoległość podczas wzrostu =====
    if (!CheckParallelDuringGrowth(start, end, startNodeIdx)) {
        std::cout << "    [LocalConstraints] ✗ FAILED parallel check" << std::endl;
        return false;
    }

    std::vector<int> nearbyRoadIndices = FindNearbyRoadIndices(start, SEARCH_RADIUS);

    for (int roadIdx : nearbyRoadIndices) {

        if (roadIdx < 0 || roadIdx >= (int)m_Roads.size())
            continue;

        const Road& otherRoad = m_Roads[roadIdx];

        // Ignoruj własne drogi
        if (otherRoad.startNodeIdx == startNodeIdx ||
            otherRoad.endNodeIdx   == startNodeIdx)
            continue;

        const Point& segA = m_RoadNodes[otherRoad.startNodeIdx];
        const Point& segB = m_RoadNodes[otherRoad.endNodeIdx];

        /* =========================================================
           1. KLASYCZNE PRZECIĘCIE SEGMENT–SEGMENT
        ========================================================= */
        Point intersection;
        if (DoSegmentsIntersect(start, end, segA, segB, intersection)) {
            std::cout << "    [LocalConstraints] Found intersection at (" 
                      << intersection.x << "," << intersection.y << ")" << std::endl;

            // Sprawdź czy węzeł nie istniał wcześniej
            int existingNode = FindNearestNode(intersection, 2.0f);
            bool wasIntersection = (existingNode != -1) ? m_RoadNodes[existingNode].isIntersection : false;
            
            int nodeIdx = CreateOrGetNode(intersection, true);
            endNodeIdx = nodeIdx;

            m_RoadNodes[nodeIdx].connectedRoadIndices.push_back(roadIdx);
            
            // Jeśli właśnie stał się skrzyżowaniem, podziel highways
            if (!wasIntersection) {
                SplitHighwaysAtIntersection(nodeIdx);
            }
            
            return true;
        }

        /* =========================================================
           2. "WPADNIĘCIE" W ŚRODEK DROGI (distance check)
        ========================================================= */
        Point closest;
        float dist = DistancePointToSegment(end, segA, segB, closest);

        if (dist < HIT_DISTANCE) {
            std::cout << "    [LocalConstraints] Hit road at distance " << dist << std::endl;

            // Sprawdź czy węzeł nie istniał wcześniej
            int existingNode = FindNearestNode(closest, 2.0f);
            bool wasIntersection = (existingNode != -1) ? m_RoadNodes[existingNode].isIntersection : false;
            
            int nodeIdx = CreateOrGetNode(closest, true);
            endNodeIdx = nodeIdx;

            m_RoadNodes[nodeIdx].connectedRoadIndices.push_back(roadIdx);
            
            // Jeśli właśnie stał się skrzyżowaniem, podziel highways
            if (!wasIntersection) {
                SplitHighwaysAtIntersection(nodeIdx);
            }
            
            return true;
        }
    }

    /* =========================================================
       3. SNAP DO ISTNIEJĄCEGO WĘZŁA (T-junction)
    ========================================================= */
    int nearbyNode = FindNearestNode(end, HIGHWAY_SEGMENT_LENGTH * 0.5f);
    if (nearbyNode != -1 && nearbyNode != startNodeIdx) {
        std::cout << "    [LocalConstraints] Snapping to existing node " << nearbyNode << std::endl;
        
        // Sprawdź czy węzeł nie był wcześniej skrzyżowaniem
        bool wasIntersection = m_RoadNodes[nearbyNode].isIntersection;
        m_RoadNodes[nearbyNode].isIntersection = true;
        
        // Jeśli właśnie stał się skrzyżowaniem, podziel highways
        if (!wasIntersection) {
            SplitHighwaysAtIntersection(nearbyNode);
        }
        
        endNodeIdx = nearbyNode;
        return true;
    }

    /* =========================================================
       4. BRAK KOLIZJI → ZWYKŁY NOWY WĘZEŁ
    ========================================================= */
    std::cout << "    [LocalConstraints] ✓ PASSED: Creating new node" << std::endl;
    endNodeIdx = CreateOrGetNode(end, false);
    return true;
}


void MapGenerator::GlobalGoalsForBranch(Branch& branch, const HighwayEnd& parent) {
    // ========== FLAGI AKTYWACJI KRYTERIÓW ==========
    const bool ENABLE_DENSITY_CHECK = false;
    const bool ENABLE_BOUNDS_CHECK = false;
    const bool ENABLE_PARALLEL_CHECK = true;
    const bool ENABLE_AREA_COVERAGE_CHECK = false;
    const bool ENABLE_HIGHWAY_PROXIMITY_CHECK = true;
    
    // ========== PARAMETRY DO DOSTRAJANIA ==========
    const float MIN_DENSITY_SCORE = BRANCH_MIN_DENSITY_SCORE;
    const float PARALLEL_ANGLE_DEG = BRANCH_PARALLEL_ANGLE_THRESHOLD;
    const float PARALLEL_RADIUS = BRANCH_PARALLEL_SEARCH_RADIUS;
    const float AREA_RADIUS = BRANCH_AREA_SEARCH_RADIUS;
    const int MAX_ROADS_IN_AREA = BRANCH_AREA_MAX_ROADS;
    
    std::cout << "[GlobalGoals] Evaluating branch from node " << branch.parentNodeIdx << std::endl;
    
    // KRYTERIUM 1: Gęstość w kierunku branch
    if (ENABLE_DENSITY_CHECK) {
        Point parentPos = m_RoadNodes[branch.parentNodeIdx];
        float angle = std::atan2(branch.direction.y, branch.direction.x);
        float score = ShootRay(parentPos, angle);
        
        std::cout << "[GlobalGoals] Density score: " << score << std::endl;
        
        if (score < MIN_DENSITY_SCORE) {
            std::cout << "[GlobalGoals] REJECTED: Low density" << std::endl;
            branch.delay = -1;
            return;
        }
    }
    
    // KRYTERIUM 2: Branch prowadzi poza mapę
    if (ENABLE_BOUNDS_CHECK) {
        Point parentPos = m_RoadNodes[branch.parentNodeIdx];
        Point targetPos(
            parentPos.x + branch.direction.x * BRANCH_LOOKAHEAD_DISTANCE,
            parentPos.y + branch.direction.y * BRANCH_LOOKAHEAD_DISTANCE
        );
        
        if (targetPos.x < 0 || targetPos.x >= m_Width || 
            targetPos.y < 0 || targetPos.y >= m_Height) {
            std::cout << "[GlobalGoals] REJECTED: Out of bounds" << std::endl;
            branch.delay = -1;
            return;
        }
    }
    
    // KRYTERIUM 3: Branch zbyt blisko równoległej drogi
    if (ENABLE_PARALLEL_CHECK) {
        Point parentPos = m_RoadNodes[branch.parentNodeIdx];
        std::vector<int> nearbyRoads = FindNearbyRoadIndices(parentPos, PARALLEL_RADIUS);
        
        for (int roadIdx : nearbyRoads) {
            const Road& road = m_Roads[roadIdx];
            const Point& roadStart = m_RoadNodes[road.startNodeIdx];
            const Point& roadEnd = m_RoadNodes[road.endNodeIdx];
            
            // Kierunek drogi
            Point roadDir(roadEnd.x - roadStart.x, roadEnd.y - roadStart.y);
            float roadLen = std::sqrt(roadDir.x * roadDir.x + roadDir.y * roadDir.y);
            if (roadLen > 0.01f) {
                roadDir.x /= roadLen;
                roadDir.y /= roadLen;
            }
            
            // Kąt między branch a drogą
            float dotProduct = branch.direction.x * roadDir.x + branch.direction.y * roadDir.y;
            float angle = std::acos(std::max(-1.0f, std::min(1.0f, dotProduct)));
            
            // Uwzględnij symetrię (180° to też równoległość)
            if (angle > 3.14159f / 2.0f) {
                angle = 3.14159f - angle;
            }
            
            if (angle < PARALLEL_ANGLE_DEG) {
                std::cout << "[GlobalGoals] REJECTED: Parallel to existing road" << std::endl;
                branch.delay = -1;
                return;
            }
        }
    }
    
    // KRYTERIUM 4: Obszar już dobrze obsłużony
    if (ENABLE_AREA_COVERAGE_CHECK) {
        Point parentPos = m_RoadNodes[branch.parentNodeIdx];
        Point targetPos(
            parentPos.x + branch.direction.x * AREA_RADIUS,
            parentPos.y + branch.direction.y * AREA_RADIUS
        );
        
        std::vector<int> nearbyRoads = FindNearbyRoadIndices(targetPos, AREA_RADIUS);
        
        if ((int)nearbyRoads.size() >= MAX_ROADS_IN_AREA) {
            std::cout << "[GlobalGoals] REJECTED: Area already covered" << std::endl;
            branch.delay = -1;
            return;
        }
    }
    
    // KRYTERIUM 5: Branch trafiłby zbyt szybko na istniejący highway
    if (ENABLE_HIGHWAY_PROXIMITY_CHECK) {
        Point parentPos = m_RoadNodes[branch.parentNodeIdx];
        
        float minHitDistance = RAY_CAST_DISTANCE + 1.0f;
        
        // Strzel kilka rayów w kierunku brancha (z małym spreadem)
        for (int i = 0; i < HIGHWAY_PROXIMITY_NUM_RAYS; ++i) {
            // Oblicz kąt dla tego raya
            float spreadFraction = (float)i / (HIGHWAY_PROXIMITY_NUM_RAYS - 1);  // 0.0 do 1.0
            float angleOffset = (spreadFraction - 0.5f) * RAY_SPREAD_ANGLE;  // -spread/2 do +spread/2
            
            float baseAngle = std::atan2(branch.direction.y, branch.direction.x);
            float rayAngle = baseAngle + angleOffset;
            
            Point rayDir(std::cos(rayAngle), std::sin(rayAngle));
            
            // Strzel rayem
            float hitDist = RaycastToHighway(parentPos, rayDir, RAY_CAST_DISTANCE);
            
            if (hitDist > 0.0f && hitDist < minHitDistance) {
                minHitDistance = hitDist;
            }
        }
        
        // Jeśli którykolwiek ray trafił bliżej niż MIN_VIABLE_HIGHWAY_LENGTH
        if (minHitDistance < MIN_VIABLE_HIGHWAY_LENGTH) {
            std::cout << "[GlobalGoals] REJECTED: Would create too short highway (hit at " 
                    << minHitDistance << " < " << MIN_VIABLE_HIGHWAY_LENGTH << ")" << std::endl;
            branch.delay = -1;
            return;
        }
    }

    // WSZYSTKIE KRYTERIA SPEŁNIONE - ustaw losowy delay
    branch.delay = BRANCH_DELAY_MIN + (rand() % (BRANCH_DELAY_MAX - BRANCH_DELAY_MIN + 1));
    std::cout << "[GlobalGoals] ACCEPTED: delay = " << branch.delay << std::endl;
}

bool MapGenerator::GrowHighwayOneStep(HighwayEnd& hw) {
    Point currentPos = m_RoadNodes[hw.currentNodeIdx];
    
    // Znajdź najlepszy kierunek
    Point newDirection = FindBestDirection(currentPos, hw.direction);
    
    if (newDirection.x == 0.0f && newDirection.y == 0.0f) {
        return false;
    }
    
    // Oblicz nową pozycję
    Point newPos(
        currentPos.x + newDirection.x * HIGHWAY_SEGMENT_LENGTH,
        currentPos.y + newDirection.y * HIGHWAY_SEGMENT_LENGTH
    );
    
    // Sprawdź granice
    if (newPos.x < 0 || newPos.x >= m_Width || 
        newPos.y < 0 || newPos.y >= m_Height) {
        return false;
    }
    
    // Local constraints
    int newNodeIdx;
    if (!CheckLocalConstraints(currentPos, newPos, hw.currentNodeIdx, newNodeIdx)) {
        return false;
    }
    bool wasIntersection = m_RoadNodes[newNodeIdx].isIntersection;
    // Sprawdź czy trafiło na skrzyżowanie (zakończ wzrost)
    if (m_RoadNodes[newNodeIdx].isIntersection && newNodeIdx != hw.currentNodeIdx) {
        std::cout << "[GrowOneStep] Hit intersection at node " << newNodeIdx << std::endl;
        
        // Dodaj ostatni segment do skrzyżowania
        m_Roads.push_back(Road(hw.currentNodeIdx, newNodeIdx));
        int lastRoadIdx = m_Roads.size() - 1;
        m_RoadNodes[hw.currentNodeIdx].connectedRoadIndices.push_back(lastRoadIdx);
        
        if (!wasIntersection) {
            SplitHighwaysAtIntersection(newNodeIdx);
        }
        
        // NOWE: Dodaj ostatni Road do listy
        hw.roadsSinceLastIntersection.push_back(lastRoadIdx);
        
        // NOWE: Utwórz Highway od ostatniego skrzyżowania do tego
        std::cout << "[GrowOneStep] Creating Highway from intersection " 
                << hw.lastIntersectionIdx << " to " << newNodeIdx << std::endl;
        std::cout << "[GrowOneStep] Highway contains " 
                << hw.roadsSinceLastIntersection.size() << " roads" << std::endl;
        
        CreateHighway(hw.lastIntersectionIdx, newNodeIdx, hw.roadsSinceLastIntersection);
        hw.roadsSinceLastIntersection.clear();
        
        return false;
    }
    
    // Dodaj drogę
    m_Roads.push_back(Road(hw.currentNodeIdx, newNodeIdx));
    int newRoadIdx = m_Roads.size() - 1;
    m_RoadNodes[hw.currentNodeIdx].connectedRoadIndices.push_back(newRoadIdx);

    // NOWE: Dodaj do śledzenia tras
    hw.roadsSinceLastIntersection.push_back(newRoadIdx);
    
    // Oblicz długość segmentu
    float segmentLength = std::sqrt(
        (newPos.x - currentPos.x) * (newPos.x - currentPos.x) +
        (newPos.y - currentPos.y) * (newPos.y - currentPos.y)
    );
    
    // Aktualizuj stan
    hw.currentNodeIdx = newNodeIdx;
    hw.direction = newDirection;
    hw.distanceSinceLastBranch += segmentLength;  // DODAJ dystans
    hw.iterationsLeft--;
    
    // *** USUNIĘTO: hw.distanceSinceLastBranch = 0.0f; ***
    // NOWE: Jeśli trafiło na zwykłe skrzyżowanie (nie końcowe), zresetuj licznik
    // NOWE: JeÅ›li trafiÅ‚o na zwykÅ‚e skrzyÅ¼owanie (nie koÅ„cowe), zresetuj licznik
    if (m_RoadNodes[newNodeIdx].isIntersection) {
        std::cout << "[GrowOneStep] Passed through intersection " << newNodeIdx << std::endl;
        
        // Jeśli węzeł nie był wcześniej skrzyżowaniem, podziel highways
        if (!wasIntersection) {
            SplitHighwaysAtIntersection(newNodeIdx);
        }
        
        // UtwÃ³rz Highway dla dotychczasowego odcinka
        if (!hw.roadsSinceLastIntersection.empty()) {
            CreateHighway(hw.lastIntersectionIdx, newNodeIdx, hw.roadsSinceLastIntersection);
        }
        
        // Zresetuj tracking
        hw.lastIntersectionIdx = newNodeIdx;
        hw.roadsSinceLastIntersection.clear();
    }
    return true;
}

void MapGenerator::CreateBranchCandidates(const HighwayEnd& parent) {
    const bool USE_DISTANCE_TRIGGER = true;
    const bool USE_PROBABILITY_TRIGGER = false;
    
    const float MIN_BRANCH_DISTANCE = 100.0f;
    const float BRANCH_PROBABILITY = 0.70f;
    
    bool shouldCreate = true;
    
    if (USE_DISTANCE_TRIGGER) {
        if (parent.distanceSinceLastBranch < MIN_BRANCH_DISTANCE) {
            shouldCreate = false;
        }
    }
    
    if (USE_PROBABILITY_TRIGGER && shouldCreate) {
        int roll = rand() % 100;
        int threshold = (int)(BRANCH_PROBABILITY * 100);
        if (roll >= threshold) {
            shouldCreate = false;
        }
    }
    
    if (!shouldCreate) {
        return;
    }
    
    float baseAngle = std::atan2(parent.direction.y, parent.direction.x);
    
    float angles[2] = {
        baseAngle + BRANCH_ANGLE,
        baseAngle - BRANCH_ANGLE
    };
    
    for (int i = 0; i < 2; ++i) {
        Point branchDir(std::cos(angles[i]), std::sin(angles[i]));
        
        Branch newBranch(
            parent.currentNodeIdx,
            branchDir,
            -1,
            parent.iterationsLeft / 2
        );
        
        GlobalGoalsForBranch(newBranch, parent);
        
        if (newBranch.delay >= 0) {
            m_SleepingBranches.push_back(newBranch);
        }
    }
    
    // NOWE: Sprawdź czy utworzono jakieś akceptowalne branche
    bool hasAcceptedBranches = false;
    for (const auto& branch : m_SleepingBranches) {
        if (branch.parentNodeIdx == parent.currentNodeIdx && branch.delay >= 0) {
            hasAcceptedBranches = true;
            break;
        }
    }
    
    // Jeśli są akceptowalne branche, oznacz węzeł jako intersection
    // i zakończ aktualny highway, tworząc nowy kontynuujący wzrost
    if (hasAcceptedBranches) {
        std::cout << "    [CreateBranch] Accepted branches created - marking node " 
                  << parent.currentNodeIdx << " as intersection" << std::endl;
        
        bool wasIntersection = m_RoadNodes[parent.currentNodeIdx].isIntersection;
        m_RoadNodes[parent.currentNodeIdx].isIntersection = true;
        
        // Jeśli węzeł nie był wcześniej skrzyżowaniem, podziel highways
        if (!wasIntersection) {
            SplitHighwaysAtIntersection(parent.currentNodeIdx);
        }
        
        // Utwórz Highway dla dotychczasowego odcinka (jeśli istnieją roads)
        if (!parent.roadsSinceLastIntersection.empty()) {
            std::cout << "    [CreateBranch] Creating Highway from " 
                      << parent.lastIntersectionIdx << " to " << parent.currentNodeIdx << std::endl;
            CreateHighway(parent.lastIntersectionIdx, parent.currentNodeIdx, 
                         parent.roadsSinceLastIntersection);
        }
        
        // KLUCZOWE: Utwórz nowy HighwayEnd kontynuujący wzrost w tym samym kierunku
        std::cout << "    [CreateBranch] Creating new HighwayEnd to continue growth from intersection" << std::endl;
        HighwayEnd newEnd(
            parent.currentNodeIdx,  // startujemy od tego węzła (teraz intersection)
            parent.direction,       // w tym samym kierunku
            parent.iterationsLeft   // z pozostałymi iteracjami
        );
        
        // Dodaj nowy end do aktywnych (będzie przetworzony w następnej iteracji)
        m_ActiveEnds.push_back(newEnd);
        
        std::cout << "    [CreateBranch] New HighwayEnd created and added to active ends" << std::endl;
    }
}

void MapGenerator::UpdateSleepingBranches() {
    for (int i = m_SleepingBranches.size() - 1; i >= 0; --i) {
        Branch& branch = m_SleepingBranches[i];
        
        branch.delay--;
        
        if (branch.delay == 0) {
            std::cout << "[UpdateBranches] Activated branch from node " 
                      << branch.parentNodeIdx << std::endl;
            
            // Oznacz wÄ™zeÅ‚ jako skrzyÅ¼owanie (bo tutaj rozgaÅ‚Ä™zia siÄ™ droga)
            if (branch.parentNodeIdx >= 0 && branch.parentNodeIdx < (int)m_RoadNodes.size()) {
                bool wasIntersection = m_RoadNodes[branch.parentNodeIdx].isIntersection;
                m_RoadNodes[branch.parentNodeIdx].isIntersection = true;
                std::cout << "[UpdateBranches] Marked node " << branch.parentNodeIdx 
                          << " as intersection (branching point)" << std::endl;
                
                // NOWE: Jeśli węzeł nie był wcześniej skrzyżowaniem, podziel highways
                if (!wasIntersection) {
                    SplitHighwaysAtIntersection(branch.parentNodeIdx);
                }
            }
            // *** KONIEC ***
            
            HighwayEnd newEnd(
                branch.parentNodeIdx,
                branch.direction,
                branch.iterationsLeft
            );
            
            m_ActiveEnds.push_back(newEnd);
            m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
        }
        else if (branch.delay < 0) {
            m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
        }
    }
}

float MapGenerator::DistancePointToSegment(const Point& p, const Point& a, const Point& b, Point& closest)
{
    Point ab(b.x - a.x, b.y - a.y);
    Point ap(p.x - a.x, p.y - a.y);

    float abLenSq = ab.x*ab.x + ab.y*ab.y;
    float t = (ap.x*ab.x + ap.y*ab.y) / abLenSq;
    t = std::max(0.0f, std::min(1.0f, t));

    float dx = p.x - closest.x;
    float dy = p.y - closest.y;
    return std::sqrt(dx*dx + dy*dy);
}

void MapGenerator::CreateHighway(int startIntersection, int endIntersection, const std::vector<int>& roads) {
    if (roads.empty()) {
        return;
    }
    
    // Sprawdź czy oba końce to skrzyżowania
    bool startIsIntersection = (startIntersection >= 0 && startIntersection < (int)m_RoadNodes.size()) 
                                ? m_RoadNodes[startIntersection].isIntersection 
                                : false;
    bool endIsIntersection = (endIntersection >= 0 && endIntersection < (int)m_RoadNodes.size()) 
                              ? m_RoadNodes[endIntersection].isIntersection 
                              : false;
    
    std::cout << "\n[HIGHWAY CREATED] " << startIntersection << " -> " << endIntersection 
              << " | Start=" << (startIsIntersection ? "INTERSECTION" : "REGULAR")
              << " | End=" << (endIsIntersection ? "INTERSECTION" : "REGULAR")
              << " | Roads=" << roads.size() << std::endl;
    
    // Oblicz całkowitą długość
    float totalLength = 0.0f;
    for (int roadIdx : roads) {
        if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) {
            std::cout << "[CreateHighway] ERROR: Invalid road index " << roadIdx << std::endl;
            continue;
        }
        
        if (m_Roads[roadIdx].isDeleted) {
            std::cout << "[CreateHighway] WARNING: Road " << roadIdx << " is already deleted" << std::endl;
            continue;
        }
        
        const Road& road = m_Roads[roadIdx];
        const Point& start = m_RoadNodes[road.startNodeIdx];
        const Point& end = m_RoadNodes[road.endNodeIdx];
        
        float dx = end.x - start.x;
        float dy = end.y - start.y;
        float length = std::sqrt(dx * dx + dy * dy);
        totalLength += length;
    }
    
    std::cout << "[CreateHighway] Total length: " << totalLength << std::endl;
    
    // Utwórz nowy Highway
    Highway newHighway(startIntersection, endIntersection, roads, totalLength);
    
    // Sprawdź redundancję
    if (CheckAndRemoveRedundantHighway(newHighway)) {
        std::cout << "[CreateHighway] ✓ Highway added (no redundancy or replaced shorter route)" << std::endl;
        m_Highways.push_back(newHighway);
    } else {
        std::cout << "[CreateHighway] ✗ Highway rejected (redundant - longer route exists)" << std::endl;
    }
    
    std::cout << "[CreateHighway] Total highways in system: " << m_Highways.size() << std::endl;
}

bool MapGenerator::CheckAndRemoveRedundantHighway(const Highway& newHighway) { 
    // Szukaj istniejących Highway łączących te same skrzyżowania
    for (int i = 0; i < (int)m_Highways.size(); ++i) {
        const Highway& existing = m_Highways[i];

        std::cout << "  [CheckRedundant] Comparing roads:" << std::endl;
        std::cout << "    Existing: " << existing.roadIndices.size() << " roads" << std::endl;
        std::cout << "    New: " << newHighway.roadIndices.size() << " roads" << std::endl;
        
        // Sprawdź czy łączy te same węzły (w obu kierunkach)
        bool sameDirection = (existing.startIntersectionIdx == newHighway.startIntersectionIdx &&
                             existing.endIntersectionIdx == newHighway.endIntersectionIdx);
        
        bool oppositeDirection = (existing.startIntersectionIdx == newHighway.endIntersectionIdx &&
                                 existing.endIntersectionIdx == newHighway.startIntersectionIdx);

        if (sameDirection || oppositeDirection) {
            std::cout << "  [CheckRedundant] Found existing highway " << i 
                      << ": " << existing.startIntersectionIdx 
                      << " -> " << existing.endIntersectionIdx 
                      << ", length: " << existing.totalLength << std::endl;
            
            // Porównaj długości
            if (newHighway.totalLength <= existing.totalLength) {
                std::cout << "  [CheckRedundant] New highway is SHORTER (" 
                          << newHighway.totalLength << " < " << existing.totalLength << ")" << std::endl;
                std::cout << "  [CheckRedundant] Removing old highway and its roads..." << std::endl;
                
                RemoveHighway(i);
                
                std::cout << "  [CheckRedundant] ✓ Old highway removed, accepting new one" << std::endl;
                return true;  // Akceptuj nowy
                
            } else {
                std::cout << "  [CheckRedundant] New highway is LONGER or EQUAL (" 
                          << newHighway.totalLength << " >= " << existing.totalLength << ")" << std::endl;
                std::cout << "  [CheckRedundant] Removing new highway's roads..." << std::endl;
                
                RemoveRoads(newHighway.roadIndices);
                
                std::cout << "  [CheckRedundant] ✗ New highway rejected" << std::endl;
                return false;  // Odrzuć nowy
            }
        }
    }
    
    std::cout << "  [CheckRedundant] No redundancy found, accepting new highway" << std::endl;
    return true;  // Nie ma konfliktu, akceptuj
}

void MapGenerator::RemoveHighway(int highwayIdx) {
    if (highwayIdx < 0 || highwayIdx >= (int)m_Highways.size()) {
        std::cout << "    [RemoveHighway] ERROR: Invalid highway index " << highwayIdx << std::endl;
        return;
    }
    
    const Highway& highway = m_Highways[highwayIdx];
    
    std::cout << "    [RemoveHighway] Removing highway " << highwayIdx 
              << " (" << highway.startIntersectionIdx 
              << " -> " << highway.endIntersectionIdx << ")" << std::endl;
    std::cout << "    [RemoveHighway] Contains " << highway.roadIndices.size() << " roads" << std::endl;
    
    // Usuń wszystkie Road'y należące do tego Highway
    RemoveRoads(highway.roadIndices);
    
    // Usuń Highway z listy
    m_Highways.erase(m_Highways.begin() + highwayIdx);
    
    std::cout << "    [RemoveHighway] Highway removed successfully" << std::endl;
}

void MapGenerator::RemoveRoads(const std::vector<int>& roadIndices) {
    std::cout << "    [RemoveRoads] Marking " << roadIndices.size() << " roads as deleted" << std::endl;
    
    int deletedCount = 0;
    for (int roadIdx : roadIndices) {
        if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) {
            std::cout << "    [RemoveRoads] WARNING: Invalid road index " << roadIdx << std::endl;
            continue;
        }
        
        if (m_Roads[roadIdx].isDeleted) {
            std::cout << "    [RemoveRoads] WARNING: Road " << roadIdx << " already deleted" << std::endl;
            continue;
        }
        
        m_Roads[roadIdx].isDeleted = true;
        deletedCount++;
        
        std::cout << "    [RemoveRoads] Marked road " << roadIdx << " as deleted" << std::endl;
    }
    
    std::cout << "    [RemoveRoads] Successfully deleted " << deletedCount << " roads" << std::endl;
}

void MapGenerator::SplitHighwaysAtIntersection(int intersectionNodeIdx) {
    std::cout << "\n[SplitHighways] Checking if any highways pass through new intersection " 
              << intersectionNodeIdx << std::endl;
    
    std::vector<int> highwaysToSplit;
    
    // Znajdź wszystkie highways, które przechodzą przez ten węzeł (ale się w nim nie zaczynają/kończą)
    for (int i = 0; i < (int)m_Highways.size(); ++i) {
        const Highway& hw = m_Highways[i];
        
        // Pomiń highways, które zaczynają się lub kończą w tym węźle
        if (hw.startIntersectionIdx == intersectionNodeIdx || 
            hw.endIntersectionIdx == intersectionNodeIdx) {
            continue;
        }
        
        // Sprawdź czy węzeł znajduje się wewnątrz tego highway
        for (int roadIdx : hw.roadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
            
            const Road& road = m_Roads[roadIdx];
            
            if (road.startNodeIdx == intersectionNodeIdx || 
                road.endNodeIdx == intersectionNodeIdx) {
                std::cout << "[SplitHighways] Highway " << i << " passes through intersection" << std::endl;
                highwaysToSplit.push_back(i);
                break;
            }
        }
    }
    
    if (highwaysToSplit.empty()) {
        std::cout << "[SplitHighways] No highways to split" << std::endl;
        return;
    }
    
    std::cout << "[SplitHighways] Found " << highwaysToSplit.size() << " highways to split" << std::endl;
    
    // Podziel każdy highway (od tyłu, żeby indeksy się nie psuły przy usuwaniu)
    for (int i = highwaysToSplit.size() - 1; i >= 0; --i) {
        int hwIdx = highwaysToSplit[i];
        
        if (hwIdx < 0 || hwIdx >= (int)m_Highways.size()) continue;
        
        const Highway& oldHighway = m_Highways[hwIdx];
        
        std::cout << "[SplitHighways] Splitting highway " << hwIdx 
                  << " (" << oldHighway.startIntersectionIdx 
                  << " -> " << oldHighway.endIntersectionIdx << ")" << std::endl;
        
        // Znajdź pozycję węzła w sekwencji road
        std::vector<int> firstPart;
        std::vector<int> secondPart;
        bool foundSplit = false;
        
        for (int roadIdx : oldHighway.roadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
            
            const Road& road = m_Roads[roadIdx];
            
            if (!foundSplit) {
                firstPart.push_back(roadIdx);
                
                // Jeśli koniec tego road to nasz węzeł, przełącz na drugą część
                if (road.endNodeIdx == intersectionNodeIdx) {
                    foundSplit = true;
                }
            } else {
                secondPart.push_back(roadIdx);
            }
        }
        
        if (firstPart.empty() || secondPart.empty()) {
            std::cout << "[SplitHighways] ERROR: Cannot split highway " << hwIdx 
                      << " (firstPart=" << firstPart.size() 
                      << ", secondPart=" << secondPart.size() << ")" << std::endl;
            continue;
        }
        
        // Oblicz długości
        float length1 = 0.0f;
        for (int roadIdx : firstPart) {
            const Road& road = m_Roads[roadIdx];
            const Point& start = m_RoadNodes[road.startNodeIdx];
            const Point& end = m_RoadNodes[road.endNodeIdx];
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            length1 += std::sqrt(dx * dx + dy * dy);
        }
        
        float length2 = 0.0f;
        for (int roadIdx : secondPart) {
            const Road& road = m_Roads[roadIdx];
            const Point& start = m_RoadNodes[road.startNodeIdx];
            const Point& end = m_RoadNodes[road.endNodeIdx];
            float dx = end.x - start.x;
            float dy = end.y - start.y;
            length2 += std::sqrt(dx * dx + dy * dy);
        }
        
        // Utwórz dwa nowe highways
        Highway hw1(oldHighway.startIntersectionIdx, intersectionNodeIdx, firstPart, length1);
        Highway hw2(intersectionNodeIdx, oldHighway.endIntersectionIdx, secondPart, length2);
        
        std::cout << "[SplitHighways] Created two new highways:" << std::endl;
        std::cout << "  Highway 1: " << hw1.startIntersectionIdx 
                  << " -> " << hw1.endIntersectionIdx 
                  << " (length=" << hw1.totalLength << ", roads=" << hw1.roadIndices.size() << ")" << std::endl;
        std::cout << "  Highway 2: " << hw2.startIntersectionIdx 
                  << " -> " << hw2.endIntersectionIdx 
                  << " (length=" << hw2.totalLength << ", roads=" << hw2.roadIndices.size() << ")" << std::endl;
        
        // Usuń stary highway
        m_Highways.erase(m_Highways.begin() + hwIdx);
        
        // Dodaj nowe highways
        m_Highways.push_back(hw1);
        m_Highways.push_back(hw2);
        
        std::cout << "[SplitHighways] Highway " << hwIdx << " successfully split" << std::endl;
    }
}

void MapGenerator::PostProcessIntersections() {
    std::cout << "[PostProcess] Checking all intersections against highways..." << std::endl;
    
    int totalSplits = 0;
    
    // Dla każdego węzła który jest intersection
    for (size_t nodeIdx = 0; nodeIdx < m_RoadNodes.size(); ++nodeIdx) {
        const Point& intersection = m_RoadNodes[nodeIdx];
        
        if (!intersection.isIntersection) {
            continue;
        }
        
        std::cout << "[PostProcess] Checking intersection at node " << nodeIdx << std::endl;
        
        // Sprawdź wszystkie highways (od tyłu, bo będziemy modyfikować listę)
        for (int hwIdx = m_Highways.size() - 1; hwIdx >= 0; --hwIdx) {
            const Highway& highway = m_Highways[hwIdx];
            
            // Pomiń highways które zaczynają się lub kończą w tym węźle
            if (highway.startIntersectionIdx == (int)nodeIdx || 
                highway.endIntersectionIdx == (int)nodeIdx) {
                continue;
            }
            
            // Sprawdź czy punkt leży na tym highway
            if (IsPointOnHighway(intersection, highway)) {
                std::cout << "[PostProcess] Intersection " << nodeIdx 
                          << " lies on highway " << hwIdx 
                          << " (" << highway.startIntersectionIdx 
                          << " -> " << highway.endIntersectionIdx << ")" << std::endl;
                
                // Znajdź pozycję węzła w sekwencji roads
                std::vector<int> firstPart;
                std::vector<int> secondPart;
                bool foundSplit = false;
                
                for (int roadIdx : highway.roadIndices) {
                    if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
                    
                    const Road& road = m_Roads[roadIdx];
                    
                    if (!foundSplit) {
                        firstPart.push_back(roadIdx);
                        
                        // Jeśli koniec tego road to nasz węzeł, przełącz na drugą część
                        if (road.endNodeIdx == (int)nodeIdx) {
                            foundSplit = true;
                        }
                    } else {
                        secondPart.push_back(roadIdx);
                    }
                }
                
                if (firstPart.empty() || secondPart.empty()) {
                    std::cout << "[PostProcess] Cannot split highway " << hwIdx 
                              << " (invalid split point)" << std::endl;
                    continue;
                }
                
                // Oblicz długości
                float length1 = 0.0f;
                for (int roadIdx : firstPart) {
                    const Road& road = m_Roads[roadIdx];
                    const Point& start = m_RoadNodes[road.startNodeIdx];
                    const Point& end = m_RoadNodes[road.endNodeIdx];
                    float dx = end.x - start.x;
                    float dy = end.y - start.y;
                    length1 += std::sqrt(dx * dx + dy * dy);
                }
                
                float length2 = 0.0f;
                for (int roadIdx : secondPart) {
                    const Road& road = m_Roads[roadIdx];
                    const Point& start = m_RoadNodes[road.startNodeIdx];
                    const Point& end = m_RoadNodes[road.endNodeIdx];
                    float dx = end.x - start.x;
                    float dy = end.y - start.y;
                    length2 += std::sqrt(dx * dx + dy * dy);
                }
                
                // Utwórz dwa nowe highways
                Highway hw1(highway.startIntersectionIdx, nodeIdx, firstPart, length1);
                Highway hw2(nodeIdx, highway.endIntersectionIdx, secondPart, length2);
                
                std::cout << "[PostProcess] Split highway into:" << std::endl;
                std::cout << "  Part 1: " << hw1.startIntersectionIdx 
                          << " -> " << hw1.endIntersectionIdx 
                          << " (length=" << hw1.totalLength << ")" << std::endl;
                std::cout << "  Part 2: " << hw2.startIntersectionIdx 
                          << " -> " << hw2.endIntersectionIdx 
                          << " (length=" << hw2.totalLength << ")" << std::endl;
                
                // Usuń stary highway
                m_Highways.erase(m_Highways.begin() + hwIdx);
                
                // Dodaj nowe highways
                m_Highways.push_back(hw1);
                m_Highways.push_back(hw2);
                
                totalSplits++;
            }
        }
    }
    
    std::cout << "[PostProcess] Total highways split: " << totalSplits << std::endl;
}

bool MapGenerator::IsPointOnHighway(const Point& point, const Highway& highway, float tolerance) {
    // Sprawdź każdy road w highway
    for (int roadIdx : highway.roadIndices) {
        if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
        
        const Road& road = m_Roads[roadIdx];
        const Point& roadStart = m_RoadNodes[road.startNodeIdx];
        const Point& roadEnd = m_RoadNodes[road.endNodeIdx];
        
        // Sprawdź czy punkt jest jednym z końców tego segmentu
        if (road.startNodeIdx == (int)(&point - &m_RoadNodes[0]) || 
            road.endNodeIdx == (int)(&point - &m_RoadNodes[0])) {
            return true;
        }
        
        // Oblicz odległość punktu od segmentu
        Point closest;
        float dist = DistancePointToSegment(point, roadStart, roadEnd, closest);
        
        if (dist < tolerance) {
            // Dodatkowo sprawdź czy punkt leży między końcami segmentu
            float segmentLength = std::sqrt(
                (roadEnd.x - roadStart.x) * (roadEnd.x - roadStart.x) +
                (roadEnd.y - roadStart.y) * (roadEnd.y - roadStart.y)
            );
            
            float distToStart = std::sqrt(
                (point.x - roadStart.x) * (point.x - roadStart.x) +
                (point.y - roadStart.y) * (point.y - roadStart.y)
            );
            
            float distToEnd = std::sqrt(
                (point.x - roadEnd.x) * (point.x - roadEnd.x) +
                (point.y - roadEnd.y) * (point.y - roadEnd.y)
            );
            
            // Punkt leży na segmencie jeśli suma odległości ≈ długość segmentu
            if (std::abs((distToStart + distToEnd) - segmentLength) < tolerance) {
                return true;
            }
        }
    }
    
    return false;
}

float MapGenerator::RaycastToHighway(const Point& origin, const Point& direction, float maxDistance) {
    float minHitDistance = maxDistance + 1.0f;  // Większe niż max = nie trafiono
    bool hitAny = false;
    
    // Dla każdego highway w systemie
    for (const Highway& highway : m_Highways) {
        // Sprawdź każdy road w highway
        for (int roadIdx : highway.roadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
            if (m_Roads[roadIdx].isDeleted) continue;
            
            const Road& road = m_Roads[roadIdx];
            const Point& roadStart = m_RoadNodes[road.startNodeIdx];
            const Point& roadEnd = m_RoadNodes[road.endNodeIdx];
            
            // Oblicz przecięcie raya z segmentem drogi
            // Ray: P = origin + t * direction (gdzie t >= 0)
            // Segment: Q = roadStart + s * (roadEnd - roadStart) (gdzie 0 <= s <= 1)
            
            Point roadDir(roadEnd.x - roadStart.x, roadEnd.y - roadStart.y);
            
            // Rozwiąż układ równań metodą Cramera
            float denom = direction.x * roadDir.y - direction.y * roadDir.x;
            
            if (std::abs(denom) < 1e-6f) continue;  // Równoległe
            
            Point originToRoadStart(roadStart.x - origin.x, roadStart.y - origin.y);
            
            float t = (originToRoadStart.x * roadDir.y - originToRoadStart.y * roadDir.x) / denom;
            float s = (originToRoadStart.x * direction.y - originToRoadStart.y * direction.x) / (-denom);
            
            // Sprawdź czy przecięcie jest w zakresie
            if (t >= 0.0f && t <= maxDistance && s >= 0.0f && s <= 1.0f) {
                hitAny = true;
                if (t < minHitDistance) {
                    minHitDistance = t;
                }
            }
        }
    }
    
    return hitAny ? minHitDistance : -1.0f;
}

void MapGenerator::GenerateStreets() {
    // TODO
}

} // namespace CityGen