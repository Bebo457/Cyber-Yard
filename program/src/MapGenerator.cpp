#include "MapGenerator.h"
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <algorithm>
#include <set>
#include <functional>
#include <random>

namespace ScotlandYard {
namespace MapGen {

Park::Park(Point c, float r) : center(c), f_BaseRadius(r) {
    i_NumSides = 4 + (rand() % 5);
    GeneratePolygon();
}

void Park::GeneratePolygon() {
    vec_Vertices.clear();
    
    float f_StartAngle = (rand() % 360) * 3.14159f / 180.0f;
    
    for (int i = 0; i < i_NumSides; ++i) {
        float f_Angle = f_StartAngle + (2.0f * 3.14159f * i) / i_NumSides;
        
        float f_RadiusVariation = 0.90f + (rand() % 20) / 100.0f;
        float f_Radius = f_BaseRadius * f_RadiusVariation;
        
        Point vertex;
        vertex.x = center.x + cos(f_Angle) * f_Radius;
        vertex.y = center.y + sin(f_Angle) * f_Radius;
        vec_Vertices.push_back(vertex);
    }
}

bool Park::ContainsPoint(float f_X, float f_Y) const {
    bool b_Inside = false;
    int j = vec_Vertices.size() - 1;
    
    for (size_t i = 0; i < vec_Vertices.size(); i++) {
        if (((vec_Vertices[i].y > f_Y) != (vec_Vertices[j].y > f_Y)) &&
            (f_X < (vec_Vertices[j].x - vec_Vertices[i].x) * (f_Y - vec_Vertices[i].y) / 
                   (vec_Vertices[j].y - vec_Vertices[i].y) + vec_Vertices[i].x)) {
            b_Inside = !b_Inside;
        }
        j = i;
    }
    
    return b_Inside;
}

float GetDistanceToRiver(const Point& p, const std::vector<Point>& vec_RiverPath) {
    float f_MinDist = 1e9f;
    for (const auto& rp : vec_RiverPath) {
        float f_Dx = p.x - rp.x;
        float f_Dy = p.y - rp.y;
        float f_Dist = sqrt(f_Dx*f_Dx + f_Dy*f_Dy);
        if (f_Dist < f_MinDist) {
            f_MinDist = f_Dist;
        }
    }
    return f_MinDist;
}

bool ParkCollidesWithRiver(const Park& park, const std::vector<Point>& vec_RiverPath) {
    float f_CenterDist = GetDistanceToRiver(park.center, vec_RiverPath);
    if (f_CenterDist < (park.f_BaseRadius + Config::MIN_RIVER_DISTANCE)) {
        return true;
    }
    
    for (const auto& vertex : park.vec_Vertices) {
        float f_Dist = GetDistanceToRiver(vertex, vec_RiverPath);
        if (f_Dist < Config::MIN_RIVER_DISTANCE) {
            return true;
        }
    }
    
    return false;
}

bool ParksCollide(const Park& park1, const Park& park2) {
    float f_Dx = park1.center.x - park2.center.x;
    float f_Dy = park1.center.y - park2.center.y;
    float f_CenterDist = sqrt(f_Dx*f_Dx + f_Dy*f_Dy);
    return f_CenterDist < (park1.f_BaseRadius + park2.f_BaseRadius + Config::MIN_PARK_DISTANCE);
}

bool IsOnMajorSide(const Point& p, const std::vector<Point>& vec_RiverPath, int i_Corner) {

    if (i_Corner == 4){
        return true;
    }

    float f_MinDist = 1e9f;
    Point closestRiverPoint;
    for (const auto& rp : vec_RiverPath) {
        float f_Dx = p.x - rp.x;
        float f_Dy = p.y - rp.y;
        float f_Dist = sqrt(f_Dx*f_Dx + f_Dy*f_Dy);
        if (f_Dist < f_MinDist) {
            f_MinDist = f_Dist;
            closestRiverPoint = rp;
        }
    }
    
    switch(i_Corner) {
        case 0: return (p.x > closestRiverPoint.x) || (p.y > closestRiverPoint.y);
        case 1: return (p.x < closestRiverPoint.x) || (p.y > closestRiverPoint.y);
        case 2: return (p.x < closestRiverPoint.x) || (p.y < closestRiverPoint.y);
        case 3: return (p.x > closestRiverPoint.x) || (p.y < closestRiverPoint.y);
    }
    return true;
}

Point BezierPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float f_T) {
    float f_U = 1 - f_T;
    float f_Tt = f_T * f_T;
    float f_Uu = f_U * f_U;
    float f_Uuu = f_Uu * f_U;
    float f_Ttt = f_Tt * f_T;
    
    Point result;
    result.x = f_Uuu * p0.x + 3 * f_Uu * f_T * p1.x + 3 * f_U * f_Tt * p2.x + f_Ttt * p3.x;
    result.y = f_Uuu * p0.y + 3 * f_Uu * f_T * p1.y + 3 * f_U * f_Tt * p2.y + f_Ttt * p3.y;
    return result;
}

std::vector<Point> GenerateGridPoints(int i_Width, int i_Height) {
    std::vector<Point> vec_GridPoints;
    float f_AvailableWidth = i_Width - 2 * Config::GRID_MARGIN;
    float f_AvailableHeight = i_Height - 2 * Config::GRID_MARGIN;
    float f_SpacingX = f_AvailableWidth / (Config::GRID_COLS - 1);
    float f_SpacingY = f_AvailableHeight / (Config::GRID_ROWS - 1);
    
    for (int i_Row = 0; i_Row < Config::GRID_ROWS; ++i_Row) {
        for (int i_Col = 0; i_Col < Config::GRID_COLS; ++i_Col) {
            float f_X = Config::GRID_MARGIN + i_Col * f_SpacingX;
            float f_Y = Config::GRID_MARGIN + i_Row * f_SpacingY;
            vec_GridPoints.push_back(Point(f_X, f_Y));
        }
    }
    return vec_GridPoints;
}

std::vector<Point> GenerateRiverControlPoints(int* p_CurrentCorner, int i_Width, int i_Height) {
    std::vector<Point> vec_ControlPoints;
    
    int i_RiverType = rand() % 100;
    
    if (i_RiverType < 70) {
        *p_CurrentCorner = 4;  
        
        int i_Orientation = rand() % 2;
        
        if (i_Orientation == 0) {
            float f_CenterY = 0.5f;
            float f_MeanderAmp = 0.1f + (rand() % 100) / 500.0f;
            
            vec_ControlPoints.push_back(Point(0, i_Height * f_CenterY));
            
            float f_Offset1 = ((rand() % 2) * 2 - 1) * f_MeanderAmp; 
            vec_ControlPoints.push_back(Point(i_Width * 0.15f, i_Height * f_CenterY));
            vec_ControlPoints.push_back(Point(i_Width * 0.20f, i_Height * (f_CenterY + f_Offset1 * 0.5f)));
            vec_ControlPoints.push_back(Point(i_Width * 0.25f, i_Height * (f_CenterY + f_Offset1)));
            
            float f_Offset2 = -f_Offset1 * (0.8f + (rand() % 40) / 100.0f);
            vec_ControlPoints.push_back(Point(i_Width * 0.30f, i_Height * (f_CenterY + f_Offset1)));
            vec_ControlPoints.push_back(Point(i_Width * 0.40f, i_Height * (f_CenterY + f_Offset2 * 0.5f)));
            vec_ControlPoints.push_back(Point(i_Width * 0.50f, i_Height * (f_CenterY + f_Offset2)));
            
            float f_Offset3 = -f_Offset2 * (0.8f + (rand() % 40) / 100.0f);
            vec_ControlPoints.push_back(Point(i_Width * 0.55f, i_Height * (f_CenterY + f_Offset2)));
            vec_ControlPoints.push_back(Point(i_Width * 0.65f, i_Height * (f_CenterY + f_Offset3 * 0.5f)));
            vec_ControlPoints.push_back(Point(i_Width * 0.75f, i_Height * (f_CenterY + f_Offset3)));
            
            vec_ControlPoints.push_back(Point(i_Width * 0.80f, i_Height * (f_CenterY + f_Offset3)));
            vec_ControlPoints.push_back(Point(i_Width * 0.90f, i_Height * f_CenterY));
            vec_ControlPoints.push_back(Point(i_Width, i_Height * f_CenterY));
            
        } else {
            float f_CenterX = 0.5f;
            float f_MeanderAmp = 0.1f + (rand() % 100) / 500.0f;
            
            vec_ControlPoints.push_back(Point(i_Width * f_CenterX, 0));
            
            float f_Offset1 = ((rand() % 2) * 2 - 1) * f_MeanderAmp;
            vec_ControlPoints.push_back(Point(i_Width * f_CenterX, i_Height * 0.15f));
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset1 * 0.5f), i_Height * 0.20f));
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset1), i_Height * 0.25f));
            
            float f_Offset2 = -f_Offset1 * (0.8f + (rand() % 40) / 100.0f);
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset1), i_Height * 0.30f));
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset2 * 0.5f), i_Height * 0.40f));
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset2), i_Height * 0.50f));
            
            float f_Offset3 = -f_Offset2 * (0.8f + (rand() % 40) / 100.0f);
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset2), i_Height * 0.55f));
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset3 * 0.5f), i_Height * 0.65f));
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset3), i_Height * 0.75f));
            
            vec_ControlPoints.push_back(Point(i_Width * (f_CenterX + f_Offset3), i_Height * 0.80f));
            vec_ControlPoints.push_back(Point(i_Width * f_CenterX, i_Height * 0.90f));
            vec_ControlPoints.push_back(Point(i_Width * f_CenterX, i_Height));
        }
        
    } else {
        int i_Corner = rand() % 4;
        *p_CurrentCorner = i_Corner;
        
        float f_CutSize = 0.30f;
        float f_Var1 = f_CutSize + (rand() % 100) / 500.0f;
        float f_Meander1 = 0.01f + (rand() % 100) / 1000.0f;
        float f_Meander2 = 0.01f + (rand() % 100) / 1000.0f;
        
        switch(i_Corner) {
            case 0:
                vec_ControlPoints.push_back(Point(i_Width * f_Var1, 0));
                vec_ControlPoints.push_back(Point(i_Width * (f_Var1 + f_Meander1), i_Height * 0.08f));
                vec_ControlPoints.push_back(Point(i_Width * (f_Var1 - f_Meander2), i_Height * 0.12f));
                vec_ControlPoints.push_back(Point(i_Width * f_Var1 * 0.7f, i_Height * f_Var1 * 0.5f));
                vec_ControlPoints.push_back(Point(i_Width * 0.03f, i_Height * f_Var1 * 0.7f));
                vec_ControlPoints.push_back(Point(i_Width * f_Meander1, i_Height * (f_Var1 - 0.02f)));
                vec_ControlPoints.push_back(Point(0, i_Height * f_Var1));
                break;
            case 1: 
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1), 0));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1 - f_Meander1), i_Height * 0.08f));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1 + f_Meander2), i_Height * 0.12f));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1 * 0.7f), i_Height * f_Var1 * 0.5f));
                vec_ControlPoints.push_back(Point(i_Width * 0.97f, i_Height * f_Var1 * 0.7f));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Meander1), i_Height * (f_Var1 - 0.02f)));
                vec_ControlPoints.push_back(Point(i_Width, i_Height * f_Var1));
                break;
            case 2: 
                vec_ControlPoints.push_back(Point(i_Width, i_Height * (1.0f - f_Var1)));
                vec_ControlPoints.push_back(Point(i_Width * 0.97f, i_Height * (1.0f - f_Var1 - f_Meander1)));
                vec_ControlPoints.push_back(Point(i_Width * 0.92f, i_Height * (1.0f - f_Var1 + f_Meander2)));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1 * 0.5f), i_Height * (1.0f - f_Var1 * 0.7f)));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1 * 0.7f), i_Height * 0.97f));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1 + 0.02f), i_Height * (1.0f - f_Meander1)));
                vec_ControlPoints.push_back(Point(i_Width * (1.0f - f_Var1), i_Height));
                break;
            case 3: 
                vec_ControlPoints.push_back(Point(0, i_Height * (1.0f - f_Var1)));
                vec_ControlPoints.push_back(Point(i_Width * 0.03f, i_Height * (1.0f - f_Var1 - f_Meander1)));
                vec_ControlPoints.push_back(Point(i_Width * 0.08f, i_Height * (1.0f - f_Var1 + f_Meander2)));
                vec_ControlPoints.push_back(Point(i_Width * (f_Var1 * 0.5f), i_Height * (1.0f - f_Var1 * 0.7f)));
                vec_ControlPoints.push_back(Point(i_Width * (f_Var1 * 0.7f), i_Height * 0.97f));
                vec_ControlPoints.push_back(Point(i_Width * (f_Var1 - 0.02f), i_Height * (1.0f - f_Meander1)));
                vec_ControlPoints.push_back(Point(i_Width * f_Var1, i_Height));
                break;
        }
    }
    
    return vec_ControlPoints;
}


