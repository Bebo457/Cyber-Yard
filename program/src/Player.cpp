#include "Player.h"
#include <sstream>

namespace ScotlandYard {
namespace Core {

Player::Player(PlayerType e_Type, int i_OccupiedNode, bool b_Visible)
    : m_e_Type(e_Type)
    , m_i_OccupiedNode(i_OccupiedNode)
    , m_b_Visible(b_Visible)
    , m_i_TaxiTickets(0)
    , m_i_BusTickets(0)
    , m_i_MetroTickets(0)
    , m_i_WaterTickets(0)
    , m_i_BlackTickets(0)
    , m_i_DoubleMoveTickets(0)
{

    if (m_e_Type == PlayerType::Detective) {
        m_i_TaxiTickets = 11;
        m_i_BusTickets = 8;
        m_i_MetroTickets = 4;
        m_i_WaterTickets = 0;
        m_i_BlackTickets = 0;
        m_i_DoubleMoveTickets = 0;
    } else {
        // MisterX
        m_i_TaxiTickets = 30; // he has implicit/unlimited access to normal tickets
        m_i_BusTickets = 30;
        m_i_MetroTickets = 30;
        m_i_WaterTickets = 30;
        m_i_BlackTickets = 5;
        m_i_DoubleMoveTickets = 2;
    }
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

bool Player::SpendWaterTicket() {
    if (m_i_WaterTickets <= 0) return false;
    --m_i_WaterTickets;
    return true;
}

Player::~Player() = default;

std::string Player::ToString() const {
    std::ostringstream ss;
    ss << (m_e_Type == PlayerType::MisterX ? "MisterX" : "Detective")
       << "@" << m_i_OccupiedNode;
    // append ticket counts for debugging/console
    ss << " [T:" << m_i_TaxiTickets << " B:" << m_i_BusTickets << " M:" << m_i_MetroTickets << " W:" << m_i_WaterTickets << "]";
    if (m_e_Type == PlayerType::MisterX) {
        ss << " [Black:" << m_i_BlackTickets << " Double:" << m_i_DoubleMoveTickets << "]";
    }
    return ss.str();
}

} // namespace Core
} // namespace ScotlandYard
