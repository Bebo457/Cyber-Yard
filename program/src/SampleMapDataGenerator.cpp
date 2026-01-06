#include "SampleMapDataGenerator.h"
#include "MapDataSerializer.h"
#include <ctime>
#include <iostream>
#include <cmath>
#include <filesystem>

namespace ScotlandYard {
namespace MapGen {

float SampleMapDataGenerator::RandomFloat(std::mt19937& rng, float min, float max) {
    std::uniform_real_distribution<float> dist(min, max);
    return dist(rng);
}

glm::vec3 SampleMapDataGenerator::RandomColor(std::mt19937& rng) {
    return glm::vec3(
        RandomFloat(rng, 0.6f, 0.95f),
        RandomFloat(rng, 0.6f, 0.95f),
        RandomFloat(rng, 0.6f, 0.95f)
    );
}

GeneratedMapData SampleMapDataGenerator::GenerateSimpleTestMap(unsigned int seed) {
    std::cout << "[SampleMapDataGenerator] Generating simple test map (seed=" << seed << ")..." << std::endl;

    GeneratedMapData data;
    data.i_Width = 1200;
    data.i_Height = 900;
    data.ui_Seed = seed;

    std::mt19937 rng(seed);

    // 1. Rzeka
    GenerateSimpleRiver(data);

    // 2. Parki
    GenerateSimpleParks(data);

    // 3. Graf (węzły)
    GenerateSimpleGraph(data);

    // 4. Ulice
    GenerateSimpleStreets(data);

    // 5. Budynki
    GenerateSimpleBuildings(data, rng);

    // 6. Drzewa
    GenerateSimpleTrees(data, rng);

    auto stats = data.GetStats();
    std::cout << "[SampleMapDataGenerator] Map generated:" << std::endl;
    std::cout << "  - Nodes: " << stats.nodes << std::endl;
    std::cout << "  - Streets: " << stats.streets << std::endl;
    std::cout << "  - Buildings: " << stats.buildings << std::endl;
    std::cout << "  - Trees: " << stats.trees << std::endl;
    std::cout << "  - Parks: " << stats.parks << std::endl;
    std::cout << "  - Bridges: " << stats.bridges << std::endl;

    return data;
}

void SampleMapDataGenerator::GenerateSimpleRiver(GeneratedMapData& data) {
    // Prosta rzeka - pozioma przez środek mapy
    data.i_RiverCorner = 4; // środek

    float centerY = data.i_Height * 0.5f;
    int numPoints = 50;

    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        float x = t * data.i_Width;

        // Dodaj lekkie fale
        float wave = sin(t * 3.14159f * 3.0f) * 30.0f;
        float y = centerY + wave;

        data.vec_RiverPath.push_back(Point(x, y));
    }

