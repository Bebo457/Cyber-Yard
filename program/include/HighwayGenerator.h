#pragma once
#include <vector>
#include <cmath>
#include <random>
#include <cstdint>

namespace CityGen {

    // Definicje typów - muszą być widoczne dla kompilatora przed użyciem w klasie
    enum class RoadType {
        HIGHWAY,
        STREET
    };

    enum class PatternType {
        ORGANIC,    // Naturalny/Losowy (np. stare miasta)
        RASTER,     // Siatka (np. New York)
        RADIAL      // Promienisty (np. Paryż)
    };

    struct Point {
        float x, y;
        bool isIntersection;
        std::vector<int> connectedRoadIndices;

        Point(float x_ = 0, float y_ = 0) : x(x_), y(y_), isIntersection(false) {}
    };

    struct ZoneInputs {
        std::vector<Point> riverPolyline;
        float riverHalfWidth = 10.0f;
        std::vector<std::vector<Point>> parkPolygons;
    };

    struct Road {
        int startNodeIdx;
        int endNodeIdx;
        bool isDeleted;
        RoadType type; // Typ drogi (Autostrada vs Ulica)

        Road(int s, int e, RoadType t = RoadType::HIGHWAY)
            : startNodeIdx(s), endNodeIdx(e), isDeleted(false), type(t) {
        }
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
            , totalLength(length) {
        }
    };

    struct Branch {
        int parentNodeIdx;
        Point direction;
        int delay;
        int iterationsLeft;
        RoadType type; // Branch pamięta jaki typ drogi tworzy

        Branch(int parent, Point dir, int del, int iters, RoadType t)
            : parentNodeIdx(parent), direction(dir), delay(del), iterationsLeft(iters), type(t) {
        }
    };

struct HighwayEnd {
        int currentNodeIdx;
        Point direction;
        int iterationsLeft;
        float distanceSinceLastBranch;
        RoadType type;

        // NOWE: Pola do tworzenia obiektów Highway
        int lastIntersectionIdx;
        std::vector<int> roadsSinceLastIntersection;

        HighwayEnd(int node, Point dir, int iters, RoadType t)
            : currentNodeIdx(node)
            , direction(dir)
            , iterationsLeft(iters)
            , distanceSinceLastBranch(0.0f)
            , type(t)
            , lastIntersectionIdx(node)  
            , roadsSinceLastIntersection() {  
        }
    };

    class HighwayGenerator {
    public:
        HighwayGenerator(int width, int height);
        ~HighwayGenerator();

        void Generate();

        void SetZoneMask(std::vector<uint8_t> mask) { m_ZoneMask = std::move(mask); }
        void ClearZoneMask() { m_ZoneMask.clear(); }

        const std::vector<Road>& GetRoads() const { return m_Roads; }
        const std::vector<Highway>& GetHighways() const { return m_Highways; }
        const std::vector<Point>& GetRoadNodes() const { return m_RoadNodes; }

        // Getters
        float GetDensityAt(int x, int y) const;
        int GetWidth() const { return m_Width; }
        int GetHeight() const { return m_Height; }
        const std::vector<std::vector<float>>& GetPopulationDensity() const { return m_PopulationDensity; }


    private:
        std::vector<uint8_t> m_ZoneMask;
        std::vector<Highway> m_Highways;

        // Parametry generacji
        static constexpr float HIGHWAY_SEGMENT_LENGTH = 15.0f;
        static constexpr float STREET_SEGMENT_LENGTH = 8.0f;
        static constexpr int HIGHWAY_MAX_ITERATIONS = 400;
        static constexpr int STREET_MAX_ITERATIONS = 50;
        static constexpr float BRANCH_ANGLE = 0.4f; // ~23 stopnie

        // Parametry ray casting
        static constexpr float HIGHWAY_VISION_RANGE = 400.0f; 
        static constexpr int HIGHWAY_VISION_SAMPLES = 40;  

        // Parametry dla FindBestDirection
        static constexpr int HIGHWAY_NUM_RAYS = 30;
        static constexpr float HIGHWAY_SAMPLE_RADIUS = 600.0f;
        static constexpr float HIGHWAY_FOV = 3.14159f * 0.333f; // ~60 stopni
        static constexpr float MIN_SCORE_THRESHOLD = 0.01f;

