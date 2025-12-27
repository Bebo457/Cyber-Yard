#pragma once
#include <cstdint>

namespace ScotlandYard {
    namespace Core {

        enum class GameMode : uint8_t { PvP, PvBot, BotvBot };

        // Random is working, TODO others
        // Heuristic: Random, DistMax, Decoy, MonteCarlo, DFS (MrX) / Random, MonteCarlo, Minimax, GSP, FSE (Detectives)
        // ML: PPO, MAPPO, DiscreteSAC
        enum class AIAlgorithm : uint8_t { 
            Random, 
            // MrX Heuristic
            DistanceMaximizationMrX, DecoyMovementMrX, MonteCarloMrX, DFSMrX, 
            // MrX ML
            PPOMrX, MAPPOMrX, DiscreteSACMrX,
            // Detectives Heuristic
            MonteCarloPolice, MinimaxPolice, GreedyShortestPathPolice, FrontSearchEncirclementPolice, 
            // Detectives ML
            PPOPolice, MAPPOPolice, DiscreteSACPolice,
            // Legacy (keep for backward compatibility)
            NeuralNet, NeuralNetPolice
        };
        enum class HumanSide : uint8_t { MrX, Detectives };

        struct GameSettings {
            GameMode e_Mode = GameMode::PvP;
            AIAlgorithm e_AIMisterX = AIAlgorithm::Random;
            AIAlgorithm e_AIDetectives = AIAlgorithm::Random;
            float f_AITurnDelay = 0.8f; // sec

            HumanSide e_PvBotHuman = HumanSide::MrX;

            bool b_ConfiguredFromCommandLine = false;
        };

        // global
        GameSettings& Settings();
        inline bool HasBeenConfigured() {
            return Settings().b_ConfiguredFromCommandLine;
        }

    }
} // namespaces
