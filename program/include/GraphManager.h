#ifndef SCOTLANDYARD_CORE_GRAPHMANAGER_H
#define SCOTLANDYARD_CORE_GRAPHMANAGER_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <string>
#include <cassert>
#include <cmath>

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

    Edge(int i_Type_ = 0, Node* p_A = nullptr, Node* p_B = nullptr)
        : i_Type(i_Type_)
    {
        p_Endpoints[0] = p_A;
        p_Endpoints[1] = p_B;
    }

    Edge(const Edge&) = delete;
    Edge& operator=(const Edge&) = delete;

    Node* OtherNode(const Node* p_Me) const {
        if (p_Me == p_Endpoints[0]) return p_Endpoints[1];
        if (p_Me == p_Endpoints[1]) return p_Endpoints[0];
        return nullptr;
    }

private:
    GeometryType m_GeomType = GeometryType::None;
    std::vector<Vec2> m_vec_PolylineNorm;

public:
    GeometryType GetGeometryType() const { return m_GeomType; }
    bool HasGeometry() const { return !m_vec_PolylineNorm.empty(); }

    bool SetPolylineNormalized(const std::vector<Vec2>& vec_Points) {
        if (vec_Points.size() < 2) return false;
        m_vec_PolylineNorm = vec_Points;
        m_GeomType = GeometryType::Polyline;
        return true;
    }

    const std::vector<Vec2>& GetPolylineNormalized() const { return m_vec_PolylineNorm; }

    void ClearGeometry() {
        m_vec_PolylineNorm.clear();
        m_GeomType = GeometryType::None;
    }
};

struct Node {
    int i_Id;
    int i_X, i_Y;

private:
    struct Slot {
        Edge* p_Edge;
        bool b_Owner;
    };
    std::vector<Slot> m_vec_Slots;

public:
    Node(int i_Id_ = 0, int i_X_ = 0, int i_Y_ = 0, bool b_Special = false)
        : i_Id(i_Id_), i_X(i_X_), i_Y(i_Y_)
    {
    }

    ~Node() {
        for (auto& slot : m_vec_Slots) {
            if (slot.p_Edge && slot.b_Owner) {
                Edge* p_Edge = slot.p_Edge;
                Node* p_Other = p_Edge->OtherNode(this);
                if (p_Other) p_Other->RemoveEdge(p_Edge);
                delete p_Edge;
                slot.p_Edge = nullptr;
                slot.b_Owner = false;
            }
        }
    }

    bool ConnectTo(Node* p_Other, int i_Type) {
        if (!p_Other) return false;

        Edge* p_Edge = new Edge(i_Type, this, p_Other);
        m_vec_Slots.push_back({p_Edge, true});
        p_Other->m_vec_Slots.push_back({p_Edge, false});
        return true;
    }

    void RemoveEdge(Edge* p_Edge) {
        for (auto it = m_vec_Slots.begin(); it != m_vec_Slots.end(); ) {
            if (it->p_Edge == p_Edge) {
                it->p_Edge = nullptr;
                it->b_Owner = false;
                it = m_vec_Slots.erase(it);
            } else {
                ++it;
            }
        }
    }

    Node* OtherNode(int i_SlotIndex) const {
        if (i_SlotIndex < 0 || i_SlotIndex >= static_cast<int>(m_vec_Slots.size())) return nullptr;
        const Slot& slot = m_vec_Slots[i_SlotIndex];
        if (!slot.p_Edge) return nullptr;
        return slot.p_Edge->OtherNode(this);
    }

    Edge* GetEdge(int i_SlotIndex) const {
        if (i_SlotIndex < 0 || i_SlotIndex >= static_cast<int>(m_vec_Slots.size())) return nullptr;
        return m_vec_Slots[i_SlotIndex].p_Edge;
    }

    int GetSlotCount() const {
        return static_cast<int>(m_vec_Slots.size());
    }

    int GetSlotType(int i_SlotIndex) const {
        if (i_SlotIndex < 0 || i_SlotIndex >= static_cast<int>(m_vec_Slots.size())) return 0;
        const Slot& slot = m_vec_Slots[i_SlotIndex];
        if (!slot.p_Edge) return 0;
        return slot.p_Edge->i_Type;
    }

    int ConnectionCount() const {
        int i_Count = 0;
        for (const auto& slot : m_vec_Slots) {
            if (slot.p_Edge) ++i_Count;
        }
        return i_Count;
    }

