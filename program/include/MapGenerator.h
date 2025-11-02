#pragma once

#include <vector>
#include <cmath>
#include <string>
#include <set>

namespace ScotlandYard {
namespace MapGen {

// Struktury
struct Point {
    float x, y;
    Point(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
};

struct Park {
    Point center;
    float f_BaseRadius;
    std::vector<float> vec_RadiusOffsets;
    int i_NumPoints;
    
    Park(Point c, float r);
    float GetRadiusAt(float f_Angle) const;
    bool ContainsPoint(float f_X, float f_Y) const;
};

// Enum for transport types
enum class TransportType {
    Taxi = 0,
    Bus = 1,
    Metro = 2,
    Ferry = 3
};

// Graph node structure
struct GraphNode {
    int i_ID;
    Point position;
    std::set<int> set_TaxiConnections;
    std::set<int> set_BusConnections;
    std::set<int> set_MetroConnections;
    std::set<int> set_FerryConnections;
    bool b_IsInPark = false;
    bool b_IsNearRiver = false;
    
    GraphNode(int id, Point pos) : i_ID(id), position(pos) {}
};

// Generation parameters
struct GenerationParams {
    int i_MapWidth = 1200;
    int i_MapHeight = 900;
    int i_NumParks = 3;
    float f_ParkMinSize = 60.0f;
    float f_ParkMaxSize = 100.0f;
    float f_MinParkDistance = 150.0f;
    float f_RiverCurviness = 0.5f; // 0.0 = straight, 1.0 = very curvy
    float f_TaxiDensity = 1.0f;    // Multiplier for taxi connections
    float f_BusDensity = 0.6f;     // Multiplier for bus connections
    float f_MetroDensity = 0.3f;   // Multiplier for metro connections
    int i_NumFerries = 4;          // Number of stops on the single ferry line
};

// Stałe generatora
namespace Config {
    constexpr int RIVER_WIDTH = 40;
    constexpr int GRID_COLS = 17;  // Changed from 20 to get ~200 nodes
    constexpr int GRID_ROWS = 12;  // Changed from 15 to get ~200 nodes (17*12 = 204)
    constexpr int GRID_MARGIN = 10;
    constexpr int NUM_PARKS = 3;
    constexpr float PARK_MIN_SIZE = 60.0f;
    constexpr float PARK_MAX_SIZE = 100.0f;
    constexpr float MIN_PARK_DISTANCE = 150.0f;
    constexpr float MIN_RIVER_DISTANCE = 50.0f;
    constexpr float TAXI_MAX_DISTANCE = 80.0f;
    constexpr float BUS_MAX_DISTANCE = 150.0f;
    constexpr float METRO_MAX_DISTANCE = 250.0f;
    constexpr int MIN_CONNECTIONS_PER_NODE = 2;
    constexpr int MAX_CONNECTIONS_PER_NODE = 5;
}

// Funkcje pomocnicze
float GetDistanceToRiver(const Point& p, const std::vector<Point>& vec_RiverPath);
bool ParkCollidesWithRiver(const Park& park, const std::vector<Point>& vec_RiverPath);
bool ParksCollide(const Park& park1, const Park& park2);
bool IsOnMajorSide(const Point& p, const std::vector<Point>& vec_RiverPath, int i_Corner);
Point BezierPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float f_T);

// Funkcje generujące
std::vector<Point> GenerateGridPoints(int i_Width, int i_Height);
std::vector<Point> GenerateRiverControlPoints(int* p_CurrentCorner, int i_Width, int i_Height, float f_Curviness = 0.5f);
std::vector<Point> GenerateRiverPath(const std::vector<Point>& vec_ControlPoints, int i_Segments = 100);
std::vector<Park> GenerateParks(const std::vector<Point>& vec_GridPoints,
                                const std::vector<Point>& vec_RiverPath,
                                int i_Corner, const GenerationParams& params);

// Graph generation
std::vector<GraphNode> GenerateGraph(const std::vector<Point>& vec_GridPoints,
                                     const std::vector<Point>& vec_RiverPath,
                                     const std::vector<Park>& vec_Parks,
                                     const GenerationParams& params);

void GenerateTaxiConnections(std::vector<GraphNode>& vec_Nodes, float f_Density);
void GenerateBusConnections(std::vector<GraphNode>& vec_Nodes, float f_Density);
void GenerateMetroConnections(std::vector<GraphNode>& vec_Nodes, float f_Density);
void GenerateFerryConnections(std::vector<GraphNode>& vec_Nodes, 
                               const std::vector<Point>& vec_RiverPath,
                               int i_NumFerries);

// Data export
bool ExportMapToJSON(const std::string& s_Filename,
                     const GenerationParams& params,
                     const std::vector<GraphNode>& vec_Nodes,
                     const std::vector<Point>& vec_RiverPath,
                     const std::vector<Park>& vec_Parks);

// PNG Export with connections
bool ExportMapToPNG(const std::string& s_Filename, 
                    int i_Width, int i_Height,
                    const std::vector<Point>& vec_GridPoints,
                    const std::vector<Point>& vec_RiverPath,
                    const std::vector<Park>& vec_Parks,
                    const std::vector<GraphNode>* p_Nodes = nullptr);

} // namespace MapGen
} // namespace ScotlandYard