std::vector<Point> GenerateRiverPath(const std::vector<Point>& vec_ControlPoints, int i_Segments) {
    std::vector<Point> vec_RiverPath;
    
    if (vec_ControlPoints.size() == 4) {
        for (int j = 0; j <= i_Segments; ++j) {
            float f_T = static_cast<float>(j) / i_Segments;
            Point p = BezierPoint(vec_ControlPoints[0], vec_ControlPoints[1],
                                 vec_ControlPoints[2], vec_ControlPoints[3], f_T);
            vec_RiverPath.push_back(p);
        }
    }
    else {
        for (size_t i = 0; i + 3 < vec_ControlPoints.size(); i += 3) {
            for (int j = 0; j <= i_Segments; ++j) {
                float f_T = static_cast<float>(j) / i_Segments;
                Point p = BezierPoint(vec_ControlPoints[i], vec_ControlPoints[i+1],
                                     vec_ControlPoints[i+2], vec_ControlPoints[i+3], f_T);
                vec_RiverPath.push_back(p);
            }
        }
    }
    return vec_RiverPath;
}

std::vector<Point> RemoveNodesOnRiver(const std::vector<Point>& vec_GridPoints,
                                       const std::vector<Point>& vec_RiverPath,
                                       float f_MinDistance) {
    std::vector<Point> vec_FilteredPoints;
    vec_FilteredPoints.reserve(vec_GridPoints.size());
    
    int i_RemovedCount = 0;
    
    for (const auto& gridPoint : vec_GridPoints) {
        float f_DistToRiver = GetDistanceToRiver(gridPoint, vec_RiverPath);
        
        if (f_DistToRiver >= f_MinDistance) {
            vec_FilteredPoints.push_back(gridPoint);
        } else {
            i_RemovedCount++;
        }
    }
    
    // std::cout << "[MapGen] Removed " << i_RemovedCount << " nodes on river (distance < " 
    //           << f_MinDistance << ")" << std::endl;
    // std::cout << "[MapGen] Remaining nodes: " << vec_FilteredPoints.size() 
    //           << " / " << vec_GridPoints.size() << std::endl;
    
    return vec_FilteredPoints;
}


