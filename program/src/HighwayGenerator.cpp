#include "HighwayGenerator.h"
#include <iostream>
#include <algorithm>
#include <random>
#include <cstdlib>
#include <cmath>
#include <ctime>

namespace CityGen {

    HighwayGenerator::HighwayGenerator(int width, int height)
        : m_Width(width)
        , m_Height(height)
        , m_MaxBridges(999) // Default: unlimited
        , m_BridgesCreated(0)
        , m_IsSeeding(false)
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
        m_BridgesCreated = 0; // Reset bridge counter

        std::cout << "[CityGen] Bridge limit set to: " << m_MaxBridges << std::endl;

        // KROK 1: Generowanie mapy gęstości zaludnienia
        // Uwzględnia ona strefy (parki, rzeki) poprzez m_ZoneMask
        GeneratePopulationDensity();

        // KROK 2: Faza 1 - Główne arterie (Highways / Bus Routes)
        // Zgodnie z PDF: Łączą centra populacji i tworzą szkielet miasta
        std::cout << "[CityGen] Phase 1: Generating Highways..." << std::endl;
        GenerateHighways();
        std::cout << "[CityGen] Phase 1 complete - Highways: " << m_Highways.size()
                  << ", Roads: " << m_Roads.size()
                  << ", Nodes: " << m_RoadNodes.size() << std::endl;

        // KROK 2.5: Seed dodatkowych mostów jeśli nie osiągnięto targetu
        std::cout << "[CityGen] Phase 1.5: Seeding additional bridges if needed..." << std::endl;
        m_IsSeeding = true; // Enable seeding mode for bridge diagnostics
        SeedAdditionalBridges();
        // m_IsSeeding is set to false inside SeedAdditionalBridges() when done

        // Post-processing dla Highways (dzielenie na skrzyżowaniach, usuwanie duplikatów)
        std::cout << "[CityGen] Phase 1 Post-processing..." << std::endl;
        PostProcessIntersections();
        MergeSimpleIntersections();
        RemoveShortHighways();
        RemoveParallelHighwaysByTrend();

        // KROK 3: Faza 2 - Ulice lokalne (Streets / Taxi Routes)
        // Zgodnie z PDF: Wypełniają luki między autostradami, używając wzorców (Raster/Organic)
        std::cout << "[CityGen] Phase 2: Generating Streets..." << std::endl;
        GenerateStreets();

        std::cout << "[CityGen] Generation complete. Nodes: " << m_RoadNodes.size()
            << ", Roads: " << m_Roads.size()
            << ", Highways: " << m_Highways.size() << std::endl;

        std::cout << "[CityGen] Bridges created: " << m_BridgesCreated << " / " << m_MaxBridges << std::endl;