    std::vector<Node*> GetNeighborsWithType(int i_Type) const {
        std::vector<Node*> vec_Neighbors;
        for (const auto& slot : m_vec_Slots) {
            if (slot.p_Edge && slot.p_Edge->i_Type == i_Type) {
                Node* p_Other = slot.p_Edge->OtherNode(this);
                if (p_Other) vec_Neighbors.push_back(p_Other);
            }
        }
        return vec_Neighbors;
    }
};

class GraphManager {
private:
    Node* m_p_Nodes;
    int m_i_NodeCount;

    std::string Trim(const std::string& s_Str) {
        size_t first = s_Str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) return "";
        size_t last = s_Str.find_last_not_of(" \t\n\r\f\v");
        return s_Str.substr(first, (last - first + 1));
    }

    int TransportTypeFromString(const std::string& s_TypeStr) {
        std::string s_Type = Trim(s_TypeStr);
        if (s_Type == "taxi") return 1;
        if (s_Type == "bus") return 2;
        if (s_Type == "metro") return 3;
        if (s_Type == "water") return 4;
        return 0;
    }

    Edge* FindEdgeOneWay(int i_A, int i_B, int i_Type) {
        if (!IsValidNode(i_A) || !IsValidNode(i_B)) return nullptr;
        Node* p_NodeA = &m_p_Nodes[i_A];
        int i_SlotCount = p_NodeA->GetSlotCount();
        for (int i = 0; i < i_SlotCount; ++i) {
            Edge* p_Edge = p_NodeA->GetEdge(i);
            if (!p_Edge) continue;
            if (p_Edge->i_Type != i_Type) continue;
            Node* p_Other = p_NodeA->OtherNode(i);
            if (p_Other && p_Other->i_Id == i_B) {
                return p_Edge;
            }
        }
        return nullptr;
    }

