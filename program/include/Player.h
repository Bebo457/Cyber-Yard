#ifndef SCOTLANDYARD_CORE_PLAYER_H
#define SCOTLANDYARD_CORE_PLAYER_H

#include <string>

namespace ScotlandYard {
namespace Core {

enum class PlayerType {
    MisterX,
    Detective
};

class Player {
public:
    Player(PlayerType e_Type, int i_OccupiedNode, bool b_Visible = true);
    ~Player();

    PlayerType GetType() const { return m_e_Type; }
    int GetOccupiedNode() const { return m_i_OccupiedNode; }
    void SetOccupiedNode(int i_Node) { m_i_OccupiedNode = i_Node; }
    // Move the player to another node (simple wrapper).
    void MoveTo(int i_Node) { SetOccupiedNode(i_Node); }

    // Tickets
    int GetTaxiTickets() const { return m_i_TaxiTickets; }
    int GetBusTickets() const { return m_i_BusTickets; }
    int GetMetroTickets() const { return m_i_MetroTickets; }
    int GetWaterTickets() const { return m_i_WaterTickets; }
    int GetBlackTickets() const { return m_i_BlackTickets; }
    int GetDoubleMoveTickets() const { return m_i_DoubleMoveTickets; }

    // Spend ticket helpers - return true if the ticket was consumed (or allowed)
    bool SpendTaxiTicket();
    bool SpendBusTicket();
    bool SpendMetroTicket();
    bool SpendWaterTicket();
    bool SpendBlackTicket();
    bool SpendDoubleMoveTicket();

    void SetVisible(bool b_Visible) { m_b_Visible = b_Visible; }
    bool IsVisible() const { return m_b_Visible; }

    void SetActive(bool b_Active) { m_b_Active = b_Active; }
    bool IsActive() const { return m_b_Active; }

    std::string ToString() const;

private:
    PlayerType m_e_Type;
    int m_i_OccupiedNode;
    bool m_b_Visible; // whether the player token should be rendered on the board
    bool m_b_Active; // whether the player is the active player (can move first)
    int m_i_TaxiTickets;
    int m_i_BusTickets;
    int m_i_MetroTickets;
    int m_i_WaterTickets;
    int m_i_BlackTickets; // MrX only
    int m_i_DoubleMoveTickets; // MrX only
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_PLAYER_H