std::vector<Park> GenerateParks(const std::vector<Point>& vec_GridPoints,
                                const std::vector<Point>& vec_RiverPath,
                                int i_Corner, int i_NumParks,
                                float f_MinSize, float f_MaxSize) {
    std::vector<Park> vec_Parks;
    std::vector<Point> vec_ValidPoints;
    
    for (const auto& gp : vec_GridPoints) {
        if (IsOnMajorSide(gp, vec_RiverPath, i_Corner)) {
            float f_DistToRiver = GetDistanceToRiver(gp, vec_RiverPath);
            if (f_DistToRiver > Config::MIN_RIVER_DISTANCE + f_MaxSize) {
                vec_ValidPoints.push_back(gp);
            }
        }
    }
    
    std::cout << "[MapGen] Valid points for parks: " << vec_ValidPoints.size() << std::endl;
    std::cout << "[MapGen] Using park size range: " << f_MinSize << " - " << f_MaxSize << std::endl;
    
    int i_Attempts = 0;
    const int MAX_ATTEMPTS = 100;
    
    while (vec_Parks.size() < static_cast<size_t>(i_NumParks) &&
           i_Attempts < MAX_ATTEMPTS &&
           !vec_ValidPoints.empty()) {
        
        int i_Idx = rand() % vec_ValidPoints.size();
        Point center = vec_ValidPoints[i_Idx];
        
        float f_Range = f_MaxSize - f_MinSize;
        float f_Size = f_MinSize + (rand() % 100) / 100.0f * f_Range;
        
        std::cout << "[MapGen] Trying park with size: " << f_Size << std::endl;
        
        Park newPark(center, f_Size);
        
        bool b_HasCollision = false;
        
        if (ParkCollidesWithRiver(newPark, vec_RiverPath)) {
            b_HasCollision = true;
        }
        
        for (const auto& existingPark : vec_Parks) {
            if (ParksCollide(newPark, existingPark)) {
                b_HasCollision = true;
                break;
            }
        }
        
        if (!b_HasCollision) {
            vec_Parks.push_back(newPark);
            std::cout << "[MapGen] Park " << vec_Parks.size() << " generated at (" 
                      << center.x << ", " << center.y << ") with radius " << f_Size << std::endl;
            vec_ValidPoints.erase(vec_ValidPoints.begin() + i_Idx);
        } else {
            i_Attempts++;
        }
    }
    
    std::cout << "[MapGen] Generated " << vec_Parks.size() << " parks after " 
              << i_Attempts << " attempts" << std::endl;
    return vec_Parks;
}

