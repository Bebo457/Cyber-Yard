#ifndef GRAPHS_GRAPH_MANAGE_H
#define GRAPHS_GRAPH_MANAGE_H

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <string>
#include <cassert>
#include <cmath>

struct Node; // forward declaration for Edge

// Simple 2D vector for normalized geometry storage [0..1]
struct Vec2
{
    float x;
    float y;
};

class Edge
{
public:
    int type; // transport type
    Node* endpoints[2]; // endpoints[0] and endpoints[1]

    // Geometry encoding for visualization/pathing on the board
    enum class GeometryType { None = 0, Polyline = 1, Bezier = 2 };

    // Construct without auto-registering; ownership is managed by Node::connectTo
    Edge(int type_ = 0, Node* a = nullptr, Node* b = nullptr)
        : type(type_)
    {
        endpoints[0] = a;
        endpoints[1] = b;
    }

    // Disable copy to avoid accidental double-deletion
    Edge(const Edge&) = delete;
    Edge& operator=(const Edge&) = delete;

    // Returns the pointer to the node that is not 'me'. If 'me' is not part of this edge, returns nullptr.
    Node* otherNode(const Node* me) const
    {
        if (me == endpoints[0]) return endpoints[1];
        if (me == endpoints[1]) return endpoints[0];
        return nullptr;
    }

    // --- Geometry API ---
private:
    GeometryType m_geomType = GeometryType::None;
    std::vector<Vec2> m_polylineNorm; // normalized [0..1] points; first/last ideally coincide with endpoints

public:
    GeometryType getGeometryType() const { return m_geomType; }
    bool hasGeometry() const { return !m_polylineNorm.empty(); }

    // Set normalized polyline; requires at least 2 points
    // Does not auto-insert endpoints; caller can enforce if needed
    bool setPolylineNormalized(const std::vector<Vec2>& pts)
    {
        if (pts.size() < 2) return false;
        m_polylineNorm = pts;
        m_geomType = GeometryType::Polyline;
        return true;
    }

    const std::vector<Vec2>& getPolylineNormalized() const { return m_polylineNorm; }
    void clearGeometry()
    {
        m_polylineNorm.clear();
        m_geomType = GeometryType::None;
    }
};

struct Node
{
    int id;
    int x, y; // coordinates for visualization

private:
    struct Slot { Edge* edge; bool owner; };
    std::vector<Slot> slots; // dynamic connections

public:
    Node(int id_ = 0, int x_ = 0, int y_ = 0, bool special = false) : id(id_), x(x_), y(y_)
    {
        // slots start empty
    }

    ~Node()
    {
        // Delete only owned edges and inform the other endpoint to forget the pointer
        for (auto& slot : slots) {
            if (slot.edge && slot.owner) {
                Edge* e = slot.edge;
                Node* other = e->otherNode(this);
                if (other) other->removeEdge(e);
                delete e;
                slot.edge = nullptr;
                slot.owner = false;
            }
        }
    }

    // Connect this node with another. This node will own the created Edge.
    bool connectTo(Node* other, int type)
    {
        if (!other) return false;

        Edge* e = new Edge(type, this, other);
        slots.push_back({e, true});
        other->slots.push_back({e, false});
        return true;
    }

    // Remove an edge pointer if present (non-owning side uses this when the owner deletes)
    void removeEdge(Edge* e)
    {
        for (auto it = slots.begin(); it != slots.end(); ) {
            if (it->edge == e) {
                it->edge = nullptr;
                it->owner = false;
                it = slots.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Return the other node for the connection at slot index, or nullptr on error
    Node* otherNode(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= static_cast<int>(slots.size())) return nullptr;
        const Slot& slot = slots[slotIndex];
        if (!slot.edge) return nullptr;
        return slot.edge->otherNode(this);
    }

    // Get edge at specific slot index - DODANE DLA KOMPATYBILNOŚCI Z graph.cpp
    Edge* getEdge(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= static_cast<int>(slots.size())) return nullptr;
        return slots[slotIndex].edge;
    }

    // Return number of connection slots (active or not)
    int GetSlotCount() const { return static_cast<int>(slots.size()); }

    // Return transport type for the given slot index or 0 if invalid
    int GetSlotType(int slotIndex) const {
        if (slotIndex < 0 || slotIndex >= static_cast<int>(slots.size())) return 0;
        const Slot& slot = slots[slotIndex];
        if (!slot.edge) return 0;
        return slot.edge->type;
    }

    // For debugging: count active connections
    int connectionCount() const
    {
        int c = 0;
        for (const auto& slot : slots) if (slot.edge) ++c;
        return c;
    }

    // Get neighbors connected by edges of a specific type
    std::vector<Node*> GetNeighborsWithType(int type) const
    {
        std::vector<Node*> neighbors;
        for (const auto& slot : slots) {
            if (slot.edge && slot.edge->type == type) {
                Node* other = slot.edge->otherNode(this);
                if (other) neighbors.push_back(other);
            }
        }
        return neighbors;
    }
};


class GraphManager{

private:
    Node* m_pNodes;  
    int m_nodeCount; //how many nodes we have
    

