#include "HighwayGenerator.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cmath>

namespace CityGen {

    HighwayGenerator::HighwayGenerator(int width, int height)
        : m_Width(width)
        , m_Height(height)
    {
        // Inicjalizacja generatora liczb losowych
        std::srand(static_cast<unsigned int>(std::time(nullptr)));
    }

    HighwayGenerator::~HighwayGenerator() {
        std::cout << "[HighwayGenerator] Destroyed" << std::endl;
    }

    void HighwayGenerator::Generate() {
        std::cout << "[CityGen] Starting procedural generation..." << std::endl;

        // Czyszczenie struktur danych przed nową generacją
        m_Roads.clear();
        m_RoadNodes.clear();
        m_Highways.clear();
        m_ActiveEnds.clear();
        m_SleepingBranches.clear();

        // KROK 1: Generowanie mapy gęstości zaludnienia
        // Uwzględnia ona strefy (parki, rzeki) poprzez m_ZoneMask
        GeneratePopulationDensity();

        // KROK 2: Faza 1 - Główne arterie (Highways / Bus Routes)
        // Zgodnie z PDF: Łączą centra populacji i tworzą szkielet miasta
        std::cout << "[CityGen] Phase 1: Generating Highways..." << std::endl;
        GenerateHighways();

        // Post-processing dla Highways (dzielenie na skrzyżowaniach, usuwanie duplikatów)
        std::cout << "[CityGen] Phase 1 Post-processing..." << std::endl;
        PostProcessIntersections();
        MergeSimpleIntersections();

        // KROK 3: Faza 2 - Ulice lokalne (Streets / Taxi Routes)
        // Zgodnie z PDF: Wypełniają luki między autostradami, używając wzorców (Raster/Organic)
        std::cout << "[CityGen] Phase 2: Generating Streets..." << std::endl;
        GenerateStreets();

        std::cout << "[CityGen] Generation complete. Nodes: " << m_RoadNodes.size()
            << ", Roads: " << m_Roads.size()
            << ", Highways: " << m_Highways.size() << std::endl;
    }

    void HighwayGenerator::GeneratePopulationDensity() {
        // Inicjalizacja mapy zerami
        m_PopulationDensity.resize(m_Height, std::vector<float>(m_Width, 0.0f));

        // Definicja centrów populacji (Dzielnice)
        struct Center { float x, y, r, i; };
        std::vector<Center> centers = {
            {m_Width * 0.5f, m_Height * 0.5f, 400.0f, 1.0f}, // Główne centrum
            {m_Width * 0.2f, m_Height * 0.3f, 200.0f, 0.8f}, // Dzielnica 1
            {m_Width * 0.8f, m_Height * 0.7f, 250.0f, 0.9f}  // Dzielnica 2
        };

        for (int y = 0; y < m_Height; ++y) {
            for (int x = 0; x < m_Width; ++x) {
                float val = 0.0f;

                // Sumowanie wpływu centrów (metoda gradientowa)
                for (const auto& c : centers) {
                    float dx = x - c.x;
                    float dy = y - c.y;
                    float dist = std::sqrt(dx * dx + dy * dy);
                    if (dist < c.r) {
                        // Funkcja kwadratowa zaniku (falloff)
                        float t = 1.0f - (dist / c.r);
                        val += c.i * (t * t);
                    }
                }

                // Zastosowanie MASKI STREF (m_ZoneMask)
                // To zapewnia, że populacja (i drogi) nie pojawią się w rzekach i parkach
                if (!m_ZoneMask.empty()) {
                    int idx = y * m_Width + x;
                    if (idx < m_ZoneMask.size() && m_ZoneMask[idx] != 0) {
                        val = 0.0f; // Zerowa gęstość w strefach wykluczonych
                    }
                }

                m_PopulationDensity[y][x] = std::min(1.0f, val);
            }
        }
    }

