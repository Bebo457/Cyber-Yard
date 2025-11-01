#ifndef SCOTLANDYARD_CORE_PLAYERCONTROLLER_H
#define SCOTLANDYARD_CORE_PLAYERCONTROLLER_H

#include <memory>
#include <vector>
#include <chrono>


class GraphManager;

namespace ScotlandYard {
namespace Core {

class Player;
class Application;

struct PlayerInfo {
    int i_Position;
    bool b_IsVisible;
    bool b_IsMisterX;

    int i_TaxiTickets;
    int i_BusTickets;
    int i_MetroTickets;
    int i_WaterTickets;
    int i_BlackTickets;
    int i_DoubleMoveTickets;
};

struct GameStateData {
    int i_CurrentPlayerIndex;
    std::vector<PlayerInfo> vec_AllPlayers;

    int i_MrXLastKnownPosition;
    int i_MrXLastKnownRound;

    int i_CurrentRound;
    bool b_IsRevealRound;

    const GraphManager* p_Graph;
};

struct PossibleMove {
    int i_DestinationNode;
    int i_TransportType;
};

struct MoveDecision {
    bool b_HasDecision = false;
    int i_DestinationNode = -1;
    int i_TransportType = -1;
};

class IPlayerController {
public:
    virtual ~IPlayerController() = default;
    virtual bool IsAIControlled() const = 0;

    virtual void RequestMove(
        const Player* p_Player,
        const std::vector<PossibleMove>& vec_PossibleMoves,
        const GameStateData& gameState,
        Application* p_App
    ) = 0;

    virtual bool HasPendingMove() const = 0;
    virtual MoveDecision GetMove() = 0;

    //update function (AI timing, animations)
    virtual void Update(float f_DeltaTime) = 0;
    virtual void Reset() = 0;
};

class HumanPlayerController : public IPlayerController {
public:
    HumanPlayerController() = default;
    ~HumanPlayerController() override = default;

    bool IsAIControlled() const override { return false; }

    void RequestMove(
        const Player* p_Player,
        const std::vector<PossibleMove>& vec_PossibleMoves,
        const GameStateData& gameState,
        Application* p_App
    ) override;

    bool HasPendingMove() const override { return false; }
    MoveDecision GetMove() override { return MoveDecision{}; }

    void Update(float f_DeltaTime) override {}
    void Reset() override {}
};

class AIPlayerController : public IPlayerController {
public:
    explicit AIPlayerController(float f_MinTurnTime = 1.0f);
    ~AIPlayerController() override = default;

    bool IsAIControlled() const override { return true; }

    void RequestMove(
        const Player* p_Player,
        const std::vector<PossibleMove>& vec_PossibleMoves,
        const GameStateData& gameState,
        Application* p_App
    ) override;

    bool HasPendingMove() const override;
    MoveDecision GetMove() override;

    void Update(float f_DeltaTime) override;
    void Reset() override;

    void SetMinTurnTime(float f_Seconds) { m_f_MinTurnTime = f_Seconds; }
    float GetMinTurnTime() const { return m_f_MinTurnTime; }

private:
    // TODO: Connect algorithms here
    MoveDecision CalculateBestMove(
        const Player* p_Player,
        const std::vector<PossibleMove>& vec_PossibleMoves,
        const GameStateData& gameState
    );

    float m_f_MinTurnTime;
    float m_f_ElapsedTime;
    bool m_b_MoveRequested;
    MoveDecision m_MoveDecision;
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_PLAYERCONTROLLER_H