std::vector<Point> GenerateNodesWithParkDensity(
    int i_Width, int i_Height,
    const std::vector<Point>& vec_RiverPath,
    const std::vector<Park>& vec_Parks,
    int i_NumNodes,
    unsigned int seed
) {
    if (seed) srand(seed);
    std::vector<Point> vec_Nodes;
    int attempts = 0;
    const int MAX_ATTEMPTS = i_NumNodes * 10;
    while ((int)vec_Nodes.size() < i_NumNodes && attempts < MAX_ATTEMPTS) {
        float x = Config::GRID_MARGIN + (rand() / (float)RAND_MAX) * (i_Width - 2 * Config::GRID_MARGIN);
        float y = Config::GRID_MARGIN + (rand() / (float)RAND_MAX) * (i_Height - 2 * Config::GRID_MARGIN);
        x += ((rand() / (float)RAND_MAX) - 0.5f) * 30.0f;
        y += ((rand() / (float)RAND_MAX) - 0.5f) * 30.0f;
        Point p(x, y);
        if (GetDistanceToRiver(p, vec_RiverPath) < Config::RIVER_WIDTH / 2.0f + 10.0f) {
            attempts++;
            continue;
        }
        bool inPark = false;
        for (const auto& park : vec_Parks) {
            if (park.ContainsPoint(p.x, p.y)) {
                inPark = true;
                break;
            }
        }
        if (inPark && (rand() % 100) > 30) {
            attempts++;
            continue;
        }
        bool tooClose = false;
        for (const auto& other : vec_Nodes) {
            float dx = other.x - p.x, dy = other.y - p.y;
            if (dx*dx + dy*dy < 900.0f) {
                tooClose = true;
                break;
            }
        }
        if (tooClose) {
            attempts++;
            continue;
        }
        vec_Nodes.push_back(p);
        attempts++;
    }
    return vec_Nodes;
}

