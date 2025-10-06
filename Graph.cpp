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
    bool connectTo(Node* other, int type) // 1 - TAXI, 2 - BUS, 3 - METRO, 4 - WATER
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
    Node nodes[] = {
    Node(1, 0, 0), Node(2, 1, 0), Node(3, 2, 0), Node(4, 3, 0), Node(5, 4, 0), Node(6, 5, 0), Node(7, 6, 0), Node(8, 7, 0), Node(9, 8, 0), Node(10, 9, 0), Node(11, 10, 0), Node(12, 11, 0), Node(13, 12, 0), Node(14, 13, 0),
    Node(15, 0, 1), Node(16, 1, 1), Node(17, 2, 1), Node(18, 3, 1), Node(19, 4, 1), Node(20, 5, 1), Node(21, 6, 1), Node(22, 7, 1), Node(23, 8, 1), Node(24, 9, 1), Node(25, 10, 1), Node(26, 11, 1), Node(27, 12, 1), Node(28, 13, 1),
    Node(29, 0, 2), Node(30, 1, 2), Node(31, 2, 2), Node(32, 3, 2), Node(33, 4, 2), Node(34, 5, 2), Node(35, 6, 2), Node(36, 7, 2), Node(37, 8, 2), Node(38, 9, 2), Node(39, 10, 2), Node(40, 11, 2), Node(41, 12, 2), Node(42, 13, 2),
    Node(43, 0, 3), Node(44, 1, 3), Node(45, 2, 3), Node(46, 3, 3), Node(47, 4, 3), Node(48, 5, 3), Node(49, 6, 3), Node(50, 7, 3), Node(51, 8, 3), Node(52, 9, 3), Node(53, 10, 3), Node(54, 11, 3), Node(55, 12, 3), Node(56, 13, 3),
    Node(57, 0, 4), Node(58, 1, 4), Node(59, 2, 4), Node(60, 3, 4), Node(61, 4, 4), Node(62, 5, 4), Node(63, 6, 4), Node(64, 7, 4), Node(65, 8, 4), Node(66, 9, 4), Node(67, 10, 4), Node(68, 11, 4), Node(69, 12, 4), Node(70, 13, 4),
    Node(71, 0, 5), Node(72, 1, 5), Node(73, 2, 5), Node(74, 3, 5), Node(75, 4, 5), Node(76, 5, 5), Node(77, 6, 5), Node(78, 7, 5), Node(79, 8, 5), Node(80, 9, 5), Node(81, 10, 5), Node(82, 11, 5), Node(83, 12, 5), Node(84, 13, 5),
    Node(85, 0, 6), Node(86, 1, 6), Node(87, 2, 6), Node(88, 3, 6), Node(89, 4, 6), Node(90, 5, 6), Node(91, 6, 6), Node(92, 7, 6), Node(93, 8, 6), Node(94, 9, 6), Node(95, 10, 6), Node(96, 11, 6), Node(97, 12, 6), Node(98, 13, 6),
    Node(99, 0, 7), Node(100, 1, 7), Node(101, 2, 7), Node(102, 3, 7), Node(103, 4, 7), Node(104, 5, 7), Node(105, 6, 7), Node(106, 7, 7), Node(107, 8, 7), Node(108, 9, 7), Node(109, 10, 7), Node(110, 11, 7), Node(111, 12, 7), Node(112, 13, 7),
    Node(113, 0, 8), Node(114, 1, 8), Node(115, 2, 8), Node(116, 3, 8), Node(117, 4, 8), Node(118, 5, 8), Node(119, 6, 8), Node(120, 7, 8), Node(121, 8, 8), Node(122, 9, 8), Node(123, 10, 8), Node(124, 11, 8), Node(125, 12, 8), Node(126, 13, 8),
    Node(127, 0, 9), Node(128, 1, 9), Node(129, 2, 9), Node(130, 3, 9), Node(131, 4, 9), Node(132, 5, 9), Node(133, 6, 9), Node(134, 7, 9), Node(135, 8, 9), Node(136, 9, 9), Node(137, 10, 9), Node(138, 11, 9), Node(139, 12, 9), Node(140, 13, 9),
    Node(141, 0, 10), Node(142, 1, 10), Node(143, 2, 10), Node(144, 3, 10), Node(145, 4, 10), Node(146, 5, 10), Node(147, 6, 10), Node(148, 7, 10), Node(149, 8, 10), Node(150, 9, 10), Node(151, 10, 10), Node(152, 11, 10), Node(153, 12, 10), Node(154, 13, 10),
    Node(155, 0, 11), Node(156, 1, 11), Node(157, 2, 11), Node(158, 3, 11), Node(159, 4, 11), Node(160, 5, 11), Node(161, 6, 11), Node(162, 7, 11), Node(163, 8, 11), Node(164, 9, 11), Node(165, 10, 11), Node(166, 11, 11), Node(167, 12, 11), Node(168, 13, 11),
    Node(169, 0, 12), Node(170, 1, 12), Node(171, 2, 12), Node(172, 3, 12), Node(173, 4, 12), Node(174, 5, 12), Node(175, 6, 12), Node(176, 7, 12), Node(177, 8, 12), Node(178, 9, 12), Node(179, 10, 12), Node(180, 11, 12), Node(181, 12, 12), Node(182, 13, 12),
    Node(183, 0, 13), Node(184, 1, 13), Node(185, 2, 13), Node(186, 3, 13), Node(187, 4, 13), Node(188, 5, 13), Node(189, 6, 13), Node(190, 7, 13), Node(191, 8, 13), Node(192, 9, 13), Node(193, 10, 13), Node(194, 11, 13), Node(195, 12, 13), Node(196, 13, 13),
    Node(197, 0, 14), Node(198, 1, 14), Node(199, 2, 14), Node(200, 3, 14)
    };


    // Use Node::connectTo which handles allocation and registration
    nodes[107].connectTo(&nodes[114], 4);
    nodes[114].connectTo(&nodes[156], 4);
    nodes[156].connectTo(&nodes[193], 4);
    nodes[0].connectTo(&nodes[45], 3);
    nodes[12].connectTo(&nodes[45], 3);
    nodes[12].connectTo(&nodes[66], 3);
    nodes[12].connectTo(&nodes[88], 3);
    nodes[45].connectTo(&nodes[73], 3);
    nodes[45].connectTo(&nodes[78], 3);
    nodes[66].connectTo(&nodes[78], 3);
    nodes[66].connectTo(&nodes[88], 3);
    nodes[66].connectTo(&nodes[110], 3);
    nodes[78].connectTo(&nodes[92], 3);
    nodes[78].connectTo(&nodes[110], 3);
    nodes[88].connectTo(&nodes[127], 3);
    nodes[88].connectTo(&nodes[139], 3);
    nodes[110].connectTo(&nodes[152], 3);
    nodes[110].connectTo(&nodes[162], 3);
    nodes[127].connectTo(&nodes[139], 3);
    nodes[127].connectTo(&nodes[184], 3);
    nodes[139].connectTo(&nodes[152], 3);
    nodes[152].connectTo(&nodes[162], 3);
    nodes[152].connectTo(&nodes[184], 3);
    nodes[0].connectTo(&nodes[45], 2);
    nodes[0].connectTo(&nodes[57], 2);
    nodes[2].connectTo(&nodes[21], 2);
    nodes[2].connectTo(&nodes[22], 2);
    nodes[6].connectTo(&nodes[41], 2);
    nodes[12].connectTo(&nodes[13], 2);
    nodes[12].connectTo(&nodes[22], 2);
    nodes[12].connectTo(&nodes[51], 2);
    nodes[13].connectTo(&nodes[14], 2);
    nodes[14].connectTo(&nodes[28], 2);
    nodes[14].connectTo(&nodes[40], 2);
    nodes[21].connectTo(&nodes[22], 2);
    nodes[21].connectTo(&nodes[33], 2);
    nodes[21].connectTo(&nodes[64], 2);
    nodes[22].connectTo(&nodes[66], 2);
    nodes[28].connectTo(&nodes[40], 2);
    nodes[28].connectTo(&nodes[41], 2);
    nodes[28].connectTo(&nodes[54], 2);
    nodes[33].connectTo(&nodes[45], 2);
    nodes[33].connectTo(&nodes[62], 2);
    nodes[40].connectTo(&nodes[51], 2);
    nodes[40].connectTo(&nodes[86], 2);
    nodes[41].connectTo(&nodes[71], 2);
    nodes[45].connectTo(&nodes[57], 2);
    nodes[45].connectTo(&nodes[77], 2);
    nodes[51].connectTo(&nodes[66], 2);
    nodes[51].connectTo(&nodes[85], 2);
    nodes[54].connectTo(&nodes[88], 2);
    nodes[57].connectTo(&nodes[73], 2);
    nodes[57].connectTo(&nodes[76], 2);
    nodes[62].connectTo(&nodes[64], 2);
    nodes[62].connectTo(&nodes[78], 2);
    nodes[62].connectTo(&nodes[99], 2);
    nodes[64].connectTo(&nodes[66], 2);
    nodes[64].connectTo(&nodes[81], 2);
    nodes[66].connectTo(&nodes[81], 2);
    nodes[66].connectTo(&nodes[101], 2);
    nodes[71].connectTo(&nodes[104], 2);
    nodes[71].connectTo(&nodes[106], 2);
    nodes[73].connectTo(&nodes[93], 2);
    nodes[76].connectTo(&nodes[77], 2);
    nodes[76].connectTo(&nodes[93], 2);
    nodes[76].connectTo(&nodes[123], 2);
    nodes[77].connectTo(&nodes[78], 2);
    nodes[81].connectTo(&nodes[99], 2);
    nodes[81].connectTo(&nodes[139], 2);
    nodes[85].connectTo(&nodes[86], 2);
    nodes[85].connectTo(&nodes[101], 2);
    nodes[85].connectTo(&nodes[115], 2);
    nodes[86].connectTo(&nodes[104], 2);
    nodes[88].connectTo(&nodes[104], 2);
    nodes[92].connectTo(&nodes[93], 2);
    nodes[99].connectTo(&nodes[110], 2);
    nodes[101].connectTo(&nodes[126], 2);
    nodes[104].connectTo(&nodes[106], 2);
    nodes[104].connectTo(&nodes[107], 2);
    nodes[106].connectTo(&nodes[160], 2);
    nodes[107].connectTo(&nodes[115], 2);
    nodes[107].connectTo(&nodes[134], 2);
    nodes[110].connectTo(&nodes[123], 2);
    nodes[115].connectTo(&nodes[126], 2);
    nodes[115].connectTo(&nodes[141], 2);
    nodes[121].connectTo(&nodes[122], 2);
    nodes[121].connectTo(&nodes[143], 2);
    nodes[122].connectTo(&nodes[123], 2);
    nodes[122].connectTo(&nodes[143], 2);
    nodes[122].connectTo(&nodes[164], 2);
    nodes[123].connectTo(&nodes[152], 2);
    nodes[126].connectTo(&nodes[132], 2);
    nodes[127].connectTo(&nodes[134], 2);
    nodes[127].connectTo(&nodes[141], 2);
    nodes[127].connectTo(&nodes[160], 2);
    nodes[127].connectTo(&nodes[186], 2);
    nodes[127].connectTo(&nodes[198], 2);
    nodes[132].connectTo(&nodes[139], 2);
    nodes[132].connectTo(&nodes[156], 2);
    nodes[134].connectTo(&nodes[160], 2);
    nodes[139].connectTo(&nodes[153], 2);
    nodes[139].connectTo(&nodes[155], 2);
    nodes[141].connectTo(&nodes[156], 2);
    nodes[143].connectTo(&nodes[162], 2);
    nodes[152].connectTo(&nodes[153], 2);
    nodes[152].connectTo(&nodes[179], 2);
    nodes[152].connectTo(&nodes[183], 2);
    nodes[153].connectTo(&nodes[155], 2);
    nodes[155].connectTo(&nodes[156], 2);
    nodes[155].connectTo(&nodes[183], 2);
    nodes[156].connectTo(&nodes[184], 2);
    nodes[160].connectTo(&nodes[198], 2);
    nodes[162].connectTo(&nodes[175], 2);
    nodes[162].connectTo(&nodes[190], 2);
    nodes[164].connectTo(&nodes[179], 2);
    nodes[164].connectTo(&nodes[190], 2);
    nodes[175].connectTo(&nodes[189], 2);
    nodes[179].connectTo(&nodes[183], 2);
    nodes[179].connectTo(&nodes[189], 2);
    nodes[183].connectTo(&nodes[184], 2);
    nodes[184].connectTo(&nodes[186], 2);
    nodes[189].connectTo(&nodes[190], 2);
    nodes[0].connectTo(&nodes[7], 1);
    nodes[0].connectTo(&nodes[8], 1);
    nodes[1].connectTo(&nodes[9], 1);
    nodes[1].connectTo(&nodes[19], 1);
    nodes[2].connectTo(&nodes[3], 1);
    nodes[2].connectTo(&nodes[10], 1);
    nodes[2].connectTo(&nodes[11], 1);
    nodes[3].connectTo(&nodes[12], 1);
    nodes[4].connectTo(&nodes[14], 1);
    nodes[4].connectTo(&nodes[15], 1);
    nodes[5].connectTo(&nodes[6], 1);
    nodes[5].connectTo(&nodes[28], 1);
    nodes[6].connectTo(&nodes[16], 1);
    nodes[7].connectTo(&nodes[17], 1);
    nodes[7].connectTo(&nodes[18], 1);
    nodes[8].connectTo(&nodes[18], 1);
    nodes[8].connectTo(&nodes[19], 1);
    nodes[9].connectTo(&nodes[10], 1);
    nodes[9].connectTo(&nodes[20], 1);
    nodes[9].connectTo(&nodes[33], 1);
    nodes[10].connectTo(&nodes[21], 1);
    nodes[11].connectTo(&nodes[22], 1);
    nodes[12].connectTo(&nodes[13], 1);
    nodes[12].connectTo(&nodes[22], 1);
    nodes[12].connectTo(&nodes[23], 1);
    nodes[13].connectTo(&nodes[14], 1);
    nodes[13].connectTo(&nodes[24], 1);
    nodes[14].connectTo(&nodes[15], 1);
    nodes[14].connectTo(&nodes[25], 1);
    nodes[14].connectTo(&nodes[27], 1);
    nodes[15].connectTo(&nodes[27], 1);
    nodes[15].connectTo(&nodes[28], 1);
    nodes[16].connectTo(&nodes[28], 1);
    nodes[16].connectTo(&nodes[29], 1);
    nodes[17].connectTo(&nodes[30], 1);
    nodes[17].connectTo(&nodes[42], 1);
    nodes[18].connectTo(&nodes[31], 1);
    nodes[19].connectTo(&nodes[32], 1);
    nodes[20].connectTo(&nodes[32], 1);
    nodes[21].connectTo(&nodes[22], 1);
    nodes[21].connectTo(&nodes[33], 1);
    nodes[21].connectTo(&nodes[34], 1);
    nodes[22].connectTo(&nodes[36], 1);
    nodes[23].connectTo(&nodes[36], 1);
    nodes[23].connectTo(&nodes[37], 1);
    nodes[24].connectTo(&nodes[37], 1);
    nodes[24].connectTo(&nodes[38], 1);
    nodes[25].connectTo(&nodes[26], 1);
    nodes[25].connectTo(&nodes[38], 1);
    nodes[26].connectTo(&nodes[27], 1);
    nodes[26].connectTo(&nodes[39], 1);
    nodes[27].connectTo(&nodes[40], 1);
    nodes[28].connectTo(&nodes[40], 1);
    nodes[28].connectTo(&nodes[41], 1);
    nodes[29].connectTo(&nodes[41], 1);
    nodes[30].connectTo(&nodes[42], 1);
    nodes[30].connectTo(&nodes[43], 1);
    nodes[31].connectTo(&nodes[32], 1);
    nodes[31].connectTo(&nodes[43], 1);
    nodes[31].connectTo(&nodes[44], 1);
    nodes[32].connectTo(&nodes[45], 1);
    nodes[33].connectTo(&nodes[46], 1);
    nodes[33].connectTo(&nodes[47], 1);
    nodes[34].connectTo(&nodes[35], 1);
    nodes[34].connectTo(&nodes[47], 1);
    nodes[34].connectTo(&nodes[64], 1);
    nodes[35].connectTo(&nodes[36], 1);
    nodes[35].connectTo(&nodes[48], 1);
    nodes[36].connectTo(&nodes[49], 1);
    nodes[37].connectTo(&nodes[49], 1);
    nodes[37].connectTo(&nodes[50], 1);
    nodes[38].connectTo(&nodes[50], 1);
    nodes[38].connectTo(&nodes[51], 1);
    nodes[39].connectTo(&nodes[40], 1);
    nodes[39].connectTo(&nodes[51], 1);
    nodes[39].connectTo(&nodes[52], 1);
    nodes[40].connectTo(&nodes[53], 1);
    nodes[41].connectTo(&nodes[55], 1);
    nodes[41].connectTo(&nodes[71], 1);
    nodes[42].connectTo(&nodes[56], 1);
    nodes[43].connectTo(&nodes[57], 1);
    nodes[44].connectTo(&nodes[45], 1);
    nodes[44].connectTo(&nodes[57], 1);
    nodes[44].connectTo(&nodes[58], 1);
    nodes[44].connectTo(&nodes[59], 1);
    nodes[45].connectTo(&nodes[46], 1);
    nodes[45].connectTo(&nodes[60], 1);
    nodes[46].connectTo(&nodes[61], 1);
    nodes[47].connectTo(&nodes[61], 1);
    nodes[47].connectTo(&nodes[62], 1);
    nodes[48].connectTo(&nodes[49], 1);
    nodes[48].connectTo(&nodes[65], 1);
    nodes[50].connectTo(&nodes[51], 1);
    nodes[50].connectTo(&nodes[66], 1);
    nodes[50].connectTo(&nodes[67], 1);
    nodes[51].connectTo(&nodes[68], 1);
    nodes[52].connectTo(&nodes[53], 1);
    nodes[52].connectTo(&nodes[68], 1);
    nodes[53].connectTo(&nodes[54], 1);
    nodes[53].connectTo(&nodes[69], 1);
    nodes[54].connectTo(&nodes[70], 1);
    nodes[55].connectTo(&nodes[90], 1);
    nodes[56].connectTo(&nodes[57], 1);
    nodes[56].connectTo(&nodes[72], 1);
    nodes[57].connectTo(&nodes[58], 1);
    nodes[57].connectTo(&nodes[73], 1);
    nodes[57].connectTo(&nodes[74], 1);
    nodes[58].connectTo(&nodes[74], 1);
    nodes[58].connectTo(&nodes[75], 1);
    nodes[59].connectTo(&nodes[60], 1);
    nodes[59].connectTo(&nodes[75], 1);
    nodes[60].connectTo(&nodes[61], 1);
    nodes[60].connectTo(&nodes[75], 1);
    nodes[60].connectTo(&nodes[77], 1);
    nodes[61].connectTo(&nodes[78], 1);
    nodes[62].connectTo(&nodes[63], 1);
    nodes[62].connectTo(&nodes[78], 1);
    nodes[62].connectTo(&nodes[79], 1);
    nodes[63].connectTo(&nodes[64], 1);
    nodes[63].connectTo(&nodes[80], 1);
    nodes[64].connectTo(&nodes[65], 1);
    nodes[64].connectTo(&nodes[81], 1);
    nodes[65].connectTo(&nodes[66], 1);
    nodes[65].connectTo(&nodes[81], 1);
    nodes[66].connectTo(&nodes[67], 1);
    nodes[66].connectTo(&nodes[83], 1);
    nodes[67].connectTo(&nodes[68], 1);
    nodes[67].connectTo(&nodes[84], 1);
    nodes[68].connectTo(&nodes[85], 1);
    nodes[69].connectTo(&nodes[70], 1);
    nodes[69].connectTo(&nodes[86], 1);
    nodes[70].connectTo(&nodes[71], 1);
    nodes[70].connectTo(&nodes[88], 1);
    nodes[71].connectTo(&nodes[89], 1);
    nodes[71].connectTo(&nodes[90], 1);
    nodes[72].connectTo(&nodes[73], 1);
    nodes[72].connectTo(&nodes[91], 1);
    nodes[73].connectTo(&nodes[74], 1);
    nodes[73].connectTo(&nodes[91], 1);
    nodes[74].connectTo(&nodes[93], 1);
    nodes[75].connectTo(&nodes[76], 1);
    nodes[76].connectTo(&nodes[77], 1);
    nodes[76].connectTo(&nodes[94], 1);
    nodes[76].connectTo(&nodes[95], 1);
    nodes[77].connectTo(&nodes[78], 1);
    nodes[77].connectTo(&nodes[96], 1);
    nodes[78].connectTo(&nodes[97], 1);
    nodes[79].connectTo(&nodes[98], 1);
    nodes[79].connectTo(&nodes[99], 1);
    nodes[80].connectTo(&nodes[81], 1);
    nodes[80].connectTo(&nodes[99], 1);
    nodes[81].connectTo(&nodes[100], 1);
    nodes[82].connectTo(&nodes[100], 1);
    nodes[82].connectTo(&nodes[101], 1);
    nodes[83].connectTo(&nodes[84], 1);
    nodes[84].connectTo(&nodes[102], 1);
    nodes[85].connectTo(&nodes[102], 1);
    nodes[85].connectTo(&nodes[103], 1);
    nodes[86].connectTo(&nodes[87], 1);
    nodes[87].connectTo(&nodes[88], 1);
    nodes[87].connectTo(&nodes[116], 1);
    nodes[88].connectTo(&nodes[104], 1);
    nodes[89].connectTo(&nodes[90], 1);
    nodes[89].connectTo(&nodes[104], 1);
    nodes[90].connectTo(&nodes[104], 1);
    nodes[90].connectTo(&nodes[106], 1);
    nodes[91].connectTo(&nodes[92], 1);
    nodes[92].connectTo(&nodes[93], 1);
    nodes[93].connectTo(&nodes[94], 1);
    nodes[94].connectTo(&nodes[121], 1);
    nodes[95].connectTo(&nodes[96], 1);
    nodes[95].connectTo(&nodes[108], 1);
    nodes[96].connectTo(&nodes[97], 1);
    nodes[96].connectTo(&nodes[108], 1);
    nodes[97].connectTo(&nodes[98], 1);
    nodes[97].connectTo(&nodes[109], 1);
    nodes[98].connectTo(&nodes[109], 1);
    nodes[98].connectTo(&nodes[111], 1);
    nodes[99].connectTo(&nodes[100], 1);
    nodes[99].connectTo(&nodes[111], 1);
    nodes[99].connectTo(&nodes[112], 1);
    nodes[100].connectTo(&nodes[113], 1);
    nodes[101].connectTo(&nodes[102], 1);
    nodes[101].connectTo(&nodes[114], 1);
    nodes[103].connectTo(&nodes[115], 1);
    nodes[104].connectTo(&nodes[105], 1);
    nodes[104].connectTo(&nodes[107], 1);
    nodes[105].connectTo(&nodes[106], 1);
    nodes[106].connectTo(&nodes[118], 1);
    nodes[107].connectTo(&nodes[116], 1);
    nodes[107].connectTo(&nodes[118], 1);
    nodes[108].connectTo(&nodes[109], 1);
    nodes[108].connectTo(&nodes[123], 1);
    nodes[109].connectTo(&nodes[110], 1);
    nodes[110].connectTo(&nodes[111], 1);
    nodes[110].connectTo(&nodes[123], 1);
    nodes[111].connectTo(&nodes[124], 1);
    nodes[112].connectTo(&nodes[113], 1);
    nodes[112].connectTo(&nodes[124], 1);
    nodes[113].connectTo(&nodes[114], 1);
    nodes[113].connectTo(&nodes[125], 1);
    nodes[113].connectTo(&nodes[130], 1);
    nodes[113].connectTo(&nodes[131], 1);
    nodes[114].connectTo(&nodes[125], 1);
    nodes[114].connectTo(&nodes[126], 1);
    nodes[115].connectTo(&nodes[116], 1);
    nodes[115].connectTo(&nodes[117], 1);
    nodes[115].connectTo(&nodes[126], 1);
    nodes[116].connectTo(&nodes[128], 1);
    nodes[117].connectTo(&nodes[128], 1);
    nodes[117].connectTo(&nodes[133], 1);
    nodes[117].connectTo(&nodes[141], 1);
    nodes[118].connectTo(&nodes[135], 1);
    nodes[119].connectTo(&nodes[120], 1);
    nodes[119].connectTo(&nodes[143], 1);
    nodes[120].connectTo(&nodes[121], 1);
    nodes[120].connectTo(&nodes[144], 1);
    nodes[121].connectTo(&nodes[122], 1);
    nodes[121].connectTo(&nodes[145], 1);
    nodes[122].connectTo(&nodes[123], 1);
    nodes[122].connectTo(&nodes[136], 1);
    nodes[122].connectTo(&nodes[147], 1);
    nodes[122].connectTo(&nodes[148], 1);
    nodes[123].connectTo(&nodes[129], 1);
    nodes[123].connectTo(&nodes[137], 1);
    nodes[124].connectTo(&nodes[130], 1);
    nodes[125].connectTo(&nodes[126], 1);
    nodes[125].connectTo(&nodes[139], 1);
    nodes[126].connectTo(&nodes[132], 1);
    nodes[126].connectTo(&nodes[133], 1);
    nodes[127].connectTo(&nodes[141], 1);
    nodes[127].connectTo(&nodes[142], 1);
    nodes[127].connectTo(&nodes[159], 1);
    nodes[127].connectTo(&nodes[171], 1);
    nodes[127].connectTo(&nodes[187], 1);
    nodes[128].connectTo(&nodes[134], 1);
    nodes[128].connectTo(&nodes[141], 1);
    nodes[128].connectTo(&nodes[142], 1);
    nodes[129].connectTo(&nodes[130], 1);
    nodes[129].connectTo(&nodes[138], 1);
    nodes[131].connectTo(&nodes[139], 1);
    nodes[132].connectTo(&nodes[139], 1);
    nodes[132].connectTo(&nodes[140], 1);
    nodes[133].connectTo(&nodes[140], 1);
    nodes[133].connectTo(&nodes[141], 1);
    nodes[134].connectTo(&nodes[135], 1);
    nodes[134].connectTo(&nodes[142], 1);
    nodes[134].connectTo(&nodes[160], 1);
    nodes[135].connectTo(&nodes[161], 1);
    nodes[136].connectTo(&nodes[146], 1);
    nodes[137].connectTo(&nodes[149], 1);
    nodes[137].connectTo(&nodes[151], 1);
    nodes[138].connectTo(&nodes[139], 1);
    nodes[138].connectTo(&nodes[152], 1);
    nodes[138].connectTo(&nodes[153], 1);
    nodes[139].connectTo(&nodes[153], 1);
    nodes[139].connectTo(&nodes[155], 1);
    nodes[140].connectTo(&nodes[141], 1);
    nodes[140].connectTo(&nodes[157], 1);
    nodes[141].connectTo(&nodes[142], 1);
    nodes[141].connectTo(&nodes[157], 1);
    nodes[142].connectTo(&nodes[159], 1);
    nodes[143].connectTo(&nodes[144], 1);
    nodes[143].connectTo(&nodes[176], 1);
    nodes[144].connectTo(&nodes[145], 1);
    nodes[145].connectTo(&nodes[146], 1);
    nodes[145].connectTo(&nodes[162], 1);
    nodes[146].connectTo(&nodes[163], 1);
    nodes[147].connectTo(&nodes[148], 1);
    nodes[147].connectTo(&nodes[163], 1);
    nodes[148].connectTo(&nodes[149], 1);
    nodes[148].connectTo(&nodes[164], 1);
    nodes[149].connectTo(&nodes[150], 1);
    nodes[150].connectTo(&nodes[151], 1);
    nodes[150].connectTo(&nodes[164], 1);
    nodes[150].connectTo(&nodes[165], 1);
    nodes[151].connectTo(&nodes[152], 1);
    nodes[152].connectTo(&nodes[153], 1);
    nodes[152].connectTo(&nodes[165], 1);
    nodes[152].connectTo(&nodes[166], 1);
    nodes[153].connectTo(&nodes[154], 1);
    nodes[154].connectTo(&nodes[155], 1);
    nodes[154].connectTo(&nodes[166], 1);
    nodes[154].connectTo(&nodes[167], 1);
    nodes[155].connectTo(&nodes[156], 1);
    nodes[155].connectTo(&nodes[168], 1);
    nodes[156].connectTo(&nodes[157], 1);
    nodes[156].connectTo(&nodes[169], 1);
    nodes[157].connectTo(&nodes[158], 1);
    nodes[158].connectTo(&nodes[169], 1);
    nodes[158].connectTo(&nodes[171], 1);
    nodes[158].connectTo(&nodes[185], 1);
    nodes[158].connectTo(&nodes[197], 1);
    nodes[159].connectTo(&nodes[160], 1);
    nodes[159].connectTo(&nodes[172], 1);
    nodes[160].connectTo(&nodes[173], 1);
    nodes[161].connectTo(&nodes[174], 1);
    nodes[162].connectTo(&nodes[176], 1);
    nodes[163].connectTo(&nodes[177], 1);
    nodes[163].connectTo(&nodes[178], 1);
    nodes[164].connectTo(&nodes[178], 1);
    nodes[164].connectTo(&nodes[179], 1);
    nodes[165].connectTo(&nodes[180], 1);
    nodes[165].connectTo(&nodes[182], 1);
    nodes[166].connectTo(&nodes[167], 1);
    nodes[166].connectTo(&nodes[182], 1);
    nodes[167].connectTo(&nodes[183], 1);
    nodes[168].connectTo(&nodes[183], 1);
    nodes[169].connectTo(&nodes[184], 1);
    nodes[170].connectTo(&nodes[172], 1);
    nodes[170].connectTo(&nodes[174], 1);
    nodes[170].connectTo(&nodes[198], 1);
    nodes[171].connectTo(&nodes[186], 1);
    nodes[172].connectTo(&nodes[173], 1);
    nodes[172].connectTo(&nodes[187], 1);
    nodes[173].connectTo(&nodes[174], 1);
    nodes[175].connectTo(&nodes[176], 1);
    nodes[175].connectTo(&nodes[188], 1);
    nodes[177].connectTo(&nodes[188], 1);
    nodes[177].connectTo(&nodes[190], 1);
    nodes[178].connectTo(&nodes[190], 1);
    nodes[179].connectTo(&nodes[180], 1);
    nodes[179].connectTo(&nodes[192], 1);
    nodes[180].connectTo(&nodes[181], 1);
    nodes[180].connectTo(&nodes[192], 1);
    nodes[181].connectTo(&nodes[182], 1);
    nodes[181].connectTo(&nodes[194], 1);
    nodes[182].connectTo(&nodes[195], 1);
    nodes[183].connectTo(&nodes[184], 1);
    nodes[183].connectTo(&nodes[195], 1);
    nodes[183].connectTo(&nodes[196], 1);
    nodes[184].connectTo(&nodes[185], 1);
    nodes[185].connectTo(&nodes[197], 1);
    nodes[186].connectTo(&nodes[187], 1);
    nodes[186].connectTo(&nodes[197], 1);
    nodes[187].connectTo(&nodes[198], 1);
    nodes[188].connectTo(&nodes[189], 1);
    nodes[189].connectTo(&nodes[190], 1);
    nodes[189].connectTo(&nodes[191], 1);
    nodes[190].connectTo(&nodes[191], 1);
    nodes[191].connectTo(&nodes[193], 1);
    nodes[192].connectTo(&nodes[193], 1);
    nodes[193].connectTo(&nodes[194], 1);
    nodes[194].connectTo(&nodes[196], 1);
    nodes[195].connectTo(&nodes[196], 1);
    nodes[197].connectTo(&nodes[198], 1);
    nodes[197].connectTo(&nodes[198], 1);

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
