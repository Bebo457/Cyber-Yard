#include "MapGenerator.h"
#include <random>
#include <iostream>
#include <algorithm>

namespace ScotlandYard {
namespace MapGen {

// Park implementation
Park::Park(Point c, float r) : m_Center(c), m_f_BaseRadius(r), m_i_NumPoints(16) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(-0.3f, 0.6f);

    for (int i = 0; i < m_i_NumPoints; ++i) {
        m_vec_RadiusOffsets.push_back(1.0f + dis(gen));
    }
}

float Park::GetRadiusAt(float f_Angle) const {
    float normalizedAngle = f_Angle / (2.0f * 3.14159f) * m_i_NumPoints;
    int idx1 = static_cast<int>(normalizedAngle) % m_i_NumPoints;
    int idx2 = (idx1 + 1) % m_i_NumPoints;
    float t = normalizedAngle - static_cast<int>(normalizedAngle);
    
    float r1 = m_f_BaseRadius * m_vec_RadiusOffsets[idx1];
    float r2 = m_f_BaseRadius * m_vec_RadiusOffsets[idx2];
    
    return r1 * (1.0f - t) + r2 * t;
}

bool Park::ContainsPoint(float f_X, float f_Y) const {
    float dx = f_X - m_Center.f_X;
    float dy = f_Y - m_Center.f_Y;
    float dist = sqrt(dx*dx + dy*dy);
    float angle = atan2(dy, dx);
    if (angle < 0) angle += 2.0f * 3.14159f;

    return dist <= GetRadiusAt(angle);
}

// Helper functions
float GetDistanceToRiver(const Point& p, const std::vector<Point>& vec_RiverPath) {
    float minDist = 1e9f;
    
    for (const auto& rp : vec_RiverPath) {
        float dx = p.f_X - rp.f_X;
        float dy = p.f_Y - rp.f_Y;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist < minDist) {
            minDist = dist;
        }
    }
    
    return minDist;
}

bool ParkCollidesWithRiver(const Park& park, const std::vector<Point>& vec_RiverPath) {
    for (int i = 0; i < park.m_i_NumPoints; ++i) {
        float angle = (2.0f * 3.14159f * i) / park.m_i_NumPoints;
        float radius = park.GetRadiusAt(angle);
        Point edgePoint;
        edgePoint.f_X = park.m_Center.f_X + cos(angle) * radius;
        edgePoint.f_Y = park.m_Center.f_Y + sin(angle) * radius;

        float dist = GetDistanceToRiver(edgePoint, vec_RiverPath);
        if (dist < Config::MIN_RIVER_DISTANCE) {
            return true;
        }
    }

    return false;
}

bool ParksCollide(const Park& park1, const Park& park2) {
    float dx = park1.m_Center.f_X - park2.m_Center.f_X;
    float dy = park1.m_Center.f_Y - park2.m_Center.f_Y;
    float centerDist = sqrt(dx*dx + dy*dy);
    
    return centerDist < (park1.m_f_BaseRadius + park2.m_f_BaseRadius + Config::MIN_PARK_DISTANCE);
}

bool IsOnMajorSide(const Point& p, const std::vector<Point>& vec_RiverPath, int i_Corner) {
    float minDist = 1e9f;
    Point closestRiverPoint;
    
    for (const auto& rp : vec_RiverPath) {
        float dx = p.f_X - rp.f_X;
        float dy = p.f_Y - rp.f_Y;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist < minDist) {
            minDist = dist;
            closestRiverPoint = rp;
        }
    }
    
    switch(i_Corner) {
        case 0: return (p.f_X > closestRiverPoint.f_X) || (p.f_Y > closestRiverPoint.f_Y);
        case 1: return (p.f_X < closestRiverPoint.f_X) || (p.f_Y > closestRiverPoint.f_Y);
        case 2: return (p.f_X < closestRiverPoint.f_X) || (p.f_Y < closestRiverPoint.f_Y);
        case 3: return (p.f_X > closestRiverPoint.f_X) || (p.f_Y < closestRiverPoint.f_Y);
    }
    return true;
}