std::vector<std::pair<Point, Point>> GenerateBridges(
    const std::vector<Point>& vec_GridPoints,
    const std::vector<Point>& vec_RiverPath,
    int i_NumBridges,
    int i_Corner
) {
    std::vector<std::pair<Point, Point>> vec_Bridges;
    
    if (i_NumBridges <= 0 || vec_GridPoints.empty() || vec_RiverPath.empty()) {
        return vec_Bridges;
    }
    
    std::vector<std::pair<Point, Point>> vec_PotentialBridges;
    
    for (size_t i = 0; i < vec_GridPoints.size(); ++i) {
        const Point& p1 = vec_GridPoints[i];
        float f_Dist1 = GetDistanceToRiver(p1, vec_RiverPath);
        
        if (f_Dist1 < 30.0f || f_Dist1 > 80.0f) continue;
        
        for (size_t j = i + 1; j < vec_GridPoints.size(); ++j) {
            const Point& p2 = vec_GridPoints[j];
            float f_Dist2 = GetDistanceToRiver(p2, vec_RiverPath);
            
            if (f_Dist2 < 30.0f || f_Dist2 > 80.0f) continue;
            
            float f_BridgeLength = sqrt(
                (p1.x - p2.x) * (p1.x - p2.x) + 
                (p1.y - p2.y) * (p1.y - p2.y)
            );
            
            if (f_BridgeLength < 50.0f || f_BridgeLength > 200.0f) continue;
            
            bool b_CrossesRiver = false;
            int i_Crossings = 0;
            
            for (size_t k = 0; k < vec_RiverPath.size() - 1; ++k) {
                const Point& r1 = vec_RiverPath[k];
                const Point& r2 = vec_RiverPath[k + 1];
                
                float f_DistToSegment = 0.0f;
                
                Point midPoint;
                midPoint.x = (p1.x + p2.x) / 2.0f;
                midPoint.y = (p1.y + p2.y) / 2.0f;
                
                float f_MidDist = GetDistanceToRiver(midPoint, vec_RiverPath);
                if (f_MidDist < Config::RIVER_WIDTH / 2.0f + 5.0f) {
                    b_CrossesRiver = true;
                    break;
                }
            }
            
            if (b_CrossesRiver) {
                vec_PotentialBridges.push_back({p1, p2});
            }
        }
    }
    
    // std::cout << "[MapGen] Found " << vec_PotentialBridges.size() 
    //           << " potential bridges" << std::endl;
    
    std::sort(vec_PotentialBridges.begin(), vec_PotentialBridges.end(),
        [](const std::pair<Point, Point>& a, const std::pair<Point, Point>& b) {
            float lenA = sqrt(
                (a.first.x - a.second.x) * (a.first.x - a.second.x) +
                (a.first.y - a.second.y) * (a.first.y - a.second.y)
            );
            float lenB = sqrt(
                (b.first.x - b.second.x) * (b.first.x - b.second.x) +
                (b.first.y - b.second.y) * (b.first.y - b.second.y)
            );
            return lenA < lenB;
        });
    
    for (const auto& bridge : vec_PotentialBridges) {
        if ((int)vec_Bridges.size() >= i_NumBridges) break;
        
        bool b_TooClose = false;
        for (const auto& existing : vec_Bridges) {
            float f_Dist = sqrt(
                (bridge.first.x - existing.first.x) * (bridge.first.x - existing.first.x) +
                (bridge.first.y - existing.first.y) * (bridge.first.y - existing.first.y)
            );
            if (f_Dist < 150.0f) {
                b_TooClose = true;
                break;
            }
        }
        
        if (!b_TooClose) {
            vec_Bridges.push_back(bridge);
        }
    }
    
    // std::cout << "[MapGen] Generated " << vec_Bridges.size() << " bridges" << std::endl;
    return vec_Bridges;
}

std::vector<Point> GenerateNodePositions(
    int i_Width, int i_Height,
    const std::vector<Point>& vec_RiverPath,
    const std::vector<Park>& vec_Parks,
    int i_NumNodes,
    unsigned int seed = 0
) {
    if (seed) srand(seed);

    std::vector<Point> vec_Nodes;
    int attempts = 0;
    const int MAX_ATTEMPTS = i_NumNodes * 10;

    while ((int)vec_Nodes.size() < i_NumNodes && attempts < MAX_ATTEMPTS) {
        float x = Config::GRID_MARGIN + (rand() / (float)RAND_MAX) * (i_Width - 2 * Config::GRID_MARGIN);
        float y = Config::GRID_MARGIN + (rand() / (float)RAND_MAX) * (i_Height - 2 * Config::GRID_MARGIN);

        x += ((rand() / (float)RAND_MAX) - 0.5f) * 30.0f;
        y += ((rand() / (float)RAND_MAX) - 0.5f) * 30.0f;

        Point p(x, y);

        if (GetDistanceToRiver(p, vec_RiverPath) < Config::RIVER_WIDTH / 2.0f + 10.0f)
        {
            attempts++;
            continue;
        }

        bool inPark = false;
        for (const auto& park : vec_Parks) {
            if (park.ContainsPoint(p.x, p.y)) {
                inPark = true;
                break;
            }
        }

        if (inPark && (rand() % 100) > 30) {
            attempts++;
            continue;
        }

        bool tooClose = false;
        for (const auto& other : vec_Nodes) {
            float dx = other.x - p.x, dy = other.y - p.y;
            if (dx*dx + dy*dy < 900.0f) { // 30px min distance
                tooClose = true;
                break;
            }
        }
        if (tooClose) {
            attempts++;
            continue;
        }

        vec_Nodes.push_back(p);
        attempts++;
    }

    return vec_Nodes;
}

// Helper: check if the point is inside the circle circumscribing the triangle
bool IsInCircumcircle(const Point& p, const Point& a, const Point& b, const Point& c) {
    float ax = a.x - p.x;
    float ay = a.y - p.y;
    float bx = b.x - p.x;
    float by = b.y - p.y;
    float cx = c.x - p.x;
    float cy = c.y - p.y;
    
    float det = (ax * ax + ay * ay) * (bx * cy - cx * by) -
                (bx * bx + by * by) * (ax * cy - cx * ay) +
                (cx * cx + cy * cy) * (ax * by - bx * ay);
    
    return det > 0;
}

