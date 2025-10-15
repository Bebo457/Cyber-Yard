#include "Player.h"
#include <sstream>

namespace ScotlandYard {
namespace Core {

Player::Player(PlayerType e_Type, int i_OccupiedNode, bool b_Visible)
    : m_e_Type(e_Type)
    , m_i_OccupiedNode(i_OccupiedNode)
    , m_b_Visible(b_Visible) {}

Player::~Player() = default;

std::string Player::ToString() const {
    std::ostringstream ss;
    ss << (m_e_Type == PlayerType::MisterX ? "MisterX" : "Detective")
       << "@" << m_i_OccupiedNode;
    return ss.str();
}

} // namespace Core
} // namespace ScotlandYard
