#include "Player.h"
#include "GameConstants.h"
#include <sstream>
#include <algorithm>
#include <iostream>

namespace ScotlandYard {
namespace Core {

Player::Player(PlayerType e_Type, int i_OccupiedNode, bool b_Visible)
    : m_e_Type(e_Type)
    , m_i_OccupiedNode(i_OccupiedNode)
    , m_b_Visible(b_Visible)
    , m_b_Active(e_Type == PlayerType::MisterX)
    , m_i_TaxiTickets(0)
    , m_i_BusTickets(0)
    , m_i_MetroTickets(0)
    , m_i_BlackTickets(0)
    , m_i_DoubleMoveTickets(0)
{
    if (m_e_Type == PlayerType::Detective) {
        m_i_TaxiTickets = k_DetectiveTaxiTickets;
        m_i_BusTickets = k_DetectiveBusTickets;
        m_i_MetroTickets = k_DetectiveMetroTickets;
        m_i_BlackTickets = 0;
        m_i_DoubleMoveTickets = 0;
    } else {
        // MisterX
        m_i_TaxiTickets = k_MrXTaxiTickets;
        m_i_BusTickets = k_MrXBusTickets;
        m_i_MetroTickets = k_MrXMetroTickets;
        m_i_BlackTickets = k_MrXBlackTickets;
        m_i_DoubleMoveTickets = k_MrXDoubleMoveTickets;
    }
    SetOccupiedNode(i_OccupiedNode);
}

void Player::SetOccupiedNode(int i_Node) {
    int i_Clamped = std::clamp(i_Node, 1, Core::k_MaxNodes);
    if (i_Clamped != i_Node) {
        std::cout << "[Player] Normalized invalid node " << i_Node
                  << " -> " << i_Clamped << "\n";
    }
    m_i_OccupiedNode = i_Clamped;
}


bool Player::SpendTaxiTicket() {
    if (m_i_TaxiTickets <= 0) return false;
    --m_i_TaxiTickets;
    return true;
}

bool Player::SpendBusTicket() {
    if (m_i_BusTickets <= 0) return false;
    --m_i_BusTickets;
    return true;
}

bool Player::SpendMetroTicket() {
    if (m_i_MetroTickets <= 0) return false;
    --m_i_MetroTickets;
    return true;
}

bool Player::SpendBlackTicket() {
    if (m_i_BlackTickets <= 0) return false;
    --m_i_BlackTickets;
    return true;
}

bool Player::SpendDoubleMoveTicket() {
    if (m_i_DoubleMoveTickets <= 0) return false;
    --m_i_DoubleMoveTickets;
    return true;
}

Player::~Player() = default;

std::string Player::ToString() const {
    std::ostringstream ss;
    ss << (m_e_Type == PlayerType::MisterX ? "MisterX" : "Detective")
       << "@" << m_i_OccupiedNode;
    // append ticket counts for debugging/console
    ss << " [T:" << m_i_TaxiTickets << " B:" << m_i_BusTickets << " M:" << m_i_MetroTickets << "]";
    if (m_e_Type == PlayerType::MisterX) {
        ss << " [Black:" << m_i_BlackTickets << " Double:" << m_i_DoubleMoveTickets << "]";
    }
    return ss.str();
}

} // namespace Core
} // namespace ScotlandYard
