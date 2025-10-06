#ifndef SCOTLANDYARD_AI_NEURALNETWORKMANAGER_H
#define SCOTLANDYARD_AI_NEURALNETWORKMANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <future>

namespace ScotlandYard {
namespace AI {

struct NetworkInput {
    std::vector<float> vec_Features;
};

struct NetworkOutput {
    std::vector<float> vec_ActionProbabilities;
    float f_Confidence;
};

class NeuralNetworkManager {
public:
    static void Initialize();
    static void Shutdown();
    static bool LoadModel(const std::string& modelPath);
    static NetworkOutput Predict(const NetworkInput& input);
    static std::future<NetworkOutput> PredictAsync(const NetworkInput& input);
    static std::vector<NetworkOutput> PredictBatch(const std::vector<NetworkInput>& vec_Inputs);
    static bool IsReady();

private:
    NeuralNetworkManager() = delete;
    ~NeuralNetworkManager() = delete;

private:
    static bool s_b_Initialized;
    static bool s_b_ModelLoaded;
    static size_t s_num_Inferences;
    static float s_f_AvgInferenceTime;
};

} // namespace AI
} // namespace ScotlandYard

#endif // SCOTLANDYARD_AI_NEURALNETWORKMANAGER_H