Point BezierPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float f_T) {
    float u = 1 - f_T;
    float tt = f_T * f_T;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * f_T;

    Point result;
    result.f_X = uuu * p0.f_X + 3 * uu * f_T * p1.f_X + 3 * u * tt * p2.f_X + ttt * p3.f_X;
    result.f_Y = uuu * p0.f_Y + 3 * uu * f_T * p1.f_Y + 3 * u * tt * p2.f_Y + ttt * p3.f_Y;

    return result;
}

// Generator functions
std::vector<Point> GenerateGridPoints(int i_Width, int i_Height) {
    std::vector<Point> gridPoints;
    
    float availableWidth = i_Width - 2 * Config::GRID_MARGIN;
    float availableHeight = i_Height - 2 * Config::GRID_MARGIN;
    
    float spacingX = availableWidth / (Config::GRID_COLS - 1);
    float spacingY = availableHeight / (Config::GRID_ROWS - 1);
    
    for (int row = 0; row < Config::GRID_ROWS; ++row) {
        for (int col = 0; col < Config::GRID_COLS; ++col) {
            float x = Config::GRID_MARGIN + col * spacingX;
            float y = Config::GRID_MARGIN + row * spacingY;
            gridPoints.push_back(Point(x, y));
        }
    }
    
    return gridPoints;
}

std::vector<Point> GenerateRiverControlPoints(int* p_CurrentCorner, int i_Width, int i_Height, float f_Curviness) {
    std::vector<Point> controlPoints;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> cornerDis(0, 3);
    std::uniform_real_distribution<float> varDis(0.0f, 0.1f);
    
    // Scale meander with curviness (0.0 = straight, 1.0 = very curvy)
    float meanderBase = 0.01f + f_Curviness * 0.15f;
    std::uniform_real_distribution<float> meanderDis(meanderBase, meanderBase + 0.05f);
    
    int corner = cornerDis(gen);
    *p_CurrentCorner = corner;
    
    float cutSize = 0.30f;
    float var1 = cutSize + varDis(gen);
    float meander1 = meanderDis(gen);
    float meander2 = meanderDis(gen);
    
    switch(corner) {
        case 0: // Top-left
            controlPoints.push_back(Point(i_Width * var1, 0));
            controlPoints.push_back(Point(i_Width * (var1 + meander1), i_Height * 0.08f));
            controlPoints.push_back(Point(i_Width * (var1 - meander2), i_Height * 0.12f));
            controlPoints.push_back(Point(i_Width * var1 * 0.7f, i_Height * var1 * 0.5f));
            controlPoints.push_back(Point(i_Width * 0.03f, i_Height * var1 * 0.7f));
            controlPoints.push_back(Point(i_Width * meander1, i_Height * (var1 - 0.02f)));
            controlPoints.push_back(Point(0, i_Height * var1));
            break;
        case 1: // Top-right
            controlPoints.push_back(Point(i_Width * (1.0f - var1), 0));
            controlPoints.push_back(Point(i_Width * (1.0f - var1 - meander1), i_Height * 0.08f));
            controlPoints.push_back(Point(i_Width * (1.0f - var1 + meander2), i_Height * 0.12f));
            controlPoints.push_back(Point(i_Width * (1.0f - var1 * 0.7f), i_Height * var1 * 0.5f));
            controlPoints.push_back(Point(i_Width * 0.97f, i_Height * var1 * 0.7f));
            controlPoints.push_back(Point(i_Width * (1.0f - meander1), i_Height * (var1 - 0.02f)));
            controlPoints.push_back(Point(i_Width, i_Height * var1));
            break;
        case 2: // Bottom-right
            controlPoints.push_back(Point(i_Width, i_Height * (1.0f - var1)));
            controlPoints.push_back(Point(i_Width * 0.97f, i_Height * (1.0f - var1 - meander1)));
            controlPoints.push_back(Point(i_Width * 0.92f, i_Height * (1.0f - var1 + meander2)));
            controlPoints.push_back(Point(i_Width * (1.0f - var1 * 0.5f), i_Height * (1.0f - var1 * 0.7f)));
            controlPoints.push_back(Point(i_Width * (1.0f - var1 * 0.7f), i_Height * 0.97f));
            controlPoints.push_back(Point(i_Width * (1.0f - var1 + 0.02f), i_Height * (1.0f - meander1)));
            controlPoints.push_back(Point(i_Width * (1.0f - var1), i_Height));
            break;
        case 3: // Bottom-left
            controlPoints.push_back(Point(0, i_Height * (1.0f - var1)));
            controlPoints.push_back(Point(i_Width * 0.03f, i_Height * (1.0f - var1 - meander1)));
            controlPoints.push_back(Point(i_Width * 0.08f, i_Height * (1.0f - var1 + meander2)));
            controlPoints.push_back(Point(i_Width * (var1 * 0.5f), i_Height * (1.0f - var1 * 0.7f)));
            controlPoints.push_back(Point(i_Width * (var1 * 0.7f), i_Height * 0.97f));
            controlPoints.push_back(Point(i_Width * (var1 - 0.02f), i_Height * (1.0f - meander1)));
            controlPoints.push_back(Point(i_Width * var1, i_Height));
            break;
    }
    
    return controlPoints;
}

