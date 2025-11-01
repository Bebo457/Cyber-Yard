#include <SDL2/SDL.h>
#include <windows.h>
#include <iostream>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <ctime>

const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 900;
const int RIVER_WIDTH = 40; 

const int GRID_COLS = 20;      
const int GRID_ROWS = 15;      
const int GRID_MARGIN = 10;    
const int POINT_RADIUS = 3;    

const int NUM_PARKS = 3;
const float PARK_MIN_SIZE = 60.0f;   
const float PARK_MAX_SIZE = 100.0f;   
const float MIN_PARK_DISTANCE = 150.0f; 
const float MIN_RIVER_DISTANCE = 50.0f;


struct Point {
    float x, y;
    Point(float x_ = 0, float y_ = 0) : x(x_), y(y_) {}
};

struct Park {
    Point center;
    float baseRadius;
    std::vector<float> radiusOffsets;
    int numPoints;
    
    Park(Point c, float r) : center(c), baseRadius(r), numPoints(16) {
        for (int i = 0; i < numPoints; ++i) {
            float offset = -0.3f + (rand() % 100) / 100.0f * 0.6f;
            radiusOffsets.push_back(1.0f + offset);
        }
    }

    float getRadiusAt(float angle) const {
        float normalizedAngle = angle / (2.0f * 3.14159f) * numPoints;
        int idx1 = static_cast<int>(normalizedAngle) % numPoints;
        int idx2 = (idx1 + 1) % numPoints;
        float t = normalizedAngle - static_cast<int>(normalizedAngle);
        
        float r1 = baseRadius * radiusOffsets[idx1];
        float r2 = baseRadius * radiusOffsets[idx2];
        
        return r1 * (1.0f - t) + r2 * t;
    }

    bool containsPoint(float x, float y) const {
        float dx = x - center.x;
        float dy = y - center.y;
        float dist = sqrt(dx*dx + dy*dy);
        float angle = atan2(dy, dx);
        if (angle < 0) angle += 2.0f * 3.14159f;
        
        return dist <= getRadiusAt(angle);
    }
};

float getDistanceToRiver(const Point& p, const std::vector<Point>& riverPath) {
    float minDist = 1e9;
    
    for (const auto& rp : riverPath) {
        float dx = p.x - rp.x;
        float dy = p.y - rp.y;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist < minDist) {
            minDist = dist;
        }
    }
    
    return minDist;
}

bool parkCollidesWithRiver(const Park& park, const std::vector<Point>& riverPath) {
    float centerDist = getDistanceToRiver(park.center, riverPath);

    for (int i = 0; i < park.numPoints; ++i) {
        float angle = (2.0f * 3.14159f * i) / park.numPoints;
        float radius = park.getRadiusAt(angle);
        Point edgePoint;
        edgePoint.x = park.center.x + cos(angle) * radius;
        edgePoint.y = park.center.y + sin(angle) * radius;
        
        float dist = getDistanceToRiver(edgePoint, riverPath);
        if (dist < MIN_RIVER_DISTANCE) {
            return true;
        }
    }
    
    return centerDist < (park.baseRadius + MIN_RIVER_DISTANCE);
}

bool parksCollide(const Park& p1, const Park& p2) {
    float dx = p1.center.x - p2.center.x;
    float dy = p1.center.y - p2.center.y;
    float centerDist = sqrt(dx*dx + dy*dy);

    return centerDist < (p1.baseRadius + p2.baseRadius + MIN_PARK_DISTANCE);
}


bool isOnMajorSide(const Point& p, const std::vector<Point>& riverPath, int corner) {
    float minDist = 1e9;
    Point closestRiverPoint;
    
    for (const auto& rp : riverPath) {
        float dx = p.x - rp.x;
        float dy = p.y - rp.y;
        float dist = sqrt(dx*dx + dy*dy);
        if (dist < minDist) {
            minDist = dist;
            closestRiverPoint = rp;
        }
    }
    
    switch(corner) {
        case 0: //Left upper
            return (p.x > closestRiverPoint.x) || (p.y > closestRiverPoint.y);
        case 1: // Right upper
            return (p.x < closestRiverPoint.x) || (p.y > closestRiverPoint.y);
        case 2: // Right lower
            return (p.x < closestRiverPoint.x) || (p.y < closestRiverPoint.y);
        case 3: // Left lower
            return (p.x > closestRiverPoint.x) || (p.y < closestRiverPoint.y);
    }
    return true;
}