        // Bridge diagnostics
        int bridgeHighwayCount = 0;
        for (size_t i = 0; i < m_Highways.size(); ++i) {
            if (m_Highways[i].containsBridge) {
                bridgeHighwayCount++;
            }
        }
        std::cout << "[Bridge] Natural: " << m_BridgeDiag.naturalBridges
                  << ", Seeded: " << m_BridgeDiag.seededBridges
                  << ", Highways with bridge flag: " << bridgeHighwayCount << std::endl;
    }


    void HighwayGenerator::GeneratePopulationDensity() {
        // Inicjalizacja mapy zerami
        m_PopulationDensity.resize(m_Height, std::vector<float>(m_Width, 0.0f));

        // Generowanie losowych centrów populacji z Poisson Disk Sampling
        struct Center { float x, y, r, i; };
        std::vector<Center> centers;

        // Parametry generacji
        const int numCenters = 10; // Stała liczba centrów
        // const int numCenters = 10 + rand() % 4; // 10-13 centrów
        const float minDistance = std::min(m_Width, m_Height) * 0.25f; // Min odległość między centrami
        const int maxAttempts = 30; // Maksymalna liczba prób na centrum
        const float MIN_RIVER_DISTANCE = 80.0f; // Minimalna odległość od brzegu rzeki

        // Sprawdź czy mamy rzekę - jeśli tak, wymuszaj centra po obu stronach
        bool hasRiver = HasRiver();
        int leftSideCenters = 0;
        int rightSideCenters = 0;
        int requiredPerSide = hasRiver ? 2 : 0; // Minimum 2 centra po każdej stronie

        for (int i = 0; i < numCenters; ++i) {
            bool placed = false;

            // Określ docelową stronę rzeki (jeśli trzeba wymusić balans)
            int targetSide = 0; // 0 = dowolna, -1 = lewa, 1 = prawa
            if (hasRiver) {
                if (leftSideCenters < requiredPerSide) {
                    targetSide = -1;
                } else if (rightSideCenters < requiredPerSide) {
                    targetSide = 1;
                }
            }

            for (int attempt = 0; attempt < maxAttempts && !placed; ++attempt) {
                // Losowa pozycja z marginesem od krawędzi
                float x = (m_Width * 0.1f) + (rand() % (int)(m_Width * 0.8f));
                float y = (m_Height * 0.1f) + (rand() % (int)(m_Height * 0.8f));

                // NOWE: Sprawdź wymagania dotyczące rzeki
                if (hasRiver) {
                    // Sprawdź odległość od rzeki
                    float distToRiver = DistanceToRiver((int)x, (int)y);
                    if (distToRiver < MIN_RIVER_DISTANCE) {
                        continue; // Za blisko rzeki
                    }

                    // Sprawdź czy jest po właściwej stronie rzeki (jeśli wymuszamy)
                    if (targetSide != 0) {
                        int actualSide = DetermineRiverSide((int)x, (int)y);
                        if (actualSide != targetSide) {
                            continue; // Niewłaściwa strona rzeki
                        }
                    }
                }

                // Sprawdź odległość od innych centrów
                bool tooClose = false;
                for (const auto& existing : centers) {
                    float dx = x - existing.x;
                    float dy = y - existing.y;
                    float dist = std::sqrt(dx * dx + dy * dy);

                    if (dist < minDistance) {
                        tooClose = true;
                        break;
                    }
                }

                if (!tooClose) {
                    // Losowe parametry centrum
                    float radius = 100.0f + (rand() % 250); // 100-350
                    float intensity = 0.7f + (rand() % 30) / 100.0f; // 0.7-1.0

                    centers.push_back({x, y, radius, intensity});
                    placed = true;

                    // Zlicz centra po stronach rzeki
                    if (hasRiver) {
                        int side = DetermineRiverSide((int)x, (int)y);
                        if (side == -1) leftSideCenters++;
                        else if (side == 1) rightSideCenters++;
                    }

                }
            }
            
            // Jeśli nie udało się umieścić centrum, spróbuj z mniejszą minDistance
            if (!placed && i > 0) {
                float relaxedDistance = minDistance * 0.6f;
                for (int attempt = 0; attempt < maxAttempts && !placed; ++attempt) {
                    float x = (m_Width * 0.1f) + (rand() % (int)(m_Width * 0.8f));
                    float y = (m_Height * 0.1f) + (rand() % (int)(m_Height * 0.8f));

                    // NOWE: Sprawdź wymagania dotyczące rzeki (także w fallback)
                    if (hasRiver) {
                        float distToRiver = DistanceToRiver((int)x, (int)y);
                        if (distToRiver < MIN_RIVER_DISTANCE * 0.7f) { // Nieco zrelaksowany wymóg
                            continue;
                        }

                        if (targetSide != 0) {
                            int actualSide = DetermineRiverSide((int)x, (int)y);
                            if (actualSide != targetSide) {
                                continue;
                            }
                        }
                    }

                    bool tooClose = false;
                    for (const auto& existing : centers) {
                        float dx = x - existing.x;
                        float dy = y - existing.y;
                        float dist = std::sqrt(dx * dx + dy * dy);

                        if (dist < relaxedDistance) {
                            tooClose = true;
                            break;
                        }
                    }

                    if (!tooClose) {
                        float radius = 100.0f + (rand() % 250);
                        float intensity = 0.7f + (rand() % 30) / 100.0f;
                        centers.push_back({x, y, radius, intensity});
                        placed = true;

                        // Zlicz centra po stronach rzeki
                        if (hasRiver) {
                            int side = DetermineRiverSide((int)x, (int)y);
                            if (side == -1) leftSideCenters++;
                            else if (side == 1) rightSideCenters++;
                        }
                    }
                }
            }
        }


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
                        // val = 0.0f; // Zerowa gęstość w strefach wykluczonych
                    }
                }

                m_PopulationDensity[y][x] = std::min(1.0f, val);
            }
        }
    }

    // stara wersja ze stałymi pozycjami centrów
    // void HighwayGenerator::GeneratePopulationDensity() {
    //     // Inicjalizacja mapy zerami
    //     m_PopulationDensity.resize(m_Height, std::vector<float>(m_Width, 0.0f));

    //     // Definicja centrów populacji (Dzielnice)
    //     struct Center { float x, y, r, i; };
    //     std::vector<Center> centers = {
    //         {m_Width * 0.30f, m_Height * 0.45f, 200.0f, 1.0f},
    //         {m_Width * 0.65f, m_Height * 0.3f, 300.0f, 0.9f},
    //         {m_Width * 0.35f, m_Height * 0.65f, 100.0f, 0.85f},
    //         {m_Width * 0.6f, m_Height * 0.65f, 250.0f, 0.95f}
    //     };

    //     for (int y = 0; y < m_Height; ++y) {
    //         for (int x = 0; x < m_Width; ++x) {
    //             float val = 0.0f;

    //             // Sumowanie wpływu centrów (metoda gradientowa)
    //             for (const auto& c : centers) {
    //                 float dx = x - c.x;
    //                 float dy = y - c.y;
    //                 float dist = std::sqrt(dx * dx + dy * dy);
    //                 if (dist < c.r) {
    //                     // Funkcja kwadratowa zaniku (falloff)
    //                     float t = 1.0f - (dist / c.r);
    //                     val += c.i * (t * t);
    //                 }
    //             }

    //             // Zastosowanie MASKI STREF (m_ZoneMask)
    //             // To zapewnia, że populacja (i drogi) nie pojawią się w rzekach i parkach
    //             if (!m_ZoneMask.empty()) {
    //                 int idx = y * m_Width + x;
    //                 if (idx < m_ZoneMask.size() && m_ZoneMask[idx] != 0) {
    //                     // val = 0.0f; // Zerowa gęstość w strefach wykluczonych
    //                 }
    //             }

    //             m_PopulationDensity[y][x] = std::min(1.0f, val);
    //         }
    //     }
    // }

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
        int totalHighwaysCreated = 0;
        while ((!m_ActiveEnds.empty() || !m_SleepingBranches.empty()) && iterations++ < 5000) {
        for (int i = m_ActiveEnds.size() - 1; i >= 0; --i) {
            bool grown = GrowOneStep(m_ActiveEnds[i]);
            
            if (!grown) {
                // Jeśli highway zakończył bieg, sfinalizuj go
                if (m_ActiveEnds[i].type == RoadType::HIGHWAY && !m_ActiveEnds[i].roadsSinceLastIntersection.empty()) {
                    CreateHighway(m_ActiveEnds[i].lastIntersectionIdx, m_ActiveEnds[i].currentNodeIdx, m_ActiveEnds[i].roadsSinceLastIntersection, m_ActiveEnds[i].hasCrossedBridge);
                    totalHighwaysCreated++;
                }
                m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                continue;
            }
            
            // NOWE: Sprawdź rozmiar przed i po CreateBranchCandidates
            size_t activeEndsBeforeCreate = m_ActiveEnds.size();
            CreateBranchCandidates(m_ActiveEnds[i]);
            size_t activeEndsAfterCreate = m_ActiveEnds.size();
            
            // Jeśli utworzono nową kontynuację, usuń starego agenta
            bool newEndCreated = (activeEndsAfterCreate > activeEndsBeforeCreate);

            if (newEndCreated) {
                m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                continue;
            }
            
            // Reset dystansu jeśli utworzono branche (ale NIE kontynuację)
            bool createdBranches = false;
            for (const auto& branch : m_SleepingBranches) {
                if (branch.parentNodeIdx == m_ActiveEnds[i].currentNodeIdx && branch.delay >= 0) {
                    createdBranches = true;
                    break;
                }
            }
            if (createdBranches) {
                m_ActiveEnds[i].distanceSinceLastBranch = 0.0f;
            }
        }
        UpdateSleepingBranches();
        }

        // Sprawdź czy wszystkie centra populacji są połączone
        CheckAndSeedUnconnectedCenters();
        
        // Jeśli zasiano nowe highways, kontynuuj generację
        if (!m_ActiveEnds.empty()) {
            while ((!m_ActiveEnds.empty() || !m_SleepingBranches.empty()) && iterations++ < 5000) {
                // Identyczna logika jak w głównej pętli
                for (int i = m_ActiveEnds.size() - 1; i >= 0; --i) {
                    bool grown = GrowOneStep(m_ActiveEnds[i]);
                    
                    if (!grown) {
                        if (m_ActiveEnds[i].type == RoadType::HIGHWAY && !m_ActiveEnds[i].roadsSinceLastIntersection.empty()) {
                            CreateHighway(m_ActiveEnds[i].lastIntersectionIdx, m_ActiveEnds[i].currentNodeIdx, m_ActiveEnds[i].roadsSinceLastIntersection, m_ActiveEnds[i].hasCrossedBridge);
                        }
                        m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                        continue;
                    }
                    
                    size_t activeEndsBeforeCreate = m_ActiveEnds.size();
                    CreateBranchCandidates(m_ActiveEnds[i]);
                    size_t activeEndsAfterCreate = m_ActiveEnds.size();
                    
                    bool newEndCreated = (activeEndsAfterCreate > activeEndsBeforeCreate);
                    
                    if (newEndCreated) {
                        m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                        continue;
                    }
                }
                UpdateSleepingBranches();
            }
        }

    }

    void HighwayGenerator::SeedAdditionalBridges() {
        if (!HasRiver()) return;
        if (m_BridgesCreated >= m_MaxBridges) return;

        int bridgesNeeded = m_MaxBridges - m_BridgesCreated;
        std::cout << "[SeedBridges] Need " << bridgesNeeded << " more bridges" << std::endl;

        // Struktura kandydata na punkt startu mostu
        struct BridgeCandidate {
            int nodeIdx;
            float distToRiver;
            int side; // -1 lub 1
            Point dirToRiver;
        };

        std::vector<BridgeCandidate> candidates;

        // Znajdź wszystkie węzły highway w pobliżu rzeki
        const float MAX_DISTANCE_FROM_RIVER = 400.0f;
        const float MIN_DISTANCE_FROM_RIVER = 30.0f;

        for (size_t i = 0; i < m_RoadNodes.size(); ++i) {
            const Point& node = m_RoadNodes[i];

            // Sprawdź czy węzeł należy do jakiejś drogi (jest podłączony)
            if (node.connectedRoadIndices.empty()) continue;

            // Sprawdź odległość od rzeki
            float distToRiver = DistanceToRiver((int)node.x, (int)node.y);

            if (distToRiver < MIN_DISTANCE_FROM_RIVER || distToRiver > MAX_DISTANCE_FROM_RIVER) {
                continue; // Za blisko lub za daleko
            }

            // Określ stronę rzeki
            int side = DetermineRiverSide((int)node.x, (int)node.y);
            if (side == 0) continue; // Na rzece

            // Znajdź kierunek do najbliższego punktu rzeki
            Point dirToRiver(0.0f, 0.0f);
            float minDist = 999999.0f;

            for (int dy = -100; dy <= 100; dy += 5) {
                for (int dx = -100; dx <= 100; dx += 5) {
                    int rx = (int)node.x + dx;
                    int ry = (int)node.y + dy;

                    if (rx >= 0 && rx < m_Width && ry >= 0 && ry < m_Height) {
                        if (IsRiver(rx, ry)) {
                            float dist = std::sqrt((float)(dx * dx + dy * dy));
                            if (dist < minDist) {
                                minDist = dist;
                                dirToRiver.x = dx / dist;
                                dirToRiver.y = dy / dist;
                            }
                        }
                    }
                }
            }

            if (minDist < 999999.0f) {
                candidates.push_back({(int)i, distToRiver, side, dirToRiver});
            }
        }

        m_BridgeDiag.candidatesFound = (int)candidates.size();

        if (candidates.empty()) {
            return;
        }

        // Sortuj kandydatów po odległości od rzeki (najbliżsi pierwsi)
        std::sort(candidates.begin(), candidates.end(),
            [](const BridgeCandidate& a, const BridgeCandidate& b) {
                return a.distToRiver < b.distToRiver;
            });

        // Wybierz bridgesNeeded kandydatów, starając się o równomierne rozmieszczenie
        std::vector<BridgeCandidate> selected;
        const float MIN_DISTANCE_BETWEEN_BRIDGES = 100.0f;

        for (const auto& candidate : candidates) {
            if ((int)selected.size() >= bridgesNeeded) break;

            // Sprawdź czy nie jest za blisko już wybranych
            bool tooClose = false;
            for (const auto& sel : selected) {
                Point& selNode = m_RoadNodes[sel.nodeIdx];
                Point& candNode = m_RoadNodes[candidate.nodeIdx];

                float dx = selNode.x - candNode.x;
                float dy = selNode.y - candNode.y;
                float dist = std::sqrt(dx * dx + dy * dy);

                if (dist < MIN_DISTANCE_BETWEEN_BRIDGES) {
                    tooClose = true;
                    break;
                }
            }

            if (!tooClose) {
                selected.push_back(candidate);
            }
        }

        m_BridgeDiag.spawnPointsSelected = (int)selected.size();

        // Spawn agentów dla każdego wybranego punktu
        for (const auto& spawn : selected) {
            Point& startNode = m_RoadNodes[spawn.nodeIdx];

            // Oblicz kierunek prostopadły do rzeki
            Point bridgeDir = FindRiverCrossingDirection(startNode, spawn.dirToRiver);

            // Utwórz agenta z wyłączonym branchowaniem - daj mu pełny limit iteracji jak normalny highway
            HighwayEnd agent(spawn.nodeIdx, bridgeDir, HIGHWAY_MAX_ITERATIONS, RoadType::HIGHWAY);
            agent.disableBranching = true; // Wyłącz tworzenie gałęzi bocznych
            m_ActiveEnds.push_back(agent);
            m_BridgeDiag.agentsSpawned++;
        }
        int iterations = 0;
        while (!m_ActiveEnds.empty() && iterations++ < 200) {
            m_BridgeDiag.seededIterations++; // Diagnostic
            for (int i = m_ActiveEnds.size() - 1; i >= 0; --i) {
                bool grown = GrowOneStep(m_ActiveEnds[i]);

                if (!grown) {
                    m_ActiveEnds.erase(m_ActiveEnds.begin() + i);
                }
            }

            // Przerwij gdy osiągniemy target
            if (m_BridgesCreated >= m_MaxBridges) {
                break;
            }
        }

        // Wyłącz tryb seedowania
        m_IsSeeding = false;
    }

    void HighwayGenerator::GenerateStreets() {
        // Space Colonization: Skanujemy mapę w poszukiwaniu pustych miejsc

        int attempts = 500; // Ilość prób zasiania ulic
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
            std::vector<int> nearby = FindNearbyRoadIndices(p, 60.0f);

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
                // std::cout << "[Highway] End " << i << " stopped growing" << std::endl;
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
        bool isHighway = (agent.type == RoadType::HIGHWAY);

        if (agent.iterationsLeft <= 0) {
            return false;
        }

        Point currentPos = m_RoadNodes[agent.currentNodeIdx];

        // 1. Wybierz kierunek
        Point newDir;

        // Jeśli jesteśmy na moście, używaj zablokowanego kierunku
        if (agent.isOnBridge) {
            newDir = agent.bridgeDirection;
        } else if (agent.type == RoadType::HIGHWAY) {
            newDir = FindBestDirection(currentPos, agent.direction);
        } else {
            // STREETS: Używaj ApplyGlobalGoals (wzorce Raster/Organic/Radial)
            newDir = ApplyGlobalGoals(currentPos, agent.direction, agent.type);
        }

        if (newDir.x == 0 && newDir.y == 0) {
            return false;
        }

        // Długość zależy od typu (autostrady dłuższe, ulice krótsze)
        float len = (agent.type == RoadType::HIGHWAY) ? HIGHWAY_SEGMENT_LENGTH : STREET_SEGMENT_LENGTH;

        // BRIDGE LOGIC: Obsługa zbliżania się do rzeki i przekraczania
        bool currentOnLand = !IsRiver((int)currentPos.x, (int)currentPos.y);

        if (agent.type == RoadType::HIGHWAY && currentOnLand && !agent.isOnBridge) {
            // Sprawdź czy zbliżamy się do rzeki
            if (IsApproachingRiver(currentPos, newDir, len * 2.0f)) {
                // Sprawdź limit mostów
                if (m_BridgesCreated >= m_MaxBridges) {
                    m_BridgeDiag.blockedByLimit++;
                    return false;
                }

                // Znajdź dokładny punkt brzegu
                Point bankIntersection;
                if (FindRiverBankIntersection(currentPos, newDir, len * 3.0f, bankIntersection)) {

                    // Utwórz węzeł na brzegu rzeki
                    int bankNodeIdx = CreateOrGetNode(bankIntersection, false);

                    // Utwórz drogę do brzegu
                    int roadIdx = (int)m_Roads.size();
                    m_Roads.push_back(Road(agent.currentNodeIdx, bankNodeIdx, agent.type));
                    m_RoadNodes[agent.currentNodeIdx].connectedRoadIndices.push_back(roadIdx);
                    m_RoadNodes[bankNodeIdx].connectedRoadIndices.push_back(roadIdx);

                    if (agent.type == RoadType::HIGHWAY) {
                        agent.roadsSinceLastIntersection.push_back(roadIdx);
                    }

                    // Teraz rozpocznij most - oblicz kierunek prostopadły
                    Point bridgeDir = FindRiverCrossingDirection(bankIntersection, newDir);
                    agent.bridgeDirection = bridgeDir;
                    agent.isOnBridge = true;
                    agent.hasCrossedBridge = true; // Mark that this agent has created a bridge
                    agent.currentNodeIdx = bankNodeIdx;
                    agent.direction = bridgeDir;
                    agent.iterationsLeft--;

                    // INCREMENT BRIDGE COUNTER
                    m_BridgesCreated++;
                    if (m_IsSeeding) {
                        m_BridgeDiag.seededBridges++;
                    } else {
                        m_BridgeDiag.naturalBridges++;
                    }
                    return true;
                }
            }
        }

        // Jeśli jesteśmy na moście i dotarliśmy do drugiego brzegu
        if (agent.isOnBridge && IsRiver((int)currentPos.x, (int)currentPos.y)) {
            // Szukaj wyjścia z rzeki
            Point exitBankIntersection;
            if (FindRiverBankIntersection(currentPos, agent.bridgeDirection, len * 3.0f, exitBankIntersection)) {
                // Sprawdź czy to rzeczywiście brzeg (wyjście z rzeki)
                if (!IsRiver((int)exitBankIntersection.x, (int)exitBankIntersection.y)) {

                    // Utwórz węzeł na brzegu wyjścia
                    int exitBankNodeIdx = CreateOrGetNode(exitBankIntersection, false);

                    // Utwórz drogę do brzegu wyjścia
                    int roadIdx = (int)m_Roads.size();
                    m_Roads.push_back(Road(agent.currentNodeIdx, exitBankNodeIdx, agent.type));
                    m_RoadNodes[agent.currentNodeIdx].connectedRoadIndices.push_back(roadIdx);
                    m_RoadNodes[exitBankNodeIdx].connectedRoadIndices.push_back(roadIdx);

                    if (agent.type == RoadType::HIGHWAY) {
                        agent.roadsSinceLastIntersection.push_back(roadIdx);
                    }

                    // Zakończ tryb mostu
                    agent.isOnBridge = false;
                    agent.bridgeDirection = Point(0.0f, 0.0f);
                    agent.currentNodeIdx = exitBankNodeIdx;
                    agent.iterationsLeft--;
                    return true;
                }
            }
        }

        Point newPos(currentPos.x + newDir.x * len, currentPos.y + newDir.y * len);

        // Granice mapy
        if (newPos.x < 0 || newPos.x >= m_Width || newPos.y < 0 || newPos.y >= m_Height) {
            return false;
        }

        // Unikanie równoległych autostrad (dla estetyki)
        // if (agent.type == RoadType::HIGHWAY) {
        //     std::cout << "  [GrowOneStep] Checking parallel constraints..." << std::endl;
        //     if (!CheckParallelDuringGrowth(currentPos, newPos, agent.currentNodeIdx)) {
        //         std::cout << "  [GrowOneStep] Parallel constraint failed, stopping" << std::endl;
        //         return false;
        //     }
        //     std::cout << "  [GrowOneStep] Parallel check passed" << std::endl;
        // }

        // 2. Local Constraints (Kolizje, Snapowanie)
        int newNodeIdx = -1;
        bool constraintsMet = CheckLocalConstraints(currentPos, newPos, agent.currentNodeIdx, newNodeIdx, agent.type);

        if (!constraintsMet) {
            return false;
        }

        bool wasIntersection = m_RoadNodes[newNodeIdx].isIntersection;

        // Dodaj fizyczną drogę do grafu
        m_Roads.push_back(Road(agent.currentNodeIdx, newNodeIdx, agent.type));
        int newRoadIdx = m_Roads.size() - 1;

        // Mark bridge roads for protection against splitting
        if (agent.isOnBridge) {
            m_Roads[newRoadIdx].isPartOfBridge = true;
        }

        m_RoadNodes[agent.currentNodeIdx].connectedRoadIndices.push_back(newRoadIdx);
        m_RoadNodes[newNodeIdx].connectedRoadIndices.push_back(newRoadIdx);

        // if (agent.type == RoadType::HIGHWAY) {
        //     ConsumePopulationDensity(newPos, DENSITY_CONSUME_RADIUS, DENSITY_CONSUME_INTENSITY);
            
        //     if (isHighway) {
        //         std::cout << "  [GrowOneStep] Consumed density at (" 
        //                 << newPos.x << ", " << newPos.y << ")" << std::endl;
        //     }
        // }


        if (agent.type == RoadType::HIGHWAY) {
            agent.roadsSinceLastIntersection.push_back(newRoadIdx);
        }

        // Logika Highways (zarządzanie obiektami Highway)
        if (agent.type == RoadType::HIGHWAY) {
            // Sprawdź czy trafiło na skrzyżowanie (inne niż aktualny węzeł)
            if (m_RoadNodes[newNodeIdx].isIntersection && newNodeIdx != agent.currentNodeIdx) {
                // Utwórz Highway dla dotychczasowego odcinka
                if (!agent.roadsSinceLastIntersection.empty()) {
                    CreateHighway(agent.lastIntersectionIdx, newNodeIdx, agent.roadsSinceLastIntersection, agent.hasCrossedBridge);
                }
                
                // Podziel highways jeśli trzeba
                if (!wasIntersection) {
                    SplitHighwaysAtIntersection(newNodeIdx);
                }
                
                // Zakończ ten agent (trafiliśmy na intersection)
                agent.currentNodeIdx = newNodeIdx;
                return false;
            }
            
            // Przeszliśmy przez zwykły węzeł - kontynuuj tracking
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
        agent.distanceSinceLastBranch += len;
        agent.iterationsLeft--;

        //Jeśli to HIGHWAY i przeszliśmy przez węzeł który stał się intersection
        if (agent.type == RoadType::HIGHWAY && m_RoadNodes[newNodeIdx].isIntersection) {
            
            // Podziel highways jeśli trzeba
            if (!wasIntersection) {
                SplitHighwaysAtIntersection(newNodeIdx);
            }
            
            // Utwórz Highway dla dotychczasowego odcinka
            if (!agent.roadsSinceLastIntersection.empty()) {
                CreateHighway(agent.lastIntersectionIdx, newNodeIdx, agent.roadsSinceLastIntersection, agent.hasCrossedBridge);
            }
            
            // Zresetuj tracking - zaczynamy nowy Highway
            agent.lastIntersectionIdx = newNodeIdx;
            agent.roadsSinceLastIntersection.clear();
        }

        return true;
    }

    void HighwayGenerator::CreateBranchCandidates(const HighwayEnd& parent) {
        // Jeśli agent ma wyłączone branchowanie, nie twórz gałęzi
        if (parent.disableBranching) {
            return;
        }

        float distThreshold = (parent.type == RoadType::HIGHWAY) ? HIGHWAY_BRANCH_DISTANCE : STREET_BRANCH_DISTANCE;

        if (parent.distanceSinceLastBranch > distThreshold) {
            // Sprawdź czy węzeł rodzica jest na rzece - jeśli tak, blokuj tworzenie branchy
            Point parentPos = m_RoadNodes[parent.currentNodeIdx];
            if (IsRiver((int)parentPos.x, (int)parentPos.y)) {
                return;
            }

            PatternType pattern = GetPatternAt(m_RoadNodes[parent.currentNodeIdx].x, m_RoadNodes[parent.currentNodeIdx].y);

            // Kąt rozgałęzienia zależny od wzorca
            float branchAngle = (parent.type == RoadType::HIGHWAY) ? BRANCH_ANGLE : (1.5707f); // 90 stopni dla ulic

            // Dla organicznych ulic losowy kąt
            if (pattern == PatternType::ORGANIC && parent.type == RoadType::STREET) {
                branchAngle = 0.5f + (rand() % 100 / 100.0f);
            }

            float currentAngle = std::atan2(parent.direction.y, parent.direction.x);
            int prob = (parent.type == RoadType::HIGHWAY) ? HIGHWAY_BRANCH_PROBABILITY : STREET_BRANCH_PROBABILITY;

            // Próba lewa i prawa
            if (rand() % 100 < prob) {
                Point dir(std::cos(currentAngle + branchAngle), std::sin(currentAngle + branchAngle));
                Branch newBranch(parent.currentNodeIdx, dir, -1, parent.iterationsLeft, parent.type);
                
                // Waliduj branch przez GlobalGoals (tylko dla HIGHWAY)
                if (parent.type == RoadType::HIGHWAY) {
                    GlobalGoalsForBranch(newBranch, parent);
                } else {
                    // Streets - ustaw prosty delay
                    newBranch.delay = 5;
                }
                
                if (newBranch.delay >= 0) {
                    m_SleepingBranches.push_back(newBranch);
                }
            }
            if (rand() % 100 < prob) {
                Point dir(std::cos(currentAngle - branchAngle), std::sin(currentAngle - branchAngle));
                Branch newBranch(parent.currentNodeIdx, dir, -1, parent.iterationsLeft, parent.type);
                
                // Waliduj branch przez GlobalGoals (tylko dla HIGHWAY)
                if (parent.type == RoadType::HIGHWAY) {
                    GlobalGoalsForBranch(newBranch, parent);
                } else {
                    // Streets - ustaw prosty delay
                    newBranch.delay = 5;
                }
                
                if (newBranch.delay >= 0) {
                    m_SleepingBranches.push_back(newBranch);
                }
            }

            // Jeśli Highway się rozgałęzia, oznaczamy węzeł jako skrzyżowanie
            if (parent.type == RoadType::HIGHWAY && !m_SleepingBranches.empty()) {
                if (m_SleepingBranches.back().parentNodeIdx == parent.currentNodeIdx) {
                    m_RoadNodes[parent.currentNodeIdx].isIntersection = true;
                    SplitHighwaysAtIntersection(parent.currentNodeIdx);
                }
            }
        }
        // Sprawdź czy utworzono jakieś akceptowalne branche
        bool hasAcceptedBranches = false;
        for (const auto& branch : m_SleepingBranches) {
            if (branch.parentNodeIdx == parent.currentNodeIdx && branch.delay >= 0) {
                hasAcceptedBranches = true;
                break;
            }
        }
        

        // Jeśli są branche i to HIGHWAY - kontynuuj wzrost po rozgałęzieniu
        if (hasAcceptedBranches && parent.type == RoadType::HIGHWAY) {
            bool wasIntersection = m_RoadNodes[parent.currentNodeIdx].isIntersection;
            m_RoadNodes[parent.currentNodeIdx].isIntersection = true;

            // Podziel highways jeśli trzeba
            if (!wasIntersection) {
                SplitHighwaysAtIntersection(parent.currentNodeIdx);
            }

            // Utwórz Highway dla dotychczasowego odcinka
            if (!parent.roadsSinceLastIntersection.empty()) {
                CreateHighway(parent.lastIntersectionIdx, parent.currentNodeIdx,
                            parent.roadsSinceLastIntersection);
            }

            //  Utwórz nowy HighwayEnd kontynuujący wzrost w tym samym kierunku
            HighwayEnd newEnd(
                parent.currentNodeIdx,
                parent.direction,
                parent.iterationsLeft,
                parent.type
            );
            
            m_ActiveEnds.push_back(newEnd);
        }
    }

    void HighwayGenerator::GlobalGoalsForBranch(Branch& branch, const HighwayEnd& parent) {
        // ========== FLAGI AKTYWACJI KRYTERIÓW ==========
        const bool ENABLE_DENSITY_CHECK = true;
        const bool ENABLE_BOUNDS_CHECK = false;
        const bool ENABLE_PARALLEL_CHECK = true;
        const bool ENABLE_AREA_COVERAGE_CHECK = false;
        const bool ENABLE_HIGHWAY_PROXIMITY_CHECK = true;
        const bool ENABLE_PERPENDICULAR_SCAN_CHECK = false;
        
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
            
            if (score < MIN_DENSITY_SCORE) {
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

        // KRYTERIUM 6: Wykrywanie równoległych branchy w sąsiedztwie
        if (ENABLE_PERPENDICULAR_SCAN_CHECK) {
            Point parentPos = m_RoadNodes[branch.parentNodeIdx];
            
            // Oblicz kierunek prostopadły do brancha
            Point perpendicular(-branch.direction.y, branch.direction.x);
            
            std::cout << "[GlobalGoals] Perpendicular scan check..." << std::endl;
            
            // Skanuj w obu kierunkach prostopadłych
            for (int side = -1; side <= 1; side += 2) {
                Point scanDir(perpendicular.x * side, perpendicular.y * side);
                
                // Strzel rayem prostopadle
                float hitDist = RaycastToHighway(parentPos, scanDir, BRANCH_PARALLEL_SCAN_DISTANCE);
                
                if (hitDist > 0.0f) {
                    // Trafiliśmy w highway - sprawdź jego kierunek
                    Point hitPoint(
                        parentPos.x + scanDir.x * hitDist,
                        parentPos.y + scanDir.y * hitDist
                    );
                    
                    // Znajdź trafiony highway i jego kierunek
                    for (const Highway& hw : m_Highways) {
                        if (IsPointOnHighway(hitPoint, hw, 5.0f)) {
                            // Znajdź segment na którym leży punkt
                            for (int roadIdx : hw.roadIndices) {
                                if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
                                
                                const Road& road = m_Roads[roadIdx];
                                const Point& roadStart = m_RoadNodes[road.startNodeIdx];
                                const Point& roadEnd = m_RoadNodes[road.endNodeIdx];
                                
                                // Sprawdź czy punkt jest blisko tego segmentu
                                float dist = DistancePointToSegment(hitPoint, roadStart, roadEnd);
                                if (dist < 5.0f) {
                                    // Oblicz kierunek tego segmentu
                                    Point hwDir(roadEnd.x - roadStart.x, roadEnd.y - roadStart.y);
                                    float hwLen = std::sqrt(hwDir.x * hwDir.x + hwDir.y * hwDir.y);
                                    if (hwLen > 0.01f) {
                                        hwDir.x /= hwLen;
                                        hwDir.y /= hwLen;
                                    }
                                    
                                    // Oblicz kąt między branch a highway
                                    float dotProduct = branch.direction.x * hwDir.x + branch.direction.y * hwDir.y;
                                    float angle = std::acos(std::max(-1.0f, std::min(1.0f, dotProduct)));
                                    
                                    // Uwzględnij symetrię (180° to też równoległość)
                                    if (angle > 3.14159f / 2.0f) {
                                        angle = 3.14159f - angle;
                                    }
                                    
                                    std::cout << "[GlobalGoals] Found highway at distance " << hitDist 
                                            << ", angle difference: " << (angle * 180.0f / 3.14159f) << " degrees" << std::endl;
                                    
                                    // Jeśli kąt mniejszy niż tolerancja - równoległe
                                    if (angle < BRANCH_PARALLEL_SCAN_ANGLE_TOLERANCE) {
                                        std::cout << "[GlobalGoals] REJECTED: Parallel highway nearby (perpendicular scan)" << std::endl;
                                        branch.delay = -1;
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // WSZYSTKIE KRYTERIA SPEŁNIONE - ustaw losowy delay
        branch.delay = BRANCH_DELAY_MIN + (rand() % (BRANCH_DELAY_MAX - BRANCH_DELAY_MIN + 1));
        std::cout << "[GlobalGoals] ACCEPTED: delay = " << branch.delay << std::endl;
    }

    bool HighwayGenerator::CheckLocalConstraints(const Point& start, const Point& end, int startNodeIdx, int& endNodeIdx, RoadType type) {
        bool isHighway = (type == RoadType::HIGHWAY);
        
        if (isHighway) {
            // std::cout << "    [LocalConstraints] Checking segment from node " << startNodeIdx 
            //         << " (" << start.x << "," << start.y << ") to (" << end.x << "," << end.y << ")" << std::endl;
        }
        
        // 1. Sprawdź maskę terenu (woda/parki) - blokada budowy TYLKO dla STREET
        int ix = (int)end.x;
        int iy = (int)end.y;
        if (type == RoadType::STREET && !m_ZoneMask.empty() && (ix >= 0 && ix < m_Width && iy >= 0 && iy < m_Height)) {
            if (m_ZoneMask[iy * m_Width + ix] != 0) {
                // std::cout << "    [LocalConstraints] STREET BLOCKED by zone mask at (" << ix << "," << iy << ")" << std::endl;
                return false;
            }
        }

        // 2. Snapowanie (przyciąganie) do istniejących węzłów
        float searchRadius = (type == RoadType::HIGHWAY) ? 15.0f : 12.0f;
        int nearest = FindNearestNode(end, searchRadius);
        if (nearest != -1 && nearest != startNodeIdx) {
            if (isHighway) {
                // std::cout << "    [LocalConstraints] Snapping to existing node " << nearest << std::endl;
            }
            endNodeIdx = nearest;
            return true;
        }

        if (isHighway) {
            // std::cout << "    [LocalConstraints] No nearby node found (radius " << searchRadius << "), checking intersections..." << std::endl;
        }

        // 3. Sprawdź przecięcia z innymi drogami
        Point intersection;
        float minDist = 10000.0f;
        int hitRoadIdx = -1;
        int checkedRoads = 0;

        for (int i = 0; i < m_Roads.size(); ++i) {
            const auto& r = m_Roads[i];
            if (r.isDeleted) continue;
            if (r.startNodeIdx == startNodeIdx || r.endNodeIdx == startNodeIdx) continue;
            if (r.isPartOfBridge) continue; // PROTECT BRIDGE ROADS FROM SPLITTING

            checkedRoads++;

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

        if (isHighway) {
            // std::cout << "    [LocalConstraints] Checked " << checkedRoads << " roads for intersection" << std::endl;
        }

        if (hitRoadIdx != -1) {
            if (isHighway) {
                // std::cout << "    [LocalConstraints] Found intersection with road " << hitRoadIdx 
                //         << " at (" << intersection.x << "," << intersection.y << ")" << std::endl;
            }
            
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

            // Update highways that reference the split road
            UpdateHighwaysAfterRoadSplit(hitRoadIdx, r1, r2);

            if (oldRoad.type == RoadType::HIGHWAY) {
                m_RoadNodes[intersectNodeIdx].isIntersection = true;
                SplitHighwaysAtIntersection(intersectNodeIdx);
            }

            endNodeIdx = intersectNodeIdx;
            
            if (isHighway) {
                // std::cout << "    [LocalConstraints] Created intersection node " << intersectNodeIdx << std::endl;
            }
            
            return true;
        }

        // 3b. "WPADNIĘCIE" W ŚRODEK DROGI (distance check)
        if (isHighway) {
            // std::cout << "    [LocalConstraints] No intersection found, checking distance to nearby roads..." << std::endl;
        }
        
        float segmentLength = (type == RoadType::HIGHWAY) ? HIGHWAY_SEGMENT_LENGTH : STREET_SEGMENT_LENGTH;
        const float SEARCH_RADIUS = segmentLength * SEARCH_RADIUS_MULTIPLIER;
        const float HIT_DISTANCE = segmentLength * HIT_DISTANCE_MULTIPLIER;
        
        if (isHighway) {
            // std::cout << "    [LocalConstraints] Search radius: " << SEARCH_RADIUS 
            //         << ", Hit distance: " << HIT_DISTANCE << std::endl;
        }
        
        std::vector<int> nearbyRoadIndices = FindNearbyRoadIndices(start, SEARCH_RADIUS);
        
        if (isHighway) {
            // std::cout << "    [LocalConstraints] Found " << nearbyRoadIndices.size() 
            //         << " nearby roads within radius " << SEARCH_RADIUS << std::endl;
        }
        
        for (int roadIdx : nearbyRoadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;

            const Road& otherRoad = m_Roads[roadIdx];
            if (otherRoad.isDeleted) continue;
            if (otherRoad.isPartOfBridge) continue; // PROTECT BRIDGE ROADS FROM SPLITTING

            // Ignoruj własne drogi
            if (otherRoad.startNodeIdx == startNodeIdx || otherRoad.endNodeIdx == startNodeIdx)
                continue;
            
            const Point& segA = m_RoadNodes[otherRoad.startNodeIdx];
            const Point& segB = m_RoadNodes[otherRoad.endNodeIdx];
            
            Point closest;
            float dist = DistancePointToSegment(end, segA, segB);
            
            if (isHighway && dist < HIT_DISTANCE * 2.0f) {  // Log jeśli blisko
                // std::cout << "    [LocalConstraints] Road " << roadIdx << " distance: " << dist 
                //         << " (threshold: " << HIT_DISTANCE << ")" << std::endl;
            }
            
            if (dist < HIT_DISTANCE) {
                // Oblicz najbliższy punkt na segmencie
                float l2 = std::pow(segA.x - segB.x, 2) + std::pow(segA.y - segB.y, 2);
                if (l2 > 0) {
                    float t = ((end.x - segA.x) * (segB.x - segA.x) + (end.y - segA.y) * (segB.y - segA.y)) / l2;
                    t = std::max(0.0f, std::min(1.0f, t));
                    closest.x = segA.x + t * (segB.x - segA.x);
                    closest.y = segA.y + t * (segB.y - segA.y);
                }
                
                if (isHighway) {
                    // std::cout << "    [LocalConstraints] HIT road " << roadIdx << " at distance " << dist 
                    //         << ", closest point: (" << closest.x << "," << closest.y << ")" << std::endl;
                }
                
                // Sprawdź czy węzeł nie istniał wcześniej
                int existingNode = FindNearestNode(closest, 2.0f);
                bool wasIntersection = (existingNode != -1) ? m_RoadNodes[existingNode].isIntersection : false;
                
                int nodeIdx = CreateOrGetNode(closest, true);
                endNodeIdx = nodeIdx;
                
                // Podziel istniejącą drogę jeśli trzeba
                if (nodeIdx != otherRoad.startNodeIdx && nodeIdx != otherRoad.endNodeIdx) {
                    if (isHighway) {
                        // std::cout << "    [LocalConstraints] Splitting road " << roadIdx << std::endl;
                    }
                    
                    // Stwórz nową drogę od closest do końca starej drogi
                    Road oldRoad = m_Roads[roadIdx];
                    m_Roads[roadIdx].isDeleted = true;

                    m_Roads.push_back(Road(oldRoad.startNodeIdx, nodeIdx, oldRoad.type));
                    int r1 = m_Roads.size() - 1;
                    m_RoadNodes[oldRoad.startNodeIdx].connectedRoadIndices.push_back(r1);
                    m_RoadNodes[nodeIdx].connectedRoadIndices.push_back(r1);

                    m_Roads.push_back(Road(nodeIdx, oldRoad.endNodeIdx, oldRoad.type));
                    int r2 = m_Roads.size() - 1;
                    m_RoadNodes[nodeIdx].connectedRoadIndices.push_back(r2);
                    m_RoadNodes[oldRoad.endNodeIdx].connectedRoadIndices.push_back(r2);

                    // Update highways that reference the split road
                    UpdateHighwaysAfterRoadSplit(roadIdx, r1, r2);

                    if (oldRoad.type == RoadType::HIGHWAY && !wasIntersection) {
                        SplitHighwaysAtIntersection(nodeIdx);
                    }
                }
                
                // Jeśli właśnie stał się intersection, podziel highways
                if (!wasIntersection && type == RoadType::HIGHWAY) {
                    SplitHighwaysAtIntersection(nodeIdx);
                }
                
                if (isHighway) {
                    // std::cout << "    [LocalConstraints] Created/reused node " << nodeIdx << std::endl;
                }
                
                return true;
            }
        }

        // 4. Brak kolizji - nowy węzeł
        if (isHighway) {
            // std::cout << "    [LocalConstraints] No constraints hit, creating new node" << std::endl;
        }
        
        endNodeIdx = CreateOrGetNode(end, false);
        
        if (isHighway) {
            // std::cout << "    [LocalConstraints] Created new node " << endNodeIdx << std::endl;
        }
        
        return true;
    }

    // Helpers 

    int HighwayGenerator::CreateOrGetNode(const Point& pos, bool isIntersection) {
        const float SNAP_RADIUS = HIGHWAY_SEGMENT_LENGTH * 0.5f;
        int existingIdx = FindNearestNode(pos, SNAP_RADIUS);
        
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
        float totalScore = 0.0f;
        bool bridgeLimitReached = (m_BridgesCreated >= m_MaxBridges);

        // Próbkuj wzdłuż promienia
        for (int i = 1; i <= HIGHWAY_VISION_SAMPLES; ++i) {
            float distance = (HIGHWAY_SAMPLE_RADIUS / HIGHWAY_VISION_SAMPLES) * i;

            // Pozycja próbki
            Point samplePos(
                pos.x + std::cos(angle) * distance,
                pos.y + std::sin(angle) * distance
            );

            // Sprawdź czy próbka jest w granicach
            int sx = static_cast<int>(samplePos.x);
            int sy = static_cast<int>(samplePos.y);

            if (sx < 0 || sx >= m_Width || sy < 0 || sy >= m_Height) {
                break; // Poza mapą - przerwij raycast
            }

            // RIVER AVOIDANCE: Jeśli limit mostów osiągnięty, traktuj rzekę jako ścianę
            if (bridgeLimitReached && IsRiver(sx, sy)) {
                // Rzeka blokuje dalsze próbkowanie - zwróć zero score
                return 0.0f;
            }

            // Pobierz gęstość (GetDensityAt sprawdza granice)
            float density = GetDensityAt(sx, sy);

            // Waga odwrotna do odległości
            float weight = density * distance;

            totalScore += weight;
        }

        return totalScore;
    }

    Point HighwayGenerator::FindBestDirection(Point pos, Point prevDirection) {
        // std::cout << "    [FindBestDir] Pos: (" << pos.x << ", " << pos.y << ")" << std::endl;
        // std::cout << "    [FindBestDir] Prev dir: (" << prevDirection.x << ", " << prevDirection.y << ")" << std::endl;
        
        float bestScore = -1.0f;
        float bestAngle = 0.0f;
        
        const float PI = 3.14159265359f;
        
        float centerAngle = 0.0f;
        if (prevDirection.x != 0.0f || prevDirection.y != 0.0f) {
            centerAngle = std::atan2(prevDirection.y, prevDirection.x);
        }
        // std::cout << "    [FindBestDir] Center angle: " << (centerAngle * 180.0f / PI) << " degrees" << std::endl;
        // std::cout << "    [FindBestDir] FOV: " << (HIGHWAY_FOV * 180.0f / PI) << " degrees" << std::endl;
        // std::cout << "    [FindBestDir] Num rays: " << HIGHWAY_NUM_RAYS << std::endl;
        
        // Strzel rayami TYLKO w zakresie FOV wokół poprzedniego kierunku
        for (int i = 0; i < HIGHWAY_NUM_RAYS; ++i) {
            // Kąt od -FOV/2 do +FOV/2 względem centerAngle
            float angleOffset = (HIGHWAY_FOV * i) / (HIGHWAY_NUM_RAYS - 1) - HIGHWAY_FOV / 2.0f;
            float angle = centerAngle + angleOffset;
            
            float score = ShootRay(pos, angle);
            
            if (score > bestScore) {
                bestScore = score;
                bestAngle = angle;
            }
        }
        
        // std::cout << "    [FindBestDir] Best score: " << bestScore 
        //         << ", Best angle: " << (bestAngle * 180.0f / PI) << " degrees" << std::endl;
        
        // Jeśli score jest zbyt niski, zatrzymaj się
        if (bestScore < MIN_SCORE_THRESHOLD) {
            // std::cout << "    [FindBestDir] Score too low (< " << MIN_SCORE_THRESHOLD 
            //         << "), returning zero direction" << std::endl;
            return Point(0.0f, 0.0f);
        }
        
        return Point(std::cos(bestAngle), std::sin(bestAngle));
    }



    float HighwayGenerator::RaycastToHighway(const Point& origin, const Point& direction, float maxDistance) {
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
            Branch& branch = m_SleepingBranches[i];
            
            branch.delay--;
            
            if (branch.delay == 0) {
                // std::cout << "[UpdateBranches] Activated branch from node " 
                        //   << branch.parentNodeIdx << std::endl;
                
                // Oznacz węzeł jako skrzyżowanie (bo tutaj rozgałęzia się droga)
                if (branch.parentNodeIdx >= 0 && branch.parentNodeIdx < (int)m_RoadNodes.size()) {
                    bool wasIntersection = m_RoadNodes[branch.parentNodeIdx].isIntersection;
                    m_RoadNodes[branch.parentNodeIdx].isIntersection = true;
                    // std::cout << "[UpdateBranches] Marked node " << branch.parentNodeIdx 
                            //   << " as intersection (branching point)" << std::endl;
                    
                    // Jeśli węzeł nie był wcześniej skrzyżowaniem, podziel highways
                    if (!wasIntersection && branch.type == RoadType::HIGHWAY) {
                        SplitHighwaysAtIntersection(branch.parentNodeIdx);
                    }
                }
                
                HighwayEnd newEnd(
                    branch.parentNodeIdx,
                    branch.direction,
                    branch.iterationsLeft,
                    branch.type
                );
                
                m_ActiveEnds.push_back(newEnd);
                m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
            }
            else if (branch.delay < 0) {
                // Branch został odrzucony przez GlobalGoals
                std::cout << "[UpdateBranches] Removing rejected branch from node " 
                          << branch.parentNodeIdx << std::endl;
                m_SleepingBranches.erase(m_SleepingBranches.begin() + i);
            }
        }
    }

    //  Highway Management Logic

    void HighwayGenerator::CreateHighway(int startIntersection, int endIntersection, const std::vector<int>& roads, bool hasBridge) {
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
        newHw.containsBridge = hasBridge; // Mark if this highway contains a bridge
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

    void HighwayGenerator::UpdateHighwaysAfterRoadSplit(int oldRoadIdx, int newRoad1Idx, int newRoad2Idx) {
        std::cout << "[UpdateHighways] Called for road " << oldRoadIdx
                  << " -> " << newRoad1Idx << "," << newRoad2Idx
                  << " (checking " << m_Highways.size() << " highways)" << std::endl;

        int updatedCount = 0;
        // Znajdź wszystkie highways które używają oldRoadIdx i zastąp go dwoma nowymi
        for (auto& highway : m_Highways) {
            for (size_t i = 0; i < highway.roadIndices.size(); ++i) {
                if (highway.roadIndices[i] == oldRoadIdx) {
                    // Zastąp oldRoadIdx przez newRoad1Idx i newRoad2Idx
                    highway.roadIndices[i] = newRoad1Idx;
                    highway.roadIndices.insert(highway.roadIndices.begin() + i + 1, newRoad2Idx);

                    std::cout << "[UpdateHighways] Highway "
                              << highway.startIntersectionIdx << "->" << highway.endIntersectionIdx
                              << ": replaced road " << oldRoadIdx
                              << " with roads " << newRoad1Idx << "," << newRoad2Idx
                              << " (containsBridge=" << highway.containsBridge << ")" << std::endl;
                    updatedCount++;
                    break; // Każdy roadIdx powinien występować tylko raz w highway
                }
            }
        }

        if (updatedCount == 0) {
            std::cout << "[UpdateHighways] WARNING: Road " << oldRoadIdx << " not found in any highway!" << std::endl;
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
        std::cout << "\n[MergeSimple] ====== MERGING SIMPLE INTERSECTIONS ======" << std::endl;
        std::cout << "[MergeSimple] Initial highways: " << m_Highways.size() << std::endl;
        
        int mergedCount = 0;
        
        // Dla każdego węzła oznaczonego jako intersection
        for (size_t nodeIdx = 0; nodeIdx < m_RoadNodes.size(); ++nodeIdx) {
            Point& node = m_RoadNodes[nodeIdx];
            
            if (!node.isIntersection) {
                continue;
            }
            
            // Znajdź wszystkie highways połączone z tym węzłem
            std::vector<int> connectedHighways;
            
            for (int hwIdx = 0; hwIdx < (int)m_Highways.size(); ++hwIdx) {
                const Highway& hw = m_Highways[hwIdx];
                
                if (hw.startIntersectionIdx == (int)nodeIdx || 
                    hw.endIntersectionIdx == (int)nodeIdx) {
                    connectedHighways.push_back(hwIdx);
                }
            }
            
            // Jeśli dokładnie 2 highways się spotykają - możemy połączyć
            if (connectedHighways.size() == 2) {
                std::cout << "[MergeSimple] Node " << nodeIdx 
                          << " connects exactly 2 highways: " 
                          << connectedHighways[0] << " and " << connectedHighways[1] << std::endl;
                
                int hw1Idx = connectedHighways[0];
                int hw2Idx = connectedHighways[1];
                
                const Highway& hw1 = m_Highways[hw1Idx];
                const Highway& hw2 = m_Highways[hw2Idx];
                
                // Określ nowy początek i koniec
                int newStart = -1;
                int newEnd = -1;
                
                // hw1 kończy się w nodeIdx, hw2 zaczyna się w nodeIdx
                if (hw1.endIntersectionIdx == (int)nodeIdx && 
                    hw2.startIntersectionIdx == (int)nodeIdx) {
                    newStart = hw1.startIntersectionIdx;
                    newEnd = hw2.endIntersectionIdx;
                }
                // hw1 zaczyna się w nodeIdx, hw2 kończy się w nodeIdx
                else if (hw1.startIntersectionIdx == (int)nodeIdx && 
                         hw2.endIntersectionIdx == (int)nodeIdx) {
                    newStart = hw2.startIntersectionIdx;
                    newEnd = hw1.endIntersectionIdx;
                }
                // hw1 zaczyna się w nodeIdx, hw2 zaczyna się w nodeIdx
                else if (hw1.startIntersectionIdx == (int)nodeIdx && 
                         hw2.startIntersectionIdx == (int)nodeIdx) {
                    newStart = hw1.endIntersectionIdx;
                    newEnd = hw2.endIntersectionIdx;
                }
                // hw1 kończy się w nodeIdx, hw2 kończy się w nodeIdx
                else if (hw1.endIntersectionIdx == (int)nodeIdx && 
                         hw2.endIntersectionIdx == (int)nodeIdx) {
                    newStart = hw1.startIntersectionIdx;
                    newEnd = hw2.startIntersectionIdx;
                }
                
                if (newStart == -1 || newEnd == -1) {
                    std::cout << "[MergeSimple] Cannot determine merge direction, skipping" << std::endl;
                    continue;
                }
                
                // Połącz roads z obu highways
                std::vector<int> mergedRoads;
                
                // Dodaj roads z hw1 (w odpowiedniej kolejności)
                if (hw1.endIntersectionIdx == (int)nodeIdx) {
                    mergedRoads.insert(mergedRoads.end(), hw1.roadIndices.begin(), hw1.roadIndices.end());
                } else {
                    mergedRoads.insert(mergedRoads.end(), hw1.roadIndices.rbegin(), hw1.roadIndices.rend());
                }
                
                // Dodaj roads z hw2 (w odpowiedniej kolejności)
                if (hw2.startIntersectionIdx == (int)nodeIdx) {
                    mergedRoads.insert(mergedRoads.end(), hw2.roadIndices.begin(), hw2.roadIndices.end());
                } else {
                    mergedRoads.insert(mergedRoads.end(), hw2.roadIndices.rbegin(), hw2.roadIndices.rend());
                }
                
                float mergedLength = hw1.totalLength + hw2.totalLength;
                
                std::cout << "[MergeSimple] Creating merged highway: "
                          << newStart << " -> " << newEnd
                          << " (length=" << mergedLength << ", roads=" << mergedRoads.size() << ")" << std::endl;

                // Utwórz nowy połączony highway
                Highway mergedHighway(newStart, newEnd, mergedRoads, mergedLength);

                // Preserve containsBridge flag - if either highway has a bridge, merged highway inherits it
                if (hw1.containsBridge || hw2.containsBridge) {
                    mergedHighway.containsBridge = true;
                    std::cout << "[MergeSimple] Preserving bridge flag in merged highway" << std::endl;
                }

                // Usuń stare highways (od tyłu!)
                int toRemove1 = std::max(hw1Idx, hw2Idx);
                int toRemove2 = std::min(hw1Idx, hw2Idx);

                m_Highways.erase(m_Highways.begin() + toRemove1);
                m_Highways.erase(m_Highways.begin() + toRemove2);

                // Dodaj nowy
                m_Highways.push_back(mergedHighway);
                
                // Usuń flagę intersection z tego węzła
                node.isIntersection = false;
                
                mergedCount++;
                std::cout << "[MergeSimple] Successfully merged!" << std::endl;
                
                // WAŻNE: Przerwij pętlę po modyfikacji, bo indeksy się zmieniły
                // Zacznij od nowa
                nodeIdx = 0;
            }
        }
        
        std::cout << "[MergeSimple] Total merges performed: " << mergedCount << std::endl;
        std::cout << "[MergeSimple] Final highways: " << m_Highways.size() << std::endl;
        std::cout << "[MergeSimple] =======================================" << std::endl;
    }

    void HighwayGenerator::PostProcessIntersections() {
        std::cout << "[PostProcess] Checking all intersections against highways..." << std::endl;
        
        int totalSplits = 0;
        
        // Dla każdego węzła który jest intersection
        for (size_t nodeIdx = 0; nodeIdx < m_RoadNodes.size(); ++nodeIdx) {
            const Point& intersection = m_RoadNodes[nodeIdx];
            
            if (!intersection.isIntersection) {
                continue;
            }
            
            // std::cout << "[PostProcess] Checking intersection at node " << nodeIdx << std::endl;
            
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

                    // Preserve containsBridge flag - if original highway had a bridge, both parts inherit it
                    if (highway.containsBridge) {
                        hw1.containsBridge = true;
                        hw2.containsBridge = true;
                        std::cout << "[PostProcess] Preserving bridge flag in split highways" << std::endl;
                    }

                    std::cout << "[PostProcess] Split highway into:" << std::endl;
                    std::cout << "  Part 1: " << hw1.startIntersectionIdx
                              << " -> " << hw1.endIntersectionIdx
                              << " (length=" << hw1.totalLength << ", bridge=" << hw1.containsBridge << ")" << std::endl;
                    std::cout << "  Part 2: " << hw2.startIntersectionIdx
                              << " -> " << hw2.endIntersectionIdx
                              << " (length=" << hw2.totalLength << ", bridge=" << hw2.containsBridge << ")" << std::endl;
                    
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

    //  Helpers Math 

    float HighwayGenerator::GetDensityAt(int x, int y) const {
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return 0.0f;
        return m_PopulationDensity[y][x];
    }

    uint8_t HighwayGenerator::GetZoneAt(int x, int y) const {
        if (m_ZoneMask.empty()) return ZONE_NORMAL;
        if (x < 0 || x >= m_Width || y < 0 || y >= m_Height) return ZONE_NORMAL;
        return m_ZoneMask[y * m_Width + x];
    }

    bool HighwayGenerator::IsRiver(int x, int y) const {
        return GetZoneAt(x, y) == ZONE_RIVER;
    }

    bool HighwayGenerator::IsPark(int x, int y) const {
        return GetZoneAt(x, y) == ZONE_PARK;
    }

    bool HighwayGenerator::IsAnyZone(int x, int y) const {
        return GetZoneAt(x, y) != ZONE_NORMAL;
    }

    // Estymuje kierunek rzeki w danym punkcie poprzez analizę sąsiednich pikseli
    Point HighwayGenerator::EstimateRiverDirection(const Point& pos) const {
        const int SAMPLE_RADIUS = 20;
        const int SAMPLE_STEP = 5;

        // Zbierz pozycje pikseli rzeki w okolicy
        std::vector<Point> riverPixels;

        for (int dy = -SAMPLE_RADIUS; dy <= SAMPLE_RADIUS; dy += SAMPLE_STEP) {
            for (int dx = -SAMPLE_RADIUS; dx <= SAMPLE_RADIUS; dx += SAMPLE_STEP) {
                int x = (int)pos.x + dx;
                int y = (int)pos.y + dy;

                if (IsRiver(x, y)) {
                    riverPixels.push_back(Point((float)x, (float)y));
                }
            }
        }

        if (riverPixels.size() < 2) {
            // Nie można określić kierunku, zwróć kierunek prostopadły do domyślnego
            return Point(0.0f, 1.0f);
        }

        // Oblicz średni kierunek rzeki używając PCA (uproszczona wersja)
        float sumX = 0.0f, sumY = 0.0f;
        for (const auto& p : riverPixels) {
            sumX += p.x;
            sumY += p.y;
        }
        float meanX = sumX / riverPixels.size();
        float meanY = sumY / riverPixels.size();

        // Oblicz kowariancję
        float cov_xx = 0.0f, cov_xy = 0.0f, cov_yy = 0.0f;
        for (const auto& p : riverPixels) {
            float dx = p.x - meanX;
            float dy = p.y - meanY;
            cov_xx += dx * dx;
            cov_xy += dx * dy;
            cov_yy += dy * dy;
        }

        // Oblicz kierunek głównej osi (największa wariancja = kierunek rzeki)
        float trace = cov_xx + cov_yy;
        float det = cov_xx * cov_yy - cov_xy * cov_xy;
        float lambda1 = trace / 2.0f + std::sqrt(trace * trace / 4.0f - det);

        // Wektor własny dla lambda1
        Point riverDir;
        if (std::abs(cov_xy) > 0.001f) {
            riverDir.x = lambda1 - cov_yy;
            riverDir.y = cov_xy;
        } else {
            riverDir.x = 1.0f;
            riverDir.y = 0.0f;
        }

        // Normalizuj
        float len = std::sqrt(riverDir.x * riverDir.x + riverDir.y * riverDir.y);
        if (len > 0.001f) {
            riverDir.x /= len;
            riverDir.y /= len;
        }

        return riverDir;
    }

    // Oblicza dystans przekroczenia rzeki w danym kierunku
    float HighwayGenerator::CalculateRiverCrossingDistance(const Point& pos, const Point& direction) const {
        const float MAX_BRIDGE_LENGTH = 100.0f;
        const float STEP = 2.0f;

        float distance = 0.0f;
        Point currentPos = pos;

        // Idź w kierunku i mierz dystans przez rzekę
        while (distance < MAX_BRIDGE_LENGTH) {
            currentPos.x += direction.x * STEP;
            currentPos.y += direction.y * STEP;
            distance += STEP;

            // Jeśli wyszliśmy poza mapę, zwróć bardzo duży dystans
            if (currentPos.x < 0 || currentPos.x >= m_Width ||
                currentPos.y < 0 || currentPos.y >= m_Height) {
                return 99999.0f;
            }

            // Jeśli wyszliśmy z rzeki, zwróć dystans
            if (!IsRiver((int)currentPos.x, (int)currentPos.y)) {
                return distance;
            }
        }

        return MAX_BRIDGE_LENGTH;
    }

    // Znajduje najlepszy kierunek przekroczenia rzeki (możliwie prostopadły do rzeki)
    Point HighwayGenerator::FindRiverCrossingDirection(const Point& pos, const Point& currentDir) const {
        // Estymuj kierunek rzeki
        Point riverDir = EstimateRiverDirection(pos);

        // Oblicz kierunek prostopadły do rzeki (dwa możliwe kierunki)
        Point perp1(-riverDir.y, riverDir.x);   // Obrót o 90 stopni w lewo
        Point perp2(riverDir.y, -riverDir.x);   // Obrót o 90 stopni w prawo

        // Wybierz kierunek prostopadły, który jest bliższy aktualnemu kierunkowi
        float dot1 = currentDir.x * perp1.x + currentDir.y * perp1.y;
        float dot2 = currentDir.x * perp2.x + currentDir.y * perp2.y;

        Point basePerp = (dot1 > dot2) ? perp1 : perp2;

        // Testuj kilka kierunków wokół prostopadłego
        const int NUM_RAYS = 7;
        const float ANGLE_SPREAD = 0.5f; // ~28 stopni w każdą stronę

        float baseAngle = std::atan2(basePerp.y, basePerp.x);
        float minDistance = 99999.0f;
        Point bestDirection = basePerp;

        for (int i = 0; i < NUM_RAYS; ++i) {
            float angleOffset = ((float)i / (NUM_RAYS - 1) - 0.5f) * 2.0f * ANGLE_SPREAD;
            float testAngle = baseAngle + angleOffset;

            Point testDir(std::cos(testAngle), std::sin(testAngle));
            float crossingDist = CalculateRiverCrossingDistance(pos, testDir);

            if (crossingDist < minDistance) {
                minDistance = crossingDist;
                bestDirection = testDir;
            }
        }

        std::cout << "    [Bridge] River crossing: dir=(" << bestDirection.x << "," << bestDirection.y
                  << ") distance=" << minDistance << std::endl;

        return bestDirection;
    }

    // Sprawdza czy mapa zawiera rzekę
    bool HighwayGenerator::HasRiver() const {
        if (m_ZoneMask.empty()) return false;

        for (uint8_t zone : m_ZoneMask) {
            if (zone == ZONE_RIVER) return true;
        }
        return false;
    }

    // Oblicza odległość od najbliższego piksela rzeki
    float HighwayGenerator::DistanceToRiver(int x, int y) const {
        if (!HasRiver()) return 99999.0f;

        const int SEARCH_RADIUS = 200;
        float minDist = 99999.0f;

        for (int dy = -SEARCH_RADIUS; dy <= SEARCH_RADIUS; ++dy) {
            for (int dx = -SEARCH_RADIUS; dx <= SEARCH_RADIUS; ++dx) {
                int nx = x + dx;
                int ny = y + dy;

                if (nx >= 0 && nx < m_Width && ny >= 0 && ny < m_Height) {
                    if (IsRiver(nx, ny)) {
                        float dist = std::sqrt((float)(dx * dx + dy * dy));
                        if (dist < minDist) {
                            minDist = dist;
                        }
                    }
                }
            }
        }

        return minDist;
    }

    // Określa po której stronie rzeki znajduje się punkt (-1: lewa, 0: na rzece, 1: prawa)
    // Zakładamy że rzeka płynie głównie w poziomie lub pionie
    int HighwayGenerator::DetermineRiverSide(int x, int y) const {
        if (!HasRiver()) return 0;
        if (IsRiver(x, y)) return 0;

        // Znajdź środek rzeki
        int riverSumX = 0, riverSumY = 0, riverCount = 0;
        for (int ry = 0; ry < m_Height; ++ry) {
            for (int rx = 0; rx < m_Width; ++rx) {
                if (IsRiver(rx, ry)) {
                    riverSumX += rx;
                    riverSumY += ry;
                    riverCount++;
                }
            }
        }

        if (riverCount == 0) return 0;

        float riverCenterX = (float)riverSumX / riverCount;
        float riverCenterY = (float)riverSumY / riverCount;

        // Estymuj kierunek rzeki w centrum
        Point riverCenter(riverCenterX, riverCenterY);
        Point riverDir = EstimateRiverDirection(riverCenter);

        // Oblicz wektor prostopadły do rzeki (normalna)
        Point normal(-riverDir.y, riverDir.x);

        // Oblicz wektor od centrum rzeki do punktu
        Point toPoint((float)x - riverCenterX, (float)y - riverCenterY);

        // Oblicz iloczyn skalarny - znak określa stronę
        float dot = toPoint.x * normal.x + toPoint.y * normal.y;

        return (dot < 0) ? -1 : 1;
    }

    // Sprawdza czy droga zbliża się do rzeki
    bool HighwayGenerator::IsApproachingRiver(const Point& pos, const Point& direction, float lookAhead) const {
        if (!HasRiver()) return false;
        if (IsRiver((int)pos.x, (int)pos.y)) return false; // Już na rzece

        // Sprawdź punkty wzdłuż kierunku
        const int SAMPLES = 5;
        for (int i = 1; i <= SAMPLES; ++i) {
            float dist = (lookAhead / SAMPLES) * i;
            Point checkPos(pos.x + direction.x * dist, pos.y + direction.y * dist);

            if (checkPos.x >= 0 && checkPos.x < m_Width && checkPos.y >= 0 && checkPos.y < m_Height) {
                if (IsRiver((int)checkPos.x, (int)checkPos.y)) {
                    return true;
                }
            }
        }

        return false;
    }

    // Znajduje dokładny punkt przecięcia z brzegiem rzeki
    bool HighwayGenerator::FindRiverBankIntersection(const Point& start, const Point& direction,
                                                      float maxDistance, Point& intersection) const {
        if (!HasRiver()) return false;

        // Binary search dla precyzyjnego punktu przecięcia
        const float PRECISION = 0.5f; // Dokładność do 0.5 piksela
        float minDist = 0.0f;
        float maxDist = maxDistance;

        bool startIsRiver = IsRiver((int)start.x, (int)start.y);

        while (maxDist - minDist > PRECISION) {
            float midDist = (minDist + maxDist) / 2.0f;
            Point midPos(start.x + direction.x * midDist, start.y + direction.y * midDist);

            if (midPos.x < 0 || midPos.x >= m_Width || midPos.y < 0 || midPos.y >= m_Height) {
                maxDist = midDist;
                continue;
            }

            bool midIsRiver = IsRiver((int)midPos.x, (int)midPos.y);

            if (startIsRiver) {
                // Szukamy wyjścia z rzeki (z river na land)
                if (midIsRiver) {
                    minDist = midDist; // Dalej w rzece
                } else {
                    maxDist = midDist; // Znaleźliśmy granicę
                }
            } else {
                // Szukamy wejścia do rzeki (z land na river)
                if (!midIsRiver) {
                    minDist = midDist; // Dalej na lądzie
                } else {
                    maxDist = midDist; // Znaleźliśmy granicę
                }
            }
        }

        float finalDist = (minDist + maxDist) / 2.0f;
        intersection.x = start.x + direction.x * finalDist;
        intersection.y = start.y + direction.y * finalDist;

        // Sprawdź czy znaleźliśmy sensowny punkt
        if (finalDist > 0.1f && finalDist < maxDistance) {
            return true;
        }

        return false;
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

    bool HighwayGenerator::IsPointOnHighway(const Point& point, const Highway& highway, float tolerance) const {
        // Sprawdź każdy road w highway
        for (int roadIdx : highway.roadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
            
            const Road& road = m_Roads[roadIdx];
            const Point& roadStart = m_RoadNodes[road.startNodeIdx];
            const Point& roadEnd = m_RoadNodes[road.endNodeIdx];
            
            // Sprawdź czy punkt jest jednym z końców tego segmentu
            // Porównanie przez indeks węzła
            for (size_t i = 0; i < m_RoadNodes.size(); ++i) {
                if (&m_RoadNodes[i] == &point) {
                    if (road.startNodeIdx == (int)i || road.endNodeIdx == (int)i) {
                        return true;
                    }
                }
            }
            
            // Oblicz odległość punktu od segmentu
            Point closest;
            float dx = roadEnd.x - roadStart.x;
            float dy = roadEnd.y - roadStart.y;
            float l2 = dx * dx + dy * dy;
            
            if (l2 < 0.01f) continue; // Segment zbyt krótki
            
            float t = ((point.x - roadStart.x) * dx + (point.y - roadStart.y) * dy) / l2;
            t = std::max(0.0f, std::min(1.0f, t));
            
            closest.x = roadStart.x + t * dx;
            closest.y = roadStart.y + t * dy;
            
            float distX = point.x - closest.x;
            float distY = point.y - closest.y;
            float dist = std::sqrt(distX * distX + distY * distY);
            
            if (dist < tolerance) {
                // Dodatkowo sprawdź czy punkt leży między końcami segmentu
                float segmentLength = std::sqrt(l2);
                
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

    void HighwayGenerator::RemoveRedundantParallelHighways() {
        std::cout << "[RemoveRedundant] Starting... Initial highways: " << m_Highways.size() << std::endl;
        
        int removedCount = 0;
        std::vector<bool> toRemove(m_Highways.size(), false);
        
        // Porównaj każdą parę highways
        for (size_t i = 0; i < m_Highways.size(); ++i) {
            if (toRemove[i]) continue;
            
            const Highway& hw1 = m_Highways[i];
            const Point& start1 = m_RoadNodes[hw1.startIntersectionIdx];
            const Point& end1 = m_RoadNodes[hw1.endIntersectionIdx];
            
            for (size_t j = i + 1; j < m_Highways.size(); ++j) {
                if (toRemove[j]) continue;
                
                const Highway& hw2 = m_Highways[j];
                const Point& start2 = m_RoadNodes[hw2.startIntersectionIdx];
                const Point& end2 = m_RoadNodes[hw2.endIntersectionIdx];
                
                // Sprawdź bliskość końców (normalna orientacja)
                float distStartStart = std::sqrt(
                    std::pow(start1.x - start2.x, 2) + std::pow(start1.y - start2.y, 2)
                );
                float distEndEnd = std::sqrt(
                    std::pow(end1.x - end2.x, 2) + std::pow(end1.y - end2.y, 2)
                );
                
                // Sprawdź bliskość końców (odwrotna orientacja)
                float distStartEnd = std::sqrt(
                    std::pow(start1.x - end2.x, 2) + std::pow(start1.y - end2.y, 2)
                );
                float distEndStart = std::sqrt(
                    std::pow(end1.x - start2.x, 2) + std::pow(end1.y - start2.y, 2)
                );
                
                bool normalOrientation = (distStartStart < MERGE_DISTANCE_THRESHOLD && 
                                        distEndEnd < MERGE_DISTANCE_THRESHOLD);
                bool reverseOrientation = (distStartEnd < MERGE_DISTANCE_THRESHOLD && 
                                        distEndStart < MERGE_DISTANCE_THRESHOLD);
                
                if (!normalOrientation && !reverseOrientation) {
                    continue; // Końce nie są bliskie
                }
                
                // Oblicz kierunki highways
                Point dir1(end1.x - start1.x, end1.y - start1.y);
                float len1 = std::sqrt(dir1.x * dir1.x + dir1.y * dir1.y);
                if (len1 > 0.01f) {
                    dir1.x /= len1;
                    dir1.y /= len1;
                }
                
                Point dir2(end2.x - start2.x, end2.y - start2.y);
                float len2 = std::sqrt(dir2.x * dir2.x + dir2.y * dir2.y);
                if (len2 > 0.01f) {
                    dir2.x /= len2;
                    dir2.y /= len2;
                }
                
                // Oblicz kąt między kierunkami
                float dotProduct = dir1.x * dir2.x + dir1.y * dir2.y;
                
                // Dla odwrotnej orientacji odwróć jeden kierunek
                if (reverseOrientation) {
                    dotProduct = dir1.x * (-dir2.x) + dir1.y * (-dir2.y);
                }
                
                float angle = std::acos(std::max(-1.0f, std::min(1.0f, dotProduct)));
                
                // Uwzględnij symetrię (180° to też równoległość)
                if (angle > 3.14159f / 2.0f) {
                    angle = 3.14159f - angle;
                }
                
                std::cout << "[RemoveRedundant] Highway " << i << " vs " << j 
                        << ": distance=" << std::min(distStartStart + distEndEnd, distStartEnd + distEndStart)
                        << ", angle=" << (angle * 180.0f / 3.14159f) << " degrees" << std::endl;
                
                // Jeśli są równoległe
                if (angle < PARALLEL_ANGLE_THRESHOLD) {
                    std::cout << "[RemoveRedundant] Highways " << i << " and " << j
                            << " are redundant (parallel and close)" << std::endl;

                    // Protect highways with bridges from removal
                    if (hw1.containsBridge && hw2.containsBridge) {
                        std::cout << "[RemoveRedundant] Both highways contain bridges - skipping removal" << std::endl;
                        continue;
                    } else if (hw1.containsBridge) {
                        std::cout << "[RemoveRedundant] Highway " << i << " contains bridge - removing " << j << " instead" << std::endl;
                        toRemove[j] = true;
                        removedCount++;
                        continue;
                    } else if (hw2.containsBridge) {
                        std::cout << "[RemoveRedundant] Highway " << j << " contains bridge - removing " << i << " instead" << std::endl;
                        toRemove[i] = true;
                        removedCount++;
                        break;
                    }

                    // Usuń dłuższy (jeśli żaden nie ma mostu)
                    if (hw1.totalLength > hw2.totalLength) {
                        std::cout << "[RemoveRedundant] Removing highway " << i
                                << " (length=" << hw1.totalLength << ")" << std::endl;
                        toRemove[i] = true;
                        removedCount++;
                        break; // Przerwij wewnętrzną pętlę
                    } else {
                        std::cout << "[RemoveRedundant] Removing highway " << j
                                << " (length=" << hw2.totalLength << ")" << std::endl;
                        toRemove[j] = true;
                        removedCount++;
                    }
                }
            }
        }
        
        // Usuń zaznaczone highways (od tyłu!)
        for (int i = m_Highways.size() - 1; i >= 0; --i) {
            if (toRemove[i]) {
                RemoveHighway(i);
            }
        }
        
        std::cout << "[RemoveRedundant] Finished. Removed " << removedCount 
                << " highways. Final count: " << m_Highways.size() << std::endl;
    }

    HighwayGenerator::TrendLine HighwayGenerator::CalculateTrendLine(const Highway& highway) const {
        if (highway.roadIndices.empty()) {
            return TrendLine{{0,0}, {0,0}, {0,0}, 0.0f, -1};
        }
        
        // Zbierz wszystkie punkty z highway
        std::vector<Point> points;
        for (int roadIdx : highway.roadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
            const Road& road = m_Roads[roadIdx];
            
            // Dodaj punkt startowy (jeśli jeszcze nie mamy)
            if (points.empty() || 
                !(points.back().x == m_RoadNodes[road.startNodeIdx].x && 
                points.back().y == m_RoadNodes[road.startNodeIdx].y)) {
                points.push_back(m_RoadNodes[road.startNodeIdx]);
            }
            points.push_back(m_RoadNodes[road.endNodeIdx]);
        }
        
        if (points.size() < 2) {
            return TrendLine{{0,0}, {0,0}, {0,0}, 0.0f, -1};
        }
        
        // Oblicz środek ciężkości
        float centerX = 0.0f, centerY = 0.0f;
        for (const auto& p : points) {
            centerX += p.x;
            centerY += p.y;
        }
        centerX /= points.size();
        centerY /= points.size();
        
        // PCA - Principal Component Analysis (najprostsza wersja)
        float xx = 0.0f, xy = 0.0f, yy = 0.0f;
        for (const auto& p : points) {
            float dx = p.x - centerX;
            float dy = p.y - centerY;
            xx += dx * dx;
            xy += dx * dy;
            yy += dy * dy;
        }
        
        // Oblicz kierunek głównej składowej
        float trace = xx + yy;
        float det = xx * yy - xy * xy;
        float eigenvalue = trace / 2.0f + std::sqrt(trace * trace / 4.0f - det);
        
        float dirX = xy;
        float dirY = eigenvalue - xx;
        float dirLen = std::sqrt(dirX * dirX + dirY * dirY);
        
        if (dirLen > 0.01f) {
            dirX /= dirLen;
            dirY /= dirLen;
        } else {
            // Fallback - użyj kierunku od początku do końca
            dirX = points.back().x - points.front().x;
            dirY = points.back().y - points.front().y;
            dirLen = std::sqrt(dirX * dirX + dirY * dirY);
            if (dirLen > 0.01f) {
                dirX /= dirLen;
                dirY /= dirLen;
            }
        }
        
        // Znajdź ekstremalne projekcje na linię trendu
        float minProj = 1e9f, maxProj = -1e9f;
        Point minPoint, maxPoint;
        
        for (const auto& p : points) {
            float proj = (p.x - centerX) * dirX + (p.y - centerY) * dirY;
            if (proj < minProj) {
                minProj = proj;
                minPoint.x = centerX + proj * dirX;
                minPoint.y = centerY + proj * dirY;
            }
            if (proj > maxProj) {
                maxProj = proj;
                maxPoint.x = centerX + proj * dirX;
                maxPoint.y = centerY + proj * dirY;
            }
        }
        
        TrendLine result;
        result.start = minPoint;
        result.end = maxPoint;
        result.direction.x = dirX;
        result.direction.y = dirY;
        result.length = maxProj - minProj;
        
        return result;
    }

    bool HighwayGenerator::AreTrendLinesParallel(const TrendLine& t1, const TrendLine& t2,
                                                float angleThreshold, float distanceThreshold) const {
        // 1. Sprawdź kąt między kierunkami
        float dotProduct = std::abs(t1.direction.x * t2.direction.x + 
                                    t1.direction.y * t2.direction.y);
        float angle = std::acos(std::max(-1.0f, std::min(1.0f, dotProduct)));
        
        if (angle > angleThreshold) {
            return false; // Nie są równoległe
        }
        
        // 2. Sprawdź czy projekcje się nakładają (czy highways są "obok siebie")
        // Rzutuj końce t2 na oś t1
        float proj1Start = (t2.start.x - t1.start.x) * t1.direction.x + 
                        (t2.start.y - t1.start.y) * t1.direction.y;
        float proj1End = (t2.end.x - t1.start.x) * t1.direction.x + 
                        (t2.end.y - t1.start.y) * t1.direction.y;
        
        // Normalizuj projekcje do [0, 1] względem długości t1
        proj1Start /= t1.length;
        proj1End /= t1.length;
        
        // Oblicz nakładanie się
        float overlapStart = std::max(0.0f, std::min(proj1Start, proj1End));
        float overlapEnd = std::min(1.0f, std::max(proj1Start, proj1End));
        float overlap = std::max(0.0f, overlapEnd - overlapStart);
        
        if (overlap < TREND_MIN_OVERLAP_RATIO) {
            return false; // Za małe nakładanie się
        }
        
        // 3. Sprawdź średnią odległość między liniami
        float avgDist = AverageDistanceBetweenTrends(t1, t2);
        
        return avgDist < distanceThreshold;
    }

    float HighwayGenerator::AverageDistanceBetweenTrends(const TrendLine& t1, const TrendLine& t2) const {
        // Próbkuj punkty wzdłuż t2 i mierz odległość do linii t1
        const int numSamples = 10;
        float totalDist = 0.0f;
        
        for (int i = 0; i <= numSamples; ++i) {
            float t = (float)i / numSamples;
            Point sample;
            sample.x = t2.start.x + t * (t2.end.x - t2.start.x);
            sample.y = t2.start.y + t * (t2.end.y - t2.start.y);
            
            // Odległość punktu od linii t1
            float dist = DistancePointToSegment(sample, t1.start, t1.end);
            totalDist += dist;
        }
        
        return totalDist / (numSamples + 1);
    }

    void HighwayGenerator::RemoveParallelHighwaysByTrend() {
        std::cout << "[TrendAnalysis] Starting... Highways: " << m_Highways.size() << std::endl;
        
        // Oblicz linie trendu dla wszystkich highways
        std::vector<TrendLine> trendLines;
        for (size_t i = 0; i < m_Highways.size(); ++i) {
            TrendLine trend = CalculateTrendLine(m_Highways[i]);
            trend.highwayIdx = i;
            trendLines.push_back(trend);
            
            std::cout << "[TrendAnalysis] Highway " << i 
                    << ": trend from (" << trend.start.x << "," << trend.start.y 
                    << ") to (" << trend.end.x << "," << trend.end.y 
                    << "), length=" << trend.length << std::endl;
        }
        
        // Znajdź pary równoległych highways
        std::vector<bool> toRemove(m_Highways.size(), false);
        int removedCount = 0;
        
        for (size_t i = 0; i < trendLines.size(); ++i) {
            if (toRemove[i]) continue;
            
            for (size_t j = i + 1; j < trendLines.size(); ++j) {
                if (toRemove[j]) continue;
                
                if (AreTrendLinesParallel(trendLines[i], trendLines[j],
                                        TREND_PARALLEL_ANGLE_THRESHOLD,
                                        TREND_PARALLEL_DISTANCE_THRESHOLD)) {
                    
                    float avgDist = AverageDistanceBetweenTrends(trendLines[i], trendLines[j]);
                    
                    std::cout << "[TrendAnalysis] Highways " << i << " and " << j
                            << " are parallel (avg distance: " << avgDist << ")" << std::endl;

                    // Protect highways with bridges from removal
                    if (m_Highways[i].containsBridge && m_Highways[j].containsBridge) {
                        std::cout << "[TrendAnalysis] Both highways contain bridges - skipping removal" << std::endl;
                        continue;
                    } else if (m_Highways[i].containsBridge) {
                        std::cout << "[TrendAnalysis] Highway " << i << " contains bridge - removing " << j << " instead" << std::endl;
                        toRemove[j] = true;
                        removedCount++;
                        continue;
                    } else if (m_Highways[j].containsBridge) {
                        std::cout << "[TrendAnalysis] Highway " << j << " contains bridge - removing " << i << " instead" << std::endl;
                        toRemove[i] = true;
                        removedCount++;
                        break;
                    }

                    // Usuń krótszy highway (jeśli żaden nie ma mostu)
                    if (m_Highways[i].totalLength < m_Highways[j].totalLength) {
                        std::cout << "[TrendAnalysis] Removing shorter highway " << i
                                << " (length=" << m_Highways[i].totalLength << ")" << std::endl;
                        toRemove[i] = true;
                        removedCount++;
                        break;
                    } else {
                        std::cout << "[TrendAnalysis] Removing shorter highway " << j
                                << " (length=" << m_Highways[j].totalLength << ")" << std::endl;
                        toRemove[j] = true;
                        removedCount++;
                    }
                }
            }
        }
        
        // Usuń zaznaczone highways (od tyłu)
        for (int i = m_Highways.size() - 1; i >= 0; --i) {
            if (toRemove[i]) {
                RemoveHighway(i);
            }
        }
        
        std::cout << "[TrendAnalysis] Finished. Removed " << removedCount 
                << " highways. Final count: " << m_Highways.size() << std::endl;
    }

    bool HighwayGenerator::IsCenterConnectedToHighway(float centerX, float centerY, float radius) {
        Point centerPos(centerX, centerY);
        
        // Sprawdź czy jakikolwiek highway przechodzi w promieniu tego centrum
        for (const Highway& highway : m_Highways) {
            for (int roadIdx : highway.roadIndices) {
                if (roadIdx < 0 || roadIdx >= (int)m_Roads.size()) continue;
                if (m_Roads[roadIdx].isDeleted) continue;
                
                const Road& road = m_Roads[roadIdx];
                const Point& roadStart = m_RoadNodes[road.startNodeIdx];
                const Point& roadEnd = m_RoadNodes[road.endNodeIdx];
                
                float dist = DistancePointToSegment(centerPos, roadStart, roadEnd);
                
                if (dist < radius) {
                    return true; // Highway przechodzi blisko centrum
                }
            }
        }
        
        return false; // Brak highways w pobliżu
    }

    void HighwayGenerator::CheckAndSeedUnconnectedCenters() {
        std::cout << "[SeedCenters] Checking for unconnected population centers..." << std::endl;
        
        // Znajdź lokalne maksima gęstości (centra populacji)
        struct PopulationCenter {
            float x, y;
            float density;
            float radius;
        };
        
        std::vector<PopulationCenter> centers;
        const int SCAN_STEP = 30; // Co ile pikseli skanować
        const float MIN_CENTER_DENSITY = 0.4f; // Minimalna gęstość centrum
        const float CENTER_DETECTION_RADIUS = 80.0f; // Promień dla grupowania centrów
        
        // Skanuj mapę w poszukiwaniu pików gęstości
        for (int y = CENTER_DETECTION_RADIUS; y < m_Height - CENTER_DETECTION_RADIUS; y += SCAN_STEP) {
            for (int x = CENTER_DETECTION_RADIUS; x < m_Width - CENTER_DETECTION_RADIUS; x += SCAN_STEP) {
                float density = GetDensityAt(x, y);
                
                if (density < MIN_CENTER_DENSITY) continue;
                
                // Sprawdź czy to lokalne maksimum
                bool isLocalMax = true;
                for (int dy = -SCAN_STEP; dy <= SCAN_STEP; dy += SCAN_STEP) {
                    for (int dx = -SCAN_STEP; dx <= SCAN_STEP; dx += SCAN_STEP) {
                        if (dx == 0 && dy == 0) continue;
                        
                        int nx = x + dx;
                        int ny = y + dy;
                        
                        if (nx < 0 || nx >= m_Width || ny < 0 || ny >= m_Height) continue;
                        
                        if (GetDensityAt(nx, ny) > density) {
                            isLocalMax = false;
                            break;
                        }
                    }
                    if (!isLocalMax) break;
                }
                
                if (isLocalMax) {
                    // Sprawdź czy nie za blisko innego centrum
                    bool tooClose = false;
                    for (const auto& existing : centers) {
                        float dist = std::sqrt(
                            std::pow(x - existing.x, 2) + 
                            std::pow(y - existing.y, 2)
                        );
                        if (dist < CENTER_DETECTION_RADIUS) {
                            tooClose = true;
                            break;
                        }
                    }
                    
                    if (!tooClose) {
                        centers.push_back({(float)x, (float)y, density, CENTER_DETECTION_RADIUS});
                        std::cout << "[SeedCenters] Found population center at (" 
                                << x << ", " << y << ") with density " << density << std::endl;
                    }
                }
            }
        }
        
        std::cout << "[SeedCenters] Found " << centers.size() << " population centers" << std::endl;
        
        // Sprawdź które centra nie mają połączenia z highway
        int seededCount = 0;
        for (const auto& center : centers) {
            if (!IsCenterConnectedToHighway(center.x, center.y, center.radius)) {
                std::cout << "[SeedCenters] Center at (" << center.x << ", " << center.y 
                        << ") is NOT connected - seeding new highway" << std::endl;
                
                // Zainicjuj nowy highway ze środka centrum w losowym kierunku
                int startNode = CreateOrGetNode(Point(center.x, center.y), true);
                
                // Losowy kierunek
                float angle = (rand() % 360) * 3.14159f / 180.0f;
                Point direction(std::cos(angle), std::sin(angle));
                
                // Dodaj nowego agenta
                m_ActiveEnds.push_back(
                    HighwayEnd(startNode, direction, HIGHWAY_MAX_ITERATIONS, RoadType::HIGHWAY)
                );
                
                seededCount++;
            } else {
                std::cout << "[SeedCenters] Center at (" << center.x << ", " << center.y 
                        << ") is already connected" << std::endl;
            }
        }
        
        std::cout << "[SeedCenters] Seeded " << seededCount << " new highways for unconnected centers" << std::endl;
    }

    void HighwayGenerator::ConsumePopulationDensity(const Point& pos, float radius, float intensity) {
        // Określ zakres do przetworzenia (z marginesem)
        int minX = std::max(0, (int)(pos.x - radius));
        int maxX = std::min(m_Width - 1, (int)(pos.x + radius));
        int minY = std::max(0, (int)(pos.y - radius));
        int maxY = std::min(m_Height - 1, (int)(pos.y + radius));
        
        // Dla każdego punktu w zakresie
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                float dx = x - pos.x;
                float dy = y - pos.y;
                float dist = std::sqrt(dx * dx + dy * dy);
                
                if (dist < radius) {
                    // Funkcja Gaussa/rozmycia - dalej = słabszy efekt
                    float falloff = 1.0f - (dist / radius);
                    falloff = falloff * falloff; // Kwadratowy zanik
                    
                    // Zmniejsz gęstość
                    float reduction = intensity * falloff;
                    m_PopulationDensity[y][x] *= (1.0f - reduction);
                    
                    // Opcjonalnie: nie pozwól zejść poniżej zera
                    if (m_PopulationDensity[y][x] < 0.0f) {
                        m_PopulationDensity[y][x] = 0.0f;
                    }
                }
            }
        }
    }

    void HighwayGenerator::RemoveShortHighways() {
        std::cout << "\n[RemoveShort] ====== REMOVING SHORT HIGHWAYS ======" << std::endl;
        std::cout << "[RemoveShort] Initial highways: " << m_Highways.size() << std::endl;
        std::cout << "[RemoveShort] Minimum length threshold: " << MIN_HIGHWAY_LENGTH_POSTPROCESS << std::endl;

        int removedCount = 0;
        int protectedCount = 0;

        // Iteruj od tyłu, bo będziemy usuwać elementy
        for (int i = m_Highways.size() - 1; i >= 0; --i) {
            const Highway& highway = m_Highways[i];

            if (highway.totalLength < MIN_HIGHWAY_LENGTH_POSTPROCESS) {
                // Protect highways with bridges from removal
                if (highway.containsBridge) {
                    std::cout << "[RemoveShort] Protecting highway " << i
                            << " (contains bridge) length=" << highway.totalLength << std::endl;
                    protectedCount++;
                    continue;
                }

                std::cout << "[RemoveShort] Removing highway " << i
                        << " (" << highway.startIntersectionIdx
                        << " -> " << highway.endIntersectionIdx
                        << ") length=" << highway.totalLength << std::endl;

                RemoveHighway(i);
                removedCount++;
            }
        }

        std::cout << "[RemoveShort] Removed " << removedCount << " short highways" << std::endl;
        std::cout << "[RemoveShort] Protected " << protectedCount << " bridge highways" << std::endl;
        std::cout << "[RemoveShort] Final highways: " << m_Highways.size() << std::endl;
        std::cout << "[RemoveShort] =======================================" << std::endl;
    }

} // namespace CityGen