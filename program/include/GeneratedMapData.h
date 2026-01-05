#pragma once

#include "MapGenerator.h"
#include "BuildingGenerator.h"
#include "TreeGenerator.h"
#include "TreePlacement.h"
#include "GraphManager.h"
#include <vector>
#include <glm/glm.hpp>

namespace ScotlandYard {
namespace MapGen {

/**
 * @struct StreetSegment
 * Reprezentuje segment ulicy (kawałek drogi między dwoma punktami)
 */
struct StreetSegment {
    int i_Node1;           // ID pierwszego węzła grafu
    int i_Node2;           // ID drugiego węzła grafu
    int i_Tier;            // 0=arteria, 1=główna, 2=lokalna, 3=uliczka
    bool b_IsInPark;       // Czy to ścieżka w parku (inna tekstura)

    // Geometria ulicy (punkty kontrolne dla krzywej)
    std::vector<Point> vec_Geometry;

    float f_Width;         // Szerokość ulicy w metrach

    StreetSegment()
        : i_Node1(-1), i_Node2(-1), i_Tier(3), b_IsInPark(false), f_Width(4.0f) {}

    StreetSegment(int n1, int n2, int tier, bool inPark = false)
        : i_Node1(n1), i_Node2(n2), i_Tier(tier), b_IsInPark(inPark), f_Width(GetWidthForTier(tier)) {}

    static float GetWidthForTier(int tier) {
        switch(tier) {
            case 0: return 12.0f;  // Arteria - szeroka
            case 1: return 8.0f;   // Główna
            case 2: return 6.0f;   // Lokalna
            case 3: return 4.0f;   // Uliczka
            default: return 4.0f;
        }
    }
};

/**
 * @struct BuildingData
 * Dane o pojedynczym budynku
 */
struct BuildingData {
    std::vector<glm::vec2> vec_BaseFootprint;  // Podstawa budynku (4 punkty)
    glm::vec3 vec3_Position;                   // Pozycja środka budynku
    float f_Height;                            // Wysokość budynku
    bool b_HasRoof;                            // Czy ma dach dwuspadowy
    float f_RoofHeight;                        // Wysokość dachu (jeśli b_HasRoof=true)
    glm::vec3 vec3_WallColor;                  // Kolor ścian
    glm::vec3 vec3_RoofColor;                  // Kolor dachu
    int i_BuildingType;                        // Typ budynku (0=dom, 1=sklep, 2=biuro, etc.)

    BuildingData()
        : vec3_Position(0.0f), f_Height(8.0f), b_HasRoof(true), f_RoofHeight(2.5f),
          vec3_WallColor(0.9f, 0.85f, 0.7f), vec3_RoofColor(0.8f, 0.2f, 0.2f),
          i_BuildingType(0) {}
};

/**
 * @struct GraphNodeData
 * Rozszerzone dane o węźle grafu gry
 */
struct GraphNodeData {
    int i_ID;                      // Unikalny ID węzła
    Point position;                // Pozycja 2D na mapie

    // Połączenia (IDs innych węzłów)
    std::vector<int> vec_TaxiConnections;
    std::vector<int> vec_BusConnections;
    std::vector<int> vec_MetroConnections;
    std::vector<int> vec_FerryConnections;

    // Flagi
    bool b_IsInPark;
    bool b_IsNearRiver;
    bool b_IsMetroStation;
    bool b_IsFerryStop;

    GraphNodeData()
        : i_ID(-1), position(0, 0), b_IsInPark(false), b_IsNearRiver(false),
          b_IsMetroStation(false), b_IsFerryStop(false) {}

    GraphNodeData(int id, const Point& pos)
        : i_ID(id), position(pos), b_IsInPark(false), b_IsNearRiver(false),
          b_IsMetroStation(false), b_IsFerryStop(false) {}
};

/**
 * @struct GeneratedMapData
 * Główna struktura przechowująca wszystkie dane wygenerowanej mapy
 */
struct GeneratedMapData {
    // Wymiary mapy
    int i_Width;
    int i_Height;

    // ==== RZEKA ====
    std::vector<Point> vec_RiverPath;          // Punkty krzywej rzeki
    std::vector<Point> vec_RiverControlPoints; // Punkty kontrolne Beziera
    int i_RiverCorner;                         // Narożnik rzeki (0-3) lub 4 (środek)

    // ==== PARKI ====
    std::vector<Park> vec_Parks;

    // ==== MOSTY ====
    std::vector<std::pair<Point, Point>> vec_Bridges;

    // ==== ULICE ====
    std::vector<StreetSegment> vec_Streets;

    // ==== BUDYNKI ====
    std::vector<BuildingData> vec_Buildings;

    // ==== DRZEWA ====
    std::vector<Core::TreeInstance> vec_Trees;

    // ==== GRAF GRY (logiczne połączenia) ====
    std::vector<GraphNodeData> vec_GraphNodes;
    int i_NumGraphNodes;

    // ==== METADATA ====
    unsigned int ui_Seed;                      // Seed użyty do generowania
    std::string s_GenerationDate;              // Data wygenerowania

    GeneratedMapData()
        : i_Width(1200), i_Height(900), i_RiverCorner(4), i_NumGraphNodes(0), ui_Seed(0) {}

    /**
     * Czyści wszystkie dane
     */
    void Clear() {
        vec_RiverPath.clear();
        vec_RiverControlPoints.clear();
        vec_Parks.clear();
        vec_Bridges.clear();
        vec_Streets.clear();
        vec_Buildings.clear();
        vec_Trees.clear();
        vec_GraphNodes.clear();
        i_NumGraphNodes = 0;
    }

    /**
     * Sprawdza czy mapa ma jakieś dane
     */
    bool IsEmpty() const {
        return vec_GraphNodes.empty() && vec_Streets.empty() && vec_Buildings.empty();
    }

    /**
     * Zwraca liczbę obiektów
     */
    struct Stats {
        int nodes;
        int streets;
        int buildings;
        int trees;
        int parks;
        int bridges;
    };

    Stats GetStats() const {
        Stats s;
        s.nodes = static_cast<int>(vec_GraphNodes.size());
        s.streets = static_cast<int>(vec_Streets.size());
        s.buildings = static_cast<int>(vec_Buildings.size());
        s.trees = static_cast<int>(vec_Trees.size());
        s.parks = static_cast<int>(vec_Parks.size());
        s.bridges = static_cast<int>(vec_Bridges.size());
        return s;
    }
};

} // namespace MapGen
} // namespace ScotlandYard
