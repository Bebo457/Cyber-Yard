#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

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

std::vector<int> parseGroup(const std::string& group) {
    std::vector<int> values;
    std::stringstream ss(group);
    std::string item;
    while (std::getline(ss, item, ';')) {
        values.push_back(std::stoi(item));
    }
    return values;
}

void read_map(string filename1, Node nodes[]){
    std::ifstream file(filename1);
    std::string line;

        while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string a1_str, group_str;
        std::vector<std::string> groups;

        std::getline(ss, a1_str, ',');
        int a1 = std::stoi(a1_str);

        // Wczytaj pozostałe grupy
        while (std::getline(ss, group_str, ',')) {
            groups.push_back(group_str);
        }

        // Tworzenie polaczen
        for (size_t i = 0; i < groups.size(); ++i) {
            std::vector<int> values = parseGroup(groups[i]);
            int typ = static_cast<int>(i + 1); // b=1, c=2, d=3
            for (int val : values) {
                // funkcja1(a1, val, typ);
                nodes[a1].connectTo(&nodes[val], typ);
                std::cout << "Nodes connected " << a1 << " to " << val << " by " << typ << "\n";
            }
        }
    }

    file.close();
}

int main()
{
    string filename1 = "london_map.csv";

    // Create nodes with constructors (id, x, y)
    Node nodes[200] = { Node(1, 0, 0), Node(2, 1, 0), Node(3, 2, 0) };

    read_map(filename1, nodes);

    return 0;
}