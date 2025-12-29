#include "Application.h"
#include "ThreadPool.h"
#include "GameSettings.h"
#include <iostream>
#include <memory>
#include <string>

// Helper function to parse MrX AI algorithm from string
ScotlandYard::Core::AIAlgorithm ParseMrXAlgorithm(const std::string& s_Algo) {
    using namespace ScotlandYard::Core;
    if (s_Algo == "random") return AIAlgorithm::Random;
    if (s_Algo == "neural") return AIAlgorithm::NeuralNet;
    if (s_Algo == "distmax") return AIAlgorithm::DistanceMaximizationMrX;
    if (s_Algo == "decoy") return AIAlgorithm::DecoyMovementMrX;
    if (s_Algo == "montecarlo") return AIAlgorithm::MonteCarloMrX;
    if (s_Algo == "dfs") return AIAlgorithm::DFSMrX;

    std::cerr << "Invalid MrX algorithm '" << s_Algo << "'. Using Random.\n";
    return AIAlgorithm::Random;
}

// Helper function to parse Detectives AI algorithm from string
ScotlandYard::Core::AIAlgorithm ParseDetectivesAlgorithm(const std::string& s_Algo) {
    using namespace ScotlandYard::Core;
    if (s_Algo == "random") return AIAlgorithm::Random;
    if (s_Algo == "montecarlo") return AIAlgorithm::MonteCarloPolice;
    if (s_Algo == "minimax") return AIAlgorithm::MinimaxPolice;
    if (s_Algo == "greedy") return AIAlgorithm::GreedyShortestPathPolice;
    if (s_Algo == "frontsearch") return AIAlgorithm::FrontSearchEncirclementPolice;
    if (s_Algo == "neural" || s_Algo == "neuralnetpolice") return AIAlgorithm::NeuralNetPolice;

    std::cerr << "Invalid Detectives algorithm '" << s_Algo << "'. Using Random.\n";
    return AIAlgorithm::Random;
}

// Helper function to configure game settings from command-line arguments
void ConfigureGameFromCommandLine(
    const std::string& s_Mode,
    const std::string& s_HumanSide,
    const std::string& s_AIMrX,
    const std::string& s_AIDetectives
) {
    using namespace ScotlandYard::Core;

    // Parse mode
    if (s_Mode == "pvp") {
        Settings().e_Mode = GameMode::PvP;
    } else if (s_Mode == "pvbot") {
        Settings().e_Mode = GameMode::PvBot;
    } else if (s_Mode == "botvbot") {
        Settings().e_Mode = GameMode::BotvBot;
    } else {
        std::cerr << "Invalid mode '" << s_Mode << "'. Use: pvp, pvbot, or botvbot. Defaulting to pvp.\n";
        Settings().e_Mode = GameMode::PvP;
    }

    // Parse human side (for PvBot)
    if (!s_HumanSide.empty()) {
        if (s_HumanSide == "mrx") {
            Settings().e_PvBotHuman = HumanSide::MrX;
        } else if (s_HumanSide == "detectives") {
            Settings().e_PvBotHuman = HumanSide::Detectives;
        } else {
            std::cerr << "Invalid human side '" << s_HumanSide << "'. Use: mrx or detectives.\n";
        }
    }

    // Parse MrX AI algorithm
    if (!s_AIMrX.empty()) {
        Settings().e_AIMisterX = ParseMrXAlgorithm(s_AIMrX);
    }

    // Parse Detectives AI algorithm
    if (!s_AIDetectives.empty()) {
        Settings().e_AIDetectives = ParseDetectivesAlgorithm(s_AIDetectives);
    }

    // Mark as configured from command line
    Settings().b_ConfiguredFromCommandLine = true;
}

int main(int argc, char* argv[]) {
    using namespace ScotlandYard;

    // Parse command-line arguments
    bool b_TrainingMode = false;
    bool b_GameConsole = false;
    bool b_HasGameConfig = false;
    int i_MaxGames = 1;
    std::string s_Mode;
    std::string s_HumanSide;
    std::string s_AIMrX;
    std::string s_AIDetectives;

    for (int i = 1; i < argc; ++i) {
        std::string s_Arg = argv[i];
        if (s_Arg == "--training" || s_Arg == "-t") {
            b_TrainingMode = true;
        } else if (s_Arg == "--game-console") {
            b_GameConsole = true;
        } else if (s_Arg == "--mode" && i + 1 < argc) {
            s_Mode = argv[++i];
            b_HasGameConfig = true;
        } else if (s_Arg == "--human" && i + 1 < argc) {
            s_HumanSide = argv[++i];
        } else if (s_Arg == "--ai-mrx" && i + 1 < argc) {
            s_AIMrX = argv[++i];
        } else if (s_Arg == "--ai-detectives" && i + 1 < argc) {
            s_AIDetectives = argv[++i];
        } else if (s_Arg == "--games" && i + 1 < argc) {
            i_MaxGames = std::atoi(argv[++i]);
            if (i_MaxGames < 1) {
                std::cerr << "Invalid games count '" << argv[i] << "'. Must be >= 1. Using 1.\n";
                i_MaxGames = 1;
            }
        } else if (s_Arg == "--server") {
            Core::Settings().onlineMode = true;
            Core::Settings().onlineIsServer = true;
            std::cout << "[CLI] Online mode: SERVER\n";
        } else if (s_Arg == "--client") {
            Core::Settings().onlineMode = true;
            Core::Settings().onlineIsServer = false;
            std::cout << "[CLI] Online mode: CLIENT\n";
        } else if (s_Arg == "--host" && i + 1 < argc) {
            Core::Settings().onlineHost = argv[++i];
            std::cout << "[CLI] Online host: " << Core::Settings().onlineHost << "\n";
        } else if (s_Arg == "--port" && i + 1 < argc) {
            Core::Settings().onlinePort = static_cast<uint16_t>(std::atoi(argv[++i]));
            std::cout << "[CLI] Online port: " << Core::Settings().onlinePort << "\n";
        }
    }

    // Apply game configuration if provided
    if (b_HasGameConfig) {
        ConfigureGameFromCommandLine(s_Mode, s_HumanSide, s_AIMrX, s_AIDetectives);
    }

    try {
        // INITIALIZATION
        Threading::ThreadPool::Initialize();

        auto p_App = std::make_unique<Core::Application>("Scotland Yard++", 1280, 720, b_TrainingMode);
        if (!p_App->Initialize()) {
            std::cerr << "Failed to initialize application!" << std::endl;
            return 1;
        }

        // Set game console flag
        p_App->SetGameConsoleEnabled(b_GameConsole);

        p_App->LoadStates();

        // MAIN LOOP
        if (b_TrainingMode) {
            p_App->RunTraining(i_MaxGames);
        } else {
            p_App->Run();
        }

        // CLEANUP
        p_App->Shutdown();
        p_App.reset();
        Threading::ThreadPool::Shutdown();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error" << std::endl;
        return 1;
    }
}