Point bezierPoint(const Point& p0, const Point& p1, const Point& p2, const Point& p3, float t) {
    float u = 1 - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;
    
    Point result;
    result.x = uuu * p0.x + 3 * uu * t * p1.x + 3 * u * tt * p2.x + ttt * p3.x;
    result.y = uuu * p0.y + 3 * uu * t * p1.y + 3 * u * tt * p2.y + ttt * p3.y;
    
    return result;
}

std::vector<Park> generateParks(const std::vector<Point>& gridPoints, 
                                const std::vector<Point>& riverPath, 
                                int corner, int numParks) {
    std::vector<Park> parks;
    std::vector<Point> validPoints;

    for (const auto& gp : gridPoints) {
        if (isOnMajorSide(gp, riverPath, corner)) {
            float distToRiver = getDistanceToRiver(gp, riverPath);
            if (distToRiver > MIN_RIVER_DISTANCE + PARK_MAX_SIZE) {
                validPoints.push_back(gp);
            }
        }
    }
    
    std::cout << "Valid points for parks: " << validPoints.size() << std::endl;

    int attempts = 0;
    const int MAX_ATTEMPTS = 100;
    
    while (parks.size() < static_cast<size_t>(numParks) && 
           attempts < MAX_ATTEMPTS && 
           !validPoints.empty()) {

        int idx = rand() % validPoints.size();
        Point center = validPoints[idx];

        float size = PARK_MIN_SIZE + (rand() % 100) / 100.0f * (PARK_MAX_SIZE - PARK_MIN_SIZE);
        Park newPark(center, size);

        bool hasCollision = false;

        if (parkCollidesWithRiver(newPark, riverPath)) {
            hasCollision = true;
        }

        for (const auto& existingPark : parks) {
            if (parksCollide(newPark, existingPark)) {
                hasCollision = true;
                break;
            }
        }
        
        if (!hasCollision) {
            parks.push_back(newPark);
            std::cout << "Park " << parks.size() << " generated at (" 
                      << center.x << ", " << center.y << ") with radius " << size << std::endl;
            validPoints.erase(validPoints.begin() + idx);
        } else {
            attempts++;
        }
    }
    
    std::cout << "Generated " << parks.size() << " parks after " << attempts << " attempts" << std::endl;
    return parks;
}

std::vector<Point> generateParkCenters(const std::vector<Point>& gridPoints, 
                                       const std::vector<Point>& riverPath, 
                                       int corner, int numParks) {
    std::vector<Point> parkCenters;
    std::vector<Point> validPoints;

    for (const auto& gp : gridPoints) {
        if (isOnMajorSide(gp, riverPath, corner)) {
            validPoints.push_back(gp);
        }
    }
    
    std::cout << "Valid points for parks: " << validPoints.size() << std::endl;

    for (int i = 0; i < numParks && !validPoints.empty(); ++i) {
        int idx = rand() % validPoints.size();
        parkCenters.push_back(validPoints[idx]);
        validPoints.erase(validPoints.begin() + idx);
    }
    
    std::cout << "Generated " << parkCenters.size() << " park centers" << std::endl;
    return parkCenters;
}

std::vector<Point> generateGridPoints() {
    std::vector<Point> gridPoints;
    
    float availableWidth = WINDOW_WIDTH - 2 * GRID_MARGIN;
    float availableHeight = WINDOW_HEIGHT - 2 * GRID_MARGIN;
    
    float spacingX = availableWidth / (GRID_COLS - 1);
    float spacingY = availableHeight / (GRID_ROWS - 1);
    
    for (int row = 0; row < GRID_ROWS; ++row) {
        for (int col = 0; col < GRID_COLS; ++col) {
            float x = GRID_MARGIN + col * spacingX;
            float y = GRID_MARGIN + row * spacingY;
            gridPoints.push_back(Point(x, y));
        }
    }
    
    return gridPoints;
}

