#include "PlayerController.h"
#include "Player.h"
#include "Application.h"
#include "GameSettings.h"
#include <iostream>
#include <random>

namespace ScotlandYard {
namespace Core {

void HumanPlayerController::RequestMove(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState,
    Application* p_App
) {
    // Human players use the GUI (color picking/arrow clicking system)
    // No action needed here - moves are handled by GameState::HandleArrowClick
}

AIPlayerController::AIPlayerController(float f_MinTurnTime)
    : m_f_MinTurnTime(f_MinTurnTime)
    , m_f_ElapsedTime(0.0f)
    , m_b_MoveRequested(false)
    , m_MoveDecision()
{
}

void AIPlayerController::RequestMove(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState,
    Application* p_App
) {
    if (vec_PossibleMoves.empty()) {
        m_MoveDecision.b_HasDecision = false;
        m_b_MoveRequested = false;
        return;
    }

    m_MoveDecision = CalculateBestMove(p_Player, vec_PossibleMoves, gameState);

    // Reset timer
    m_f_ElapsedTime = 0.0f;
    m_b_MoveRequested = true;
}

bool AIPlayerController::HasPendingMove() const {
    return m_b_MoveRequested && m_MoveDecision.b_HasDecision && (m_f_ElapsedTime >= m_f_MinTurnTime);
}

MoveDecision AIPlayerController::GetMove() {
    if (!HasPendingMove()) {
        return MoveDecision{};
    }

    MoveDecision decision = m_MoveDecision;
    m_MoveDecision = MoveDecision{};
    m_b_MoveRequested = false;
    m_f_ElapsedTime = 0.0f;

    return decision;
}

void AIPlayerController::Update(float f_DeltaTime) {
    if (m_b_MoveRequested) {
        m_f_ElapsedTime += f_DeltaTime;
    }
}

void AIPlayerController::Reset() {
    m_MoveDecision = MoveDecision{};
    m_b_MoveRequested = false;
    m_f_ElapsedTime = 0.0f;
}

MoveDecision AIPlayerController::CalculateBestMove(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;

    if (vec_PossibleMoves.empty()) {
        decision.b_HasDecision = false;
        return decision;
    }

    switch (m_e_Algorithm) {
        case Core::AIAlgorithm::Random: {
            std::random_device rd; std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, (int)vec_PossibleMoves.size() - 1);
            const auto& sel = vec_PossibleMoves[dis(gen)];
            decision = { true, sel.i_DestinationNode, sel.i_TransportType };
            break;
        }
        case Core::AIAlgorithm::GreedyShortestPath:
            // TODO implementing algorithm here, rn fallback to Random
            [[fallthrough]];
        case Core::AIAlgorithm::NeuralNet:
            // TODO implementing algorithm here, rn fallback to Random
            {
                std::random_device rd; std::mt19937 gen(rd());
                std::uniform_int_distribution<> dis(0, (int)vec_PossibleMoves.size() - 1);
                const auto& sel = vec_PossibleMoves[dis(gen)];
                decision = { true, sel.i_DestinationNode, sel.i_TransportType };
            }
            break;
    }
    return decision;
}


} // namespace Core
} // namespace ScotlandYard