    std::cout << "[SampleMapDataGenerator] River: " << data.vec_RiverPath.size() << " points" << std::endl;
}

void SampleMapDataGenerator::GenerateSimpleParks(GeneratedMapData& data) {
    // 2 proste parki - jeden nad rzeką, jeden pod
    Point park1Center(300.0f, 250.0f);
    Park park1(park1Center, 80.0f);
    data.vec_Parks.push_back(park1);

    Point park2Center(900.0f, 650.0f);
    Park park2(park2Center, 70.0f);
    data.vec_Parks.push_back(park2);

    std::cout << "[SampleMapDataGenerator] Parks: " << data.vec_Parks.size() << std::endl;
}

void SampleMapDataGenerator::GenerateSimpleGraph(GeneratedMapData& data) {
    // Prosty graf 3x3 (9 węzłów)
    int gridCols = 3;
    int gridRows = 3;

    float marginX = 200.0f;
    float marginY = 150.0f;
    float spacingX = (data.i_Width - 2 * marginX) / (gridCols - 1);
    float spacingY = (data.i_Height - 2 * marginY) / (gridRows - 1);

    int nodeID = 1;

    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            float x = marginX + col * spacingX;
            float y = marginY + row * spacingY;

            GraphNodeData node(nodeID, Point(x, y));

            // Sprawdź czy w parku
            for (const auto& park : data.vec_Parks) {
                if (park.ContainsPoint(x, y)) {
                    node.b_IsInPark = true;
                    break;
                }
            }

            // Sprawdź czy przy rzece
            float minDist = 1e9f;
            for (const auto& rp : data.vec_RiverPath) {
                float dist = sqrt((x - rp.x) * (x - rp.x) + (y - rp.y) * (y - rp.y));
                if (dist < minDist) minDist = dist;
            }
            if (minDist < 60.0f) {
                node.b_IsNearRiver = true;
            }

            data.vec_GraphNodes.push_back(node);
            nodeID++;
        }
    }

    data.i_NumGraphNodes = static_cast<int>(data.vec_GraphNodes.size());

    // Dodaj połączenia taxi (sąsiedzi w siatce)
    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            int idx = row * gridCols + col;
            int id = idx + 1;

            // Prawo
            if (col + 1 < gridCols) {
                int neighborID = id + 1;
                data.vec_GraphNodes[idx].vec_TaxiConnections.push_back(neighborID);
            }
            // Dół
            if (row + 1 < gridRows) {
                int neighborID = id + gridCols;
                data.vec_GraphNodes[idx].vec_TaxiConnections.push_back(neighborID);
            }
            // Lewo
            if (col - 1 >= 0) {
                int neighborID = id - 1;
                data.vec_GraphNodes[idx].vec_TaxiConnections.push_back(neighborID);
            }
            // Góra
            if (row - 1 >= 0) {
                int neighborID = id - gridCols;
                data.vec_GraphNodes[idx].vec_TaxiConnections.push_back(neighborID);
            }
        }
    }

    // Dodaj kilka połączeń bus (dłuższe)
    data.vec_GraphNodes[0].vec_BusConnections.push_back(8); // 1 -> 9
    data.vec_GraphNodes[8].vec_BusConnections.push_back(1);

    data.vec_GraphNodes[2].vec_BusConnections.push_back(6); // 3 -> 7
    data.vec_GraphNodes[6].vec_BusConnections.push_back(3);

    // Metro - linia przez środek
    data.vec_GraphNodes[1].b_IsMetroStation = true;
    data.vec_GraphNodes[4].b_IsMetroStation = true;
    data.vec_GraphNodes[7].b_IsMetroStation = true;

    data.vec_GraphNodes[1].vec_MetroConnections.push_back(5);
    data.vec_GraphNodes[4].vec_MetroConnections.push_back(2);
    data.vec_GraphNodes[4].vec_MetroConnections.push_back(8);
    data.vec_GraphNodes[7].vec_MetroConnections.push_back(5);

    // Ferry - przez rzekę (jeśli węzły są przy rzece)
    for (auto& node : data.vec_GraphNodes) {
        if (node.b_IsNearRiver) {
            node.b_IsFerryStop = true;
        }
    }

    std::cout << "[SampleMapDataGenerator] Graph: " << data.vec_GraphNodes.size() << " nodes" << std::endl;
}

void SampleMapDataGenerator::GenerateSimpleStreets(GeneratedMapData& data) {
    // Generuj ulice na podstawie połączeń taxi
    for (const auto& node : data.vec_GraphNodes) {
        for (int neighborID : node.vec_TaxiConnections) {
            // Sprawdź czy już nie dodaliśmy odwrotnej krawędzi
            bool exists = false;
            for (const auto& street : data.vec_Streets) {
                if ((street.i_Node1 == node.i_ID && street.i_Node2 == neighborID) ||
                    (street.i_Node2 == node.i_ID && street.i_Node1 == neighborID)) {
                    exists = true;
                    break;
                }
            }

            if (!exists && neighborID > node.i_ID) { // Dodaj tylko raz (unikaj duplikatów)
                StreetSegment street;
                street.i_Node1 = node.i_ID;
                street.i_Node2 = neighborID;

                // Określ tier na podstawie odległości
                const auto& neighborNode = data.vec_GraphNodes[neighborID - 1];
                float dx = node.position.x - neighborNode.position.x;
                float dy = node.position.y - neighborNode.position.y;
                float dist = sqrt(dx * dx + dy * dy);

                if (dist > 400.0f) {
                    street.i_Tier = 0; // Arteria
                } else if (dist > 300.0f) {
                    street.i_Tier = 1; // Główna
                } else if (dist > 200.0f) {
                    street.i_Tier = 2; // Lokalna
                } else {
                    street.i_Tier = 3; // Uliczka
                }

                // Sprawdź czy w parku
                bool inPark1 = node.b_IsInPark;
                bool inPark2 = neighborNode.b_IsInPark;
                street.b_IsInPark = (inPark1 && inPark2);

                // Prosta geometria - linia prosta między węzłami
                street.vec_Geometry.push_back(node.position);
                street.vec_Geometry.push_back(neighborNode.position);

                street.f_Width = StreetSegment::GetWidthForTier(street.i_Tier);

                data.vec_Streets.push_back(street);
            }
        }
    }

    std::cout << "[SampleMapDataGenerator] Streets: " << data.vec_Streets.size() << std::endl;
}