std::vector<Point> generateRiverControlPoints(int *g_currentCorner) {
    std::vector<Point> controlPoints;

    int corner = rand() % 4;

    *g_currentCorner = corner;

    float cutSize = 0.30f;
    float var1 = cutSize + (rand() % 100) / 500.0f;

    float meander1 = 0.01f + (rand() % 100) / 1000.0f; 
    float meander2 = 0.01f + (rand() % 100) / 1000.0f; 
    
    switch(corner) {
        case 0: 
            std::cout << "Generated river cutting top-left corner" << std::endl;
            controlPoints.push_back(Point(WINDOW_WIDTH * var1, 0));
            controlPoints.push_back(Point(WINDOW_WIDTH * (var1 + meander1), WINDOW_HEIGHT * 0.08f));
            controlPoints.push_back(Point(WINDOW_WIDTH * (var1 - meander2), WINDOW_HEIGHT * 0.12f));
            controlPoints.push_back(Point(WINDOW_WIDTH * var1 * 0.7f, WINDOW_HEIGHT * var1 * 0.5f));
            controlPoints.push_back(Point(WINDOW_WIDTH * 0.03f, WINDOW_HEIGHT * var1 * 0.7f));
            controlPoints.push_back(Point(WINDOW_WIDTH * meander1, WINDOW_HEIGHT * (var1 - 0.02f)));
            controlPoints.push_back(Point(0, WINDOW_HEIGHT * var1));
            break;
            
        case 1:
            std::cout << "Generated river cutting top-right corner" << std::endl;
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1), 0));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1 - meander1), WINDOW_HEIGHT * 0.08f));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1 + meander2), WINDOW_HEIGHT * 0.12f));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1 * 0.7f), WINDOW_HEIGHT * var1 * 0.5f));
            controlPoints.push_back(Point(WINDOW_WIDTH * 0.97f, WINDOW_HEIGHT * var1 * 0.7f));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - meander1), WINDOW_HEIGHT * (var1 - 0.02f)));
            controlPoints.push_back(Point(WINDOW_WIDTH, WINDOW_HEIGHT * var1));
            break;
            
        case 2:
            std::cout << "Generated river cutting bottom-right corner" << std::endl;
            controlPoints.push_back(Point(WINDOW_WIDTH, WINDOW_HEIGHT * (1.0f - var1)));
            controlPoints.push_back(Point(WINDOW_WIDTH * 0.97f, WINDOW_HEIGHT * (1.0f - var1 - meander1)));
            controlPoints.push_back(Point(WINDOW_WIDTH * 0.92f, WINDOW_HEIGHT * (1.0f - var1 + meander2)));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1 * 0.5f), WINDOW_HEIGHT * (1.0f - var1 * 0.7f)));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1 * 0.7f), WINDOW_HEIGHT * 0.97f));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1 + 0.02f), WINDOW_HEIGHT * (1.0f - meander1)));
            controlPoints.push_back(Point(WINDOW_WIDTH * (1.0f - var1), WINDOW_HEIGHT));
            break;
            
        case 3: 
            std::cout << "Generated river cutting bottom-left corner" << std::endl;
            controlPoints.push_back(Point(0, WINDOW_HEIGHT * (1.0f - var1)));
            controlPoints.push_back(Point(WINDOW_WIDTH * 0.03f, WINDOW_HEIGHT * (1.0f - var1 - meander1)));
            controlPoints.push_back(Point(WINDOW_WIDTH * 0.08f, WINDOW_HEIGHT * (1.0f - var1 + meander2)));
            controlPoints.push_back(Point(WINDOW_WIDTH * (var1 * 0.5f), WINDOW_HEIGHT * (1.0f - var1 * 0.7f)));
            controlPoints.push_back(Point(WINDOW_WIDTH * (var1 * 0.7f), WINDOW_HEIGHT * 0.97f));
            controlPoints.push_back(Point(WINDOW_WIDTH * (var1 - 0.02f), WINDOW_HEIGHT * (1.0f - meander1)));
            controlPoints.push_back(Point(WINDOW_WIDTH * var1, WINDOW_HEIGHT));
            break;
    }
    
    return controlPoints;
}


std::vector<Point> generateRiverPath(const std::vector<Point>& controlPoints, int segments = 100) {
    std::vector<Point> riverPath;
    
    if (controlPoints.size() == 4) {
        for (int j = 0; j <= segments; ++j) {
            float t = static_cast<float>(j) / segments;
            Point p = bezierPoint(controlPoints[0], controlPoints[1], 
                                 controlPoints[2], controlPoints[3], t);
            riverPath.push_back(p);
        }
    }
    else {
        for (size_t i = 0; i + 3 < controlPoints.size(); i += 3) {
            for (int j = 0; j <= segments; ++j) {
                float t = static_cast<float>(j) / segments;
                Point p = bezierPoint(controlPoints[i], controlPoints[i+1], 
                                     controlPoints[i+2], controlPoints[i+3], t);
                riverPath.push_back(p);
            }
        }
    }
    
    return riverPath;
}