public:
    GraphManager(int i_MaxNodes)
        : m_p_Nodes(nullptr),
          m_i_NodeCount(i_MaxNodes)
    {
        m_p_Nodes = new Node[i_MaxNodes + 1];

        for (int i = 1; i <= i_MaxNodes; ++i) {
            m_p_Nodes[i].i_Id = i;
            m_p_Nodes[i].i_X = 0;
            m_p_Nodes[i].i_Y = 0;
        }
    }

    ~GraphManager() {
        delete[] m_p_Nodes;
        m_p_Nodes = nullptr;
    }

    void LoadNodeData(const std::string& s_Filename, bool b_Verbose = false) {
        std::ifstream file(s_Filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open node file '" << s_Filename << "'.\n";
            return;
        }

        std::string s_Line;
        std::getline(file, s_Line);
        while (std::getline(file, s_Line)) {
            std::stringstream ss(s_Line);
            std::string s_IdStr, s_XStr, s_YStr, s_TypeStr;

            std::getline(ss, s_IdStr, ',');
            std::getline(ss, s_XStr, ',');
            std::getline(ss, s_YStr, ',');
            std::getline(ss, s_TypeStr, ',');

            int i_Id = std::stoi(s_IdStr);
            if (IsValidNode(i_Id)) {
                m_p_Nodes[i_Id].i_X = std::stoi(s_XStr);
                m_p_Nodes[i_Id].i_Y = std::stoi(s_YStr);
                if (b_Verbose) std::cout << "Node: " << i_Id << " loaded\n";
            }
        }

        file.close();
    }

    void LoadConnections(const std::string& s_Filename, bool b_Verbose = false) {
        std::ifstream file(s_Filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open connection file '" << s_Filename << "'.\n";
            return;
        }

        if (b_Verbose) std::cout << "Connection file opened\n";

        std::string s_Line;
        std::getline(file, s_Line);
        while (std::getline(file, s_Line)) {
            std::stringstream ss(s_Line);
            std::string s_SrcStr, s_DstStr, s_TypeStr;

            std::getline(ss, s_SrcStr, ',');
            std::getline(ss, s_DstStr, ',');
            std::getline(ss, s_TypeStr, ',');

            int i_Src = std::stoi(s_SrcStr);
            int i_Dst = std::stoi(s_DstStr);
            int i_Type = TransportTypeFromString(s_TypeStr);

            if (IsValidNode(i_Src) && IsValidNode(i_Dst) && i_Type > 0) {
                m_p_Nodes[i_Src].ConnectTo(&m_p_Nodes[i_Dst], i_Type);
                if (b_Verbose) std::cout << "Connected " << i_Src << " to " << i_Dst << " via type " << i_Type << "\n";
            }
        }

        file.close();
    }

    bool SetEdgePolylineNormalized(int i_Src, int i_Dst, int i_TransportType, const std::vector<Vec2>& vec_PointsNorm) {
        if (!IsValidNode(i_Src) || !IsValidNode(i_Dst) || i_TransportType <= 0) return false;

        Edge* p_Edge = FindEdgeOneWay(i_Src, i_Dst, i_TransportType);
        if (!p_Edge) {
            p_Edge = FindEdgeOneWay(i_Dst, i_Src, i_TransportType);
        }
        if (!p_Edge) return false;
        return p_Edge->SetPolylineNormalized(vec_PointsNorm);
    }

    bool LoadEdgeGeometryCSV(const std::string& s_Filename, bool b_Verbose = false) {
        std::ifstream file(s_Filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open edge geometry file '" << s_Filename << "'.\n";
            return false;
        }

        auto ParsePoints = [&](const std::string& s_Str) -> std::vector<Vec2> {
            std::vector<Vec2> vec_Out;
            std::stringstream ss(s_Str);
            std::string s_Token;
            while (std::getline(ss, s_Token, '|')) {
                std::string s_T = Trim(s_Token);
                if (s_T.empty()) continue;
                size_t sep = s_T.find_first_of(";:");
                if (sep == std::string::npos) continue;
                std::string s_XStr = Trim(s_T.substr(0, sep));
                std::string s_YStr = Trim(s_T.substr(sep + 1));
                try {
                    float f_X = std::stof(s_XStr);
                    float f_Y = std::stof(s_YStr);
                    vec_Out.push_back({f_X, f_Y});
                } catch (...) {
                }
            }
            return vec_Out;
        };

        std::string s_Line;
        if (std::getline(file, s_Line)) {
            std::string s_Lower = s_Line;
            std::transform(s_Lower.begin(), s_Lower.end(), s_Lower.begin(),
                [](unsigned char c){ return std::tolower(c); });
            if (s_Lower.find("source") == std::string::npos || s_Lower.find("dest") == std::string::npos) {
                file.clear();
                file.seekg(0, std::ios::beg);
            }
        }

        int i_Applied = 0;
        while (std::getline(file, s_Line)) {
            if (s_Line.empty() || s_Line[0] == '#') continue;

            std::stringstream ss(s_Line);
            std::string s_SrcStr, s_DstStr, s_TypeStr, s_FmtStr, s_PtsStr;

            std::getline(ss, s_SrcStr, ',');
            std::getline(ss, s_DstStr, ',');
            std::getline(ss, s_TypeStr, ',');
            std::getline(ss, s_FmtStr, ',');
            std::getline(ss, s_PtsStr, '\n');

            try {
                int i_Src = std::stoi(Trim(s_SrcStr));
                int i_Dst = std::stoi(Trim(s_DstStr));
                std::string s_TypeClean = Trim(s_TypeStr);
                std::transform(s_TypeClean.begin(), s_TypeClean.end(), s_TypeClean.begin(),
                    [](unsigned char c){ return std::tolower(c); });
                int i_Type = TransportTypeFromString(s_TypeClean);
                std::string s_Fmt = Trim(s_FmtStr);
                if (i_Type <= 0) continue;

                std::transform(s_Fmt.begin(), s_Fmt.end(), s_Fmt.begin(),
                    [](unsigned char c){ return std::tolower(c); });

                bool b_DoChaikin = false;
                int i_Iterations = 2;
                float f_Alpha = 0.25f;

                if (s_Fmt == "polyline") {
                    b_DoChaikin = false;
                } else if (s_Fmt.rfind("chaikin", 0) == 0) {
                    b_DoChaikin = true;
                    size_t colon = s_Fmt.find(':');
                    if (colon != std::string::npos && colon + 1 < s_Fmt.size()) {
                        std::string s_Params = s_Fmt.substr(colon + 1);
                        std::stringstream ssParams(s_Params);
                        std::string s_Kv;
                        while (std::getline(ssParams, s_Kv, ',')) {
                            size_t eq = s_Kv.find('=');
                            if (eq != std::string::npos) {
                                std::string s_Key = Trim(s_Kv.substr(0, eq));
                                std::string s_Val = Trim(s_Kv.substr(eq + 1));
                                if (s_Key == "i" || s_Key == "iter" || s_Key == "iterations") {
                                    try { i_Iterations = std::stoi(s_Val); } catch (...) {}
                                } else if (s_Key == "a" || s_Key == "alpha") {
                                    try { f_Alpha = std::stof(s_Val); } catch (...) {}
                                }
                            }
                        }
                    }
                    if (i_Iterations < 1) i_Iterations = 1;
                    if (i_Iterations > 5) i_Iterations = 5;
                    if (!(f_Alpha > 0.0f && f_Alpha < 0.5f)) f_Alpha = 0.25f;
                } else {
                    continue;
                }

                std::vector<Vec2> vec_PtsMid = ParsePoints(s_PtsStr);

                float f_GridMaxX = static_cast<float>(GetBoundsX(m_i_NodeCount));
                float f_GridMaxY = static_cast<float>(GetBoundsY(m_i_NodeCount));
                Vec2 vec2_StartNorm{0.0f, 0.0f}, vec2_EndNorm{0.0f, 0.0f};

                if (IsValidNode(i_Src)) {
                    vec2_StartNorm.f_X = (f_GridMaxX > 0.0f) ? (m_p_Nodes[i_Src].i_X / f_GridMaxX) : 0.0f;
                    vec2_StartNorm.f_Y = (f_GridMaxY > 0.0f) ? (m_p_Nodes[i_Src].i_Y / f_GridMaxY) : 0.0f;
                }
                if (IsValidNode(i_Dst)) {
                    vec2_EndNorm.f_X = (f_GridMaxX > 0.0f) ? (m_p_Nodes[i_Dst].i_X / f_GridMaxX) : 0.0f;
                    vec2_EndNorm.f_Y = (f_GridMaxY > 0.0f) ? (m_p_Nodes[i_Dst].i_Y / f_GridMaxY) : 0.0f;
                }

                auto ApproxEq = [](const Vec2& a, const Vec2& b, float f_Eps = 1e-4f){
                    return std::fabs(a.f_X - b.f_X) <= f_Eps && std::fabs(a.f_Y - b.f_Y) <= f_Eps;
                };

                bool b_AllWithinUnit = !vec_PtsMid.empty();
                for (const auto& p : vec_PtsMid) {
                    if (!(p.f_X >= 0.0f && p.f_X <= 1.0f && p.f_Y >= 0.0f && p.f_Y <= 1.0f)) {
                        b_AllWithinUnit = false;
                        break;
                    }
                }
                if (b_AllWithinUnit) {
                    if (b_Verbose) {
                        std::cerr << "Edge geometry for " << i_Src << "->" << i_Dst
                                  << " appears to be normalized in [0..1]. Only grid units are allowed. Entry skipped.\n";
                    }
                    continue;
                }

                std::vector<Vec2> vec_PtsMidNorm;
                vec_PtsMidNorm.reserve(vec_PtsMid.size());
                for (const auto& p : vec_PtsMid) {
                    Vec2 q;
                    q.f_X = (f_GridMaxX > 0.0f) ? (p.f_X / f_GridMaxX) : 0.0f;
                    q.f_Y = (f_GridMaxY > 0.0f) ? (p.f_Y / f_GridMaxY) : 0.0f;
                    vec_PtsMidNorm.push_back(q);
                }

                std::vector<Vec2> vec_PtsBase;
                vec_PtsBase.reserve(vec_PtsMid.size() + 2);
                vec_PtsBase.push_back(vec2_StartNorm);
                if (!vec_PtsMidNorm.empty()) {
                    size_t beginIdx = 0;
                    if (ApproxEq(vec_PtsMidNorm.front(), vec2_StartNorm)) beginIdx = 1;
                    size_t endCount = vec_PtsMidNorm.size();
                    if (endCount > beginIdx && ApproxEq(vec_PtsMidNorm.back(), vec2_EndNorm)) {
                        endCount -= 1;
                    }
                    for (size_t i = beginIdx; i < endCount; ++i) {
                        vec_PtsBase.push_back(vec_PtsMidNorm[i]);
                    }
                }
                vec_PtsBase.push_back(vec2_EndNorm);

                auto ChaikinSmooth = [&](const std::vector<Vec2>& vec_InPts, int i_Iters, float f_A) -> std::vector<Vec2> {
                    if (vec_InPts.size() < 2) return vec_InPts;
                    std::vector<Vec2> vec_Cur = vec_InPts;
                    for (int it = 0; it < i_Iters; ++it) {
                        if (vec_Cur.size() < 2) break;
                        std::vector<Vec2> vec_Nxt;
                        vec_Nxt.reserve(vec_Cur.size() * 2);
                        vec_Nxt.push_back(vec_Cur.front());
                        for (size_t i = 0; i + 1 < vec_Cur.size(); ++i) {
                            const Vec2& A = vec_Cur[i];
                            const Vec2& B = vec_Cur[i + 1];
                            Vec2 Q{(1.0f - f_A) * A.f_X + f_A * B.f_X, (1.0f - f_A) * A.f_Y + f_A * B.f_Y};
                            Vec2 R{f_A * A.f_X + (1.0f - f_A) * B.f_X, f_A * A.f_Y + (1.0f - f_A) * B.f_Y};
                            vec_Nxt.push_back(Q);
                            vec_Nxt.push_back(R);
                        }
                        vec_Nxt.push_back(vec_Cur.back());
                        vec_Cur.swap(vec_Nxt);
                    }
                    return vec_Cur;
                };

                std::vector<Vec2> vec_PtsFinal = vec_PtsBase;
                if (b_DoChaikin) {
                    vec_PtsFinal = ChaikinSmooth(vec_PtsBase, i_Iterations, f_Alpha);
                }

                if (SetEdgePolylineNormalized(i_Src, i_Dst, i_Type, vec_PtsFinal)) {
                    i_Applied++;
                    if (b_Verbose) {
                        std::cout << "Geometry applied: " << i_Src << "->" << i_Dst << " type " << i_Type
                                  << " with " << vec_PtsFinal.size() << " pts\n";
                    }
                }
            } catch (...) {
            }
        }

        if (b_Verbose) {
            std::cout << "Loaded edge geometries: " << i_Applied << " entries applied from '" << s_Filename << "'\n";
        }
        return i_Applied > 0;
    }

    void LoadData(const std::string& s_PosFile, const std::string& s_ConFile, bool b_Verbose = false) {
        if (b_Verbose) std::cout << "Loading graph data\n";
        LoadNodeData(s_PosFile, b_Verbose);
        if (b_Verbose) std::cout << "Node data loaded\n";
        LoadConnections(s_ConFile, b_Verbose);
    }

    int GetBoundsX(int i_NumNodes) {
        int i_MaxX = 1;
        for (int i = 1; i <= i_NumNodes; ++i) {
            if (m_p_Nodes[i].i_X > i_MaxX) i_MaxX = m_p_Nodes[i].i_X;
        }
        return i_MaxX;
    }

    int GetBoundsY(int i_NumNodes) {
        int i_MaxY = 1;
        for (int i = 1; i <= i_NumNodes; ++i) {
            if (m_p_Nodes[i].i_Y > i_MaxY) i_MaxY = m_p_Nodes[i].i_Y;
        }
        return i_MaxY;
    }

    Node* GetNode(int i_Id) {
        if (i_Id < 1 || i_Id > m_i_NodeCount) {
            return nullptr;
        }
        return &m_p_Nodes[i_Id];
    }

    const Node* GetNode(int i_Id) const {
        if (i_Id < 1 || i_Id > m_i_NodeCount) {
            return nullptr;
        }
        return &m_p_Nodes[i_Id];
    }

    std::vector<Node*> GetNeighbors(int i_NodeId) {
        Node* p_Node = GetNode(i_NodeId);

        if (p_Node == nullptr) {
            return std::vector<Node*>();
        }

        std::vector<Node*> vec_Neighbors;

        int i_ConnectionCount = p_Node->ConnectionCount();
        for (int i = 0; i < i_ConnectionCount; ++i) {
            Node* p_Neighbor = p_Node->OtherNode(i);
            if (p_Neighbor != nullptr) {
                vec_Neighbors.push_back(p_Neighbor);
            }
        }

        return vec_Neighbors;
    }

    struct Connection {
        int i_NodeId;
        int i_TransportType;
    };

    std::vector<Connection> GetConnections(int i_NodeId) const {
        std::vector<Connection> vec_Out;
        const Node* p_Node = GetNode(i_NodeId);
        if (!p_Node) return vec_Out;
        int i_SlotCount = p_Node->GetSlotCount();
        for (int i = 0; i < i_SlotCount; ++i) {
            Node* p_Other = p_Node->OtherNode(i);
            if (p_Other) {
                vec_Out.push_back({p_Other->i_Id, p_Node->GetSlotType(i)});
            }
        }
        return vec_Out;
    }

    std::vector<Node*> GetNeighborsByType(int i_NodeId, int i_Type) {
        Node* p_Node = GetNode(i_NodeId);

        if (p_Node == nullptr) {
            return std::vector<Node*>();
        }

        return p_Node->GetNeighborsWithType(i_Type);
    }

    int GetNodeCount() const {
        return m_i_NodeCount;
    }

    bool IsValidNode(int i_Id) const {
        return (i_Id >= 1 && i_Id <= m_i_NodeCount);
    }
};

} // namespace Core
} // namespace ScotlandYard

// Legacy compatibility: allow old code to reference GraphManager without namespace
using GraphManager = ScotlandYard::Core::GraphManager;
using Node = ScotlandYard::Core::Node;
using Edge = ScotlandYard::Core::Edge;

#endif // SCOTLANDYARD_CORE_GRAPHMANAGER_H