    // Trim whitespace from string
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(first, (last - first + 1));
    }

    // Return int for conn type written as a string
    int transportTypeFromString(const std::string& typeStr) {
        std::string type = trim(typeStr);
        if (type == "taxi") return 1;
        if (type == "bus") return 2;
        if (type == "metro") return 3;
        if (type == "water") return 4;
        return 0; // unknown
    }

    // Find an existing edge between nodes a and b with a given transport type
    // Returns the Edge* if found, otherwise nullptr. Searches only from node 'a'.
    Edge* findEdgeOneWay(int a, int b, int type)
    {
        if (!IsValidNode(a) || !IsValidNode(b)) return nullptr;
        Node* na = &m_pNodes[a];
        int sc = na->GetSlotCount();
        for (int i = 0; i < sc; ++i) {
            Edge* e = na->getEdge(i);
            if (!e) continue;
            if (e->type != type) continue;
            Node* other = na->otherNode(i);
            if (other && other->id == b) {
                return e;
            }
        }
        return nullptr;
    }



public:
    // Constructor
    GraphManager(int maxNodes) 
        : m_pNodes(nullptr),      
          m_nodeCount(maxNodes)  
    {
        // allocate array on heap (maxNodes + 1 because IDs start at 1, not 0 - for better data management)
        m_pNodes = new Node[maxNodes + 1];
        
        // initialize each node with its id and default coordinates
        for (int i = 1; i <= maxNodes; ++i) {
            m_pNodes[i].id = i;    //node id
            m_pNodes[i].x = 0;     //def x 
            m_pNodes[i].y = 0;     //def y
        }
    }
    
    // Destructor
    ~GraphManager() {
        delete[] m_pNodes; 
        m_pNodes = nullptr;
    }

