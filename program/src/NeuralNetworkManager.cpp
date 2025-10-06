#include "NeuralNetworkManager.h"
#include "ThreadPool.h"
#include <iostream>

namespace ScotlandYard {
namespace AI {

bool NeuralNetworkManager::s_b_Initialized = false;
bool NeuralNetworkManager::s_b_ModelLoaded = false;
size_t NeuralNetworkManager::s_num_Inferences = 0;
float NeuralNetworkManager::s_f_AvgInferenceTime = 0.0f;

void NeuralNetworkManager::Initialize() {
    if (s_b_Initialized) {
        return;
    }

    s_b_Initialized = true;
}

void NeuralNetworkManager::Shutdown() {
    if (!s_b_Initialized) {
        return;
    }

    s_b_ModelLoaded = false;
    s_b_Initialized = false;
}

bool NeuralNetworkManager::LoadModel(const std::string& modelPath) {
    if (!s_b_Initialized) {
        std::cerr << "NeuralNetworkManager not initialized!" << std::endl;
        return false;
    }

    s_b_ModelLoaded = true;
    return true;
}

NetworkOutput NeuralNetworkManager::Predict(const NetworkInput& input) {
    if (!s_b_ModelLoaded) {
        std::cerr << "No model loaded!" << std::endl;
        return NetworkOutput{};
    }

    NetworkOutput output;
    output.vec_ActionProbabilities = {0.2f, 0.3f, 0.5f};
    output.f_Confidence = 0.85f;
    s_num_Inferences++;
    return output;
}

std::future<NetworkOutput> NeuralNetworkManager::PredictAsync(const NetworkInput& input) {
    return Threading::ThreadPool::Submit([input]() {
        return Predict(input);
    });
}

std::vector<NetworkOutput> NeuralNetworkManager::PredictBatch(const std::vector<NetworkInput>& vec_Inputs) {
    std::vector<NetworkOutput> vec_Outputs;
    vec_Outputs.reserve(vec_Inputs.size());

    for (const auto& input : vec_Inputs) {
        vec_Outputs.push_back(Predict(input));
    }

    return vec_Outputs;
}

bool NeuralNetworkManager::IsReady() {
    return s_b_Initialized && s_b_ModelLoaded;
}

} // namespace AI
} // namespace ScotlandYard