void drawParks(SDL_Renderer* renderer, const std::vector<Park>& parks) {
    SDL_SetRenderDrawColor(renderer, 34, 139, 34, 255);
    
    for (const auto& park : parks) {
        int minX = static_cast<int>(park.center.x - park.baseRadius * 1.5f);
        int maxX = static_cast<int>(park.center.x + park.baseRadius * 1.5f);
        int minY = static_cast<int>(park.center.y - park.baseRadius * 1.5f);
        int maxY = static_cast<int>(park.center.y + park.baseRadius * 1.5f);
        
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (x >= 0 && x < WINDOW_WIDTH && y >= 0 && y < WINDOW_HEIGHT) {
                    if (park.containsPoint(x, y)) {
                        SDL_RenderDrawPoint(renderer, x, y);
                    }
                }
            }
        }
    }
}

void drawGridPoints(SDL_Renderer* renderer, const std::vector<Point>& gridPoints) {
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    for (const auto& p : gridPoints) {
        for (int dx = -POINT_RADIUS; dx <= POINT_RADIUS; ++dx) {
            for (int dy = -POINT_RADIUS; dy <= POINT_RADIUS; ++dy) {
                if (dx*dx + dy*dy <= POINT_RADIUS * POINT_RADIUS) {
                    SDL_RenderDrawPoint(renderer, 
                                       static_cast<int>(p.x) + dx, 
                                       static_cast<int>(p.y) + dy);
                }
            }
        }
    }
}

void drawRiver(SDL_Renderer* renderer, const std::vector<Point>& riverPath) {
    SDL_SetRenderDrawColor(renderer, 50, 150, 255, 255);
    
    for (size_t i = 0; i < riverPath.size(); ++i) {
        for (int offset = -RIVER_WIDTH/2; offset <= RIVER_WIDTH/2; ++offset) {
            int x1 = static_cast<int>(riverPath[i].x) + offset;
            int y1 = static_cast<int>(riverPath[i].y);
            
            if (i + 1 < riverPath.size()) {
                int x2 = static_cast<int>(riverPath[i+1].x) + offset;
                int y2 = static_cast<int>(riverPath[i+1].y);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
            }
        }
    }
}

void drawControlPoints(SDL_Renderer* renderer, const std::vector<Point>& points) {
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); 
    
    for (const auto& p : points) {
        for (int dx = -3; dx <= 3; ++dx) {
            for (int dy = -3; dy <= 3; ++dy) {
                if (dx*dx + dy*dy <= 9) {
                    SDL_RenderDrawPoint(renderer, 
                                       static_cast<int>(p.x) + dx, 
                                       static_cast<int>(p.y) + dy);
                }
            }
        }
    }
}



int main(int argc, char* argv[]) {
    srand(static_cast<unsigned>(time(nullptr)));
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        MessageBoxA(NULL, SDL_GetError(), "SDL Init Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Board Generator - River",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    if (!window) {
        MessageBoxA(NULL, SDL_GetError(), "Window Creation Error", MB_ICONERROR | MB_OK);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!renderer) {
        MessageBoxA(NULL, SDL_GetError(), "Renderer Creation Error", MB_ICONERROR | MB_OK);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    int g_currentCorner = 0;

    std::cout << "Board Generator - River" << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  R - Regenerate river" << std::endl;
    std::cout << "  C - Toggle control points" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;

    bool running = true;
    bool showControlPoints = false;
    SDL_Event event;

    std::vector<Point> gridPoints = generateGridPoints();
    std::cout << "Generated " << gridPoints.size() << " grid points (" 
            << GRID_COLS << "x" << GRID_ROWS << ")" << std::endl;

    std::vector<Point> controlPoints = generateRiverControlPoints(&g_currentCorner);
    std::vector<Point> riverPath = generateRiverPath(controlPoints, 150);

    std::vector<Point> parkCenters = generateParkCenters(gridPoints, riverPath, 
                                                      g_currentCorner, NUM_PARKS);
    std::vector<Park> parks = generateParks(gridPoints, riverPath, g_currentCorner, NUM_PARKS);


    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.keysym.sym == SDLK_r) {
                    controlPoints = generateRiverControlPoints(&g_currentCorner);
                    riverPath = generateRiverPath(controlPoints, 150);
                    parks = generateParks(gridPoints, riverPath, g_currentCorner, NUM_PARKS);
                    std::cout << "River and parks regenerated" << std::endl;
                }
                if (event.key.keysym.sym == SDLK_c) {
                    showControlPoints = !showControlPoints;
                    std::cout << "Control points: " << (showControlPoints ? "ON" : "OFF") << std::endl;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        drawParks(renderer, parks);

        drawGridPoints(renderer, gridPoints);

        drawRiver(renderer, riverPath);
        
        if (showControlPoints) {
            drawControlPoints(renderer, controlPoints);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    return SDL_main(__argc, __argv);
}