    // Funkcja określająca wzorzec ulic w danym miejscu
    PatternType HighwayGenerator::GetPatternAt(float x, float y) {
        int ix = std::clamp((int)x, 0, m_Width - 1);
        int iy = std::clamp((int)y, 0, m_Height - 1);

        float density = m_PopulationDensity[iy][ix];

        // Wysoka gęstość -> Siatka (Raster/Grid) - typowe dla centrum (np. Manhattan)
        if (density > 0.5f) return PatternType::RASTER;

        // Niska gęstość -> Organiczny (Organic) - typowe dla przedmieść
        return PatternType::ORGANIC;
    }

    // Implementacja "Global Goals"
    // Decyduje o kierunku rozwoju drogi w zależności od jej typu i otoczenia
    Point HighwayGenerator::ApplyGlobalGoals(Point pos, Point currentDir, RoadType type) {
        if (type == RoadType::HIGHWAY) {
            // HIGHWAYS: Podążają za gęstością populacji
            // Szukamy kierunku o największej populacji w zasięgu wzroku
            float bestScore = -1.0f;
            float bestAngle = 0.0f;
            float currentAngle = std::atan2(currentDir.y, currentDir.x);

            int numRays = 15;
            float fov = 1.0f; // ~60 stopni stożek poszukiwań

            for (int i = 0; i < numRays; ++i) {
                float angle = currentAngle + (i / (float)(numRays - 1) - 0.5f) * fov;
                float score = ShootRay(pos, angle);
                if (score > bestScore) {
                    bestScore = score;
                    bestAngle = angle;
                }
            }

            // Jeśli gęstość jest znikoma, przestań budować autostradę
            if (bestScore < 0.01f) return Point(0, 0);

            return Point(std::cos(bestAngle), std::sin(bestAngle));
        }
        else {
            // STREETS: Podążają za wzorcami (Patterns)
            PatternType pattern = GetPatternAt(pos.x, pos.y);
            float currentAngle = std::atan2(currentDir.y, currentDir.x);

            if (pattern == PatternType::RASTER) {
                // RASTER (New York Rule): Dążenie do kątów prostych
                // Uproszczona implementacja: Staramy się trzymać osi siatki

                float bestGridAngle = currentAngle;
                float minDiff = 1000.0f;
                // Kąty główne: 0, 90, 180, 270 stopni
                float gridAngles[] = { 0.0f, 1.5707f, 3.14159f, 4.71239f, -1.5707f };

                for (float ga : gridAngles) {
                    float diff = std::abs(currentAngle - ga);
                    if (diff < minDiff) { minDiff = diff; bestGridAngle = ga; }
                }

                // Snapowanie do siatki jeśli blisko
                if (minDiff < 0.3f) {
                    return Point(std::cos(bestGridAngle), std::sin(bestGridAngle));
                }
                return currentDir;
            }
            else if (pattern == PatternType::RADIAL) {
                // RADIAL (Paris Rule): Do centrum lub po okręgu
                float centerX = m_Width * 0.5f;
                float centerY = m_Height * 0.5f;
                float toCenterX = centerX - pos.x;
                float toCenterY = centerY - pos.y;
                float dist = std::sqrt(toCenterX * toCenterX + toCenterY * toCenterY);
                if (dist > 0.001f) {
                    toCenterX /= dist; toCenterY /= dist;

                    float dot = currentDir.x * toCenterX + currentDir.y * toCenterY;
                    if (std::abs(dot) > 0.5f) {
                        return Point(toCenterX, toCenterY); // Promieniście do centrum
                    }
                    else {
                        return Point(-toCenterY, toCenterX); // Po okręgu
                    }
                }
                return currentDir;
            }
            else { // ORGANIC
                // Lekki szum losowy dla naturalnego wyglądu
                float noise = ((rand() % 100) / 100.0f - 0.5f) * 0.4f;
                float newAngle = currentAngle + noise;
                return Point(std::cos(newAngle), std::sin(newAngle));
            }
        }
    }