        // Parametry GlobalGoals dla branchy
        static constexpr float BRANCH_MIN_DENSITY_SCORE = 0.3f;
        static constexpr float BRANCH_PARALLEL_ANGLE_THRESHOLD = 20.0f * 3.14159f / 180.0f;
        static constexpr float BRANCH_PARALLEL_SEARCH_RADIUS = 60.0f;
        static constexpr float BRANCH_AREA_SEARCH_RADIUS = 120.0f;
        static constexpr int BRANCH_AREA_MAX_ROADS = 2;
        static constexpr float BRANCH_LOOKAHEAD_DISTANCE = 120.0f;
        static constexpr int BRANCH_DELAY_MIN = 3;
        static constexpr int BRANCH_DELAY_MAX = 8;
        
        // Parametry Highway Proximity Check
        static constexpr float MIN_VIABLE_HIGHWAY_LENGTH = 200.0f;
        static constexpr float RAY_CAST_DISTANCE = 200.0f;
        static constexpr int HIGHWAY_PROXIMITY_NUM_RAYS = 5;
        static constexpr float RAY_SPREAD_ANGLE = 20.0f * 3.14159f / 180.0f;

        // Parametry sprawdzania równoległości
        static constexpr float PARALLEL_GROWTH_CHECK_RADIUS = 50.0f;
        static constexpr float PARALLEL_GROWTH_ANGLE_THRESHOLD = 0.35f; // ~20 stopni

        // Parametry CheckLocalConstraints
        static constexpr float SEARCH_RADIUS_MULTIPLIER = 1.5f;
        static constexpr float HIT_DISTANCE_MULTIPLIER = 0.5f;

        int m_Width;
        int m_Height;

        std::vector<Road> m_Roads;
        std::vector<Point> m_RoadNodes;
        std::vector<std::vector<float>> m_PopulationDensity;

        // Kolejki agentów
        std::vector<HighwayEnd> m_ActiveEnds;
        std::vector<Branch> m_SleepingBranches;

        // Główne metody
        void GeneratePopulationDensity();
        void GenerateHighways(); // Faza 1
        void GenerateStreets();  // Faza 2

        // Logika wzorców (Global Goals)
        Point ApplyGlobalGoals(Point pos, Point currentDir, RoadType type);
        PatternType GetPatternAt(float x, float y); // Decyduje o wzorcu w danym miejscu

        // Logika wzrostu
        bool GrowOneStep(HighwayEnd& agent);
        void CreateBranchCandidates(const HighwayEnd& parent);
        void UpdateSleepingBranches();
        void GlobalGoalsForBranch(Branch& branch, const HighwayEnd& parent);

        // Ograniczenia lokalne
        bool CheckLocalConstraints(const Point& start, const Point& end, int startNodeIdx, int& endNodeIdx, RoadType type);
        bool CheckParallelDuringGrowth(const Point& start, const Point& end, int startNodeIdx);

        // Narzędzia geometryczne
        int CreateOrGetNode(const Point& pos, bool isIntersection);
        int FindNearestNode(const Point& pos, float maxDist, bool intersectionsOnly = false);
        float ShootRay(Point pos, float angle);
        Point FindBestDirection(Point pos, Point prevDirection);
        std::vector<int> FindNearbyRoadIndices(const Point& pos, float radius);

        bool DoSegmentsIntersect(const Point& a1, const Point& a2, const Point& b1, const Point& b2, Point& intersection) const;

        float DistancePointToSegmentClosest(const Point& p, const Point& a, const Point& b, Point& closest) const;
        float DistancePointToSegment(const Point& p, const Point& a, const Point& b) const;

        // Zarządzanie grafem
        void CreateHighway(int startIntersection, int endIntersection, const std::vector<int>& roads);
        bool CheckAndRemoveRedundantHighway(const Highway& newHighway);
        void RemoveHighway(int highwayIdx);
        void RemoveRoads(const std::vector<int>& roadIndices);
        void SplitHighwaysAtIntersection(int intersectionNodeIdx);
        void PostProcessIntersections();
        void MergeSimpleIntersections();

        // Metody pomocnicze (dla kompatybilności i geometrii)
        bool IsPointOnHighway(const Point& point, const Highway& highway, float tolerance = 2.0f) const;
        bool IsPointInPolygon(const Point& p, const std::vector<Point>& poly) const;
        bool SegmentIntersectsPolygon(const Point& a, const Point& b, const std::vector<Point>& poly) const;
        float DistanceSegmentToPolyline(const Point& a, const Point& b, const std::vector<Point>& poly) const;
        float RaycastToHighway(const Point& origin, const Point& direction, float maxDistance);
    
    };

} // namespace CityGen