    // Loads positions of Nodes from a file
    void LoadNodeData(const std::string& filename, bool b_Verbose = false) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open node file '" << filename << "'.\n";
            return;
        }

        std::string line;
        std::getline(file, line);
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string idStr, xStr, yStr, typeStr;

            std::getline(ss, idStr, ',');
            std::getline(ss, xStr, ',');
            std::getline(ss, yStr, ',');
            std::getline(ss, typeStr, ',');

            int id = std::stoi(idStr);
            if (IsValidNode(id)) {
                m_pNodes[id].x = std::stoi(xStr);
                m_pNodes[id].y = std::stoi(yStr);
                if (b_Verbose) std::cout << "Node: " << id << " gotowy \n";
                // m_pNodes[id].stationType = trim(typeStr); // assuming Node has stationType field
            }
        }

        file.close();
    }

    // Loads connections info from a file
    void LoadConnections(const std::string& filename, bool b_Verbose = false) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open connection file '" << filename << "'.\n";
            return;
        }

        if (b_Verbose) std::cout << "Plik otwarty" << std::endl;

        std::string line;
        std::getline(file, line);
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string srcStr, dstStr, typeStr;

            std::getline(ss, srcStr, ',');
            std::getline(ss, dstStr, ',');
            std::getline(ss, typeStr, ',');

            int src = std::stoi(srcStr);
            int dst = std::stoi(dstStr);
            int type = transportTypeFromString(typeStr);

            if (IsValidNode(src) && IsValidNode(dst) && type > 0) {
                m_pNodes[src].connectTo(&m_pNodes[dst], type);
                if (b_Verbose) std::cout << "Connected " << src << " to " << dst << " via type " << type << "\n";
            }
        }

        file.close();
    }

    bool SetEdgePolylineNormalized(int src, int dst, int transportType, const std::vector<Vec2>& pointsNorm)
    {
        if (!IsValidNode(src) || !IsValidNode(dst) || transportType <= 0) return false;
        // Try src->dst
        Edge* e = findEdgeOneWay(src, dst, transportType);
        if (!e) {
            // Try the opposite direction if graph was built from the other endpoint
            e = findEdgeOneWay(dst, src, transportType);
        }
        if (!e) return false;
        return e->setPolylineNormalized(pointsNorm);
    }


    bool LoadEdgeGeometryCSV(const std::string& filename, bool b_Verbose = false)
    {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open edge geometry file '" << filename << "'.\n";
            return false;
        }

        auto parsePoints = [&](const std::string& s) -> std::vector<Vec2>
        {
            std::vector<Vec2> out;
            std::stringstream ss(s);
            std::string token;
            while (std::getline(ss, token, '|')) {
                std::string t = trim(token);
                if (t.empty()) continue;
                size_t sep = t.find_first_of(";:"); // accept ';' or ':' as separator
                if (sep == std::string::npos) continue;
                std::string xs = trim(t.substr(0, sep));
                std::string ys = trim(t.substr(sep + 1));
                try {
                    float x = std::stof(xs);
                    float y = std::stof(ys);
                    out.push_back({x, y});
                } catch (...) {
                    // skip malformed point
                }
            }
            return out;
        };

        std::string line;
        // skip optional header
        if (std::getline(file, line)) {
            // Heuristic: if first line contains non-numeric tokens like 'source', treat as header and continue
            std::string lower = line;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c){ return std::tolower(c); });
            if (lower.find("source") == std::string::npos || lower.find("dest") == std::string::npos) {
                // It might have been a data line; process it below by resetting stream to start
                file.clear();
                file.seekg(0, std::ios::beg);
            }
        }

        int applied = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            std::stringstream ss(line);
            std::string srcStr, dstStr, typeStr, fmtStr, ptsStr;

            std::getline(ss, srcStr, ',');
            std::getline(ss, dstStr, ',');
            std::getline(ss, typeStr, ',');
            std::getline(ss, fmtStr, ',');
            std::getline(ss, ptsStr, '\n');

            try {
                int src = std::stoi(trim(srcStr));
                int dst = std::stoi(trim(dstStr));
                std::string typeClean = trim(typeStr);
                std::transform(typeClean.begin(), typeClean.end(), typeClean.begin(), [](unsigned char c){ return std::tolower(c); });
                int t = transportTypeFromString(typeClean);
                std::string fmt = trim(fmtStr);
                if (t <= 0) continue;
                std::transform(fmt.begin(), fmt.end(), fmt.begin(), [](unsigned char c){ return std::tolower(c); });
                bool b_DoChaikin = false;
                int iterationsParam = 2;    // defaults
                float alphaParam = 0.25f;    // defaults
                if (fmt == "polyline") {
                    b_DoChaikin = false;
                } else if (fmt.rfind("chaikin", 0) == 0) {
                    b_DoChaikin = true;
                    // Optional parameters after ':' e.g. chaikin:i=3,a=0.3
                    size_t colon = fmt.find(':');
                    if (colon != std::string::npos && colon + 1 < fmt.size()) {
                        std::string params = fmt.substr(colon + 1);
                        // split by ',' or ';'
                        std::stringstream ssParams(params);
                        std::string kv;
                        while (std::getline(ssParams, kv, ',')) {
                            if (kv.find('=') == std::string::npos) {
                                std::stringstream ssAlt(params);
                                while (std::getline(ssAlt, kv, ';')) {
                                    size_t eq = kv.find('=');
                                    if (eq == std::string::npos) continue;
                                    std::string key = trim(kv.substr(0, eq));
                                    std::string val = trim(kv.substr(eq + 1));
                                    if (key == "i" || key == "iter" || key == "iterations") {
                                        try {
                                            iterationsParam = std::stoi(val);
                                        } catch (...) {}
                                    } else if (key == "a" || key == "alpha") {
                                        try {
                                            alphaParam = std::stof(val);
                                        } catch (...) {}
                                    }
                                }
                                kv.clear();
                                break;
                            } else {
                                size_t eq = kv.find('=');
                                std::string key = trim(kv.substr(0, eq));
                                std::string val = trim(kv.substr(eq + 1));
                                if (key == "i" || key == "iter" || key == "iterations") {
                                    try {
                                        iterationsParam = std::stoi(val);
                                    } catch (...) {}
                                } else if (key == "a" || key == "alpha") {
                                    try {
                                        alphaParam = std::stof(val);
                                    } catch (...) {}
                                }
                            }
                        }
                    }
                    if (iterationsParam < 1) iterationsParam = 1;
                    if (iterationsParam > 5) iterationsParam = 5;
                    if (!(alphaParam > 0.0f && alphaParam < 0.5f)) alphaParam = 0.25f;
                } else {
                    continue; // unsupported format
                }

                std::vector<Vec2> ptsMid = parsePoints(ptsStr);

                float gridMaxX = static_cast<float>(getBoundsX(m_nodeCount));
                float gridMaxY = static_cast<float>(getBoundsY(m_nodeCount));
                Vec2 startNorm{0.0f, 0.0f}, endNorm{0.0f, 0.0f};
                if (IsValidNode(src)) {
                    startNorm.x = (gridMaxX > 0.0f) ? (m_pNodes[src].x / gridMaxX) : 0.0f;
                    startNorm.y = (gridMaxY > 0.0f) ? (m_pNodes[src].y / gridMaxY) : 0.0f;
                }
                if (IsValidNode(dst)) {
                    endNorm.x = (gridMaxX > 0.0f) ? (m_pNodes[dst].x / gridMaxX) : 0.0f;
                    endNorm.y = (gridMaxY > 0.0f) ? (m_pNodes[dst].y / gridMaxY) : 0.0f;
                }

                auto approxEq = [](const Vec2& a, const Vec2& b, float eps = 1e-4f){
                    return std::fabs(a.x - b.x) <= eps && std::fabs(a.y - b.y) <= eps;
                };

                bool b_AllWithinUnit = !ptsMid.empty();
                for (const auto& p : ptsMid) {
                    if (!(p.x >= 0.0f && p.x <= 1.0f && p.y >= 0.0f && p.y <= 1.0f)) { b_AllWithinUnit = false; break; }
                }
                if (b_AllWithinUnit) {
                    if (b_Verbose) {
                        std::cerr << "Edge geometry for " << src << "->" << dst << " appears to be normalized in [0..1]. "
                                  << "Only grid units are allowed. Entry skipped.\n";
                    }
                    continue;
                }

                std::vector<Vec2> ptsMidNorm;
                ptsMidNorm.reserve(ptsMid.size());
                for (const auto& p : ptsMid) {
                    Vec2 q;
                    q.x = (gridMaxX > 0.0f) ? (p.x / gridMaxX) : 0.0f;
                    q.y = (gridMaxY > 0.0f) ? (p.y / gridMaxY) : 0.0f;
                    ptsMidNorm.push_back(q);
                }

                std::vector<Vec2> ptsBase;
                ptsBase.reserve(ptsMid.size() + 2);
                ptsBase.push_back(startNorm);
                if (!ptsMidNorm.empty()) {
                    size_t beginIdx = 0;
                    if (approxEq(ptsMidNorm.front(), startNorm)) beginIdx = 1;
                    size_t endCount = ptsMidNorm.size();
                    if (endCount > beginIdx && approxEq(ptsMidNorm.back(), endNorm)) {
                        endCount -= 1; 
                    }
                    for (size_t i = beginIdx; i < endCount; ++i) ptsBase.push_back(ptsMidNorm[i]);
                }
                ptsBase.push_back(endNorm);

                // Optional smoothing via Chaikin corner-cutting (cuts corners, does not pass through interior points)
                auto chaikinSmooth = [&](const std::vector<Vec2>& inPts, int iterations, float alpha) -> std::vector<Vec2> {
                    if (inPts.size() < 2) return inPts;
                    std::vector<Vec2> cur = inPts;
                    for (int it = 0; it < iterations; ++it) {
                        if (cur.size() < 2) break;
                        std::vector<Vec2> nxt;
                        nxt.reserve(cur.size() * 2);
                        nxt.push_back(cur.front());
                        for (size_t i = 0; i + 1 < cur.size(); ++i) {
                            const Vec2& A = cur[i];
                            const Vec2& B = cur[i+1];
                            Vec2 Q{ (1.0f - alpha) * A.x + alpha * B.x, (1.0f - alpha) * A.y + alpha * B.y };
                            Vec2 R{ alpha * A.x + (1.0f - alpha) * B.x, alpha * A.y + (1.0f - alpha) * B.y };
                            nxt.push_back(Q);
                            nxt.push_back(R);
                        }
                        nxt.push_back(cur.back());
                        cur.swap(nxt);
                    }
                    return cur;
                };

                std::vector<Vec2> ptsFinal = ptsBase;
                if (b_DoChaikin) {
                    ptsFinal = chaikinSmooth(ptsBase, iterationsParam, alphaParam);
                }

                if (SetEdgePolylineNormalized(src, dst, t, ptsFinal)) {
                    applied++;
                    if (b_Verbose) {
                        std::cout << "Geometry applied: " << src << "->" << dst << " type " << t 
                                  << " with " << ptsFinal.size() << " pts" << "\n";
                    }
                }
            } catch (...) {
                // skip malformed line
            }
        }

        if (b_Verbose) {
            std::cout << "Loaded edge geometries: " << applied << " entries applied from '" << filename << "'\n";
        }
        return applied > 0;
    }

    // Loads graphs data from files
    void LoadData(const std::string& posFile, const std::string& conFile, bool verbose = false){
        if (verbose) std::cout << "W nowym loadzie" << std::endl;
    LoadNodeData(posFile, verbose);
        if (verbose) std::cout << "Za load data" << std::endl;
        LoadConnections(conFile, verbose);
    }

    int getBoundsX(int nNodes){
        int maxX = 1;
        // IDs start at 1; iterate inclusively to cover all nodes
        for (int i = 1; i <= nNodes; ++i ){
            if (m_pNodes[i].x > maxX) maxX = m_pNodes[i].x;
        }
        return maxX;
    }

    int getBoundsY(int nNodes){
        int maxY = 1;
        // IDs start at 1; iterate inclusively to cover all nodes
        for (int i = 1; i <= nNodes; ++i ){
            if (m_pNodes[i].y > maxY) maxY = m_pNodes[i].y;
        }
        return maxY;
    }

    Node* GetNode(int id) {
        if (id < 1 || id > m_nodeCount) {
            return nullptr;  
        }
        
        return &m_pNodes[id];
    }

    // const overload so callers with a const GraphManager can access node info wstawione przez AI
    const Node* GetNode(int id) const {
        if (id < 1 || id > m_nodeCount) {
            return nullptr;
        }
        return &m_pNodes[id];
    }

    //get all node's neighborth (regardless of transport type)
    std::vector<Node*> GetNeighbors(int nodeId) {
        Node* node = GetNode(nodeId);

        if (node == nullptr) {
            return std::vector<Node*>();
        }
        
        std::vector<Node*> neighbors;
        
        int connectionCount = node->connectionCount();
        for (int i = 0; i < connectionCount; ++i) {
            Node* neighbor = node->otherNode(i);
            if (neighbor != nullptr) {
                neighbors.push_back(neighbor);
            }
        }
        
        return neighbors;
    }

    struct Connection { int i_NodeId; int i_TransportType; };

    // Return all connections from nodeId with transport types and destination ids
    std::vector<Connection> GetConnections(int nodeId) const { // dodane przez nas AI const
        std::vector<Connection> out;
        const Node* node = GetNode(nodeId); //dodane przez nas AI const
        if (!node) return out;
        int sc = node->GetSlotCount();
        for (int i = 0; i < sc; ++i) {
            Node* other = node->otherNode(i);
            if (other) {
                out.push_back({ other->id, node->GetSlotType(i) });
            }
        }
        return out;
    }

    std::vector<Node*> GetNeighborsByType(int nodeId, int type) {
        Node* node = GetNode(nodeId);
        
        //empty vector if node doesnt excist
        if (node == nullptr) {
            return std::vector<Node*>();
        }
        
    return node->GetNeighborsWithType(type);
    }

    int GetNodeCount() const {
        return m_nodeCount;
    }

    bool IsValidNode(int id) const {
        return (id >= 1 && id <= m_nodeCount);
    }
};

#endif // GRAPHS_GRAPH_MANAGE_H