    void HighwayGenerator::GenerateHighways() {
        // 1. Znajdź punkt startowy o najwyższej gęstości
        Point startPos;
        float maxDensity = 0.0f;

        for (int y = 0; y < m_Height; y += 20) {
            for (int x = 0; x < m_Width; x += 20) {
                float d = GetDensityAt(x, y);
                if (d > maxDensity) {
                    maxDensity = d;
                    startPos = Point((float)x, (float)y);
                }
            }
        }

        if (maxDensity == 0.0f) startPos = Point(m_Width / 2.0f, m_Height / 2.0f);

        // 2. Zainicjuj pierwszego agenta (Highway)
        int startNode = CreateOrGetNode(startPos, true);
        Point startDir(1.0f, 0.0f); // Początkowy kierunek

        m_ActiveEnds.push_back(HighwayEnd(startNode, startDir, HIGHWAY_MAX_ITERATIONS, RoadType::HIGHWAY));

        // 3. Pętla generacji (L-System growth)
        int iterations = 0;
        while ((!m_ActiveEnds.empty() || !m_SleepingBranches.empty()) && iterations++ < 5000) {
            for (int i = m_ActiveEnds.size() - 1; i >= 0; --i) {
                bool grown = GrowOneStep(m_ActiveEnds[i]);
                if (!grown) {
                    // Jeśli highway zakończył bieg, sfinalizuj go
                    if (m_ActiveEnds[i].type == RoadType::HIGHWAY && !m_ActiveEnds[i].roadsSinceLastIntersection.empty()) {
                        CreateHighway(m_ActiveEnds[i].lastIntersectionIdx, m_ActiveEnds[i].currentNodeIdx, m_ActiveEnds[i].roadsSinceLastIntersection);
                    }
                    m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                }
                else {
                    CreateBranchCandidates(m_ActiveEnds[i]);
                }
            }
            UpdateSleepingBranches();
        }
    }

    void HighwayGenerator::GenerateStreets() {
        // Space Colonization: Skanujemy mapę w poszukiwaniu pustych miejsc

        int attempts = 800; // Ilość prób zasiania ulic
        for (int i = 0; i < attempts; ++i) {
            float rx = static_cast<float>(rand() % m_Width);
            float ry = static_cast<float>(rand() % m_Height);

            // Warunek 1: Tylko tam gdzie jest populacja
            if (GetDensityAt((int)rx, (int)ry) < 0.15f) continue;

            // Warunek 2: Sprawdź czy to legalny teren (nie rzeka/park)
            int idx = (int)ry * m_Width + (int)rx;
            if (!m_ZoneMask.empty() && m_ZoneMask[idx] != 0) continue;

            // Warunek 3: Sprawdź czy nie jesteśmy zbyt blisko istniejącej drogi
            Point p(rx, ry);
            std::vector<int> nearby = FindNearbyRoadIndices(p, 40.0f);

            if (nearby.empty()) {
                // Zasiej nową ulicę
                int startNode = CreateOrGetNode(p, false);
                float angle = (rand() % 360) * 3.14159f / 180.0f;
                Point dir(std::cos(angle), std::sin(angle));

                m_ActiveEnds.push_back(HighwayEnd(startNode, dir, STREET_MAX_ITERATIONS, RoadType::STREET));
            }
        }

        // Pętla wzrostu dla ulic
        int iterations = 0;
        while ((!m_ActiveEnds.empty() || !m_SleepingBranches.empty()) && iterations++ < 5000) {
            for (int i = m_ActiveEnds.size() - 1; i >= 0; --i) {
                bool grown = GrowOneStep(m_ActiveEnds[i]);
                if (!grown) {
                    m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                }
                else {
                    CreateBranchCandidates(m_ActiveEnds[i]);
                }
            }
            UpdateSleepingBranches();
        }
    }

