#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
//NOTE FOR NEXT DEVELOPER:
//code is created based on read_connections.cpp and Graph.cpp AND london_map.csv, other .csv wasnt created during my work on that code, 
//so it should be adjusted to work with them (talking about nodes_with_station.csv and polaczenia.csv, which i got from git pull second before commiting my code)

struct Node; // forward declaration for Edge

class Edge
{
public:
    int type; // transport type
    Node* endpoints[2]; // endpoints[0] and endpoints[1]

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

    // For debugging: count active connections
    int connectionCount() const
    {
        int c = 0;
        for (const auto& slot : slots) if (slot.edge) ++c;
        return c;
    }

    // Get neighbors connected by edges of a specific type
    std::vector<Node*> getNeighborsWithType(int type) const
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
    void LoadNodeData(const std::string& filename) {
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
                std::cout<<"Node: " << id << " gotowy \n";
               // m_pNodes[id].stationType = trim(typeStr); // assuming Node has stationType field
            }
        }

        file.close();
    }

    // Loads connections info from a file
    void LoadConnections(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open connection file '" << filename << "'.\n";
            return;
        }

        std::cout << "Plik otwarty" << std::endl;

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
                std::cout << "Connected " << src << " to " << dst << " via type " << type << "\n";
            }
        }

        file.close();
    }

    // Loads graphs data from files
    void LoadData(const std::string& posFile, const std::string& conFile){
        std::cout << "W nowym loadzie" << std::endl;
        LoadNodeData(posFile);
        std::cout << "Za load data" << std::endl;
        LoadConnections(conFile);
    }

    int getBoundsX(int nNodes){
        int maxX = 1;
        for (int i = 0; i < nNodes; ++i ){
            if (m_pNodes[i].x > maxX) maxX = m_pNodes[i].x;
        }
        return maxX;
    }

    int getBoundsY(int nNodes){
        int maxY = 1;
        for (int i = 0; i < nNodes; ++i ){
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

    std::vector<Node*> GetNeighborsByType(int nodeId, int type) {
        Node* node = GetNode(nodeId);
        
        //empty vector if node doesnt excist
        if (node == nullptr) {
            return std::vector<Node*>();
        }
        
        return node->getNeighborsWithType(type);
    }

    int GetNodeCount() const {
        return m_nodeCount;
    }

    bool IsValidNode(int id) const {
        return (id >= 1 && id <= m_nodeCount);
    }
};