void SampleMapDataGenerator::GenerateSimpleBuildings(GeneratedMapData& data, std::mt19937& rng) {
    // Generuj budynki wokół węzłów grafu i wzdłuż ulic

    // 1. Budynki przy węzłach (nie w parkach, nie przy rzece)
    for (const auto& node : data.vec_GraphNodes) {
        if (node.b_IsInPark || node.b_IsNearRiver) continue;

        // 3-5 budynków na węzeł (zwiększone)
        int numBuildings = (rng() % 3) + 3;

        for (int i = 0; i < numBuildings; ++i) {
            BuildingData building;

            // Offset od węzła - większy zasięg
            float angle = RandomFloat(rng, 0.0f, 2.0f * 3.14159f);
            float distance = RandomFloat(rng, 30.0f, 80.0f);
            float offsetX = cos(angle) * distance;
            float offsetY = sin(angle) * distance;

            building.vec3_Position = glm::vec3(
                node.position.x + offsetX,
                node.position.y + offsetY,
                0.0f
            );

            // Sprawdź czy nie w parku
            bool inPark = false;
            for (const auto& park : data.vec_Parks) {
                if (park.ContainsPoint(building.vec3_Position.x, building.vec3_Position.y)) {
                    inPark = true;
                    break;
                }
            }
            if (inPark) continue;

            // Losowy rozmiar - różnorodny
            float width = RandomFloat(rng, 12.0f, 30.0f);
            float depth = RandomFloat(rng, 12.0f, 30.0f);

            // Prostokątna podstawa (obracana losowo)
            float rotation = RandomFloat(rng, -0.3f, 0.3f);
            float cosR = cos(rotation);
            float sinR = sin(rotation);

            auto rotatePoint = [&](float x, float y) -> glm::vec2 {
                return glm::vec2(x * cosR - y * sinR, x * sinR + y * cosR);
            };

            building.vec_BaseFootprint.push_back(rotatePoint(-width/2, -depth/2));
            building.vec_BaseFootprint.push_back(rotatePoint( width/2, -depth/2));
            building.vec_BaseFootprint.push_back(rotatePoint( width/2,  depth/2));
            building.vec_BaseFootprint.push_back(rotatePoint(-width/2,  depth/2));

            // Losowa wysokość - zróżnicowana
            float heightCategory = RandomFloat(rng, 0.0f, 1.0f);
            if (heightCategory < 0.5f) {
                building.f_Height = RandomFloat(rng, 5.0f, 8.0f);  // Niskie budynki
            } else if (heightCategory < 0.85f) {
                building.f_Height = RandomFloat(rng, 8.0f, 15.0f); // Średnie
            } else {
                building.f_Height = RandomFloat(rng, 15.0f, 25.0f); // Wysokie
            }

            // Losowy dach
            building.b_HasRoof = (rng() % 10) < 7; // 70% szansa na dach
            building.f_RoofHeight = building.b_HasRoof ? RandomFloat(rng, 2.0f, 4.0f) : 0.0f;

            // Kolory - bardziej naturalne
            float colorType = RandomFloat(rng, 0.0f, 1.0f);
            if (colorType < 0.4f) {
                // Cegła/czerwone
                building.vec3_WallColor = glm::vec3(
                    RandomFloat(rng, 0.65f, 0.85f),
                    RandomFloat(rng, 0.45f, 0.55f),
                    RandomFloat(rng, 0.35f, 0.45f)
                );
            } else if (colorType < 0.7f) {
                // Beżowe/kremowe
                building.vec3_WallColor = glm::vec3(
                    RandomFloat(rng, 0.85f, 0.95f),
                    RandomFloat(rng, 0.80f, 0.90f),
                    RandomFloat(rng, 0.65f, 0.75f)
                );
            } else {
                // Szare/białe
                float gray = RandomFloat(rng, 0.7f, 0.95f);
                building.vec3_WallColor = glm::vec3(gray, gray, gray);
            }

            building.vec3_RoofColor = glm::vec3(
                RandomFloat(rng, 0.55f, 0.75f),
                RandomFloat(rng, 0.15f, 0.30f),
                RandomFloat(rng, 0.15f, 0.30f)
            );

            building.i_BuildingType = rng() % 3;

            data.vec_Buildings.push_back(building);
        }
    }

    // 2. Dodatkowe budynki wzdłuż ulic
    for (const auto& street : data.vec_Streets) {
        if (street.b_IsInPark) continue;

        // 2-4 budynki na ulicę
        int numBuildings = (rng() % 3) + 2;

        for (int i = 0; i < numBuildings; ++i) {
            // Wybierz losowy punkt wzdłuż ulicy
            float t = RandomFloat(rng, 0.2f, 0.8f);
            int idx1 = 0, idx2 = 1;
            if (street.vec_Geometry.size() > 1) {
                idx2 = street.vec_Geometry.size() - 1;
            }

            Point p1 = street.vec_Geometry[idx1];
            Point p2 = street.vec_Geometry[idx2];

            float x = p1.x + t * (p2.x - p1.x);
            float y = p1.y + t * (p2.y - p1.y);

            // Offset prostopadle do ulicy
            float dx = p2.x - p1.x;
            float dy = p2.y - p1.y;
            float len = sqrt(dx*dx + dy*dy);
            if (len < 0.001f) continue;

            float nx = -dy / len;  // Normalny
            float ny = dx / len;

            float side = (rng() % 2) * 2 - 1; // -1 lub 1
            float offset = RandomFloat(rng, 15.0f, 35.0f);

            BuildingData building;
            building.vec3_Position = glm::vec3(
                x + nx * offset * side,
                y + ny * offset * side,
                0.0f
            );

            // Sprawdź czy nie w parku
            bool inPark = false;
            for (const auto& park : data.vec_Parks) {
                if (park.ContainsPoint(building.vec3_Position.x, building.vec3_Position.y)) {
                    inPark = true;
                    break;
                }
            }
            if (inPark) continue;

            float width = RandomFloat(rng, 10.0f, 20.0f);
            float depth = RandomFloat(rng, 10.0f, 20.0f);

            building.vec_BaseFootprint.push_back(glm::vec2(-width/2, -depth/2));
            building.vec_BaseFootprint.push_back(glm::vec2( width/2, -depth/2));
            building.vec_BaseFootprint.push_back(glm::vec2( width/2,  depth/2));
            building.vec_BaseFootprint.push_back(glm::vec2(-width/2,  depth/2));

            building.f_Height = RandomFloat(rng, 6.0f, 12.0f);
            building.b_HasRoof = (rng() % 10) < 8;
            building.f_RoofHeight = building.b_HasRoof ? RandomFloat(rng, 2.0f, 3.5f) : 0.0f;

            float gray = RandomFloat(rng, 0.75f, 0.92f);
            building.vec3_WallColor = glm::vec3(gray, gray * 0.95f, gray * 0.9f);
            building.vec3_RoofColor = glm::vec3(0.65f, 0.2f, 0.2f);
            building.i_BuildingType = rng() % 3;

            data.vec_Buildings.push_back(building);
        }
    }

    std::cout << "[SampleMapDataGenerator] Buildings: " << data.vec_Buildings.size() << std::endl;
}