    bool HighwayGenerator::GrowOneStep(HighwayEnd& agent) {
        if (agent.iterationsLeft <= 0) return false;

        Point currentPos = m_RoadNodes[agent.currentNodeIdx];

        // 1. Wybierz kierunek (Global Goals)
        Point newDir = ApplyGlobalGoals(currentPos, agent.direction, agent.type);
        if (newDir.x == 0 && newDir.y == 0) return false;

        // Długość zależy od typu (autostrady dłuższe, ulice krótsze)
        float len = (agent.type == RoadType::HIGHWAY) ? HIGHWAY_SEGMENT_LENGTH : STREET_SEGMENT_LENGTH;
        Point newPos(currentPos.x + newDir.x * len, currentPos.y + newDir.y * len);

        // Granice mapy
        if (newPos.x < 0 || newPos.x >= m_Width || newPos.y < 0 || newPos.y >= m_Height) return false;

        // Unikanie równoległych autostrad (dla estetyki)
        if (agent.type == RoadType::HIGHWAY) {
            if (!CheckParallelDuringGrowth(currentPos, newPos, agent.currentNodeIdx)) {
                return false;
            }
        }

        // 2. Local Constraints (Kolizje, Snapowanie)
        int newNodeIdx = -1;
        bool constraintsMet = CheckLocalConstraints(currentPos, newPos, agent.currentNodeIdx, newNodeIdx, agent.type);

        if (!constraintsMet) return false; 

        bool wasIntersection = m_RoadNodes[newNodeIdx].isIntersection;

        // Dodaj fizyczną drogę do grafu
        m_Roads.push_back(Road(agent.currentNodeIdx, newNodeIdx, agent.type));
        int newRoadIdx = m_Roads.size() - 1;

        m_RoadNodes[agent.currentNodeIdx].connectedRoadIndices.push_back(newRoadIdx);
        m_RoadNodes[newNodeIdx].connectedRoadIndices.push_back(newRoadIdx);

        //  Logika Highways (zarządzanie obiektami Highway) 
        if (agent.type == RoadType::HIGHWAY) {
            agent.roadsSinceLastIntersection.push_back(newRoadIdx);

            // Jeśli trafiliśmy na węzeł będący skrzyżowaniem
            if (m_RoadNodes[newNodeIdx].isIntersection && newNodeIdx != agent.currentNodeIdx) {
                if (!wasIntersection) {
                    SplitHighwaysAtIntersection(newNodeIdx);
                }

                CreateHighway(agent.lastIntersectionIdx, newNodeIdx, agent.roadsSinceLastIntersection);
                agent.lastIntersectionIdx = newNodeIdx;
                agent.roadsSinceLastIntersection.clear();
            }
        }

        // Logika Streets 
        if (agent.type == RoadType::STREET) {
            // Jeśli ulica dobiła do innej drogi, kończymy ją (niech nie przebija się dalej)
            if (m_RoadNodes[newNodeIdx].connectedRoadIndices.size() > 1 && newNodeIdx != agent.currentNodeIdx) {
                agent.currentNodeIdx = newNodeIdx;
                return false;
            }
        }

        // Aktualizuj stan agenta
        agent.currentNodeIdx = newNodeIdx;
        agent.direction = newDir;
        agent.iterationsLeft--;
        agent.distanceSinceLastBranch += len;

        return true;
    }