std::vector<Point> GenerateRiverPath(const std::vector<Point>& vec_ControlPoints, int i_Segments) {
    std::vector<Point> riverPath;
    
    for (size_t i = 0; i + 3 < vec_ControlPoints.size(); i += 3) {
        for (int j = 0; j <= i_Segments; ++j) {
            float t = static_cast<float>(j) / i_Segments;
            Point p = BezierPoint(vec_ControlPoints[i], vec_ControlPoints[i+1], 
                                 vec_ControlPoints[i+2], vec_ControlPoints[i+3], t);
            riverPath.push_back(p);
        }
    }
    
    return riverPath;
}

std::vector<Park> GenerateParks(const std::vector<Point>& vec_GridPoints,
                                const std::vector<Point>& vec_RiverPath,
                                int i_Corner, const GenerationParams& params) {
    std::vector<Park> parks;
    std::vector<Point> validPoints;
    
    for (const auto& gp : vec_GridPoints) {
        if (IsOnMajorSide(gp, vec_RiverPath, i_Corner)) {
            float distToRiver = GetDistanceToRiver(gp, vec_RiverPath);
            if (distToRiver > Config::MIN_RIVER_DISTANCE + params.f_ParkMaxSize) {
                validPoints.push_back(gp);
            }
        }
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> sizeDis(params.f_ParkMinSize, params.f_ParkMaxSize);
    
    int attempts = 0;
    const int MAX_ATTEMPTS = 100;
    
    while (parks.size() < static_cast<size_t>(params.i_NumParks) && 
           attempts < MAX_ATTEMPTS && 
           !validPoints.empty()) {
        
        std::uniform_int_distribution<> idxDis(0, validPoints.size() - 1);
        int idx = idxDis(gen);
        Point center = validPoints[idx];
        
        float size = sizeDis(gen);
        Park newPark(center, size);
        
        bool hasCollision = ParkCollidesWithRiver(newPark, vec_RiverPath);
        
        for (const auto& existingPark : parks) {
            if (ParksCollide(newPark, existingPark)) {
                hasCollision = true;
                break;
            }
        }
        
        if (!hasCollision) {
            parks.push_back(newPark);
            validPoints.erase(validPoints.begin() + idx);
        } else {
            attempts++;
        }
    }
    
    std::cout << "Generated " << parks.size() << " parks" << std::endl;
    return parks;
}

} // namespace MapGen
} // namespace ScotlandYard