std::vector<Triangle> DelaunayTriangulation(const std::vector<Point>& vec_Points) {
    std::vector<Triangle> vec_Triangles;
    
    if (vec_Points.size() < 3) return vec_Triangles;
    
    //Bowyer-Watson algorithm
    // Supertriangle
    Point p1(-10000, -10000);
    Point p2(10000, -10000);
    Point p3(0, 10000);
    
    vec_Triangles.push_back(Triangle(vec_Points.size(), vec_Points.size() + 1, vec_Points.size() + 2));
    
    std::vector<Point> allPoints = vec_Points;
    allPoints.push_back(p1);
    allPoints.push_back(p2);
    allPoints.push_back(p3);
    
    for (size_t i = 0; i < vec_Points.size(); ++i) {
        std::vector<std::pair<int, int>> edges;
        std::vector<Triangle> badTriangles;
        
        for (const auto& tri : vec_Triangles) {
            if (IsInCircumcircle(vec_Points[i], 
                                 allPoints[tri.i_A], 
                                 allPoints[tri.i_B], 
                                 allPoints[tri.i_C])) {
                badTriangles.push_back(tri);
                edges.push_back({tri.i_A, tri.i_B});
                edges.push_back({tri.i_B, tri.i_C});
                edges.push_back({tri.i_C, tri.i_A});
            }
        }
        
        // delete bad triangles
        for (const auto& bad : badTriangles) {
            vec_Triangles.erase(
                std::remove_if(vec_Triangles.begin(), vec_Triangles.end(),
                    [&](const Triangle& t) {
                        return t.i_A == bad.i_A && t.i_B == bad.i_B && t.i_C == bad.i_C;
                    }), 
                vec_Triangles.end());
        }
        
        std::vector<std::pair<int, int>> uniqueEdges;
        for (const auto& edge : edges) {
            int count = 0;
            for (const auto& e2 : edges) {
                if ((edge.first == e2.first && edge.second == e2.second) ||
                    (edge.first == e2.second && edge.second == e2.first)) {
                    count++;
                }
            }
            if (count == 1) {
                uniqueEdges.push_back(edge);
            }
        }
        
        for (const auto& edge : uniqueEdges) {
            vec_Triangles.push_back(Triangle(edge.first, edge.second, i));
        }
    }
    
    vec_Triangles.erase(
        std::remove_if(vec_Triangles.begin(), vec_Triangles.end(),
            [&](const Triangle& t) {
                return t.i_A >= (int)vec_Points.size() || 
                       t.i_B >= (int)vec_Points.size() || 
                       t.i_C >= (int)vec_Points.size();
            }), 
        vec_Triangles.end());
    
    std::cout << "[MapGen] Delaunay: " << vec_Triangles.size() << " triangles" << std::endl;
    return vec_Triangles;
}

std::vector<Edge> MinimumSpanningTree(const std::vector<Point>& vec_Points, 
                                       const std::vector<Edge>& vec_Edges) {
    std::vector<Edge> vec_MST;
    std::vector<int> parent(vec_Points.size());
    
    // Union-Find init
    for (size_t i = 0; i < vec_Points.size(); ++i) {
        parent[i] = i;
    }
    
    std::function<int(int)> find = [&](int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    };
    
    // Sort edges by length (Kruskal)
    std::vector<Edge> sortedEdges = vec_Edges;
    std::sort(sortedEdges.begin(), sortedEdges.end(),
        [](const Edge& a, const Edge& b) { return a.f_Length < b.f_Length; });
    
    for (const auto& edge : sortedEdges) {
        int root1 = find(edge.i_Node1);
        int root2 = find(edge.i_Node2);
        
        if (root1 != root2) {
            vec_MST.push_back(edge);
            parent[root1] = root2;
        }
    }
    
    std::cout << "[MapGen] MST: " << vec_MST.size() << " edges" << std::endl;
    return vec_MST;
}

bool LineIntersectsRiver(const Point& p1, const Point& p2, 
                         const std::vector<Point>& vec_RiverPath,
                         const std::vector<std::pair<Point, Point>>& vec_Bridges) {
    // check for bridge
    for (const auto& bridge : vec_Bridges) {
        float dist1 = sqrt((p1.x - bridge.first.x) * (p1.x - bridge.first.x) +
                          (p1.y - bridge.first.y) * (p1.y - bridge.first.y));
        float dist2 = sqrt((p2.x - bridge.second.x) * (p2.x - bridge.second.x) +
                          (p2.y - bridge.second.y) * (p2.y - bridge.second.y));
        
        if ((dist1 < 5.0f && dist2 < 5.0f) || (dist1 < 5.0f && dist2 < 5.0f)) {
            return false; // bridge
        }
    }
    
    //check river crossing
    Point mid;
    mid.x = (p1.x + p2.x) / 2.0f;
    mid.y = (p1.y + p2.y) / 2.0f;
    
    float minDist = GetDistanceToRiver(mid, vec_RiverPath);
    return minDist < Config::RIVER_WIDTH / 2.0f + 5.0f;
}

