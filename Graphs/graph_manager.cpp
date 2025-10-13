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

    // load graph data from CSV file
    //previously known as read_map function
    void LoadFromFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file '" << filename << "'.\n";
            return;
        }
        std::string line;

        while (std::getline(file, line)) {
            std::stringstream ss(line);
            std::string a1_str, group_str;
            std::vector<std::string> groups;

            std::getline(ss, a1_str, ',');
            int a1 = std::stoi(a1_str);

            // Read remaining groups (transport types)
            while (std::getline(ss, group_str, ',')) {
                groups.push_back(group_str);
            }

            // Creating connections
            for (size_t i = 0; i < groups.size(); ++i) {
                std::vector<int> values = parseGroup(groups[i]);
                int type = static_cast<int>(i + 1); // taxi=1, bus=2, underground=3, ferry=4
                for (int val : values) {
                    //create connection only if a1 < val (prevents duplicates)
                    if (a1 < val) {
                        m_pNodes[a1].connectTo(&m_pNodes[val], type);
                        std::cout << "Nodes connected " << a1 << " to " << val << " by " << type << "\n";
                    }
                }
            }
        }

        file.close();
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

int main()
{
    std::string filename = "london_map.csv";
    
    GraphManager manager(200);
    
    manager.LoadFromFile(filename);
    
    std::cout << "\n======= Testing =========\n";
    
    std::cout << "Total nodes: " << manager.GetNodeCount() << "\n";
    
    std::cout << "is node 1 valid? " << (manager.IsValidNode(1) ? "Yes" : "No") << "\n";
    std::cout << "is node 2137 valid? " << (manager.IsValidNode(999) ? "Yes" : "No") << "\n";
    
    Node* node1 = manager.GetNode(1);
    if (node1 != nullptr) {
        std::cout << "\nNode 1 info:\n";
        std::cout << "  id: " << node1->id << "\n";
        std::cout << "  pos: (" << node1->x << ", " << node1->y << ")\n";
        std::cout << "  n connections: " << node1->connectionCount() << "\n";
    }

    std::vector<Node*> neighbors = manager.GetNeighbors(1);
    std::cout << "\nNode 1 has " << neighbors.size() << " neighbors:\n";
    for (Node* neighbor : neighbors) {
        std::cout << "  - Node " << neighbor->id << "\n";
    }
    
    std::vector<Node*> taxiNeighbors = manager.GetNeighborsByType(1, 1);
    std::cout << "\nNode 1 has " << taxiNeighbors.size() << " taxi connections:\n";
    for (Node* neighbor : taxiNeighbors) {
        std::cout << "  - Node " << neighbor->id << "\n";
    }

    return 0;
}