void SampleMapDataGenerator::GenerateSimpleTrees(GeneratedMapData& data, std::mt19937& rng) {
    // Generuj drzewa w parkach
    for (const auto& park : data.vec_Parks) {
        int treesPerPark = 8 + (rng() % 7); // 8-15 drzew na park

        for (int i = 0; i < treesPerPark; ++i) {
            Core::TreeInstance tree;

            // Losowa pozycja w parku (metoda rejection sampling)
            int attempts = 0;
            while (attempts < 50) {
                float angle = RandomFloat(rng, 0.0f, 2.0f * 3.14159f);
                float radius = RandomFloat(rng, 0.0f, park.f_BaseRadius * 0.8f);

                float x = park.center.x + cos(angle) * radius;
                float y = park.center.y + sin(angle) * radius;

                if (park.ContainsPoint(x, y)) {
                    tree.position = glm::vec3(x, y, 0.0f);
                    tree.scale = RandomFloat(rng, 0.8f, 1.3f);
                    tree.seed = rng();

                    data.vec_Trees.push_back(tree);
                    break;
                }
                attempts++;
            }
        }
    }

    std::cout << "[SampleMapDataGenerator] Trees: " << data.vec_Trees.size() << std::endl;
}

GeneratedMapData SampleMapDataGenerator::GenerateRealisticCityMap(unsigned int seed) {
    std::cout << "[SampleMapDataGenerator] Generating realistic city map (seed=" << seed << ")..." << std::endl;

    GeneratedMapData data;
    data.i_Width = 1200;
    data.i_Height = 900;
    data.ui_Seed = seed;

    std::mt19937 rng(seed);

    // 1. RZEKA - bardziej naturalna, wijąca się
    data.i_RiverCorner = 4;
    float centerY = data.i_Height * 0.55f;
    int numPoints = 80;

    for (int i = 0; i <= numPoints; ++i) {
        float t = static_cast<float>(i) / numPoints;
        float x = t * data.i_Width;

        // Złożona fala - bardziej naturalna
        float wave1 = sin(t * 3.14159f * 4.0f) * 35.0f;
        float wave2 = sin(t * 3.14159f * 7.0f) * 15.0f;
        float wave3 = cos(t * 3.14159f * 11.0f) * 8.0f;
        float y = centerY + wave1 + wave2 + wave3;

        data.vec_RiverPath.push_back(Point(x, y));
    }

    // 2. PARKI - 3 parki w różnych miejscach
    Point park1Center(250.0f, 200.0f);
    Park park1(park1Center, 90.0f);
    data.vec_Parks.push_back(park1);

    Point park2Center(950.0f, 700.0f);
    Park park2(park2Center, 75.0f);
    data.vec_Parks.push_back(park2);

    Point park3Center(600.0f, 300.0f);
    Park park3(park3Center, 60.0f);
    data.vec_Parks.push_back(park3);

    // 3. GRAF - siatka miejska 5x4 + dodatkowe węzły
    int gridCols = 5;
    int gridRows = 4;

    float marginX = 150.0f;
    float marginY = 100.0f;
    float spacingX = (data.i_Width - 2 * marginX) / (gridCols - 1);
    float spacingY = (data.i_Height - 2 * marginY) / (gridRows - 1);

    int nodeID = 1;

    // Główna siatka
    for (int row = 0; row < gridRows; ++row) {
        for (int col = 0; col < gridCols; ++col) {
            float x = marginX + col * spacingX;
            float y = marginY + row * spacingY;

            // Dodaj małe losowe przesunięcie dla realizmu
            x += RandomFloat(rng, -20.0f, 20.0f);
            y += RandomFloat(rng, -20.0f, 20.0f);

            GraphNodeData node(nodeID, Point(x, y));

            // Sprawdź czy w parku
            for (const auto& park : data.vec_Parks) {
                if (park.ContainsPoint(x, y)) {
                    node.b_IsInPark = true;
                    break;
                }
            }

            // Sprawdź czy przy rzece
            float minDist = 1e9f;
            for (const auto& rp : data.vec_RiverPath) {
                float dist = sqrt((x - rp.x) * (x - rp.x) + (y - rp.y) * (y - rp.y));
                if (dist < minDist) minDist = dist;
            }
            if (minDist < 70.0f) {
                node.b_IsNearRiver = true;
            }

            data.vec_GraphNodes.push_back(node);
            nodeID++;
        }
    }

    // Dodatkowe węzły między głównymi (gęstsza sieć)
    for (int row = 0; row < gridRows - 1; ++row) {
        for (int col = 0; col < gridCols - 1; ++col) {
            float x = marginX + (col + 0.5f) * spacingX;
            float y = marginY + (row + 0.5f) * spacingY;

            x += RandomFloat(rng, -15.0f, 15.0f);
            y += RandomFloat(rng, -15.0f, 15.0f);

            GraphNodeData node(nodeID, Point(x, y));

            for (const auto& park : data.vec_Parks) {
                if (park.ContainsPoint(x, y)) {
                    node.b_IsInPark = true;
                    break;
                }
            }

            float minDist = 1e9f;
            for (const auto& rp : data.vec_RiverPath) {
                float dist = sqrt((x - rp.x) * (x - rp.x) + (y - rp.y) * (y - rp.y));
                if (dist < minDist) minDist = dist;
            }
            if (minDist < 70.0f) {
                node.b_IsNearRiver = true;
            }

            data.vec_GraphNodes.push_back(node);
            nodeID++;
        }
    }

    data.i_NumGraphNodes = static_cast<int>(data.vec_GraphNodes.size());

    // 4. POŁĄCZENIA - realistyczna sieć transportu

    // Helper: znajdź najbliższych sąsiadów
    auto findNearestNodes = [&](int nodeIdx, float maxDist) -> std::vector<int> {
        std::vector<int> neighbors;
        const auto& node = data.vec_GraphNodes[nodeIdx];

        for (size_t i = 0; i < data.vec_GraphNodes.size(); ++i) {
            if (i == nodeIdx) continue;

            const auto& other = data.vec_GraphNodes[i];
            float dx = node.position.x - other.position.x;
            float dy = node.position.y - other.position.y;
            float dist = sqrt(dx*dx + dy*dy);

            if (dist < maxDist) {
                neighbors.push_back(other.i_ID);
            }
        }
        return neighbors;
    };

    // Taxi - gęsta sieć lokalnych połączeń
    for (size_t i = 0; i < data.vec_GraphNodes.size(); ++i) {
        auto& node = data.vec_GraphNodes[i];
        auto nearby = findNearestNodes(i, 180.0f);

        for (int id : nearby) {
            if (node.vec_TaxiConnections.size() < 6) { // Max 6 połączeń taxi
                node.vec_TaxiConnections.push_back(id);
            }
        }
    }

    // Bus - średnie odległości, główne drogi
    for (size_t i = 0; i < data.vec_GraphNodes.size(); ++i) {
        auto& node = data.vec_GraphNodes[i];
        if (node.b_IsInPark) continue;

        auto nearby = findNearestNodes(i, 280.0f);

        for (int id : nearby) {
            if (node.vec_BusConnections.size() < 3) {
                // Sprawdź czy już nie ma taxi
                bool hasTaxi = false;
                for (int taxiID : node.vec_TaxiConnections) {
                    if (taxiID == id) {
                        hasTaxi = true;
                        break;
                    }
                }
                if (!hasTaxi) {
                    node.vec_BusConnections.push_back(id);
                }
            }
        }
    }

    // Metro - linie metra (2 linie)
    std::vector<int> metroLine1 = {1, 6, 11, 16};  // Pionowa linia
    std::vector<int> metroLine2 = {3, 8, 13, 18}; // Druga pionowa

    for (int id : metroLine1) {
        if (id - 1 < data.vec_GraphNodes.size()) {
            data.vec_GraphNodes[id - 1].b_IsMetroStation = true;
        }
    }
    for (int id : metroLine2) {
        if (id - 1 < data.vec_GraphNodes.size()) {
            data.vec_GraphNodes[id - 1].b_IsMetroStation = true;
        }
    }

    // Połącz linie metro
    for (size_t i = 0; i + 1 < metroLine1.size(); ++i) {
        int id1 = metroLine1[i];
        int id2 = metroLine1[i + 1];
        if (id1 - 1 < data.vec_GraphNodes.size() && id2 - 1 < data.vec_GraphNodes.size()) {
            data.vec_GraphNodes[id1 - 1].vec_MetroConnections.push_back(id2);
            data.vec_GraphNodes[id2 - 1].vec_MetroConnections.push_back(id1);
        }
    }
    for (size_t i = 0; i + 1 < metroLine2.size(); ++i) {
        int id1 = metroLine2[i];
        int id2 = metroLine2[i + 1];
        if (id1 - 1 < data.vec_GraphNodes.size() && id2 - 1 < data.vec_GraphNodes.size()) {
            data.vec_GraphNodes[id1 - 1].vec_MetroConnections.push_back(id2);
            data.vec_GraphNodes[id2 - 1].vec_MetroConnections.push_back(id1);
        }
    }

    // Ferry - wzdłuż rzeki
    for (auto& node : data.vec_GraphNodes) {
        if (node.b_IsNearRiver) {
            node.b_IsFerryStop = true;
        }
    }

    std::cout << "[SampleMapDataGenerator] Graph: " << data.vec_GraphNodes.size() << " nodes" << std::endl;

    // 5. ULICE - na podstawie połączeń
    for (const auto& node : data.vec_GraphNodes) {
        for (int neighborID : node.vec_TaxiConnections) {
            if (neighborID <= node.i_ID) continue; // Unikaj duplikatów

            bool exists = false;
            for (const auto& street : data.vec_Streets) {
                if ((street.i_Node1 == node.i_ID && street.i_Node2 == neighborID) ||
                    (street.i_Node2 == node.i_ID && street.i_Node1 == neighborID)) {
                    exists = true;
                    break;
                }
            }

            if (!exists) {
                const auto& neighborNode = data.vec_GraphNodes[neighborID - 1];
                float dx = node.position.x - neighborNode.position.x;
                float dy = node.position.y - neighborNode.position.y;
                float dist = sqrt(dx * dx + dy * dy);

                StreetSegment street;
                street.i_Node1 = node.i_ID;
                street.i_Node2 = neighborID;

                // Określ tier
                if (dist > 250.0f) {
                    street.i_Tier = 1; // Główna
                } else if (dist > 180.0f) {
                    street.i_Tier = 2; // Lokalna
                } else {
                    street.i_Tier = 3; // Uliczka
                }

                street.b_IsInPark = (node.b_IsInPark && neighborNode.b_IsInPark);
                street.vec_Geometry.push_back(node.position);
                street.vec_Geometry.push_back(neighborNode.position);
                street.f_Width = StreetSegment::GetWidthForTier(street.i_Tier);

                data.vec_Streets.push_back(street);
            }
        }
    }

    std::cout << "[SampleMapDataGenerator] Streets: " << data.vec_Streets.size() << std::endl;

    // 6. BUDYNKI - dużo budynków!
    GenerateSimpleBuildings(data, rng);

    // 7. DRZEWA
    GenerateSimpleTrees(data, rng);

    auto stats = data.GetStats();
    std::cout << "[SampleMapDataGenerator] Realistic city map generated:" << std::endl;
    std::cout << "  - Nodes: " << stats.nodes << std::endl;
    std::cout << "  - Streets: " << stats.streets << std::endl;
    std::cout << "  - Buildings: " << stats.buildings << std::endl;
    std::cout << "  - Trees: " << stats.trees << std::endl;
    std::cout << "  - Parks: " << stats.parks << std::endl;

    return data;
}

