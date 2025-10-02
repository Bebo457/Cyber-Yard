#include <iostream>
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

int main()
{
    // Create nodes with constructors (id, x, y)
    Node nodes[] = { Node(1, 0, 0), Node(2, 1, 0), Node(3, 2, 0) };

    // Use Node::connectTo which handles allocation and registration
    nodes[0].connectTo(&nodes[1], 1);
    nodes[1].connectTo(&nodes[2], 2);
    nodes[0].connectTo(&nodes[2], 3);

    cout << "Node 0's first neighbor: " << nodes[0].otherNode(0)->id << endl;
    cout << "Node 0's second neighbor: " << nodes[0].otherNode(1)->id << endl;

    // Example of double otherNode: access to node at distance 2
    cout << "Node at distance 2 from Node 0: " << nodes[0].otherNode(0)->otherNode(1)->id << endl; // nodes[0] -> nodes[1] -> nodes[2], id=3

    // Example of getNeighborsWithType: get neighbors connected by type 1 (bus)
    auto neighbors = nodes[0].getNeighborsWithType(1);
    cout << "Node 0's neighbors with type 1: ";
    for (auto n : neighbors) cout << n->id << " ";
    cout << endl;

    // connectionCount for sanity
    cout << "node0 connections: " << nodes[0].connectionCount() << endl;
    cout << "node1 connections: " << nodes[1].connectionCount() << endl;
    cout << "node2 connections: " << nodes[2].connectionCount() << endl;

    return 0;
}