    void HighwayGenerator::CreateBranchCandidates(const HighwayEnd& parent) {
        float distThreshold = (parent.type == RoadType::HIGHWAY) ? 50.0f : 25.0f;

        if (parent.distanceSinceLastBranch > distThreshold) {
            PatternType pattern = GetPatternAt(m_RoadNodes[parent.currentNodeIdx].x, m_RoadNodes[parent.currentNodeIdx].y);

            // Kąt rozgałęzienia zależny od wzorca
            float branchAngle = (parent.type == RoadType::HIGHWAY) ? BRANCH_ANGLE : (1.5707f); // 90 stopni dla ulic

            // Dla organicznych ulic losowy kąt
            if (pattern == PatternType::ORGANIC && parent.type == RoadType::STREET) {
                branchAngle = 0.5f + (rand() % 100 / 100.0f);
            }

            float currentAngle = std::atan2(parent.direction.y, parent.direction.x);
            int prob = (parent.type == RoadType::HIGHWAY) ? 40 : 60;

            // Próba lewa i prawa
            if (rand() % 100 < prob) {
                Point dir(std::cos(currentAngle + branchAngle), std::sin(currentAngle + branchAngle));
                m_SleepingBranches.push_back(Branch(parent.currentNodeIdx, dir, 5, parent.iterationsLeft, parent.type));
            }
            if (rand() % 100 < prob) {
                Point dir(std::cos(currentAngle - branchAngle), std::sin(currentAngle - branchAngle));
                m_SleepingBranches.push_back(Branch(parent.currentNodeIdx, dir, 5, parent.iterationsLeft, parent.type));
            }

            // Jeśli Highway się rozgałęzia, oznaczamy węzeł jako skrzyżowanie
            if (parent.type == RoadType::HIGHWAY && !m_SleepingBranches.empty()) {
                if (m_SleepingBranches.back().parentNodeIdx == parent.currentNodeIdx) {
                    m_RoadNodes[parent.currentNodeIdx].isIntersection = true;
                    SplitHighwaysAtIntersection(parent.currentNodeIdx);
                }
            }
        }
    }

    bool HighwayGenerator::CheckLocalConstraints(const Point& start, const Point& end, int startNodeIdx, int& endNodeIdx, RoadType type) {
        // 1. Sprawdź maskę terenu (woda/parki) - blokada budowy
        int ix = (int)end.x;
        int iy = (int)end.y;
        if (!m_ZoneMask.empty() && (ix >= 0 && ix < m_Width && iy >= 0 && iy < m_Height)) {
            if (m_ZoneMask[iy * m_Width + ix] != 0) return false;
        }

        // 2. Snapowanie (przyciąganie) do istniejących węzłów
        float searchRadius = (type == RoadType::HIGHWAY) ? 15.0f : 12.0f;
        int nearest = FindNearestNode(end, searchRadius);
        if (nearest != -1 && nearest != startNodeIdx) {
            endNodeIdx = nearest;
            return true;
        }

        // 3. Sprawdź przecięcia z innymi drogami
        Point intersection;
        float minDist = 10000.0f;
        int hitRoadIdx = -1;

        for (int i = 0; i < m_Roads.size(); ++i) {
            const auto& r = m_Roads[i];
            if (r.isDeleted) continue;
            if (r.startNodeIdx == startNodeIdx || r.endNodeIdx == startNodeIdx) continue;

            Point p1 = m_RoadNodes[r.startNodeIdx];
            Point p2 = m_RoadNodes[r.endNodeIdx];

            if (DoSegmentsIntersect(start, end, p1, p2, intersection)) {
                float d = std::sqrt(std::pow(start.x - intersection.x, 2) + std::pow(start.y - intersection.y, 2));
                if (d < minDist) {
                    minDist = d;
                    hitRoadIdx = i;
                }
            }
        }

        if (hitRoadIdx != -1) {
            // Znaleziono przecięcie -> Podziel istniejącą drogę i wstaw węzeł
            int intersectNodeIdx = CreateOrGetNode(intersection, true);
            Road oldRoad = m_Roads[hitRoadIdx];
            m_Roads[hitRoadIdx].isDeleted = true;

            m_Roads.push_back(Road(oldRoad.startNodeIdx, intersectNodeIdx, oldRoad.type));
            int r1 = m_Roads.size() - 1;
            m_RoadNodes[oldRoad.startNodeIdx].connectedRoadIndices.push_back(r1);
            m_RoadNodes[intersectNodeIdx].connectedRoadIndices.push_back(r1);

            m_Roads.push_back(Road(intersectNodeIdx, oldRoad.endNodeIdx, oldRoad.type));
            int r2 = m_Roads.size() - 1;
            m_RoadNodes[intersectNodeIdx].connectedRoadIndices.push_back(r2);
            m_RoadNodes[oldRoad.endNodeIdx].connectedRoadIndices.push_back(r2);

            if (oldRoad.type == RoadType::HIGHWAY) {
                m_RoadNodes[intersectNodeIdx].isIntersection = true;
                SplitHighwaysAtIntersection(intersectNodeIdx);
            }

            endNodeIdx = intersectNodeIdx;
            return true;
        }

        // 4. Brak kolizji - nowy węzeł
        endNodeIdx = CreateOrGetNode(end, false);
        return true;
    }

