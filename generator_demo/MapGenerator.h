#pragma once
#include <vector>
#include <cmath>
#include <random>

namespace CityGen {

struct Point {
    float x, y;
    bool isIntersection;
    std::vector<int> connectedRoadIndices;
    
    Point(float x_ = 0, float y_ = 0) : x(x_), y(y_), isIntersection(false) {}
};

struct Road {
    int startNodeIdx;
    int endNodeIdx;
    bool isDeleted;  // NOWE
    
    Road(int s, int e) : startNodeIdx(s), endNodeIdx(e), isDeleted(false) {}  // ZMIENIONE
};

struct Highway {
    int startIntersectionIdx;
    int endIntersectionIdx;
    std::vector<int> roadIndices;
    float totalLength;
    
    Highway(int start, int end, const std::vector<int>& roads, float length)
        : startIntersectionIdx(start)
        , endIntersectionIdx(end)
        , roadIndices(roads)
        , totalLength(length) {}
};

struct Branch {
    int parentNodeIdx;
    Point direction;
    int delay;
    int iterationsLeft;
    
    Branch(int parent, Point dir, int del, int iters) 
        : parentNodeIdx(parent), direction(dir), delay(del), iterationsLeft(iters) {}
};

struct HighwayEnd {
    int currentNodeIdx;
    Point direction;
    int iterationsLeft;
    float distanceSinceLastBranch;
    
    // NOWE pola:
    int lastIntersectionIdx;
    std::vector<int> roadsSinceLastIntersection;
    
    HighwayEnd(int node, Point dir, int iters) 
        : currentNodeIdx(node)
        , direction(dir)
        , iterationsLeft(iters)
        , distanceSinceLastBranch(0.0f)
        , lastIntersectionIdx(node)  // NOWE: startujemy od pierwszego węzła jako "skrzyżowania"
        , roadsSinceLastIntersection() {}  // NOWE: pusta lista
};

class MapGenerator {
public:
    MapGenerator(int width, int height);
    ~MapGenerator();
    
    void Generate();
    
    const std::vector<Road>& GetRoads() const { return m_Roads; }
    const std::vector<Highway>& GetHighways() const { return m_Highways; }
    
    // Getters
    float GetDensityAt(int x, int y) const;
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    const std::vector<Point>& GetRoadNodes() const { return m_RoadNodes; }
    
    
private:
    static constexpr float HIGHWAY_SAMPLE_RADIUS = 500.0f;
    static constexpr int HIGHWAY_NUM_RAYS = 30;
    static constexpr int HIGHWAY_SAMPLES_PER_RAY = 50;
    static constexpr float HIGHWAY_SEGMENT_LENGTH = 10.0f;
    static constexpr int HIGHWAY_MAX_ITERATIONS = 300;
    static constexpr float HIGHWAY_FOV = 3.14159f * 0.333f;

    // PARAMETRY GLOBALGOALS
    static constexpr float BRANCH_MIN_DENSITY_SCORE = 0.3f;
    static constexpr float BRANCH_PARALLEL_ANGLE_THRESHOLD = 20.0f * 3.14159f / 180.0f;
    static constexpr float BRANCH_PARALLEL_SEARCH_RADIUS = 50.0f;
    static constexpr float BRANCH_AREA_SEARCH_RADIUS = 100.0f;
    static constexpr int BRANCH_AREA_MAX_ROADS = 2;
    static constexpr float BRANCH_LOOKAHEAD_DISTANCE = 100.0f;
    
    static constexpr int BRANCH_DELAY_MIN = 3;
    static constexpr int BRANCH_DELAY_MAX = 8;
    static constexpr float BRANCH_ANGLE = 60.0f * 3.14159f / 180.0f;

    // parametry sprawdzania równoległości podczas wzrostu
    static constexpr float PARALLEL_GROWTH_CHECK_RADIUS = 50.0f;
    static constexpr float PARALLEL_GROWTH_ANGLE_THRESHOLD = 20.0f * 3.14159f / 180.0f; // 20 stopni

    // PARAMETRY HIGHWAY PROXIMITY CHECK
    static constexpr float MIN_VIABLE_HIGHWAY_LENGTH = 200.0f;
    static constexpr float RAY_CAST_DISTANCE = 200.0f;  // Trochę więcej niż MIN
    static constexpr int HIGHWAY_PROXIMITY_NUM_RAYS = 5;
    static constexpr float RAY_SPREAD_ANGLE = 20.0f * 3.14159f / 180.0f;  // ±10°

    std::vector<Highway> m_Highways;

    
    int m_Width;
    int m_Height;
    std::vector<Road> m_Roads;
    std::vector<Point> m_RoadNodes;
    std::vector<std::vector<float>> m_PopulationDensity;
    std::vector<HighwayEnd> m_ActiveEnds;
    std::vector<Branch> m_SleepingBranches;

    void GeneratePopulationDensity();
    
    void GenerateHighways();
    
    bool CheckLocalConstraints(const Point& start, const Point& end, int startNodeIdx, int& endNodeIdx);

    void GenerateStreets();

    void GlobalGoalsForBranch(Branch& branch, const HighwayEnd& parent);

    // metody pomocnicze generowania highway
    float ShootRay(Point pos, float angle);
    Point FindBestDirection(Point pos, Point prevDirection);

    // metody pomocnicze wykrywania przecięcia ulic
    bool DoSegmentsIntersect(const Point& a1, const Point& a2, const Point& b1, const Point& b2, Point& intersection);
    int FindNearestNode(const Point& pos, float maxDist, bool intersectionsOnly = false);
    int CreateOrGetNode(const Point& pos, bool isIntersection = false);
    std::vector<int> FindNearbyRoadIndices(const Point& pos, float radius);

    // Metody pomocnicze tworzenia nowych branchy
    bool GrowHighwayOneStep(HighwayEnd& hw);
    void CreateBranchCandidates(const HighwayEnd& parent);
    void UpdateSleepingBranches();
    float DistancePointToSegment(const Point& p, const Point& a, const Point& b, Point& closest);

    // NOWA METODA: Sprawdzanie równoległości podczas wzrostu
    bool CheckParallelDuringGrowth(const Point& start, const Point& end, int startNodeIdx);

    // NOWE: Zarządzanie Highway
    void CreateHighway(int startIntersection, int endIntersection, const std::vector<int>& roads);
    bool CheckAndRemoveRedundantHighway(const Highway& newHighway);
    void RemoveHighway(int highwayIdx);
    void RemoveRoads(const std::vector<int>& roadIndices);
    void SplitHighwaysAtIntersection(int intersectionNodeIdx);

    void PostProcessIntersections();
    bool IsPointOnHighway(const Point& point, const Highway& highway, float tolerance = 2.0f);

    float RaycastToHighway(const Point& origin, const Point& direction, float maxDistance);


};

} // namespace CityGen