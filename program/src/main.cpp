#include "Application.h"
#include "MemoryManager.h"
#include "ThreadPool.h"
#include "NeuralNetworkManager.h"
#include <iostream>
#include <memory>
#include <string>

int main(int argc, char* argv[]) {
    using namespace ScotlandYard;

    // Parse command-line arguments
    bool b_TrainingMode = false;
    int i_MaxSteps = 10000;

    for (int i = 1; i < argc; ++i) {
        std::string s_Arg = argv[i];
        if (s_Arg == "--training" || s_Arg == "-t") {
            b_TrainingMode = true;
        } else if (s_Arg == "--steps" && i + 1 < argc) {
            i_MaxSteps = std::atoi(argv[++i]);
        }
    }

    try {
        // INITIALIZATION
        Memory::MemoryManager::Initialize();
        Threading::ThreadPool::Initialize();
        AI::NeuralNetworkManager::Initialize();

        auto p_App = Memory::MakeUnique<Core::Application>("Scotland Yard++", 1280, 720, b_TrainingMode);
        if (!p_App->Initialize()) {
            std::cerr << "Failed to initialize application!" << std::endl;
            return 1;
        }

        p_App->LoadStates();

        // MAIN LOOP
        if (b_TrainingMode) {
            p_App->RunTraining(i_MaxSteps);
        } else {
            p_App->Run();
        }

        // CLEANUP
        p_App->Shutdown();
        p_App.reset();
        AI::NeuralNetworkManager::Shutdown();
        Threading::ThreadPool::Shutdown();
        Memory::MemoryManager::Shutdown();

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Unknown fatal error" << std::endl;
        return 1;
    }
}