GeneratedMapData SampleMapDataGenerator::GenerateMediumTestMap(unsigned int seed) {
    std::cout << "[SampleMapDataGenerator] Medium test map redirects to realistic city." << std::endl;
    return GenerateRealisticCityMap(seed);
}

GeneratedMapData SampleMapDataGenerator::GenerateFullMap(
    int width, int height, unsigned int seed,
    int numNodes, int numParks, int maxStreets)
{
    std::cout << "[SampleMapDataGenerator] Full map generation not implemented yet." << std::endl;
    std::cout << "[SampleMapDataGenerator] This would call the real MapGenerator functions." << std::endl;
    std::cout << "[SampleMapDataGenerator] Returning realistic city map for now..." << std::endl;

    GeneratedMapData data = GenerateRealisticCityMap(seed);
    data.i_Width = width;
    data.i_Height = height;

    return data;
}

bool SampleMapDataGenerator::GenerateAndSave(const std::string& filepath, unsigned int seed) {
    std::cout << "[SampleMapDataGenerator] Generating and saving map to: " << filepath << std::endl;

    // Generate realistic city map
    GeneratedMapData data = GenerateRealisticCityMap(seed);

    // Add generation date
    std::time_t now = std::time(nullptr);
    char buf[80];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    data.s_GenerationDate = std::string(buf);

    // Create directory if it doesn't exist
    std::filesystem::path path(filepath);
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    // Save to file
    return MapDataSerializer::SaveToFile(data, filepath);
}

} // namespace MapGen
} // namespace ScotlandYard