    // Helpers 

    int HighwayGenerator::CreateOrGetNode(const Point& pos, bool isIntersection) {
        Point p = pos;
        p.isIntersection = isIntersection;
        m_RoadNodes.push_back(p);
        return m_RoadNodes.size() - 1;
    }

    int HighwayGenerator::FindNearestNode(const Point& pos, float maxDist, bool intersectionsOnly) {
        int best = -1;
        float bestDSq = maxDist * maxDist;
        for (int i = 0; i < m_RoadNodes.size(); ++i) {
            if (intersectionsOnly && !m_RoadNodes[i].isIntersection) continue;
            float dx = m_RoadNodes[i].x - pos.x;
            float dy = m_RoadNodes[i].y - pos.y;
            float dSq = dx * dx + dy * dy;
            if (dSq < bestDSq) {
                bestDSq = dSq;
                best = i;
            }
        }
        return best;
    }

    float HighwayGenerator::ShootRay(Point pos, float angle) {
        float step = 15.0f;
        float score = 0.0f;
        for (int i = 1; i <= 8; ++i) {
            float px = pos.x + std::cos(angle) * step * i;
            float py = pos.y + std::sin(angle) * step * i;
            score += GetDensityAt((int)px, (int)py);
        }
        return score;
    }

    float HighwayGenerator::RaycastToHighway(const Point& origin, const Point& direction, float maxDistance) {
        return -1.0f; 
    }

    std::vector<int> HighwayGenerator::FindNearbyRoadIndices(const Point& pos, float radius) {
        std::vector<int> result;
        for (int i = 0; i < m_Roads.size(); ++i) {
            if (m_Roads[i].isDeleted) continue;
            Point p1 = m_RoadNodes[m_Roads[i].startNodeIdx];
            Point p2 = m_RoadNodes[m_Roads[i].endNodeIdx];
            Point closest;
            float d = DistancePointToSegmentClosest(pos, p1, p2, closest);
            if (d < radius) result.push_back(i);
        }
        return result;
    }

