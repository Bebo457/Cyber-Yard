#pragma once
#include <cstdint>

namespace ScotlandYard {
    namespace Core {

        enum class GameMode : uint8_t { PvP, PvBot, BotvBot };

        // Random is working, TODO others
        enum class AIAlgorithm : uint8_t { Random, NeuralNet, DistanceMaximizationMrX, DecoyMovementMrX, MonteCarloMrX, DFSMrX, 
            MonteCarloPolice, MinimaxPolice, GreedyShortestPathPolice, FrontSearchEncirclementPolice};
        enum class HumanSide : uint8_t { MrX, Detectives };

        struct GameSettings {
            GameMode e_Mode = GameMode::PvP;
            AIAlgorithm e_AIMisterX = AIAlgorithm::Random;
            AIAlgorithm e_AIDetectives = AIAlgorithm::Random;
            float f_AITurnDelay = 0.8f; // sec

            HumanSide e_PvBotHuman = HumanSide::MrX;
        };

        // global
        GameSettings& Settings();

    }
} // namespaces
