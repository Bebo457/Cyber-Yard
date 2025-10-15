#ifndef SCOTLANDYARD_CORE_PLAYER_H
#define SCOTLANDYARD_CORE_PLAYER_H

#include <string>
#include <glm/glm.hpp>

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

    // Visibility control for rendering
    void SetVisible(bool b_Visible) { m_b_Visible = b_Visible; }
    bool IsVisible() const { return m_b_Visible; }

    std::string ToString() const;

private:
    PlayerType m_e_Type;
    int m_i_OccupiedNode;
    bool m_b_Visible; // whether the player token should be rendered on the board
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_PLAYER_H