std::vector<Edge> GenerateStreetNetwork(
    const std::vector<Point>& vec_GridPoints,
    const std::vector<Point>& vec_RiverPath,
    const std::vector<std::pair<Point, Point>>& vec_Bridges,
    int i_MaxStreets
) {
    std::vector<Edge> vec_Streets;
    
    if (vec_GridPoints.size() < 3 || i_MaxStreets <= 0) return vec_Streets;
    
    std::cout << "[MapGen] Generating street network (max: " << i_MaxStreets << ")..." << std::endl;
    
    // Rozkład ulic według tier (proporcje)
    int i_Tier0Count = std::max(2, i_MaxStreets * 5 / 100);   // 5% - arterie główne (min 2)
    int i_Tier1Count = i_MaxStreets * 15 / 100;                // 15% - drogi główne
    int i_Tier2Count = i_MaxStreets * 35 / 100;                // 35% - drogi lokalne
    int i_Tier3Count = i_MaxStreets - i_Tier0Count - i_Tier1Count - i_Tier2Count; // 45% - uliczki
    
    std::cout << "[MapGen] Target distribution: T0=" << i_Tier0Count 
              << ", T1=" << i_Tier1Count << ", T2=" << i_Tier2Count 
              << ", T3=" << i_Tier3Count << std::endl;
    
    // 1. Delaunay triangulation
    std::vector<Triangle> triangles = DelaunayTriangulation(vec_GridPoints);
    
    // 2. Konwertuj trójkąty na krawędzie
    std::vector<Edge> allEdges;
    std::set<std::pair<int, int>> edgeSet;
    
    for (const auto& tri : triangles) {
        auto addEdge = [&](int a, int b) {
            if (a > b) std::swap(a, b);
            if (edgeSet.find({a, b}) == edgeSet.end()) {
                float len = sqrt(
                    (vec_GridPoints[a].x - vec_GridPoints[b].x) * 
                    (vec_GridPoints[a].x - vec_GridPoints[b].x) +
                    (vec_GridPoints[a].y - vec_GridPoints[b].y) * 
                    (vec_GridPoints[a].y - vec_GridPoints[b].y)
                );
                
                // Filtruj bardzo długie krawędzie (poza arteriami)
                if (len < 200.0f) {
                    edgeSet.insert({a, b});
                    allEdges.push_back(Edge(a, b, len));
                }
            }
        };
        
        addEdge(tri.i_A, tri.i_B);
        addEdge(tri.i_B, tri.i_C);
        addEdge(tri.i_C, tri.i_A);
    }
    
    std::cout << "[MapGen] Delaunay edges: " << allEdges.size() << std::endl;
    
    // 3. Usuń krawędzie przecinające rzekę (nie będące mostami)
    allEdges.erase(
        std::remove_if(allEdges.begin(), allEdges.end(),
            [&](const Edge& e) {
                return LineIntersectsRiver(vec_GridPoints[e.i_Node1], 
                                          vec_GridPoints[e.i_Node2],
                                          vec_RiverPath, vec_Bridges);
            }),
        allEdges.end());
    
    std::cout << "[MapGen] Edges after river filter: " << allEdges.size() << std::endl;
    
    // 4. MST dla spójności (podstawa dla tier 2 i 3)
    std::vector<Edge> mst = MinimumSpanningTree(vec_GridPoints, allEdges);
    std::cout << "[MapGen] MST size: " << mst.size() << std::endl;
    
    // 5. TIER 0 - Arterie główne (długie połączenia)
    std::vector<Edge> tier0Candidates;
    for (size_t i = 0; i < vec_GridPoints.size(); i += 15) {
        for (size_t j = i + 20; j < vec_GridPoints.size(); j += 15) {
            float dist = sqrt(
                (vec_GridPoints[i].x - vec_GridPoints[j].x) * 
                (vec_GridPoints[i].x - vec_GridPoints[j].x) +
                (vec_GridPoints[i].y - vec_GridPoints[j].y) * 
                (vec_GridPoints[i].y - vec_GridPoints[j].y)
            );
            
            if (dist > 250.0f && dist < 700.0f) {
                if (!LineIntersectsRiver(vec_GridPoints[i], vec_GridPoints[j],
                                        vec_RiverPath, vec_Bridges)) {
                    tier0Candidates.push_back(Edge(i, j, dist, 0));
                }
            }
        }
    }
    
    // Sortuj według długości (preferuj długie arterie)
    std::sort(tier0Candidates.begin(), tier0Candidates.end(),
        [](const Edge& a, const Edge& b) { return a.f_Length > b.f_Length; });
    
    // Dodaj tier 0
    for (int i = 0; i < i_Tier0Count && i < (int)tier0Candidates.size(); ++i) {
        vec_Streets.push_back(tier0Candidates[i]);
    }
    
    // 6. TIER 1 - Drogi główne (z MST, dłuższe)
    std::vector<Edge> tier1Candidates;
    for (auto edge : mst) {
        if (edge.f_Length > 100.0f) {
            edge.i_Tier = 1;
            tier1Candidates.push_back(edge);
        }
    }
    
    // Dodaj dodatkowe długie krawędzie z Delaunay
    for (const auto& edge : allEdges) {
        if (edge.f_Length > 100.0f && edge.f_Length < 180.0f) {
            bool exists = false;
            for (const auto& mstEdge : mst) {
                if ((edge.i_Node1 == mstEdge.i_Node1 && edge.i_Node2 == mstEdge.i_Node2) ||
                    (edge.i_Node1 == mstEdge.i_Node2 && edge.i_Node2 == mstEdge.i_Node1)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                Edge e = edge;
                e.i_Tier = 1;
                tier1Candidates.push_back(e);
            }
        }
    }
    
    std::sort(tier1Candidates.begin(), tier1Candidates.end(),
        [](const Edge& a, const Edge& b) { return a.f_Length > b.f_Length; });
    
    for (int i = 0; i < i_Tier1Count && i < (int)tier1Candidates.size(); ++i) {
        vec_Streets.push_back(tier1Candidates[i]);
    }
    
    // 7. TIER 2 - Drogi lokalne (z MST, średnie)
    std::vector<Edge> tier2Candidates;
    for (auto edge : mst) {
        if (edge.f_Length >= 50.0f && edge.f_Length <= 100.0f) {
            // Sprawdź czy nie jest już w tier 1
            bool inTier1 = false;
            for (const auto& t1 : vec_Streets) {
                if (t1.i_Tier == 1 && 
                    ((t1.i_Node1 == edge.i_Node1 && t1.i_Node2 == edge.i_Node2) ||
                     (t1.i_Node1 == edge.i_Node2 && t1.i_Node2 == edge.i_Node1))) {
                    inTier1 = true;
                    break;
                }
            }
            if (!inTier1) {
                edge.i_Tier = 2;
                tier2Candidates.push_back(edge);
            }
        }
    }
    
    for (int i = 0; i < i_Tier2Count && i < (int)tier2Candidates.size(); ++i) {
        vec_Streets.push_back(tier2Candidates[i]);
    }
    
    // 8. TIER 3 - Uliczki (z MST, krótkie + dodatkowe)
    std::vector<Edge> tier3Candidates;
    
    // Krótkie z MST
    for (auto edge : mst) {
        if (edge.f_Length < 50.0f) {
            bool alreadyAdded = false;
            for (const auto& existing : vec_Streets) {
                if ((existing.i_Node1 == edge.i_Node1 && existing.i_Node2 == edge.i_Node2) ||
                    (existing.i_Node1 == edge.i_Node2 && existing.i_Node2 == edge.i_Node1)) {
                    alreadyAdded = true;
                    break;
                }
            }
            if (!alreadyAdded) {
                edge.i_Tier = 3;
                tier3Candidates.push_back(edge);
            }
        }
    }
    
    // Krótkie z pozostałych krawędzi Delaunay
    for (const auto& edge : allEdges) {
        if (edge.f_Length < 80.0f) {
            bool exists = false;
            for (const auto& existing : vec_Streets) {
                if ((existing.i_Node1 == edge.i_Node1 && existing.i_Node2 == edge.i_Node2) ||
                    (existing.i_Node1 == edge.i_Node2 && existing.i_Node2 == edge.i_Node1)) {
                    exists = true;
                    break;
                }
            }
            for (const auto& t3 : tier3Candidates) {
                if ((t3.i_Node1 == edge.i_Node1 && t3.i_Node2 == edge.i_Node2) ||
                    (t3.i_Node1 == edge.i_Node2 && t3.i_Node2 == edge.i_Node1)) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                Edge e = edge;
                e.i_Tier = 3;
                tier3Candidates.push_back(e);
            }
        }
    }
    
    std::sort(tier3Candidates.begin(), tier3Candidates.end(),
        [](const Edge& a, const Edge& b) { return a.f_Length < b.f_Length; });
    
    for (int i = 0; i < i_Tier3Count && i < (int)tier3Candidates.size(); ++i) {
        vec_Streets.push_back(tier3Candidates[i]);
    }
    
    // Podsumowanie
    int actual[4] = {0, 0, 0, 0};
    for (const auto& street : vec_Streets) {
        actual[street.i_Tier]++;
    }
    
    std::cout << "[MapGen] Generated streets: " << vec_Streets.size() << " total" << std::endl;
    std::cout << "[MapGen] Actual: T0=" << actual[0] << ", T1=" << actual[1] 
              << ", T2=" << actual[2] << ", T3=" << actual[3] << std::endl;
    
    return vec_Streets;
}

std::vector<Point> GenerateNodesOnStreets(
    const std::vector<std::vector<Point>>& vec_Streets,
    int i_NumNodes,
    unsigned int seed
) {
    if (seed) srand(seed);
    std::vector<Point> vec_Nodes;
    std::vector<Point> sampledPoints;

    // Sample points along each street polyline
    for (const auto& street : vec_Streets) {
        for (size_t i = 1; i < street.size(); ++i) {
            const Point& p0 = street[i-1];
            const Point& p1 = street[i];
            int samples = (int)(sqrt((p1.x-p0.x)*(p1.x-p0.x)+(p1.y-p0.y)*(p1.y-p0.y))/30.0f);
            samples = std::max(samples, 1);
            for (int s = 0; s <= samples; ++s) {
                float t = s / (float)samples;
                Point pt;
                pt.x = p0.x + t * (p1.x - p0.x);
                pt.y = p0.y + t * (p1.y - p0.y);
                sampledPoints.push_back(pt);
            }
        }
    }

    // Randomly select node positions from sampled points
    std::random_shuffle(sampledPoints.begin(), sampledPoints.end());

    // Another version
    // std::random_device rd;
    // std::mt19937 g(rd());
    // std::shuffle(sampledPoints.begin(), sampledPoints.end(), g);

    for (size_t i = 0; i < sampledPoints.size() && (int)vec_Nodes.size() < i_NumNodes; ++i) {
        vec_Nodes.push_back(sampledPoints[i]);
    }

    return vec_Nodes;
}

} // namespace MapGen
} // namespace ScotlandYard