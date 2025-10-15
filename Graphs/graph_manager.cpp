#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <map>

struct Node; // forward declaration for Edge

class Edge
{
public:
    int type; // transport type: 1=taxi, 2=bus, 3=metro, 4=water
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
    std::string stationType; // e.g., "metro_bus_taxi", "taxi", etc.

private:
    struct Slot { Edge* edge; bool owner; };
    std::vector<Slot> slots; // dynamic connections

public:
    Node(int id_ = 0, int x_ = 0, int y_ = 0, const std::string& stationType_ = "") 
        : id(id_), x(x_), y(y_), stationType(stationType_)
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

    // Get normalized position (0.0 to 1.0) for board scaling
    float getNormalizedX(int maxX) const {
        return maxX > 0 ? static_cast<float>(x) / static_cast<float>(maxX) : 0.0f;
    }

    float getNormalizedY(int maxY) const {
        return maxY > 0 ? static_cast<float>(y) / static_cast<float>(maxY) : 0.0f;
    }

    // Check if station has specific transport type
    bool hasTransportType(const std::string& type) const {
        return stationType.find(type) != std::string::npos;
    }
};

class GraphManager{

private:
    Node* m_pNodes;  
    int m_nodeCount;
    int m_maxX, m_maxY; // for normalization

    // Trim whitespace from string
    std::string trim(const std::string& str) {
        size_t first = str.find_first_not_of(" \t\n\r\f\v");
        if (first == std::string::npos) return "";
        size_t last = str.find_last_not_of(" \t\n\r\f\v");
        return str.substr(first, (last - first + 1));
    }

    std::vector<int> parseGroup(const std::string& group) {
        std::vector<int> values;
        std::stringstream ss(group);
        std::string item;
        while (std::getline(ss, item, ';')) {
            item = trim(item);
            if (!item.empty()) {
                try {
                    values.push_back(std::stoi(item));
                } catch (const std::exception&) {
                    // Skip invalid items
                }
            }
        }
        return values;
    }

    // Parse transport type from string (e.g., "taxi", "bus", "metro", "water")
    int parseTransportType(const std::string& type) {
        std::string lowerType = type;
        std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(), ::tolower);
        
        if (lowerType == "taxi") return 1;
        if (lowerType == "bus") return 2;
        if (lowerType == "metro") return 3;
        if (lowerType == "water") return 4;
        return 1; // default to taxi
    }

public:
    // Constructor
    GraphManager(int maxNodes) 
        : m_pNodes(nullptr),      
          m_nodeCount(maxNodes),
          m_maxX(0),
          m_maxY(0)
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

    // Load node coordinates and station types from nodes CSV
    bool LoadNodesFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file '" << filename << "'.\n";
            return false;
        }

        std::string line;
        std::getline(file, line); // skip header

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string idStr, xStr, yStr, stationType;

            std::getline(ss, idStr, ',');
            std::getline(ss, xStr, ',');
            std::getline(ss, yStr, ',');
            std::getline(ss, stationType, ',');

            try {
                int id = std::stoi(trim(idStr));
                int x = std::stoi(trim(xStr));
                int y = std::stoi(trim(yStr));
                stationType = trim(stationType);

                if (id >= 1 && id <= m_nodeCount) {
                    m_pNodes[id].id = id;
                    m_pNodes[id].x = x;
                    m_pNodes[id].y = y;
                    m_pNodes[id].stationType = stationType;

                    // Track max coordinates for normalization
                    if (x > m_maxX) m_maxX = x;
                    if (y > m_maxY) m_maxY = y;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing line: " << line << "\n";
            }
        }

        file.close();
        std::cout << "Loaded " << m_nodeCount << " nodes. Grid size: " << m_maxX << "x" << m_maxY << "\n";
        return true;
    }

    // Load connections from polaczenia CSV
    bool LoadConnectionsFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file '" << filename << "'.\n";
            return false;
        }

        std::string line;
        std::getline(file, line); // skip header

        int connectionCount = 0;
        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string sourceStr, destStr, typeStr;

            std::getline(ss, sourceStr, ',');
            std::getline(ss, destStr, ',');
            std::getline(ss, typeStr, ',');

            try {
                int source = std::stoi(trim(sourceStr));
                int dest = std::stoi(trim(destStr));
                int type = parseTransportType(trim(typeStr));

                if (IsValidNode(source) && IsValidNode(dest)) {
                    // Only create connection if source < dest to avoid duplicates
                    if (source < dest) {
                        m_pNodes[source].connectTo(&m_pNodes[dest], type);
                        connectionCount++;
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing connection: " << line << "\n";
            }
        }

        file.close();
        std::cout << "Loaded " << connectionCount << " connections.\n";
        return true;
    }

    // Legacy function for backward compatibility
    void LoadFromFile(const std::string& filename) {
        std::cerr << "Warning: LoadFromFile is deprecated. Use LoadNodesFromFile and LoadConnectionsFromFile instead.\n";
    }

    // Get normalized position (0.0 to 1.0) for a node
    bool GetNormalizedPosition(int nodeId, float& outX, float& outY) const {
        if (!IsValidNode(nodeId)) return false;
        
        outX = m_pNodes[nodeId].getNormalizedX(m_maxX);
        outY = m_pNodes[nodeId].getNormalizedY(m_maxY);
        return true;
    }

    // Get grid dimensions
    void GetGridDimensions(int& maxX, int& maxY) const {
        maxX = m_maxX;
        maxY = m_maxY;
    }

    // Check if node has specific transport type
    bool NodeHasTransportType(int nodeId, const std::string& type) const {
        if (!IsValidNode(nodeId)) return false;
        return m_pNodes[nodeId].hasTransportType(type);
    }

    // Get station type for a node
    std::string GetStationType(int nodeId) const {
        if (!IsValidNode(nodeId)) return "";
        return m_pNodes[nodeId].stationType;
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

    // Show statistics about the graph
    void ShowStatistics() const {
        int taxiOnly = 0, withBus = 0, withMetro = 0, withWater = 0;
        int totalConnections = 0;
        
        for (int i = 1; i <= m_nodeCount; i++) {
            const Node* node = &m_pNodes[i];
            if (node->stationType.empty()) continue;
            
            if (node->hasTransportType("metro")) withMetro++;
            if (node->hasTransportType("bus")) withBus++;
            if (node->hasTransportType("water")) withWater++;
            if (node->stationType == "taxi") taxiOnly++;
            
            totalConnections += node->connectionCount();
        }
        
        std::cout << "\n========== GRAPH STATISTICS ==========\n";
        std::cout << "Total nodes: " << m_nodeCount << "\n";
        std::cout << "Grid size: " << m_maxX << " x " << m_maxY << "\n";
        std::cout << "Total connections: " << (totalConnections / 2) << "\n\n";
        std::cout << "Station types:\n";
        std::cout << "  With Metro: " << withMetro << "\n";
        std::cout << "  With Bus: " << withBus << "\n";
        std::cout << "  With Water: " << withWater << "\n";
        std::cout << "  Taxi only: " << taxiOnly << "\n";
        std::cout << "======================================\n\n";
    }
};

