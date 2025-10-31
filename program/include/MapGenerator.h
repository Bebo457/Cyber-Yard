#pragma once

#include <vector>
#include <cmath>

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

// Stałe generatora
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

// Funkcje pomocnicze
float GetDistanceToRiver(const Point& p, const std::vector<Point>& vec_RiverPath);
bool ParkCollidesWithRiver(const Park& park, const std::vector<Point>& vec_RiverPath);
bool ParksCollide(const Park& park1, const Park& park2);
bool IsOnMajorSide(const Point& p, const std::vector<Point>& vec_RiverPath, int i_Corner);
Point BezierPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float f_T);

// Funkcje generujące
std::vector<Point> GenerateGridPoints(int i_Width, int i_Height);
std::vector<Point> GenerateRiverControlPoints(int* p_CurrentCorner, int i_Width, int i_Height);
std::vector<Point> GenerateRiverPath(const std::vector<Point>& vec_ControlPoints, int i_Segments = 100);
std::vector<Park> GenerateParks(const std::vector<Point>& vec_GridPoints,
                                const std::vector<Point>& vec_RiverPath,
                                int i_Corner, int i_NumParks);

} // namespace MapGen
} // namespace ScotlandYard
