#pragma once

#include <vector>
#include <cmath>

namespace ScotlandYard {
namespace MapGen {

struct Point {
    float x, y;
    Point(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
};

struct Park {
    Point center;
    float f_BaseRadius;
    std::vector<Point> vec_Vertices;
    int i_NumSides;
    
    Park(Point c, float r);
    bool ContainsPoint(float f_X, float f_Y) const;
    void GeneratePolygon(); 
};

struct Edge {
    int i_Node1;
    int i_Node2;
    float f_Length;
    int i_Tier;  // 0 = Main artery, 1= important street, 2=street, 3= little sweet street
    
    Edge(int n1, int n2, float len, int tier = 3) 
        : i_Node1(n1), i_Node2(n2), f_Length(len), i_Tier(tier) {}
};

struct Triangle {
    int i_A, i_B, i_C;
    Triangle(int a, int b, int c) : i_A(a), i_B(b), i_C(c) {}
};

namespace Config {
    constexpr int RIVER_WIDTH = 40;
    constexpr int GRID_COLS = 20;
    constexpr int GRID_ROWS = 15;
    constexpr int GRID_MARGIN = 10;
    constexpr int NUM_PARKS = 3;
    constexpr float PARK_MIN_SIZE = 60.0f;
    constexpr float PARK_MAX_SIZE = 100.0f;
    constexpr float MIN_PARK_DISTANCE = 150.0f;
    constexpr float MIN_RIVER_DISTANCE = 50.0f;
}

float GetDistanceToRiver(const Point& p, const std::vector<Point>& vec_RiverPath);
bool ParkCollidesWithRiver(const Park& park, const std::vector<Point>& vec_RiverPath);
bool ParksCollide(const Park& park1, const Park& park2);
bool IsOnMajorSide(const Point& p, const std::vector<Point>& vec_RiverPath, int i_Corner);
Point BezierPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float f_T);

std::vector<Point> GenerateGridPoints(int i_Width, int i_Height);
std::vector<Point> GenerateRiverControlPoints(int* p_CurrentCorner, int i_Width, int i_Height);
std::vector<Point> GenerateRiverPath(const std::vector<Point>& vec_ControlPoints, int i_Segments = 100);
std::vector<Point> RemoveNodesOnRiver(const std::vector<Point>& vec_GridPoints,
                                       const std::vector<Point>& vec_RiverPath,
                                       float f_MinDistance = Config::RIVER_WIDTH / 2.0f + 10.0f);
std::vector<Park> GenerateParks(const std::vector<Point>& vec_GridPoints,
                                const std::vector<Point>& vec_RiverPath,
                                int i_Corner, int i_NumParks,
                                float f_MinSize = Config::PARK_MIN_SIZE,
                                float f_MaxSize = Config::PARK_MAX_SIZE);
std::vector<std::pair<Point, Point>> GenerateBridges(
    const std::vector<Point>& vec_GridPoints,
    const std::vector<Point>& vec_RiverPath,
    int i_NumBridges,
    int i_Corner
);

std::vector<Point> GenerateNodePositions(
    int i_Width, int i_Height,
    const std::vector<Point>& vec_RiverPath,
    const std::vector<Park>& vec_Parks,
    int i_NumNodes,
    unsigned int seed 
);

std::vector<Point> GenerateNodesWithParkDensity(
    int i_Width, int i_Height,
    const std::vector<Point>& vec_RiverPath,
    const std::vector<Park>& vec_Parks,
    int i_NumNodes,
    unsigned int seed
);

std::vector<Triangle> DelaunayTriangulation(const std::vector<Point>& vec_Points);
std::vector<Edge> MinimumSpanningTree(const std::vector<Point>& vec_Points, 
                                       const std::vector<Edge>& vec_Edges);
std::vector<Edge> GenerateStreetNetwork(
    const std::vector<Point>& vec_GridPoints,
    const std::vector<Point>& vec_RiverPath,
    const std::vector<std::pair<Point, Point>>& vec_Bridges,
     int i_MaxStreets = 100
);
bool LineIntersectsRiver(const Point& p1, const Point& p2, 
                         const std::vector<Point>& vec_RiverPath,
                         const std::vector<std::pair<Point, Point>>& vec_Bridges);

std::vector<Point> GenerateNodesOnStreets(
    const std::vector<std::vector<Point>>& vec_Streets,
    int i_NumNodes,
    unsigned int seed
);

} // namespace MapGen
} // namespace ScotlandYard