int main()
{
    GraphManager manager(200);
    
    // Load graph data from CSV files in the same directory
    std::cout << "Loading nodes...\n";
    if (!manager.LoadNodesFromFile("nodes_with_station.csv")) {
        std::cerr << "Failed to load nodes!\n";
        std::cerr << "Make sure the CSV files are in the same directory as the executable.\n";
        return 1;
    }

    std::cout << "Loading connections...\n";
    if (!manager.LoadConnectionsFromFile("polaczenia.csv")) {
        std::cerr << "Failed to load connections!\n";
        return 1;
    }
    
    // Show statistics
    manager.ShowStatistics();
    
    std::cout << "\n======= Analytical Testing =========\n";
    
    std::cout << "Total nodes: " << manager.GetNodeCount() << "\n";
    
    int maxX, maxY;
    manager.GetGridDimensions(maxX, maxY);
    std::cout << "Grid dimensions: " << maxX << " x " << maxY << "\n\n";

    // Test node 1
    Node* node1 = manager.GetNode(1);
    if (node1 != nullptr) {
        std::cout << "Node 1 info:\n";
        std::cout << "  id: " << node1->id << "\n";
        std::cout << "  pos: (" << node1->x << ", " << node1->y << ")\n";
        
        float normX, normY;
        if (manager.GetNormalizedPosition(1, normX, normY)) {
            std::cout << "  normalized: (" << normX << ", " << normY << ")\n";
        }
        
        std::cout << "  station type: " << node1->stationType << "\n";
        std::cout << "  has metro: " << (node1->hasTransportType("metro") ? "Yes" : "No") << "\n";
        std::cout << "  connections: " << node1->connectionCount() << "\n";
    }

    // Test neighbors
    std::vector<Node*> neighbors = manager.GetNeighbors(1);
    std::cout << "\nNode 1 has " << neighbors.size() << " neighbors:\n";
    for (Node* neighbor : neighbors) {
        std::cout << "  - Node " << neighbor->id << " at (" << neighbor->x << ", " << neighbor->y << ")\n";
    }
    
    // Test transport-specific neighbors
    std::vector<Node*> taxiNeighbors = manager.GetNeighborsByType(1, 1);
    std::cout << "\nNode 1 has " << taxiNeighbors.size() << " taxi connections:\n";
    for (Node* neighbor : taxiNeighbors) {
        std::cout << "  - Node " << neighbor->id << "\n";
    }

    std::vector<Node*> metroNeighbors = manager.GetNeighborsByType(1, 3);
    std::cout << "\nNode 1 has " << metroNeighbors.size() << " metro connections:\n";
    for (Node* neighbor : metroNeighbors) {
        std::cout << "  - Node " << neighbor->id << "\n";
    }

    // Show some example normalized positions for visualization
    std::cout << "\n======= Visualization Data (Normalized Positions) =========\n";
    for (int i = 1; i <= 10; i++) {
        float x, y;
        if (manager.GetNormalizedPosition(i, x, y)) {
            std::cout << "Node " << i << ": (" << x << ", " << y << ") - " 
                      << manager.GetStationType(i) << "\n";
        }
    }

    return 0;
}