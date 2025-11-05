#ifndef SCOTLANDYARD_CORE_GRAPHMANAGER_H
#define SCOTLANDYARD_CORE_GRAPHMANAGER_H

#include <vector>
#include <string>
#include <memory>

namespace ScotlandYard {
namespace Core {

struct Node;

struct Vec2 {
    float f_X;
    float f_Y;
};

class Edge {
public:
    int i_Type;
    Node* p_Endpoints[2];

    enum class GeometryType { None = 0, Polyline = 1, Bezier = 2 };

    Edge(int i_Type_ = 0, Node* p_A = nullptr, Node* p_B = nullptr);

    Edge(const Edge&) = delete;
    Edge& operator=(const Edge&) = delete;

    Node* OtherNode(const Node* p_Me) const;
    GeometryType GetGeometryType() const;
    bool HasGeometry() const;
    bool SetPolylineNormalized(const std::vector<Vec2>& vec_Points);
    const std::vector<Vec2>& GetPolylineNormalized() const;
    void ClearGeometry();

private:
    GeometryType m_GeomType;
    std::vector<Vec2> m_vec_PolylineNorm;
};

struct Node {
    int i_Id;
    int i_X, i_Y;

private:
    struct Slot {
        std::shared_ptr<Edge> s_Edge;
    };
    std::vector<Slot> m_vec_Slots;

public:
    Node(int i_Id_ = 0, int i_X_ = 0, int i_Y_ = 0, bool b_Special = false);

    bool ConnectTo(Node* p_Other, int i_Type);
    void RemoveEdge(Edge* p_Edge);
    Node* OtherNode(int i_SlotIndex) const;
    Edge* GetEdge(int i_SlotIndex) const;
    int GetSlotCount() const;
    int GetSlotType(int i_SlotIndex) const;
    int ConnectionCount() const;
    std::vector<Node*> GetNeighborsWithType(int i_Type) const;
};

class GraphManager {
private:
    std::unique_ptr<Node[]> u_Nodes;
    int m_i_NodeCount;

    std::string Trim(const std::string& s_Str);
    int TransportTypeFromString(const std::string& s_TypeStr);
    Edge* FindEdgeOneWay(int i_A, int i_B, int i_Type);

public:
    GraphManager(int i_MaxNodes);

    void LoadNodeData(const std::string& s_Filename, bool b_Verbose = false);
    void LoadConnections(const std::string& s_Filename, bool b_Verbose = false);
    bool SetEdgePolylineNormalized(int i_Src, int i_Dst, int i_TransportType, const std::vector<Vec2>& vec_PointsNorm);
    bool LoadEdgeGeometryCSV(const std::string& s_Filename, bool b_Verbose = false);
    void LoadData(const std::string& s_PosFile, const std::string& s_ConFile, bool b_Verbose = false);
    int GetBoundsX(int i_NumNodes);
    int GetBoundsY(int i_NumNodes);
    Node* GetNode(int i_Id);
    const Node* GetNode(int i_Id) const;
    std::vector<Node*> GetNeighbors(int i_NodeId);

    struct Connection {
        int i_NodeId;
        int i_TransportType;
    };

    std::vector<Connection> GetConnections(int i_NodeId) const;
    std::vector<Node*> GetNeighborsByType(int i_NodeId, int i_Type);
    int GetNodeCount() const;
    bool IsValidNode(int i_Id) const;
};

} // namespace Core
} // namespace ScotlandYard

// Legacy compatibility: allow old code to reference GraphManager without namespace
using GraphManager = ScotlandYard::Core::GraphManager;
using Node = ScotlandYard::Core::Node;
using Edge = ScotlandYard::Core::Edge;

#endif // SCOTLANDYARD_CORE_GRAPHMANAGER_H