    bool HighwayGenerator::CheckParallelDuringGrowth(const Point& start, const Point& end, int startNodeIdx) {
        Point mid((start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f);
        auto nearby = FindNearbyRoadIndices(mid, PARALLEL_GROWTH_CHECK_RADIUS);

        Point dir(end.x - start.x, end.y - start.y);
        float len = std::sqrt(dir.x * dir.x + dir.y * dir.y);
        if (len < 0.1f) return true;
        dir.x /= len; dir.y /= len;

        for (int rIdx : nearby) {
            const auto& road = m_Roads[rIdx];
            if (road.startNodeIdx == startNodeIdx || road.endNodeIdx == startNodeIdx) continue;

            Point p1 = m_RoadNodes[road.startNodeIdx];
            Point p2 = m_RoadNodes[road.endNodeIdx];
            Point rDir(p2.x - p1.x, p2.y - p1.y);
            float rLen = std::sqrt(rDir.x * rDir.x + rDir.y * rDir.y);
            if (rLen < 0.1f) continue;
            rDir.x /= rLen; rDir.y /= rLen;

            float dot = std::abs(dir.x * rDir.x + dir.y * rDir.y);
            if (dot > std::cos(PARALLEL_GROWTH_ANGLE_THRESHOLD)) return false;
        }
        return true;
    }

    void HighwayGenerator::UpdateSleepingBranches() {
        for (int i = m_SleepingBranches.size() - 1; i >= 0; --i) {
            m_SleepingBranches[i].delay--;
            if (m_SleepingBranches[i].delay <= 0) {
                m_ActiveEnds.push_back(HighwayEnd(
                    m_SleepingBranches[i].parentNodeIdx,
                    m_SleepingBranches[i].direction,
                    m_SleepingBranches[i].iterationsLeft,
                    m_SleepingBranches[i].type
                ));
                m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
            }
        }
    }

    //  Highway Management Logic

    void HighwayGenerator::CreateHighway(int startIntersection, int endIntersection, const std::vector<int>& roads) {
        if (roads.empty()) return;

        float totalLen = 0.0f;
        for (int rIdx : roads) {
            if (rIdx >= 0 && rIdx < m_Roads.size()) {
                Point p1 = m_RoadNodes[m_Roads[rIdx].startNodeIdx];
                Point p2 = m_RoadNodes[m_Roads[rIdx].endNodeIdx];
                totalLen += std::sqrt(std::pow(p2.x - p1.x, 2) + std::pow(p2.y - p1.y, 2));
            }
        }

        Highway newHw(startIntersection, endIntersection, roads, totalLen);
        if (CheckAndRemoveRedundantHighway(newHw)) {
            m_Highways.push_back(newHw);
        }
    }

    bool HighwayGenerator::CheckAndRemoveRedundantHighway(const Highway& newHighway) {
        for (int i = m_Highways.size() - 1; i >= 0; --i) {
            const auto& existing = m_Highways[i];
            bool same = (existing.startIntersectionIdx == newHighway.startIntersectionIdx && existing.endIntersectionIdx == newHighway.endIntersectionIdx);
            bool opp = (existing.startIntersectionIdx == newHighway.endIntersectionIdx && existing.endIntersectionIdx == newHighway.startIntersectionIdx);

            if (same || opp) {
                if (newHighway.totalLength < existing.totalLength) {
                    RemoveHighway(i);
                    return true;
                }
                else {
                    RemoveRoads(newHighway.roadIndices);
                    return false;
                }
            }
        }
        return true;
    }

    void HighwayGenerator::RemoveHighway(int highwayIdx) {
        if (highwayIdx >= 0 && highwayIdx < m_Highways.size()) {
            RemoveRoads(m_Highways[highwayIdx].roadIndices);
            m_Highways.erase(m_Highways.begin() + highwayIdx);
        }
    }

    void HighwayGenerator::RemoveRoads(const std::vector<int>& roadIndices) {
        for (int idx : roadIndices) {
            if (idx >= 0 && idx < m_Roads.size()) {
                m_Roads[idx].isDeleted = true;
            }
        }
    }

    void HighwayGenerator::SplitHighwaysAtIntersection(int intersectionNodeIdx) {
        for (int i = m_Highways.size() - 1; i >= 0; --i) {
            const auto& hw = m_Highways[i];
            if (hw.startIntersectionIdx == intersectionNodeIdx || hw.endIntersectionIdx == intersectionNodeIdx) continue;

            auto it = std::find_if(hw.roadIndices.begin(), hw.roadIndices.end(), [&](int rIdx) {
                const auto& r = m_Roads[rIdx];
                return r.endNodeIdx == intersectionNodeIdx || r.startNodeIdx == intersectionNodeIdx;
                });

            if (it != hw.roadIndices.end()) {
                std::vector<int> part1, part2;
                bool passedSplit = false;

                for (int rIdx : hw.roadIndices) {
                    if (!passedSplit) {
                        part1.push_back(rIdx);
                        if (m_Roads[rIdx].endNodeIdx == intersectionNodeIdx || m_Roads[rIdx].startNodeIdx == intersectionNodeIdx) {
                            passedSplit = true;
                        }
                    }
                    else {
                        part2.push_back(rIdx);
                    }
                }

                int oldStart = hw.startIntersectionIdx;
                int oldEnd = hw.endIntersectionIdx;

                m_Highways.erase(m_Highways.begin() + i);
                CreateHighway(oldStart, intersectionNodeIdx, part1);
                CreateHighway(intersectionNodeIdx, oldEnd, part2);
            }
        }
    }

    void HighwayGenerator::MergeSimpleIntersections() {
        // Logika opcjonalna - łączenie highwayów
    }

    void HighwayGenerator::PostProcessIntersections() {
        for (size_t i = 0; i < m_RoadNodes.size(); ++i) {
            if (m_RoadNodes[i].isIntersection) {
                SplitHighwaysAtIntersection(i);
            }
        }
    }

    //  Helpers Math 

    float HighwayGenerator::GetDensityAt(int x, int y) const {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return 0.0f;
        return m_PopulationDensity[y][x];
    }

    bool HighwayGenerator::DoSegmentsIntersect(const Point& a1, const Point& a2, const Point& b1, const Point& b2, Point& intersection) const {
        float s1_x, s1_y, s2_x, s2_y;
        s1_x = a2.x - a1.x; s1_y = a2.y - a1.y;
        s2_x = b2.x - b1.x; s2_y = b2.y - b1.y;
        float s, t;
        float denom = -s2_x * s1_y + s1_x * s2_y;
        if (denom == 0) return false;
        s = (-s1_y * (a1.x - b1.x) + s1_x * (a1.y - b1.y)) / denom;
        t = (s2_x * (a1.y - b1.y) - s2_y * (a1.x - b1.x)) / denom;
        if (s >= 0 && s <= 1 && t >= 0 && t <= 1) {
            intersection.x = a1.x + (t * s1_x);
            intersection.y = a1.y + (t * s1_y);
            return true;
        }
        return false;
    }

    float HighwayGenerator::DistancePointToSegmentClosest(const Point& p, const Point& a, const Point& b, Point& closest) const {
        float l2 = std::pow(a.x - b.x, 2) + std::pow(a.y - b.y, 2);
        if (l2 == 0) { closest = a; return std::sqrt(std::pow(p.x - a.x, 2) + std::pow(p.y - a.y, 2)); }
        float t = ((p.x - a.x) * (b.x - a.x) + (p.y - a.y) * (b.y - a.y)) / l2;
        t = std::max(0.0f, std::min(1.0f, t));
        closest.x = a.x + t * (b.x - a.x);
        closest.y = a.y + t * (b.y - a.y);
        return std::sqrt(std::pow(p.x - closest.x, 2) + std::pow(p.y - closest.y, 2));
    }

    float HighwayGenerator::DistancePointToSegment(const Point& p, const Point& a, const Point& b) const {
        Point c;
        return DistancePointToSegmentClosest(p, a, b, c);
    }

    bool HighwayGenerator::IsNodeOnHighway(int nodeIdx, const Highway& highway, float tolerance) const {
        return false;
    }

    bool HighwayGenerator::IsPointInPolygon(const Point& p, const std::vector<Point>& poly) const {
        bool inside = false;
        for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
            if (((poly[i].y > p.y) != (poly[j].y > p.y)) &&
                (p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x)) {
                inside = !inside;
            }
        }
        return inside;
    }

    bool HighwayGenerator::SegmentIntersectsPolygon(const Point& a, const Point& b, const std::vector<Point>& poly) const {
        if (IsPointInPolygon(a, poly) || IsPointInPolygon(b, poly)) return true;
        for (size_t i = 0; i < poly.size(); ++i) {
            Point p1 = poly[i];
            Point p2 = poly[(i + 1) % poly.size()];
            Point dummy;
            if (DoSegmentsIntersect(a, b, p1, p2, dummy)) return true;
        }
        return false;
    }

    float HighwayGenerator::DistanceSegmentToPolyline(const Point& a, const Point& b, const std::vector<Point>& poly) const {
        float minDist = 1e9f;
        for (size_t i = 0; i < poly.size() - 1; ++i) {
            Point c;
            minDist = std::min(minDist, DistancePointToSegmentClosest(a, poly[i], poly[i + 1], c));
            minDist = std::min(minDist, DistancePointToSegmentClosest(b, poly[i], poly[i + 1], c));
        }
        return minDist;
    }

} // namespace CityGen