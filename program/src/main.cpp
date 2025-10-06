#include "Application.h"
#include "MemoryManager.h"
#include "ThreadPool.h"
#include "NeuralNetworkManager.h"

#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    using namespace ScotlandYard;

    try {
        // INITIALIZATION
        Memory::MemoryManager::Initialize();
        Threading::ThreadPool::Initialize();
        AI::NeuralNetworkManager::Initialize();

        auto p_app = Memory::MakeUnique<Core::Application>("Scotland Yard++", 1280, 720);
        if (!p_app->Initialize()) {
            std::cerr << "Failed to initialize application!" << std::endl;
            return 1;
        }

        p_app->LoadStates();

        // MAIN LOOP
        p_app->Run();

        // CLEANUP
        p_app->Shutdown();
        p_app.reset();
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
