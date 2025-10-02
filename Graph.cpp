#include <iostream>

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

private:
    struct Slot { Edge* edge; bool owner; };
    Slot slots[4]; // private connection slots, ownership flag

public:
    Node(int id_ = 0) : id(id_)
    {
        for (int i = 0; i < 4; ++i) { slots[i].edge = nullptr; slots[i].owner = false; }
    }

    ~Node()
    {
        // Delete only owned edges and inform the other endpoint to forget the pointer
        for (int i = 0; i < 4; ++i) {
            if (slots[i].edge && slots[i].owner) {
                Edge* e = slots[i].edge;
                Node* other = e->otherNode(this);
                if (other) other->removeEdge(e);
                delete e;
                slots[i].edge = nullptr;
                slots[i].owner = false;
            }
        }
    }

    // Connect this node with another. This node will own the created Edge.
    bool connectTo(Node* other, int type)
    {
        if (!other) return false;
        // find free slot in this
        int mySlot = -1, otherSlot = -1;
        for (int i = 0; i < 4; ++i) if (!slots[i].edge) { mySlot = i; break; }
        // find free slot in other
        for (int i = 0; i < 4; ++i) if (!other->slots[i].edge) { otherSlot = i; break; }
        if (mySlot == -1 || otherSlot == -1) return false; // no space

        Edge* e = new Edge(type, this, other);
        slots[mySlot].edge = e;
        slots[mySlot].owner = true;
        other->slots[otherSlot].edge = e;
        other->slots[otherSlot].owner = false;
        return true;
    }

    // Remove an edge pointer if present (non-owning side uses this when the owner deletes)
    void removeEdge(Edge* e)
    {
        for (int i = 0; i < 4; ++i) {
            if (slots[i].edge == e) {
                slots[i].edge = nullptr;
                slots[i].owner = false;
            }
        }
    }

    // Return the other node's id for the connection at slot index, or -1 on error
    int otherNodeId(int slotIndex) const
    {
        if (slotIndex < 0 || slotIndex >= 4) return -1;
        Edge* e = slots[slotIndex].edge;
        if (!e) return -1;
        Node* other = e->otherNode(this);
        if (!other) return -1;
        return other->id;
    }

    // For debugging: count active connections
    int connectionCount() const
    {
        int c = 0;
        for (int i = 0; i < 4; ++i) if (slots[i].edge) ++c;
        return c;
    }
};

int main()
{
    // Create nodes with constructors
    Node nodes[] = { Node(1), Node(2), Node(3) };

    // Use Node::connectTo which handles allocation and registration
    nodes[0].connectTo(&nodes[1], 1);
    nodes[1].connectTo(&nodes[2], 2);
    nodes[0].connectTo(&nodes[2], 3);

    cout << nodes[0].otherNodeId(0) << endl; // Should print 2
    cout << nodes[0].otherNodeId(1) << endl; // Should print 3 (second connection)

    // connectionCount for sanity
    cout << "node0 connections: " << nodes[0].connectionCount() << endl;
    cout << "node1 connections: " << nodes[1].connectionCount() << endl;
    cout << "node2 connections: " << nodes[2].connectionCount() << endl;

    return 0;
}
