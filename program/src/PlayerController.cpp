#include "PlayerController.h"
#include "Player.h"
#include "Application.h"
#include "GameSettings.h"
#include "ThreadPool.h"
#include <iostream>
#include <random>
#include "GraphManager.h"
#include "GameConstants.h"
#include <queue>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <vector>
#include <numeric>
#include <utility>
#include <cmath>
#include <tuple>
#include <functional>
#include <limits>
#include <optional>
#include "PythonBridge.h"
#include <nlohmann/json.hpp>

// Extern declaration for detective Python bridge (defined in GameState.cpp, global scope)
extern PythonBridge* g_pDetectiveBridge;
// Extern declaration for Mr X Python bridge (defined in GameState.cpp, global scope)
extern PythonBridge* g_pBridge;

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
    , m_b_CalculationInProgress(false)
{
}

void AIPlayerController::RequestMove(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState,
    Application* p_App
) {
    if (vec_PossibleMoves.empty()) {
        std::lock_guard<std::mutex> lock(m_mtx_MoveDecision);
        m_MoveDecision.b_HasDecision = false;
        m_b_MoveRequested = false;
        m_b_CalculationInProgress = false;
        return;
    }

    // Submit AI calculation to ThreadPool
    m_b_CalculationInProgress = true;
    m_Future_MoveCalculation = Threading::ThreadPool::Submit(
        [this, vec_PossibleMoves, gameState]() -> MoveDecision {
            //don't capture p_Player pointer to avoid thread-safety issues position, which is already in gameState.
            return CalculateBestMove(nullptr, vec_PossibleMoves, gameState);
        }
    );

    // Reset timer
    m_f_ElapsedTime = 0.0f;
    m_b_MoveRequested = true;
}

bool AIPlayerController::HasPendingMove() const {
    if (m_b_CalculationInProgress && m_Future_MoveCalculation.valid()) {
        if (m_Future_MoveCalculation.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            //retrieve the result
            std::lock_guard<std::mutex> lock(m_mtx_MoveDecision);
            m_MoveDecision = m_Future_MoveCalculation.get();
            m_b_CalculationInProgress = false;
        }
    }

    return m_b_MoveRequested && m_MoveDecision.b_HasDecision && (m_f_ElapsedTime >= m_f_MinTurnTime);
}

MoveDecision AIPlayerController::GetMove() {
    if (!HasPendingMove()) {
        return MoveDecision{};
    }

    std::lock_guard<std::mutex> lock(m_mtx_MoveDecision);
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
    if (m_b_CalculationInProgress && m_Future_MoveCalculation.valid()) {
        m_Future_MoveCalculation.wait();
    }

    std::lock_guard<std::mutex> lock(m_mtx_MoveDecision);
    m_MoveDecision = MoveDecision{};
    m_b_MoveRequested = false;
    m_f_ElapsedTime = 0.0f;
    m_b_CalculationInProgress = false;
}

static double EvaluateMoveReward(
    const Player* p_Player,
    const PossibleMove& move,
    const GameStateData& gameState
) {
    // Longest practical path on the map is well below this clamp (graph diameter < ~25 moves)
    static constexpr int k_MaxReasonableDistance = 25;

    // Budowa grafu (tylko sąsiedzi)
    std::map<int, std::vector<int>> map_Graph;
    for (int i_Node = 1; i_Node <= 200; ++i_Node) {
        auto vec_Connections = gameState.p_Graph->GetConnections(i_Node);
        for (const auto& conn : vec_Connections) {
            map_Graph[i_Node].push_back(conn.i_NodeId);
        }
    }

    // BFS pomocnicze
    auto fn_BFS = [&](int i_Start) {
        std::map<int,int> map_Dist;
        std::queue<int> q;
        map_Dist[i_Start] = 0;
        q.push(i_Start);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int v : map_Graph[u]) {
                if (!map_Dist.count(v)) {
                    map_Dist[v] = map_Dist[u] + 1;
                    q.push(v);
                }
            }
        }
        return map_Dist;
    };

    // Pozycje policjantów
    std::vector<int> policePositions;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (!info.b_IsMisterX) policePositions.push_back(info.i_Position);
    }

    // BFS dla każdego policjanta
    std::vector<std::map<int,int>> policeDists;
    for (int pos : policePositions) {
        policeDists.push_back(fn_BFS(pos));
    }

    // Liczba węzłów w zagłębieniu 2
    auto countDepth2Neighbors = [&](int node) {
        std::set<int> depth2;
        for (int n1 : map_Graph[node]) {
            depth2.insert(n1);
            for (int n2 : map_Graph[n1]) depth2.insert(n2);
        }
        depth2.erase(node);
        return static_cast<int>(depth2.size());
    };

    // Parametry wagowe do obliczenia nagrody
    double w_min = 2.0;
    double w_avg = 1.0;
    double w_depth2 = 0.7;

    int dest = move.i_DestinationNode;

    // minimalna i średnia odległość do policjantów
    int minDist = k_MaxReasonableDistance;
    double sumDist = 0.0;
    int reachableCount = 0;
    for (const auto& distMap : policeDists) {
        auto it = distMap.find(dest);
        if (it == distMap.end()) {
            continue;
        }
        int d = it->second;
        minDist = std::min(minDist, d);
        sumDist += d;
        ++reachableCount;
    }
    double avgDist = (reachableCount > 0)
        ? (sumDist / static_cast<double>(reachableCount))
        : static_cast<double>(k_MaxReasonableDistance);
    if (reachableCount == 0) {
        minDist = k_MaxReasonableDistance;
    }

    // liczba węzłów w zagłębieniu 2
    int depth2Count = countDepth2Neighbors(dest);

    // obliczenie nagrody
    double reward = w_min * minDist + w_avg * avgDist + w_depth2 * depth2Count;

    return reward;
}


static MoveDecision ExternalPythonAlgorithmmrX(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    std::cout << "PYTHON\n";
    MoveDecision decision;
    decision.b_HasDecision = false;
    if (vec_PossibleMoves.empty()) return decision;



    auto randomFallback = [&]() -> MoveDecision {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(vec_PossibleMoves.size()) - 1);
        const auto& sel = vec_PossibleMoves[dis(gen)];
        MoveDecision d;
        d.b_HasDecision = true;
        d.i_DestinationNode = sel.i_DestinationNode;
        d.i_TransportType = sel.i_TransportType;
        std::cout << "[Fallback] Using random move: dest=" 
                  << sel.i_DestinationNode << ", transport=" << sel.i_TransportType << "\n";
        return d;
    };

    // Use direct pointer instead of singleton (since Detective bridge may overwrite it)
    PythonBridge* bridge = ::g_pBridge;
    if (!bridge) {
        std::cout << "[Fallback] Python bridge (MrX) not available\n";
        return randomFallback();
    }

    nlohmann::json req;
    static double reward_for_move = 0.0;
    req["game_state"] = {
        {"current_player_index", gameState.i_CurrentPlayerIndex},
        {"current_round", gameState.i_CurrentRound},
        {"is_reveal_round", gameState.b_IsRevealRound},
        {"is_next_reveal_round", gameState.b_IsNextRevealRound},
        {"mr_x_last_known_position", gameState.i_MrXLastKnownPosition},
        {"mr_x_last_known_round", gameState.i_MrXLastKnownRound},
        {"reward", reward_for_move},
    };

    nlohmann::json jplayers = nlohmann::json::array();
    for (const auto& info : gameState.vec_AllPlayers) {
        jplayers.push_back({
            {"position", info.i_Position},
            {"is_visible", info.b_IsVisible},
            {"is_mister_x", info.b_IsMisterX},
            {"tickets", {
                {"taxi", info.i_TaxiTickets},
                {"bus", info.i_BusTickets},
                {"metro", info.i_MetroTickets},
                {"black", info.i_BlackTickets},
                {"double", info.i_DoubleMoveTickets}
            }}
        });
    }
    req["players"] = jplayers;

    nlohmann::json jmoves = nlohmann::json::array();
    for (const auto& m : vec_PossibleMoves) {
        jmoves.push_back({ {"dest", m.i_DestinationNode}, {"transport", m.i_TransportType} });
    }
    req["possible_moves"] = jmoves;

    try {
        auto fut = bridge->sendRequestAsync(req);
        nlohmann::json resp = fut.get();

        if (resp.is_object()) {
            // wybór po indeksie
            if (resp.contains("selected_index") && resp["selected_index"].is_number_integer()) {
                int idx = resp["selected_index"].get<int>();
                if (idx >= 0 && idx < static_cast<int>(vec_PossibleMoves.size())) {
                    const auto& mv = vec_PossibleMoves[idx];
                    MoveDecision d; 
                    d.b_HasDecision = true; 
                    d.i_DestinationNode = mv.i_DestinationNode; 
                    d.i_TransportType = mv.i_TransportType;
                    std::cout << "[Python] Selected move by index: dest=" 
                              << mv.i_DestinationNode << ", transport=" << mv.i_TransportType << "\n";
                    double reward = EvaluateMoveReward(p_Player, mv, gameState);
                    reward_for_move = reward;
                    std::cout << "[Reward] Calculated reward for move: " << reward << "\n";
                    return d;
                }
            }

            // wybór po destination (+ opcjonalnie transport)
            // if (resp.contains("destination") && !resp["destination"].is_null()) {
            //     int dest = resp["destination"].get<int>();
            //     int transport = resp.value("transport", 0);
            //     for (const auto& mv : vec_PossibleMoves) {
            //         if (mv.i_DestinationNode == dest && (transport == 0 || mv.i_TransportType == transport)) {
            //             MoveDecision d; 
            //             d.b_HasDecision = true; 
            //             d.i_DestinationNode = mv.i_DestinationNode; 
            //             d.i_TransportType = mv.i_TransportType;
            //             std::cout << "[Python] Selected move by dest+transport: dest=" 
            //                       << mv.i_DestinationNode << ", transport=" << mv.i_TransportType << "\n";
            //             return d;
            //         }
            //     }
            //     for (const auto& mv : vec_PossibleMoves) {
            //         if (mv.i_DestinationNode == dest) {
            //             MoveDecision d; 
            //             d.b_HasDecision = true; 
            //             d.i_DestinationNode = mv.i_DestinationNode; 
            //             d.i_TransportType = mv.i_TransportType;
            //             std::cout << "[Python] Selected move by dest only: dest=" 
            //                       << mv.i_DestinationNode << ", transport=" << mv.i_TransportType << "\n";
            //             return d;
            //         }
            //     }
            // }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ExternalPythonAlgorithmmrX] Python bridge error: " << e.what() << "\n";
        return randomFallback();
    } catch (...) {
        std::cerr << "[ExternalPythonAlgorithmmrX] Unknown Python bridge error\n";
        return randomFallback();
    }

    // Jeśli doszliśmy tutaj, Python nie zwrócił poprawnego ruchu
    std::cout << "[Fallback] Python returned invalid response\n";
    return randomFallback();
}

// Reward evaluation for Detective moves (opposite of Mr X - reward for being closer to Mr X)
static double EvaluateDetectiveMoveReward(
    const Player* p_Player,
    const PossibleMove& move,
    const GameStateData& gameState
) {
    static constexpr int k_MaxReasonableDistance = 25;

    // Build graph (neighbors only)
    std::map<int, std::vector<int>> map_Graph;
    for (int i_Node = 1; i_Node <= 200; ++i_Node) {
        auto vec_Connections = gameState.p_Graph->GetConnections(i_Node);
        for (const auto& conn : vec_Connections) {
            map_Graph[i_Node].push_back(conn.i_NodeId);
        }
    }

    // BFS helper
    auto fn_BFS = [&](int i_Start) {
        std::map<int,int> map_Dist;
        std::queue<int> q;
        map_Dist[i_Start] = 0;
        q.push(i_Start);
        while (!q.empty()) {
            int u = q.front(); q.pop();
            for (int v : map_Graph[u]) {
                if (!map_Dist.count(v)) {
                    map_Dist[v] = map_Dist[u] + 1;
                    q.push(v);
                }
            }
        }
        return map_Dist;
    };

    // Find Mr X position (use last known if not visible)
    int mrXPos = gameState.i_MrXLastKnownPosition;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (info.b_IsMisterX && info.b_IsVisible) {
            mrXPos = info.i_Position;
            break;
        }
    }

    if (mrXPos <= 0) {
        // No known Mr X position, give neutral reward
        return 0.0;
    }

    // BFS from destination to Mr X
    auto distMap = fn_BFS(move.i_DestinationNode);
    int distToMrX = distMap.count(mrXPos) ? distMap.at(mrXPos) : k_MaxReasonableDistance;

    // Detective reward: CLOSER to Mr X is BETTER (opposite of Mr X reward)
    // Lower distance = higher reward
    double reward = 10.0 - static_cast<double>(distToMrX);
    
    return reward;
}

// External Python Algorithm for Detectives (Police)
// Uses g_pDetectiveBridge on port 5556
static MoveDecision ExternalPythonAlgorithmPolice(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    std::cout << "PYTHON (Detective)\n";
    MoveDecision decision;
    decision.b_HasDecision = false;
    if (vec_PossibleMoves.empty()) return decision;

    auto randomFallback = [&]() -> MoveDecision {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(vec_PossibleMoves.size()) - 1);
        const auto& sel = vec_PossibleMoves[dis(gen)];
        MoveDecision d;
        d.b_HasDecision = true;
        d.i_DestinationNode = sel.i_DestinationNode;
        d.i_TransportType = sel.i_TransportType;
        std::cout << "[Fallback Detective] Using random move: dest=" 
                  << sel.i_DestinationNode << ", transport=" << sel.i_TransportType << "\n";
        return d;
    };

    // Use the detective bridge (extern from GameState.cpp)
    PythonBridge* bridge = g_pDetectiveBridge;
    if (!bridge) {
        std::cout << "[Fallback] Detective Python bridge not available\n";
        return randomFallback();
    }

    nlohmann::json req;
    static double reward_for_move = 0.0;
    req["game_state"] = {
        {"current_player_index", gameState.i_CurrentPlayerIndex},
        {"current_round", gameState.i_CurrentRound},
        {"is_reveal_round", gameState.b_IsRevealRound},
        {"is_next_reveal_round", gameState.b_IsNextRevealRound},
        {"mr_x_last_known_position", gameState.i_MrXLastKnownPosition},
        {"mr_x_last_known_round", gameState.i_MrXLastKnownRound},
        {"reward", reward_for_move}
    };

    nlohmann::json jplayers = nlohmann::json::array();
    for (const auto& info : gameState.vec_AllPlayers) {
        jplayers.push_back({
            {"position", info.i_Position},
            {"is_visible", info.b_IsVisible},
            {"is_mister_x", info.b_IsMisterX},
            {"tickets", {
                {"taxi", info.i_TaxiTickets},
                {"bus", info.i_BusTickets},
                {"metro", info.i_MetroTickets},
                {"black", info.i_BlackTickets},
                {"double", info.i_DoubleMoveTickets}
            }}
        });
    }
    req["players"] = jplayers;

    nlohmann::json jmoves = nlohmann::json::array();
    for (const auto& m : vec_PossibleMoves) {
        jmoves.push_back({ {"dest", m.i_DestinationNode}, {"transport", m.i_TransportType} });
    }
    req["possible_moves"] = jmoves;

    try {
        auto fut = bridge->sendRequestAsync(req);
        nlohmann::json resp = fut.get();

        if (resp.is_object()) {
            if (resp.contains("selected_index") && resp["selected_index"].is_number_integer()) {
                int idx = resp["selected_index"].get<int>();
                if (idx >= 0 && idx < static_cast<int>(vec_PossibleMoves.size())) {
                    const auto& mv = vec_PossibleMoves[idx];
                    MoveDecision d; 
                    d.b_HasDecision = true; 
                    d.i_DestinationNode = mv.i_DestinationNode; 
                    d.i_TransportType = mv.i_TransportType;
                    std::cout << "[Python Detective] Selected move by index: dest=" 
                              << mv.i_DestinationNode << ", transport=" << mv.i_TransportType << "\n";
                    double reward = EvaluateDetectiveMoveReward(p_Player, mv, gameState);
                    reward_for_move = reward;
                    std::cout << "[Reward Detective] Calculated reward for move: " << reward << "\n";
                    return d;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[ExternalPythonAlgorithmPolice] Python bridge error: " << e.what() << "\n";
        return randomFallback();
    } catch (...) {
        std::cerr << "[ExternalPythonAlgorithmPolice] Unknown Python bridge error\n";
        return randomFallback();
    }

    std::cout << "[Fallback] Python Detective returned invalid response\n";
    return randomFallback();
}



static MoveDecision DistanceMaximizationAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;

    if (vec_PossibleMoves.empty()) {
        decision.b_HasDecision = false;
        return decision;
    }

    std::map<int, std::vector<int>> map_Graph;
    for (int i_Node = 1; i_Node <= 200; ++i_Node) {
        auto vec_Connections = gameState.p_Graph->GetConnections(i_Node);
        for (const auto& conn : vec_Connections) {
            map_Graph[i_Node].push_back(conn.i_NodeId);
        }
    }

    auto fn_BFS = [&](int i_Start) {
        std::map<int, int> map_Dist;
        std::queue<int> queue_Nodes;
        map_Dist[i_Start] = 0;
        queue_Nodes.push(i_Start);

        while (!queue_Nodes.empty()) {
            int i_U = queue_Nodes.front();
            queue_Nodes.pop();

            for (int i_V : map_Graph[i_U]) {
                if (!map_Dist.count(i_V)) {
                    map_Dist[i_V] = map_Dist[i_U] + 1;
                    queue_Nodes.push(i_V);
                }
            }
        }
        return map_Dist;
    };

    std::vector<std::map<int, int>> vec_PoliceDists;
    std::set<int> set_Occupied;

    for (const auto& playerInfo : gameState.vec_AllPlayers) {
        if (!playerInfo.b_IsMisterX) {
            vec_PoliceDists.push_back(fn_BFS(playerInfo.i_Position));
            set_Occupied.insert(playerInfo.i_Position);
        }
    }

    std::vector<PossibleMove> vec_ValidMoves;
    for (const auto& move : vec_PossibleMoves) {
        if (set_Occupied.count(move.i_DestinationNode) == 0) {
            vec_ValidMoves.push_back(move);
        }
    }
    if (vec_ValidMoves.empty()) {
        vec_ValidMoves = vec_PossibleMoves;
    }

    constexpr int i_k_INF = 999999;
    int i_BestScore = -i_k_INF;
    std::vector<PossibleMove> vec_BestMoves;

    for (const auto& move : vec_ValidMoves) {
        int i_MinDist = i_k_INF;
        for (const auto& map_Dist : vec_PoliceDists) {
            auto it = map_Dist.find(move.i_DestinationNode);
            if (it != map_Dist.end()) {
                i_MinDist = std::min(i_MinDist, it->second);
            }
        }

        int i_Score = (i_MinDist == i_k_INF) ? 0 : i_MinDist;

        if (i_Score > i_BestScore) {
            i_BestScore = i_Score;
            vec_BestMoves.clear();
            vec_BestMoves.push_back(move);
        } else if (i_Score == i_BestScore) {
            vec_BestMoves.push_back(move);
        }
    }

    if (!vec_BestMoves.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(vec_BestMoves.size()) - 1);

        const auto& sel = vec_BestMoves[dis(gen)];
        decision = { true, sel.i_DestinationNode, sel.i_TransportType };
    } else {
        decision.b_HasDecision = false;
    }

    return decision;
}



static MoveDecision DecoyMovementAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;
    decision.b_HasDecision = false;

    if (vec_PossibleMoves.empty()) {
        return decision;
    }

    std::map<int, std::vector<std::pair<int, int>>> map_Graph;
    for (int i_Node = 1; i_Node <= 200; ++i_Node) {
        auto vec_Connections = gameState.p_Graph->GetConnections(i_Node);
        for (const auto& conn : vec_Connections) {
            map_Graph[i_Node].push_back({conn.i_NodeId, conn.i_TransportType});
        }
    }

    int i_CurrentPlayerPos = gameState.vec_AllPlayers[gameState.i_CurrentPlayerIndex].i_Position;

    auto fn_BFS = [&](int i_Start) {
        std::map<int, int> map_Dist;
        std::queue<int> queue_Nodes;
        map_Dist[i_Start] = 0;
        queue_Nodes.push(i_Start);

        while (!queue_Nodes.empty()) {
            int i_U = queue_Nodes.front();
            queue_Nodes.pop();

            for (auto [i_V, _] : map_Graph[i_U]) {
                if (!map_Dist.count(i_V)) {
                    map_Dist[i_V] = map_Dist[i_U] + 1;
                    queue_Nodes.push(i_V);
                }
            }
        }
        return map_Dist;
    };

    std::vector<std::map<int, int>> vec_PoliceDists;
    std::set<int> set_Occupied;

    for (const auto& playerInfo : gameState.vec_AllPlayers) {
        if (!playerInfo.b_IsMisterX) {
            vec_PoliceDists.push_back(fn_BFS(playerInfo.i_Position));
            set_Occupied.insert(playerInfo.i_Position);
        }
    }

    std::vector<PossibleMove> vec_ValidMoves;
    for (const auto& move : vec_PossibleMoves) {
        if (set_Occupied.count(move.i_DestinationNode) == 0) {
            vec_ValidMoves.push_back(move);
        }
    }
    if (vec_ValidMoves.empty()) {
        vec_ValidMoves = vec_PossibleMoves;
    }

    std::vector<int> vec_police_positions;
    for (const auto& playerInfo : gameState.vec_AllPlayers) {
        if (!playerInfo.b_IsMisterX) vec_police_positions.push_back(playerInfo.i_Position);
    }

    auto map_PredictPolicePositions = [&](int i_turns_ahead = 1) {
        std::map<int, std::set<int>> map_predicted;
        std::set<int> set_s0(vec_police_positions.begin(), vec_police_positions.end());
        map_predicted[0] = set_s0;

        for (int i_t = 1; i_t <= i_turns_ahead; ++i_t) {
            std::set<int> set_nextset;
            for (int i_police_pos : vec_police_positions) {
                std::set<int> set_frontier;
                set_frontier.insert(i_police_pos);
                for (int i_step = 0; i_step < i_t; ++i_step) {
                    std::set<int> set_new_front;
                    for (int i_pos : set_frontier) {
                        auto it = map_Graph.find(i_pos);
                        if (it == map_Graph.end()) continue;
                        for (const auto& nb : it->second) set_new_front.insert(nb.first);
                    }
                    if (!set_new_front.empty()) set_frontier.swap(set_new_front);
                }
                set_nextset.insert(set_frontier.begin(), set_frontier.end());
            }
            map_predicted[i_t] = std::move(set_nextset);
        }
        return map_predicted;
    };

    std::map<int, std::set<int>> predicted = map_PredictPolicePositions();

    auto fn_SafetyRisk = [&](int i_node) {
        int i_MinDist = 999999;
        for (const auto& map_Dist : vec_PoliceDists) {
            auto it = map_Dist.find(i_node);
            if (it != map_Dist.end()) {
                i_MinDist = std::min(i_MinDist, it->second);
            }
        }
        if (i_MinDist == 999999) i_MinDist = 8;
        int i_degree = static_cast<int>(map_Graph[i_node].size());
        return 1.0 / (i_MinDist + 1e-6) + 0.8 / std::max(1, i_degree);
    };

    auto fn_DeceptionScore = [&](int i_dest) {
        double d_f_Score = 0.0;
        int i_currentPos = i_CurrentPlayerPos;
        int i_lastKnown = gameState.i_MrXLastKnownPosition;

        if (i_lastKnown != 0 && i_lastKnown != i_currentPos && i_lastKnown != i_dest) {
            auto map_DistPrev = fn_BFS(i_lastKnown);
            int i_dDest = map_DistPrev.count(i_dest) ? map_DistPrev[i_dest] : 999999;
            int i_dCurr = map_DistPrev.count(i_currentPos) ? map_DistPrev[i_currentPos] : 999999;
            if (i_dDest > i_dCurr) {
                d_f_Score += 0.6;
            }
        }

        if (i_lastKnown != 0) {
            auto map_DistLast = fn_BFS(i_lastKnown);
            int i_Dist = map_DistLast.count(i_dest) ? map_DistLast[i_dest] : 999999;
            if (i_Dist != 999999) {
                d_f_Score += 0.5 / (i_Dist + 1.0);
            }
        }

        if (predicted.count(1) && !predicted[1].empty()) {
            int i_min_pred = 999999;
            for (int i_p : predicted[1]) {
                auto distMap = fn_BFS(i_p);
                auto it = distMap.find(i_dest);
                int i_dbuf = (it != distMap.end()) ? it->second : 999999;
                i_min_pred = std::min(i_min_pred, i_dbuf);
            }
            d_f_Score += 0.4 / (i_min_pred + 1.0);
        }

        return d_f_Score;
    };

    auto RolloutScoreForCandidate = [&](int i_startNode, int i_depth = 3, int i_numRollouts = 6) {
        double d_totalScore = 0.0;
        std::random_device rd;
        std::mt19937 gen(rd());

        for (int i_rollout = 0; i_rollout < i_numRollouts; ++i_rollout) {
            int i_mrPos = i_startNode;
            std::vector<int> policePos = vec_police_positions;
            bool b_caught = false;
            double d_cumMinDist = 0.0;

            for (int i_dbuf = 0; i_dbuf < i_depth; ++i_dbuf) {
                auto mrDistMap = fn_BFS(i_mrPos);
                std::vector<int> vec_newPolicePos;

                for (int i_pp : policePos) {
                    if (i_pp == i_mrPos) { b_caught = true; break; }

                    int i_bestNb = i_pp;
                    int i_bestDist = mrDistMap.count(i_pp) ? mrDistMap[i_pp] : 999999;
                    for (auto [v, _] : map_Graph[i_pp]) {
                        int i_dist = mrDistMap.count(v) ? mrDistMap[v] : 999999;
                        if (i_dist < i_bestDist) { i_bestDist = i_dist; i_bestNb = v; }
                    }
                    vec_newPolicePos.push_back(i_bestNb);
                }

                if (b_caught || std::find(vec_newPolicePos.begin(), vec_newPolicePos.end(), i_mrPos) != vec_newPolicePos.end()) {
                    d_cumMinDist += 0.0;
                    break;
                }

                policePos = vec_newPolicePos;

                std::vector<int> vec_neighbors;
                for (auto [v, _] : map_Graph[i_mrPos]) vec_neighbors.push_back(v);
                if (vec_neighbors.empty()) break;

                std::uniform_int_distribution<> disNeighbor(0, static_cast<int>(vec_neighbors.size()) - 1);
                i_mrPos = vec_neighbors[disNeighbor(gen)];

                int i_minD = 999999;
                for (int i_pp : policePos) {
                    auto distMap = fn_BFS(i_pp);
                    i_minD = std::min(i_minD, distMap.count(i_mrPos) ? distMap[i_mrPos] : 999999);
                }
                d_cumMinDist += (i_minD != 999999) ? static_cast<double>(i_minD) : static_cast<double>(std::max(1, static_cast<int>(map_Graph.size())));
            }

            d_totalScore += b_caught ? -100.0 : d_cumMinDist / std::max(1, i_depth);
        }

        return d_totalScore / std::max(1, i_numRollouts);
    };

    struct Candidate {
        PossibleMove move;
        double d_Risk;
        double d_Score;  
    };

    std::vector<Candidate> vec_Candidates;
    for (const auto& move : vec_ValidMoves) {
        double d_Risk = fn_SafetyRisk(move.i_DestinationNode);
        double d_Deception = fn_DeceptionScore(move.i_DestinationNode);
        double d_Rollout = RolloutScoreForCandidate(move.i_DestinationNode);

        if (d_Risk <= 1.8) {
            vec_Candidates.push_back({move, d_Risk, d_Deception + d_Rollout});
        }
    }

    if (vec_Candidates.empty()) {
        return DistanceMaximizationAlgorithm(p_Player, vec_PossibleMoves, gameState);
    }

    std::sort(vec_Candidates.begin(), vec_Candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.d_Risk < b.d_Risk; });

    std::vector<double> vec_Weights;
    for (const auto& c : vec_Candidates) {
        double d_w = c.d_Score - c.d_Risk + 1.0;
        vec_Weights.push_back(std::max(1e-6, d_w));
    }

    double d_Total = std::accumulate(vec_Weights.begin(), vec_Weights.end(), 0.0);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> dis(0.0, d_Total);

    double d_Pick = dis(gen);
    double d_Cum = 0.0;
    for (size_t i = 0; i < vec_Candidates.size(); ++i) {
        d_Cum += vec_Weights[i];
        if (d_Pick <= d_Cum) {
            decision.b_HasDecision = true;
            decision.i_DestinationNode = vec_Candidates[i].move.i_DestinationNode;
            decision.i_TransportType = vec_Candidates[i].move.i_TransportType;
            return decision;
        }
    }

    const auto& last = vec_Candidates.back().move;
    decision.b_HasDecision = true;
    decision.i_DestinationNode = last.i_DestinationNode;
    decision.i_TransportType = last.i_TransportType;
    return decision;
}


struct TicketState
{
    int taxi;
    int bus;
    int metro;
    int black;
};


static MoveDecision MonteCarloMrXAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;
    decision.b_HasDecision = false;

    if (vec_PossibleMoves.empty()) {
        return decision;
    }

    if (vec_PossibleMoves.size() == 1) {
        const auto& move = vec_PossibleMoves[0];
        decision = { true, move.i_DestinationNode, move.i_TransportType };
        return decision;
    }

    constexpr int i_kSimulationsPerMove = 60;
    constexpr int i_kSimulationDepth = 6;
    constexpr double d_kSurvivalDiscount = 0.95;

    std::map<int, std::vector<std::pair<int, int>>> map_Graph;
    for (int i_Node = 1; i_Node <= 200; ++i_Node) {
        auto vec_Connections = gameState.p_Graph->GetConnections(i_Node);
        for (const auto& conn : vec_Connections) {
            map_Graph[i_Node].push_back({conn.i_NodeId, conn.i_TransportType});
        }
    }

    //bfs
    std::map<int, std::map<int, int>> cache_Distances;
    auto fn_BFSDistanceMap = [&](int i_Start) -> std::map<int, int> {
        if (cache_Distances.count(i_Start)) {
            return cache_Distances[i_Start];
        }

        std::map<int, int> map_Distances;
        std::queue<int> queue_Nodes;
        map_Distances[i_Start] = 0;
        queue_Nodes.push(i_Start);

        while (!queue_Nodes.empty()) {
            int i_Node = queue_Nodes.front();
            queue_Nodes.pop();

            for (const auto& [i_Neighbor, _] : map_Graph[i_Node]) {
                if (!map_Distances.count(i_Neighbor)) {
                    map_Distances[i_Neighbor] = map_Distances[i_Node] + 1;
                    queue_Nodes.push(i_Neighbor);
                }
            }
        }

        cache_Distances[i_Start] = map_Distances;
        return map_Distances;
    };

    auto fn_GetDistance = [&](int i_Pos1, int i_Pos2) -> int {
        auto map_Dist = fn_BFSDistanceMap(i_Pos1);
        return map_Dist.count(i_Pos2) ? map_Dist[i_Pos2] : 999;
    };

    auto fn_GetMovesForPosition = [&](int i_Pos, const std::map<int, int>& map_Tickets) 
        -> std::vector<std::pair<int, int>> {
        std::vector<std::pair<int, int>> vec_ValidMoves;
        
        if (!map_Graph.count(i_Pos)) {
            return vec_ValidMoves;
        }

        for (const auto& [i_Neighbor, i_Transport] : map_Graph[i_Pos]) {
            auto it = map_Tickets.find(i_Transport);
            if (it != map_Tickets.end() && it->second > 0) {
                vec_ValidMoves.push_back({i_Neighbor, i_Transport});
            }
        }
        return vec_ValidMoves;
    };

    std::vector<int> vec_InitialPolicePositions;
    std::vector<std::map<int, int>> vec_InitialPoliceTickets;

    for (const auto& playerInfo : gameState.vec_AllPlayers) {
        if (!playerInfo.b_IsMisterX) {
            vec_InitialPolicePositions.push_back(playerInfo.i_Position);
            std::map<int, int> map_Tickets;
            map_Tickets[Core::k_TransportTypeTaxi] = playerInfo.i_TaxiTickets;
            map_Tickets[Core::k_TransportTypeBus] = playerInfo.i_BusTickets;
            map_Tickets[Core::k_TransportTypeMetro] = playerInfo.i_MetroTickets;
            vec_InitialPoliceTickets.push_back(map_Tickets);
        }
    }

    const PlayerInfo* p_MrXInfo = nullptr;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (info.b_IsMisterX) {
            p_MrXInfo = &info;
            break;
        }
    }

    if (!p_MrXInfo) {
        decision.b_HasDecision = false;
        return decision;
    }

    std::random_device rd;
    std::mt19937 gen(rd());

    auto fn_SimulateOnce = [&](const PossibleMove& initial_Move) -> double {
        int i_DestNode = initial_Move.i_DestinationNode;
        int i_Transport = initial_Move.i_TransportType;

        //if moves are legal?
        int i_TicketCount = 0;
        switch (i_Transport) {
            case Core::k_TransportTypeTaxi:
                i_TicketCount = p_MrXInfo->i_TaxiTickets;
                break;
            case Core::k_TransportTypeBus:
                i_TicketCount = p_MrXInfo->i_BusTickets;
                break;
            case Core::k_TransportTypeMetro:
                i_TicketCount = p_MrXInfo->i_MetroTickets;
                break;
            case Core::k_TransportTypeWater:
                i_TicketCount = p_MrXInfo->i_BlackTickets;
                break;
        }

        if (i_TicketCount <= 0) {
            return -1000.0;  //kara
        }

        int i_MrXPos = i_DestNode;
        std::map<int, int> map_MrXTickets;
        map_MrXTickets[Core::k_TransportTypeTaxi] = p_MrXInfo->i_TaxiTickets;
        map_MrXTickets[Core::k_TransportTypeBus] = p_MrXInfo->i_BusTickets;
        map_MrXTickets[Core::k_TransportTypeMetro] = p_MrXInfo->i_MetroTickets;
        map_MrXTickets[Core::k_TransportTypeWater] = p_MrXInfo->i_BlackTickets;

        if (map_MrXTickets[i_Transport] > 0) {
            map_MrXTickets[i_Transport] -= 1;
        }

        std::vector<int> vec_PolicePositions = vec_InitialPolicePositions;
        std::vector<std::map<int, int>> vec_PoliceTickets = vec_InitialPoliceTickets;

        //imeediate capture
        if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) 
            != vec_PolicePositions.end()) {
            return -1000.0;
        }

        //loop
        double d_Value = 0.0;
        double d_Discount = 1.0;

        std::uniform_real_distribution<> dis_Prob(0.0, 1.0);

        for (int i_Step = 0; i_Step < i_kSimulationDepth; ++i_Step) {
            //police move
            std::vector<int> vec_NewPolicePositions;

            for (size_t i = 0; i < vec_PolicePositions.size(); ++i) {
                int i_Pos = vec_PolicePositions[i];
                auto& map_Tickets = vec_PoliceTickets[i];
                auto vec_Moves = fn_GetMovesForPosition(i_Pos, map_Tickets);

                if (vec_Moves.empty()) {
                    vec_NewPolicePositions.push_back(i_Pos);  // Stay in place
                    continue;
                }

                int i_Dest;
                int i_Trans;

                //police strategy - 90% ściganie MrX, 10% - spacerek parkiem (to znaczy, random)
                if (dis_Prob(gen) < 0.9) {
                    auto it_Best = std::min_element(vec_Moves.begin(), vec_Moves.end(),
                        [&](const auto& a, const auto& b) {
                            return fn_GetDistance(a.first, i_MrXPos) < fn_GetDistance(b.first, i_MrXPos);
                        });
                    i_Dest = it_Best->first;
                    i_Trans = it_Best->second;
                } else {
                    //random
                    std::uniform_int_distribution<> dis_Move(0, static_cast<int>(vec_Moves.size()) - 1);
                    auto& move = vec_Moves[dis_Move(gen)];
                    i_Dest = move.first;
                    i_Trans = move.second;
                }

                vec_NewPolicePositions.push_back(i_Dest);

                if (map_Tickets[i_Trans] > 0) {
                    map_Tickets[i_Trans] -= 1;
                }
            }

            vec_PolicePositions = vec_NewPolicePositions;

            //check if caught after police moves
            if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) 
                != vec_PolicePositions.end()) {
                d_Value -= 1000.0 * d_Discount;
                return d_Value;  // game over - caught
            }

            //MrX move
            auto vec_MrMoves = fn_GetMovesForPosition(i_MrXPos, map_MrXTickets);

            if (vec_MrMoves.empty()) {
                d_Value -= 1000.0 * d_Discount;
                return d_Value;  //nie ma drogi, wszędzie policja
            }

            //MrX strategy - 80% smart, 20% random
            if (dis_Prob(gen) < 0.8) {
                //Ucieczka - maximize minimum distance to police
                std::vector<std::tuple<double, int, int>> vec_MoveScores;

                for (const auto& [i_Neighbor, i_Trans] : vec_MrMoves) {
                    int i_MinDist = std::numeric_limits<int>::max();
                    for (int i_PolicePos : vec_PolicePositions) {
                        i_MinDist = std::min(i_MinDist, fn_GetDistance(i_Neighbor, i_PolicePos));
                    }

                    //connectivity bonus
                    int i_Connectivity = static_cast<int>(map_Graph[i_Neighbor].size());

                    double d_Score = i_MinDist * 10.0 + i_Connectivity;
                    vec_MoveScores.push_back({d_Score, i_Neighbor, i_Trans});
                }

                //pick best
                std::sort(vec_MoveScores.begin(), vec_MoveScores.end(),
                    [](const auto& a, const auto& b) { return std::get<0>(a) > std::get<0>(b); });

                i_MrXPos = std::get<1>(vec_MoveScores[0]);
                i_Transport = std::get<2>(vec_MoveScores[0]);
            } else {
                //random
                std::uniform_int_distribution<> dis_Move(0, static_cast<int>(vec_MrMoves.size()) - 1);
                auto& move = vec_MrMoves[dis_Move(gen)];
                i_MrXPos = move.first;
                i_Transport = move.second;
            }

            if (map_MrXTickets[i_Transport] > 0) {
                map_MrXTickets[i_Transport] -= 1;
            }

            //check if going to jail (caught)
            if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) 
                != vec_PolicePositions.end()) {
                d_Value -= 1000.0 * d_Discount;
                return d_Value;
            }

            //score turn
            int i_MinDistance = std::numeric_limits<int>::max();
            int i_TotalDistance = 0;
            for (int i_PolicePos : vec_PolicePositions) {
                int i_Dist = fn_GetDistance(i_MrXPos, i_PolicePos);
                i_MinDistance = std::min(i_MinDistance, i_Dist);
                i_TotalDistance += i_Dist;
            }

            double d_AvgDistance = static_cast<double>(i_TotalDistance) / vec_PolicePositions.size();
            double d_TurnScore = i_MinDistance * 3.0 + d_AvgDistance * 0.5;
            d_Value += d_TurnScore * d_Discount;

            d_Discount *= d_kSurvivalDiscount;
        }

        //survived? - get your bonus:)
        int i_FinalMinDist = std::numeric_limits<int>::max();
        for (int i_PolicePos : vec_PolicePositions) {
            i_FinalMinDist = std::min(i_FinalMinDist, fn_GetDistance(i_MrXPos, i_PolicePos));
        }
        d_Value += 100.0 + i_FinalMinDist * 5.0;

        return d_Value;
    };

    //evaluation of all moves
    auto fn_PossibleMoveLess = [](const PossibleMove& a, const PossibleMove& b) {
        return a.i_DestinationNode < b.i_DestinationNode ||
               (a.i_DestinationNode == b.i_DestinationNode && a.i_TransportType < b.i_TransportType);
    };

    std::map<PossibleMove, double, decltype(fn_PossibleMoveLess)> map_MoveScores(fn_PossibleMoveLess);

    for (const auto& move : vec_PossibleMoves) {
        double d_TotalScore = 0.0;
        int i_Wins = 0;

        for (int i_Sim = 0; i_Sim < i_kSimulationsPerMove; ++i_Sim) {
            double d_Score = fn_SimulateOnce(move);
            d_TotalScore += d_Score;

            if (d_Score > 0.0) {
                i_Wins++;
            }
        }

        double d_AvgScore = d_TotalScore / i_kSimulationsPerMove;
        map_MoveScores[move] = d_AvgScore;
    }

    //best move?
    auto it_Best = std::max_element(map_MoveScores.begin(), map_MoveScores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    if (it_Best != map_MoveScores.end()) {
        decision.b_HasDecision = true;
        decision.i_DestinationNode = it_Best->first.i_DestinationNode;
        decision.i_TransportType = it_Best->first.i_TransportType;
    }

    //cache cleanup (keep last 300 entries if cache is too large)
    if (cache_Distances.size() > 500) {
        auto it = cache_Distances.begin();
        std::advance(it, 200);
        cache_Distances.erase(cache_Distances.begin(), it);
    }

    return decision;
};

static MoveDecision DFSMrXAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;
    decision.b_HasDecision = false;

    if (vec_PossibleMoves.empty()) {
        return decision;
    }

    constexpr int i_kTargetLength = 6;
    constexpr int i_kMaxDepth = 10;
    constexpr int i_kMaxAttemptsPerNode = 2;
    constexpr int i_kMinimaxDepth = 3;

    // Persistent caches (static to maintain between calls)
    static std::map<int, std::map<int, int>> s_DistanceCache;
    static std::map<int, std::vector<std::pair<int, int>>> s_NeighborsCache;
    static std::map<int, int> s_ConnectivityCache;
    static std::map<std::tuple<int, int, std::string>, std::map<int, int>> s_ReachableCache;
    static std::map<std::tuple<int, int>, std::vector<int>> s_PolicePredictions;
    static std::vector<int> s_LastPolicePositions;
    static std::mutex s_CacheMutex;

    // Local caches for this turn
    std::map<int, std::map<int, int>> local_DistanceCache;
    std::map<std::string, double> local_EvalCache;

    // Build graph structure
    std::map<int, std::vector<std::pair<int, int>>> map_Graph;
    for (int i_Node = 1; i_Node <= 200; ++i_Node) {
        auto vec_Connections = gameState.p_Graph->GetConnections(i_Node);
        for (const auto& conn : vec_Connections) {
            map_Graph[i_Node].push_back({conn.i_NodeId, conn.i_TransportType});
        }
    }

    const PlayerInfo* p_MrXInfo = nullptr;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (info.b_IsMisterX) {
            p_MrXInfo = &info;
            break;
        }
    }

    if (!p_MrXInfo) {
        decision.b_HasDecision = false;
        return decision;
    }

    int i_MrXPos = p_MrXInfo->i_Position;
    int i_CurrentTurn = gameState.i_CurrentRound;

    // === HELPER FUNCTIONS ===

    // Get neighbors with caching
    auto fn_GetNeighbors = [&](int i_Node) -> std::vector<std::pair<int, int>> {
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            if (s_NeighborsCache.count(i_Node)) {
                return s_NeighborsCache[i_Node];
            }
        }

        std::vector<std::pair<int, int>> vec_Neighbors;
        if (map_Graph.count(i_Node)) {
            vec_Neighbors = map_Graph[i_Node];
        }

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_NeighborsCache[i_Node] = vec_Neighbors;
        }
        return vec_Neighbors;
    };

    // BFS distances with caching
    auto fn_BFSDistances = [&](int i_Start) -> std::map<int, int> {
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            if (s_DistanceCache.count(i_Start)) {
                return s_DistanceCache[i_Start];
            }
        }

        if (local_DistanceCache.count(i_Start)) {
            return local_DistanceCache[i_Start];
        }

        std::map<int, int> map_Distances;
        std::queue<int> queue_Nodes;
        map_Distances[i_Start] = 0;
        queue_Nodes.push(i_Start);

        while (!queue_Nodes.empty()) {
            int i_Current = queue_Nodes.front();
            queue_Nodes.pop();

            auto vec_Neighbors = fn_GetNeighbors(i_Current);
            for (const auto& [i_Neighbor, _] : vec_Neighbors) {
                if (!map_Distances.count(i_Neighbor)) {
                    map_Distances[i_Neighbor] = map_Distances[i_Current] + 1;
                    queue_Nodes.push(i_Neighbor);
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_DistanceCache[i_Start] = map_Distances;
        }
        local_DistanceCache[i_Start] = map_Distances;
        return map_Distances;
    };

    // Get connectivity with caching
    auto fn_GetConnectivity = [&](int i_Node) -> int {
        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            if (s_ConnectivityCache.count(i_Node)) {
                return s_ConnectivityCache[i_Node];
            }
        }

        int i_Connectivity = static_cast<int>(fn_GetNeighbors(i_Node).size());

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_ConnectivityCache[i_Node] = i_Connectivity;
        }
        return i_Connectivity;
    };

    // Get all reachable positions
    auto fn_GetAllReachablePositions = [&](int i_Start, int i_MaxSteps,
                                            const std::map<int, int>& map_Tickets)
        -> std::map<int, int> {

        std::string str_TicketKey;
        for (const auto& [k, v] : map_Tickets) {
            str_TicketKey += std::to_string(k) + ":" + std::to_string(v) + ",";
        }

        auto cache_Key = std::make_tuple(i_Start, i_MaxSteps, str_TicketKey);

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            if (s_ReachableCache.count(cache_Key)) {
                return s_ReachableCache[cache_Key];
            }
        }

        std::map<int, int> map_Reachable;
        map_Reachable[i_Start] = 0;

        struct QueueItem {
            int i_pos;
            int i_steps;
            std::map<int, int> tickets;
        };

        std::queue<QueueItem> queue_Items;
        queue_Items.push({i_Start, 0, map_Tickets});

        while (!queue_Items.empty()) {
            auto item = queue_Items.front();
            queue_Items.pop();

            if (item.i_steps >= i_MaxSteps) {
                continue;
            }

            auto vec_Neighbors = fn_GetNeighbors(item.i_pos);
            for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
                auto it = item.tickets.find(i_Transport);
                if (it == item.tickets.end() || it->second <= 0) {
                    continue;
                }

                int i_NewSteps = item.i_steps + 1;

                if (!map_Reachable.count(i_Neighbor) || i_NewSteps < map_Reachable[i_Neighbor]) {
                    map_Reachable[i_Neighbor] = i_NewSteps;

                    std::map<int, int> map_NewTickets = item.tickets;
                    if (map_NewTickets[i_Transport] != std::numeric_limits<int>::max()) {
                        map_NewTickets[i_Transport] -= 1;
                    }

                    queue_Items.push({i_Neighbor, i_NewSteps, map_NewTickets});
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            s_ReachableCache[cache_Key] = map_Reachable;
        }
        return map_Reachable;
    };

    // Get police info
    std::vector<const PlayerInfo*> vec_PoliceInfos;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (!info.b_IsMisterX) {
            vec_PoliceInfos.push_back(&info);
        }
    }

    // Simulate detective moves
    auto fn_SimulateDetectiveMovesAdvanced = [&](const std::vector<const PlayerInfo*>& vec_Police,
                                                  int i_MrXPosition) 
        -> std::vector<int> {
        
        std::vector<int> vec_NewPositions;
        std::set<int> set_Occupied;
        
        for (const auto* p : vec_Police) {
            set_Occupied.insert(p->i_Position);
        }

        auto map_MrXDistances = fn_BFSDistances(i_MrXPosition);

        for (const auto* p_Police : vec_Police) {
            int i_BestMove = p_Police->i_Position;
            double d_BestScore = std::numeric_limits<double>::lowest();

            auto vec_Neighbors = fn_GetNeighbors(p_Police->i_Position);

            for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
                if (set_Occupied.count(i_Neighbor)) {
                    continue;
                }

                int i_TicketCount = 0;
                switch (i_Transport) {
                    case Core::k_TransportTypeTaxi:
                        i_TicketCount = p_Police->i_TaxiTickets;
                        break;
                    case Core::k_TransportTypeBus:
                        i_TicketCount = p_Police->i_BusTickets;
                        break;
                    case Core::k_TransportTypeMetro:
                        i_TicketCount = p_Police->i_MetroTickets;
                        break;
                }

                if (i_TicketCount <= 0) {
                    continue;
                }

                double d_Score = 0.0;

                int i_DistToMrX = map_MrXDistances.count(i_Neighbor) ? 
                                  map_MrXDistances[i_Neighbor] : 999;
                d_Score -= i_DistToMrX * 100.0;

                int i_Connectivity = fn_GetConnectivity(i_Neighbor);
                d_Score += i_Connectivity * 10.0;

                if (map_MrXDistances.count(i_Neighbor) && map_MrXDistances[i_Neighbor] == 1) {
                    d_Score += 500.0;
                }

                if (d_Score > d_BestScore) {
                    d_BestScore = d_Score;
                    i_BestMove = i_Neighbor;
                }
            }

            vec_NewPositions.push_back(i_BestMove);
            set_Occupied.insert(i_BestMove);
        }

        return vec_NewPositions;
    };

    // Update police predictions with caching
    auto fn_UpdatePolicePredictions = [&](const std::vector<const PlayerInfo*>& vec_Police,
                                          int i_MrXPosition)
        -> std::vector<int> {

        std::vector<int> vec_CurrentPositions;
        for (const auto* p : vec_Police) {
            vec_CurrentPositions.push_back(p->i_Position);
        }

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            if (s_LastPolicePositions.empty() || s_LastPolicePositions == vec_CurrentPositions) {
                s_LastPolicePositions = vec_CurrentPositions;
            } else {
                auto cache_Key = std::make_tuple(i_MrXPosition,
                                                 std::accumulate(vec_CurrentPositions.begin(),
                                                                vec_CurrentPositions.end(), 0));

                if (s_PolicePredictions.count(cache_Key)) {
                    s_LastPolicePositions = vec_CurrentPositions;
                    return s_PolicePredictions[cache_Key];
                }
                s_LastPolicePositions = vec_CurrentPositions;
            }
        }

        auto vec_NewPredictions = fn_SimulateDetectiveMovesAdvanced(vec_Police, i_MrXPosition);

        {
            std::lock_guard<std::mutex> lock(s_CacheMutex);
            auto cache_Key = std::make_tuple(i_MrXPosition,
                                             std::accumulate(vec_CurrentPositions.begin(),
                                                            vec_CurrentPositions.end(), 0));
            s_PolicePredictions[cache_Key] = vec_NewPredictions;
        }

        return vec_NewPredictions;
    };

    // Comprehensive position evaluation
    auto fn_EvaluatePositionComprehensive = [&](int i_Position, 
                                                const std::map<int, int>& map_Tickets,
                                                const std::vector<const PlayerInfo*>& vec_Police) 
        -> double {
        
        double d_Score = 0.0;

        // Distance calculations
        std::vector<int> vec_DistancesToPolice;
        for (const auto* p : vec_Police) {
            auto map_Dist = fn_BFSDistances(p->i_Position);
            int i_Dist = map_Dist.count(i_Position) ? map_Dist[i_Position] : 999;
            vec_DistancesToPolice.push_back(i_Dist);
        }

        int i_MinDistance = *std::min_element(vec_DistancesToPolice.begin(), 
                                             vec_DistancesToPolice.end());
        double d_AvgDistance = std::accumulate(vec_DistancesToPolice.begin(), 
                                              vec_DistancesToPolice.end(), 0.0) / 
                              vec_DistancesToPolice.size();

        // Distance scoring
        if (i_MinDistance <= 1) {
            d_Score -= 50000.0;
        } else if (i_MinDistance <= 2) {
            d_Score -= 10000.0;
        } else if (i_MinDistance <= 3) {
            d_Score -= 2000.0;
        }

        d_Score += i_MinDistance * 500.0;
        d_Score += d_AvgDistance * 100.0;

        // Available moves
        auto vec_Neighbors = fn_GetNeighbors(i_Position);
        int i_AvailableMoves = 0;
        std::set<int> set_AvailableTransports;
        
        for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
            auto it = map_Tickets.find(i_Transport);
            if (it != map_Tickets.end() && it->second > 0) {
                i_AvailableMoves++;
                set_AvailableTransports.insert(i_Transport);
            }
        }

        d_Score += i_AvailableMoves * 150.0;
        d_Score += set_AvailableTransports.size() * 100.0;

        // Secondary mobility
        double d_SecondaryMobility = 0.0;
        for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
            auto it = map_Tickets.find(i_Transport);
            if (it != map_Tickets.end() && it->second > 0) {
                d_SecondaryMobility += fn_GetConnectivity(i_Neighbor);
            }
        }

        double d_AvgSecondary = vec_Neighbors.empty() ? 0.0 : 
                                d_SecondaryMobility / vec_Neighbors.size();
        d_Score += d_AvgSecondary * 30.0;

        // Strategic positioning
        if (i_AvailableMoves <= 2) {
            d_Score -= 500.0;
        } else if (i_AvailableMoves >= 5) {
            d_Score += 300.0;
        }

        // Police vicinity
        int i_PoliceInVicinity = std::count_if(vec_DistancesToPolice.begin(),
                                               vec_DistancesToPolice.end(),
                                               [](int i_d) { return i_d <= 3; });
        if (i_PoliceInVicinity >= 3) {
            d_Score -= 2000.0;
        } else if (i_PoliceInVicinity >= 2) {
            d_Score -= 800.0;
        }

        // Ticket management
        int i_TotalTickets = 0;
        for (const auto& [k, v] : map_Tickets) {
            if (v != std::numeric_limits<int>::max()) {
                i_TotalTickets += v;
            }
        }
        d_Score += i_TotalTickets * 20.0;

        auto it_Black = map_Tickets.find(Core::k_TransportTypeWater);
        if (it_Black != map_Tickets.end()) {
            d_Score += it_Black->second * 200.0;
        }

        // Metro access
        bool b_HasMetro = false;
        for (const auto& [_, i_Transport] : vec_Neighbors) {
            if (i_Transport == Core::k_TransportTypeMetro) {
                b_HasMetro = true;
                break;
            }
        }
        
        auto it_Metro = map_Tickets.find(Core::k_TransportTypeMetro);
        if (b_HasMetro && it_Metro != map_Tickets.end() && it_Metro->second > 0) {
            d_Score += 250.0;
        }

        // Police ticket depletion
        for (const auto* p : vec_Police) {
            int i_TotalPoliceTickets = p->i_TaxiTickets + p->i_BusTickets + p->i_MetroTickets;
            if (i_TotalPoliceTickets < 5) {
                d_Score += 400.0;
            }
            if (i_TotalPoliceTickets < 3) {
                d_Score += 1000.0;
            }
        }

        // Future mobility
        auto map_FutureReachable = fn_GetAllReachablePositions(i_Position, 2, map_Tickets);
        d_Score += map_FutureReachable.size() * 50.0;

        return d_Score;
    };

    // Forward declaration for minimax
    std::function<double(int, const std::map<int, int>&, const std::vector<const PlayerInfo*>&,
                        int, bool, double, double, int)> fn_MinimaxEvaluatePosition;

    // Minimax evaluation
    fn_MinimaxEvaluatePosition = [&](int i_Position, 
                                     const std::map<int, int>& map_Tickets,
                                     const std::vector<const PlayerInfo*>& vec_Police,
                                     int i_Depth, bool b_IsMrXTurn,
                                     double d_Alpha, double d_Beta, int i_MrXStart) 
        -> double {
        
        // Create cache key
        std::string str_CacheKey = std::to_string(i_Position) + "_" + std::to_string(i_Depth) + "_" + 
                                   std::to_string(b_IsMrXTurn);
        
        if (local_EvalCache.count(str_CacheKey)) {
            return local_EvalCache[str_CacheKey];
        }

        // Terminal conditions
        if (i_Depth == 0) {
            double d_Result = fn_EvaluatePositionComprehensive(i_Position, map_Tickets, vec_Police);
            local_EvalCache[str_CacheKey] = d_Result;
            return d_Result;
        }

        // Check if caught
        for (const auto* p : vec_Police) {
            if (i_Position == p->i_Position) {
                return -100000.0;
            }
        }

        if (b_IsMrXTurn) {
            double d_MaxEval = std::numeric_limits<double>::lowest();
            auto vec_Neighbors = fn_GetNeighbors(i_Position);

            // Pre-calculate and sort neighbors
            std::vector<std::tuple<int, int, int>> vec_NeighborScores;
            
            for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
                auto it = map_Tickets.find(i_Transport);
                if (it == map_Tickets.end() || it->second <= 0) {
                    continue;
                }

                int i_MinDist = 999;
                for (const auto* p : vec_Police) {
                    auto map_Dist = fn_BFSDistances(p->i_Position);
                    int i_Dist = map_Dist.count(i_Neighbor) ? map_Dist[i_Neighbor] : 999;
                    i_MinDist = std::min(i_MinDist, i_Dist);
                }

                vec_NeighborScores.push_back({i_Neighbor, i_Transport, i_MinDist});
            }

            std::sort(vec_NeighborScores.begin(), vec_NeighborScores.end(),
                     [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

            int i_Count = 0;
            for (const auto& [i_Neighbor, i_Transport, _] : vec_NeighborScores) {
                if (i_Count >= 8) break;
                i_Count++;

                std::map<int, int> map_NewTickets = map_Tickets;
                if (map_NewTickets[i_Transport] != std::numeric_limits<int>::max()) {
                    map_NewTickets[i_Transport] -= 1;
                }

                double d_EvalScore = fn_MinimaxEvaluatePosition(
                    i_Neighbor, map_NewTickets, vec_Police, i_Depth - 1,
                    false, d_Alpha, d_Beta, i_MrXStart
                );

                d_MaxEval = std::max(d_MaxEval, d_EvalScore);
                d_Alpha = std::max(d_Alpha, d_EvalScore);

                if (d_Beta <= d_Alpha) {
                    break;
                }
            }

            double d_Result = (d_MaxEval != std::numeric_limits<double>::lowest()) ? 
                             d_MaxEval : -10000.0;
            local_EvalCache[str_CacheKey] = d_Result;
            return d_Result;

        } else {
            // Police turn
            auto vec_NewPolicePositions = fn_UpdatePolicePredictions(vec_Police, i_Position);

            std::vector<const PlayerInfo*> vec_NewPolice;
            for (size_t i = 0; i < vec_Police.size(); ++i) {
                // Create temporary police info (we can't modify the original)
                // In practice, we just use the positions for evaluation
                vec_NewPolice.push_back(vec_Police[i]);
            }

            double d_EvalScore = fn_MinimaxEvaluatePosition(
                i_Position, map_Tickets, vec_NewPolice, i_Depth - 1,
                true, d_Alpha, d_Beta, i_MrXStart
            );

            local_EvalCache[str_CacheKey] = d_EvalScore;
            return d_EvalScore;
        }
    };

    // Strategic move evaluation
    auto fn_EvaluateMoveStrategic = [&](int i_Position, int i_Transport,
                                        const std::map<int, int>& map_Tickets,
                                        const std::vector<const PlayerInfo*>& vec_Police,
                                        int i_TurnNumber) 
        -> double {
        
        double d_Bonus = 0.0;

        // Calculate min distance once if needed
        auto fn_GetMinDistance = [&]() -> int {
            int i_MinDist = 999;
            for (const auto* p : vec_Police) {
                auto map_Dist = fn_BFSDistances(p->i_Position);
                int i_Dist = map_Dist.count(i_Position) ? map_Dist[i_Position] : 999;
                i_MinDist = std::min(i_MinDist, i_Dist);
            }
            return i_MinDist;
        };

        // Transport type bonuses
        if (i_Transport == Core::k_TransportTypeWater) {
            int i_Dist = fn_GetMinDistance();
            if (i_Dist <= 3) {
                d_Bonus += 300.0;
            }
            
            auto it = map_Tickets.find(Core::k_TransportTypeWater);
            if (i_TurnNumber < 8 && it != map_Tickets.end() && it->second > 3) {
                d_Bonus -= 150.0;
            }
        } else if (i_Transport == Core::k_TransportTypeMetro) {
            d_Bonus += 150.0;
        }

        // Game phase strategy
        double d_GameProgress = static_cast<double>(i_TurnNumber) / 24.0;

        if (d_GameProgress < 0.3) {
            int i_Connectivity = fn_GetConnectivity(i_Position);
            d_Bonus += i_Connectivity * 50.0;
        } else if (d_GameProgress < 0.7) {
            int i_Dist = fn_GetMinDistance();
            d_Bonus += i_Dist * 80.0;
        } else {
            int i_Dist = fn_GetMinDistance();
            d_Bonus += i_Dist * 200.0;
            if (i_Dist <= 2) {
                d_Bonus -= 1000.0;
            }
        }

        // Hub control
        int i_Connectivity = fn_GetConnectivity(i_Position);
        if (i_Connectivity >= 6) {
            d_Bonus += 400.0;
        }

        // Escape corridors
        int i_EscapeDirections = 0;
        auto vec_Neighbors = fn_GetNeighbors(i_Position);
        for (const auto& [i_Neighbor, _] : vec_Neighbors) {
            if (fn_GetConnectivity(i_Neighbor) >= 4) {
                i_EscapeDirections++;
            }
        }

        d_Bonus += i_EscapeDirections * 60.0;

        return d_Bonus;
    };

    // Path quality calculation
    auto fn_CalculatePathQuality = [&](const std::vector<std::pair<int, int>>& vec_Path,
                                       const std::vector<const PlayerInfo*>& vec_Police,
                                       int i_MrXStart,
                                       const std::map<int, int>& map_InitialTickets,
                                       int i_TurnNumber) 
        -> double {
        
        double d_Score = 0.0;
        std::map<int, int> map_CurrentTickets = map_InitialTickets;

        for (size_t i = 0; i < vec_Path.size(); ++i) {
            int i_Position = vec_Path[i].first;
            int i_Transport = vec_Path[i].second;

            if (map_CurrentTickets[i_Transport] != std::numeric_limits<int>::max()) {
                map_CurrentTickets[i_Transport] -= 1;
            }

            int i_Depth = std::min({2, i_kMinimaxDepth, static_cast<int>(vec_Path.size() - i)});

            double d_MinimaxScore = fn_MinimaxEvaluatePosition(
                i_Position, map_CurrentTickets, vec_Police,
                i_Depth, true,
                std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::max(), i_MrXStart
            );
            d_Score += d_MinimaxScore;

            double d_StrategicBonus = fn_EvaluateMoveStrategic(
                i_Position, i_Transport, map_CurrentTickets, vec_Police, i_TurnNumber + i
            );
            d_Score += d_StrategicBonus;
            d_Score += i * 50.0;
        }

        // Final position evaluation
        double d_FinalScore = fn_EvaluatePositionComprehensive(
            vec_Path.back().first, map_CurrentTickets, vec_Police
        );
        d_Score += d_FinalScore * 2.0;

        return d_Score;
    };

    // === MAIN DFS FUNCTION ===

    std::vector<std::pair<int, int>> vec_BestPath;
    double d_BestScore = std::numeric_limits<double>::lowest();

    std::function<void(int, std::vector<std::pair<int, int>>&, std::set<int>&,
                      std::map<int, int>&, std::map<int, int>&, int)> fn_DFS;

    fn_DFS = [&](int i_Current, std::vector<std::pair<int, int>>& vec_Path,
                std::set<int>& set_Visited, std::map<int, int>& map_Tickets,
                std::map<int, int>& map_Attempts, int i_Depth) {
        
        if (map_Attempts[i_Current] >= i_kMaxAttemptsPerNode) {
            return;
        }

        map_Attempts[i_Current]++;

        // Evaluate every 2nd step
        if (vec_Path.size() % 2 == 0) {
            double d_CurrentScore = fn_CalculatePathQuality(
                vec_Path, vec_PoliceInfos, i_MrXPos,
                map_Tickets, gameState.i_CurrentRound
            );

            if (d_CurrentScore > d_BestScore) {
                d_BestScore = d_CurrentScore;
                vec_BestPath = vec_Path;
            }
        }

        if (vec_Path.size() >= i_kTargetLength || i_Depth >= i_kMaxDepth) {
            return;
        }

        auto vec_Neighbors = fn_GetNeighbors(i_Current);

        // Calculate priorities
        std::vector<std::tuple<int, int, double>> vec_NeighborPriorities;

        for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
            if (set_Visited.count(i_Neighbor)) {
                continue;
            }

            auto it = map_Tickets.find(i_Transport);
            if (it == map_Tickets.end() || it->second <= 0) {
                continue;
            }

            double d_Score = 0.0;

            int i_MinDist = 999;
            for (const auto* p : vec_PoliceInfos) {
                auto map_Dist = fn_BFSDistances(p->i_Position);
                int i_Dist = map_Dist.count(i_Neighbor) ? map_Dist[i_Neighbor] : 999;
                i_MinDist = std::min(i_MinDist, i_Dist);
            }

            d_Score += i_MinDist * 100.0;
            d_Score += fn_GetConnectivity(i_Neighbor) * 50.0;

            vec_NeighborPriorities.push_back({i_Neighbor, i_Transport, d_Score});
        }

        std::sort(vec_NeighborPriorities.begin(), vec_NeighborPriorities.end(),
                 [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

        // Explore top moves
        int i_Count = 0;
        for (const auto& [i_Neighbor, i_Transport, _] : vec_NeighborPriorities) {
            if (i_Count >= 10) break;
            i_Count++;

            set_Visited.insert(i_Neighbor);
            vec_Path.push_back({i_Neighbor, i_Transport});

            std::map<int, int> map_NewTickets = map_Tickets;
            if (map_NewTickets[i_Transport] != std::numeric_limits<int>::max()) {
                map_NewTickets[i_Transport] -= 1;
            }

            std::map<int, int> map_NewAttempts = map_Attempts;

            fn_DFS(i_Neighbor, vec_Path, set_Visited, map_NewTickets, map_NewAttempts, i_Depth + 1);

            set_Visited.erase(i_Neighbor);
            vec_Path.pop_back();
        }
    };

    // === MAIN LOGIC ===

    // Build Mr. X tickets map
    std::map<int, int> map_MrXTickets;
    map_MrXTickets[Core::k_TransportTypeTaxi] = p_MrXInfo->i_TaxiTickets;
    map_MrXTickets[Core::k_TransportTypeBus] = p_MrXInfo->i_BusTickets;
    map_MrXTickets[Core::k_TransportTypeMetro] = p_MrXInfo->i_MetroTickets;
    map_MrXTickets[Core::k_TransportTypeWater] = p_MrXInfo->i_BlackTickets;

    // Clean old cache entries with lock
    {
        std::lock_guard<std::mutex> lock(s_CacheMutex);
        if (s_DistanceCache.size() > 500) {
            auto it = s_DistanceCache.begin();
            std::advance(it, 200);
            s_DistanceCache.erase(s_DistanceCache.begin(), it);
        }

        if (s_ReachableCache.size() > 200) {
            auto it = s_ReachableCache.begin();
            std::advance(it, 100);
            s_ReachableCache.erase(s_ReachableCache.begin(), it);
        }
    }

    // Initial move evaluation
    std::vector<std::tuple<int, int, double>> vec_InitialMoves;

    for (const auto& move : vec_PossibleMoves) {
        std::map<int, int> map_TempTickets = map_MrXTickets;
        if (map_TempTickets[move.i_TransportType] != std::numeric_limits<int>::max()) {
            map_TempTickets[move.i_TransportType] -= 1;
        }

        double d_QuickScore = 0.0;
        for (const auto* p : vec_PoliceInfos) {
            auto map_Dist = fn_BFSDistances(p->i_Position);
            d_QuickScore += (map_Dist.count(move.i_DestinationNode) ? 
                            map_Dist[move.i_DestinationNode] : 0) * 100.0;
        }

        d_QuickScore += fn_GetConnectivity(move.i_DestinationNode) * 50.0;

        vec_InitialMoves.push_back({move.i_DestinationNode, move.i_TransportType, d_QuickScore});
    }

    std::sort(vec_InitialMoves.begin(), vec_InitialMoves.end(),
             [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

    // Full evaluation for top candidates
    std::vector<std::tuple<int, int, double>> vec_EvaluatedMoves;

    int i_CandidateCount = 0;
    for (const auto& [i_Dest, i_Transport, _] : vec_InitialMoves) {
        if (i_CandidateCount >= 7) break;
        i_CandidateCount++;

        std::map<int, int> map_TempTickets = map_MrXTickets;
        if (map_TempTickets[i_Transport] != std::numeric_limits<int>::max()) {
            map_TempTickets[i_Transport] -= 1;
        }

        double d_Score = fn_MinimaxEvaluatePosition(
            i_Dest, map_TempTickets, vec_PoliceInfos, i_kMinimaxDepth,
            false, std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(), i_MrXPos
        );

        double d_StrategicScore = fn_EvaluateMoveStrategic(
            i_Dest, i_Transport, map_TempTickets, vec_PoliceInfos, i_CurrentTurn
        );

        double d_TotalScore = d_Score + d_StrategicScore;
        vec_EvaluatedMoves.push_back({i_Dest, i_Transport, d_TotalScore});
    }

    std::sort(vec_EvaluatedMoves.begin(), vec_EvaluatedMoves.end(),
             [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

    // Explore with DFS
    int i_ExploreCount = 0;
    for (const auto& [i_Dest, i_Transport, _] : vec_EvaluatedMoves) {
        if (i_ExploreCount >= 5) break;
        i_ExploreCount++;

        std::set<int> set_Visited;
        set_Visited.insert(i_MrXPos);
        set_Visited.insert(i_Dest);

        std::vector<std::pair<int, int>> vec_Path;
        vec_Path.push_back({i_Dest, i_Transport});

        std::map<int, int> map_Tickets = map_MrXTickets;
        if (map_Tickets[i_Transport] != std::numeric_limits<int>::max()) {
            map_Tickets[i_Transport] -= 1;
        }

        std::map<int, int> map_Attempts;

        fn_DFS(i_Dest, vec_Path, set_Visited, map_Tickets, map_Attempts, 1);
    }

    // Return best move
    if (!vec_BestPath.empty()) {
        decision.b_HasDecision = true;
        decision.i_DestinationNode = vec_BestPath[0].first;
        decision.i_TransportType = vec_BestPath[0].second;
        return decision;
    }

    if (!vec_EvaluatedMoves.empty()) {
        decision.b_HasDecision = true;
        decision.i_DestinationNode = std::get<0>(vec_EvaluatedMoves[0]);
        decision.i_TransportType = std::get<1>(vec_EvaluatedMoves[0]);
        return decision;
    }

    // Random fallback
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, static_cast<int>(vec_PossibleMoves.size()) - 1);
    const auto& move = vec_PossibleMoves[dis(gen)];
    decision = { true, move.i_DestinationNode, move.i_TransportType };

    return decision;
}

static bool ConsumeTicketForTransport(TicketState& tickets, int i_transportType)
{
    switch (i_transportType) {
        case Core::k_TransportTypeTaxi:
            if (tickets.taxi > 0) { --tickets.taxi; return true; }
            break;
        case Core::k_TransportTypeBus:
            if (tickets.bus > 0) { --tickets.bus; return true; }
            break;
        case Core::k_TransportTypeMetro:
            if (tickets.metro > 0) { --tickets.metro; return true; }
            break;
        case Core::k_TransportTypeWater:
            // handled by black ticket fallback below
            break;
        default:
            break;
    }

    if (tickets.black > 0) {
        --tickets.black;
        return true;
    }
    return false;
}

static std::set<int> GenerateReachableNodes(
    const GraphManager* p_Graph,
    int i_startNode,
    int i_turnsSinceReveal,
    const PlayerInfo& mrXInfo
)
{
    std::set<int> reachable;
    if (!p_Graph || !p_Graph->IsValidNode(i_startNode)) {
        return reachable;
    }

    i_turnsSinceReveal = std::max(0, i_turnsSinceReveal);

    TicketState initialTickets{
        std::max(0, mrXInfo.i_TaxiTickets),
        std::max(0, mrXInfo.i_BusTickets),
        std::max(0, mrXInfo.i_MetroTickets),
        std::max(0, mrXInfo.i_BlackTickets)
    };

    struct State {
        int i_node;
        int i_depth;
        TicketState tickets;
    };

    std::queue<State> queueStates;
    std::set<std::tuple<int, int, int, int, int>> visited;

    reachable.insert(i_startNode);
    queueStates.push({i_startNode, 0, initialTickets});
    visited.insert(std::make_tuple(i_startNode, initialTickets.taxi, initialTickets.bus, initialTickets.metro, initialTickets.black));

    while (!queueStates.empty()) {
        State state = queueStates.front();
        queueStates.pop();

        if (state.i_depth >= i_turnsSinceReveal) {
            continue;
        }

        auto connections = p_Graph->GetConnections(state.i_node);
        for (const auto& conn : connections) {
            TicketState nextTickets = state.tickets;
            if (!ConsumeTicketForTransport(nextTickets, conn.i_TransportType)) {
                continue;
            }

            int i_nextNode = conn.i_NodeId;
            int i_nextDepth = state.i_depth + 1;
            reachable.insert(i_nextNode);

            auto key = std::make_tuple(i_nextNode, nextTickets.taxi, nextTickets.bus, nextTickets.metro, nextTickets.black);
            if (visited.insert(key).second) {
                queueStates.push({i_nextNode, i_nextDepth, nextTickets});
            }
        }
    }

    if (reachable.empty()) {
        reachable.insert(i_startNode);
    }
    return reachable;
}

static int ShortestPathDistance(const GraphManager* p_Graph, int i_startNode, int i_goalNode)
{
    if (!p_Graph || !p_Graph->IsValidNode(i_startNode) || !p_Graph->IsValidNode(i_goalNode)) {
        return -1;
    }

    if (i_startNode == i_goalNode) {
        return 0;
    }

    int i_nodeCount = p_Graph->GetNodeCount();
    std::vector<int> dist(i_nodeCount + 1, -1);
    std::queue<int> queueNodes;
    dist[i_startNode] = 0;
    queueNodes.push(i_startNode);

    while (!queueNodes.empty()) {
        int i_node = queueNodes.front();
        queueNodes.pop();
        int i_nextDist = dist[i_node] + 1;

        auto connections = p_Graph->GetConnections(i_node);
        for (const auto& conn : connections) {
            int i_neighbor = conn.i_NodeId;
            if (!p_Graph->IsValidNode(i_neighbor)) {
                continue;
            }
            if (dist[i_neighbor] != -1) {
                continue;
            }
            dist[i_neighbor] = i_nextDist;
            if (i_neighbor == i_goalNode) {
                return dist[i_neighbor];
            }
            queueNodes.push(i_neighbor);
        }
    }

    return dist[i_goalNode];
}

// Lightweight symmetric distance cache to speed up repeated shortest-path queries
static int GetCachedDistance(const GraphManager* p_Graph, int a, int b) {
    if (a == b) return 0;
    // static cache across calls; key is (a<<32)|b
    static std::unordered_map<uint64_t, int> s_DistCache;
    uint64_t keyAB = (static_cast<uint64_t>(static_cast<uint32_t>(a)) << 32) | static_cast<uint32_t>(b);
    auto it = s_DistCache.find(keyAB);
    if (it != s_DistCache.end()) return it->second;

    int d = ShortestPathDistance(p_Graph, a, b);
    s_DistCache[keyAB] = d;
    // store symmetric as well (graph is effectively undirected for path length purposes)
    uint64_t keyBA = (static_cast<uint64_t>(static_cast<uint32_t>(b)) << 32) | static_cast<uint32_t>(a);
    s_DistCache[keyBA] = d;
    // Optional: simple cap to prevent unbounded growth
    if (s_DistCache.size() > 20000) {
        s_DistCache.clear();
    }
    return d;
}

static std::map<int, double> ComputeProbabilityMap(const GameStateData& gameState, const Player* /*p_Player*/)
{
    std::map<int, double> probabilityMap;

    const GraphManager* p_Graph = gameState.p_Graph;
    if (!p_Graph) {
        return probabilityMap;
    }

    const PlayerInfo* p_MrXInfo = nullptr;
    std::vector<const PlayerInfo*> vec_PoliceInfos;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (info.b_IsMisterX) {
            p_MrXInfo = &info;
        } else {
            vec_PoliceInfos.push_back(&info);
        }
    }

    if (!p_MrXInfo) {
        return probabilityMap;
    }

    if (p_MrXInfo->b_IsVisible) {
        probabilityMap[p_MrXInfo->i_Position] = 1.0;
        return probabilityMap;
    }

    int i_lastKnownPos = gameState.i_MrXLastKnownPosition;
    int i_lastKnownRound = gameState.i_MrXLastKnownRound;

    if (i_lastKnownPos <= 0 || i_lastKnownRound < 0) {
        int i_nodeCount = p_Graph->GetNodeCount();
        if (i_nodeCount <= 0) {
            return probabilityMap;
        }
        double d_uniformProb = 1.0 / static_cast<double>(i_nodeCount);
        for (int i_node = 1; i_node <= i_nodeCount; ++i_node) {
            probabilityMap[i_node] = d_uniformProb;
        }
        return probabilityMap;
    }

    int i_turnsSinceReveal = std::max(0, gameState.i_CurrentRound - i_lastKnownRound);
    std::set<int> reachable = GenerateReachableNodes(p_Graph, i_lastKnownPos, i_turnsSinceReveal, *p_MrXInfo);

    if (reachable.empty()) {
        reachable.insert(i_lastKnownPos);
    }

    double d_baseProb = reachable.empty() ? 0.0 : 1.0 / static_cast<double>(reachable.size());
    for (int i_node : reachable) {
        probabilityMap[i_node] = d_baseProb;
    }

    constexpr double d_kAlpha = 0.15;
    for (auto& entry : probabilityMap) {
        int i_node = entry.first;
        double& d_probRef = entry.second;

        int i_minDistance = std::numeric_limits<int>::max();
        for (const auto* p_Police : vec_PoliceInfos) {
            int i_dist = ShortestPathDistance(p_Graph, p_Police->i_Position, i_node);
            if (i_dist >= 0) {
                i_minDistance = std::min(i_minDistance, i_dist);
            }
        }

        if (i_minDistance != std::numeric_limits<int>::max()) {
            d_probRef *= (1.0 + d_kAlpha * static_cast<double>(i_minDistance));
        }
    }

    double d_totalProb = 0.0;
    for (const auto& entry : probabilityMap) {
        d_totalProb += entry.second;
    }

    if (d_totalProb > 0.0) {
        for (auto& entry : probabilityMap) {
            entry.second /= d_totalProb;
        }
    }

    return probabilityMap;
}

static double HeuristicDistance(int i_NodeA, int i_NodeB, const GraphManager* p_Graph)
{
    if (!p_Graph || !p_Graph->IsValidNode(i_NodeA) || !p_Graph->IsValidNode(i_NodeB)) {
        return std::numeric_limits<double>::infinity();
    }

    const Node* p_NodeA = p_Graph->GetNode(i_NodeA);
    const Node* p_NodeB = p_Graph->GetNode(i_NodeB);
    if (!p_NodeA || !p_NodeB) {
        int i_PathDistance = ShortestPathDistance(p_Graph, i_NodeA, i_NodeB);
        return (i_PathDistance >= 0) ? static_cast<double>(i_PathDistance) : std::numeric_limits<double>::infinity();
    }

    double d_Dx = static_cast<double>(p_NodeA->i_X - p_NodeB->i_X);
    double d_Dy = static_cast<double>(p_NodeA->i_Y - p_NodeB->i_Y);
    double d_Distance = std::sqrt(d_Dx * d_Dx + d_Dy * d_Dy);

    if (d_Distance > 0.0) {
        return d_Distance;
    }

    int i_PathDistance = ShortestPathDistance(p_Graph, i_NodeA, i_NodeB);
    return (i_PathDistance >= 0) ? static_cast<double>(i_PathDistance) : 0.0;
}

static std::vector<int> AStarShortestPathPolice(
    int i_StartNode,
    int i_TargetNode,
    const GameStateData& gameState,
    const PlayerInfo& playerInfo
)
{
    std::vector<int> vec_Path;

    const GraphManager* p_Graph = gameState.p_Graph;
    if (!p_Graph || !p_Graph->IsValidNode(i_StartNode) || !p_Graph->IsValidNode(i_TargetNode)) {
        return vec_Path;
    }

    struct QueueEntry {
        double d_FScore;
        int i_Node;

        bool operator<(const QueueEntry& rhs) const {
            return d_FScore > rhs.d_FScore;
        }
    };

    std::priority_queue<QueueEntry> queue_Open;
    std::set<int> set_Closed;
    std::map<int, int> map_CameFrom;
    std::map<int, double> map_GScore;

    auto fn_HasTicketForTransport = [&](int i_TransportType) {
        switch (i_TransportType) {
            case k_TransportTypeTaxi:
                return playerInfo.i_TaxiTickets > 0;
            case k_TransportTypeBus:
                return playerInfo.i_BusTickets > 0;
            case k_TransportTypeMetro:
                return playerInfo.i_MetroTickets > 0;
            case k_TransportTypeWater:
                return playerInfo.i_BlackTickets > 0;
            default:
                return false;
        }
    };

    double d_StartHeuristic = HeuristicDistance(i_StartNode, i_TargetNode, p_Graph);
    if (std::isinf(d_StartHeuristic)) {
        return vec_Path;
    }

    map_GScore[i_StartNode] = 0.0;
    queue_Open.push({ d_StartHeuristic, i_StartNode });

    while (!queue_Open.empty()) {
        QueueEntry entry = queue_Open.top();
        queue_Open.pop();

        int i_CurrentNode = entry.i_Node;
        if (i_CurrentNode == i_TargetNode) {
            int i_Reconstruct = i_CurrentNode;
            vec_Path.push_back(i_Reconstruct);
            while (map_CameFrom.find(i_Reconstruct) != map_CameFrom.end()) {
                i_Reconstruct = map_CameFrom[i_Reconstruct];
                vec_Path.push_back(i_Reconstruct);
            }
            std::reverse(vec_Path.begin(), vec_Path.end());
            return vec_Path;
        }

        if (set_Closed.find(i_CurrentNode) != set_Closed.end()) {
            continue;
        }
        set_Closed.insert(i_CurrentNode);

        auto vec_Connections = p_Graph->GetConnections(i_CurrentNode);
        for (const auto& conn : vec_Connections) {
            int i_Neighbor = conn.i_NodeId;
            int i_TransportType = conn.i_TransportType;

            if (!fn_HasTicketForTransport(i_TransportType)) {
                continue;
            }

            if (set_Closed.find(i_Neighbor) != set_Closed.end()) {
                continue;
            }

            auto it_CurrentG = map_GScore.find(i_CurrentNode);
            if (it_CurrentG == map_GScore.end()) {
                continue;
            }

            double d_CurrentG = it_CurrentG->second;
            double d_TentativeG = d_CurrentG + 1.0;

            auto it_GNeighbor = map_GScore.find(i_Neighbor);
            if (it_GNeighbor != map_GScore.end() && d_TentativeG >= it_GNeighbor->second) {
                continue;
            }

            double d_Heuristic = HeuristicDistance(i_Neighbor, i_TargetNode, p_Graph);
            if (std::isinf(d_Heuristic)) {
                continue;
            }

            map_CameFrom[i_Neighbor] = i_CurrentNode;
            map_GScore[i_Neighbor] = d_TentativeG;
            double d_FScore = d_TentativeG + d_Heuristic;
            queue_Open.push({ d_FScore, i_Neighbor });
        }
    }

    return vec_Path;
}

// Greedy police AI: chase the most probable Mr. X position via A* pathing.
static MoveDecision GreedyShortestPathPoliceAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
)
{
    MoveDecision decision;
    decision.b_HasDecision = false;

    if (vec_PossibleMoves.empty()) {
        return decision;
    }

    auto fn_SelectRandomMove = [&]() -> MoveDecision {
        MoveDecision randomDecision;
        randomDecision.b_HasDecision = true;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(vec_PossibleMoves.size()) - 1);
        const auto& move = vec_PossibleMoves[dis(gen)];
        randomDecision.i_DestinationNode = move.i_DestinationNode;
        randomDecision.i_TransportType = move.i_TransportType;
        return randomDecision;
    };

    const GraphManager* p_Graph = gameState.p_Graph;
    if (!p_Graph) {
        return fn_SelectRandomMove();
    }

    if (gameState.i_CurrentPlayerIndex < 0 || gameState.i_CurrentPlayerIndex >= static_cast<int>(gameState.vec_AllPlayers.size())) {
        return fn_SelectRandomMove();
    }

    const PlayerInfo& playerInfo = gameState.vec_AllPlayers[gameState.i_CurrentPlayerIndex];
    if (playerInfo.b_IsMisterX) {
        return fn_SelectRandomMove();
    }

    std::map<int, double> map_Probabilities = ComputeProbabilityMap(gameState, p_Player);
    if (map_Probabilities.empty()) {
        return fn_SelectRandomMove();
    }

    double d_MaxProbability = -std::numeric_limits<double>::infinity();
    for (const auto& entry : map_Probabilities) {
        d_MaxProbability = std::max(d_MaxProbability, entry.second);
    }

    if (!std::isfinite(d_MaxProbability)) {
        return fn_SelectRandomMove();
    }

    constexpr double d_kProbabilityEpsilon = 1e-9;
    std::vector<int> vec_TargetCandidates;
    vec_TargetCandidates.reserve(map_Probabilities.size());
    for (const auto& entry : map_Probabilities) {
        if (std::fabs(entry.second - d_MaxProbability) <= d_kProbabilityEpsilon) {
            vec_TargetCandidates.push_back(entry.first);
        }
    }

    if (vec_TargetCandidates.empty()) {
        return fn_SelectRandomMove();
    }

    int i_TargetNode = -1;
    double d_BestHeuristic = std::numeric_limits<double>::infinity();
    for (int i_Candidate : vec_TargetCandidates) {
        double d_Distance = HeuristicDistance(playerInfo.i_Position, i_Candidate, p_Graph);
        if (d_Distance < d_BestHeuristic) {
            d_BestHeuristic = d_Distance;
            i_TargetNode = i_Candidate;
        }
    }

    if (i_TargetNode <= 0 || std::isinf(d_BestHeuristic)) {
        return fn_SelectRandomMove();
    }

    std::vector<int> vec_Path = AStarShortestPathPolice(playerInfo.i_Position, i_TargetNode, gameState, playerInfo);
    if (vec_Path.size() > 1U) {
        int i_NextNode = vec_Path[1];
        for (const auto& move : vec_PossibleMoves) {
            if (move.i_DestinationNode == i_NextNode) {
                decision.b_HasDecision = true;
                decision.i_DestinationNode = move.i_DestinationNode;
                decision.i_TransportType = move.i_TransportType;
                return decision;
            }
        }
    }

    const PossibleMove* p_BestMove = nullptr;
    double d_BestMoveScore = std::numeric_limits<double>::infinity();
    for (const auto& move : vec_PossibleMoves) {
        double d_Distance = HeuristicDistance(move.i_DestinationNode, i_TargetNode, p_Graph);
        if (d_Distance < d_BestMoveScore) {
            d_BestMoveScore = d_Distance;
            p_BestMove = &move;
        }
    }

    if (p_BestMove) {
        decision.b_HasDecision = true;
        decision.i_DestinationNode = p_BestMove->i_DestinationNode;
        decision.i_TransportType = p_BestMove->i_TransportType;
        return decision;
    }

    return fn_SelectRandomMove();
}

static MoveDecision MonteCarloPoliceAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;

    if (vec_PossibleMoves.empty()) {
        decision.b_HasDecision = false;
        return decision;
    }

    constexpr int i_kSimulationsPerOption = 100;
    constexpr int i_kSimulationDepth = 3;
    constexpr double d_kCaptureDiscount = 0.9;

    // --- Probability map for Mr. X ---
    std::map<int, double> map_ProbMap = ComputeProbabilityMap(gameState, p_Player);
    if (map_ProbMap.empty()) {
        // Random move if probability data unavailable
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(vec_PossibleMoves.size()) - 1);
        const auto& move = vec_PossibleMoves[dis(gen)];
        decision = { true, move.i_DestinationNode, move.i_TransportType };
        return decision;
    }

    // --- Normalize probabilities ---
    std::vector<int> vec_Nodes;
    std::vector<double> vec_Probs;
    for (const auto& [i_Node, f_Prob] : map_ProbMap) {
        vec_Nodes.push_back(i_Node);
        vec_Probs.push_back(f_Prob);
    }

    double d_Total = std::accumulate(vec_Probs.begin(), vec_Probs.end(), 0.0);
    if (d_Total > 0.0) {
        for (auto& f_P : vec_Probs) f_P /= d_Total;
    } else {
        double d_Uniform = 1.0 / static_cast<double>(vec_Nodes.size());
        vec_Probs.assign(vec_Nodes.size(), d_Uniform);
    }

    // --- RNG setup ---
    std::random_device rd;
    std::mt19937 gen(rd());
    std::discrete_distribution<> disMrX(vec_Probs.begin(), vec_Probs.end());

    // --- Monte Carlo Simulation ---
    auto fn_SimulateOnce = [&](const PossibleMove& move) -> double {
        std::vector<int> vec_PolicePositions;
        std::vector<int> vec_PoliceTicketsMetro;
        std::vector<int> vec_PoliceTicketsTaxi;
        std::vector<int> vec_PoliceTicketsBus;

        for (const auto& info : gameState.vec_AllPlayers) {
            if (!info.b_IsMisterX) {
                vec_PolicePositions.push_back(info.i_Position);
                vec_PoliceTicketsMetro.push_back(info.i_MetroTickets);
                vec_PoliceTicketsTaxi.push_back(info.i_TaxiTickets);
                vec_PoliceTicketsBus.push_back(info.i_BusTickets);
            }
        }

        int i_PawnIndex = gameState.i_CurrentPlayerIndex;
        // for (size_t i = 0; i < gameState.vec_AllPlayers.size(); ++i) {
        //     if (gameState.i_CurrentPlayerIndex == p_Player->i_PlayerId) {
        //         i_PawnIndex = static_cast<int>(i);
        //         break;
        //     }
        // }

        // --- Initial move ---
        int i_DestNode = move.i_DestinationNode;
        int i_TransportType = move.i_TransportType;

        if (vec_PoliceTicketsMetro[i_PawnIndex] <= 0 && i_TransportType == 3)
            return 0.0;  // no ticket, invalid move

        if (vec_PoliceTicketsTaxi[i_PawnIndex] <= 0 && i_TransportType == 1)
            return 0.0;  // no ticket, invalid move

        if (vec_PoliceTicketsBus[i_PawnIndex] <= 0 && i_TransportType == 2)
            return 0.0;  // no ticket, invalid move

        vec_PolicePositions[i_PawnIndex] = i_DestNode;

        switch (i_TransportType) {
            case 1:  // Taxi
                vec_PoliceTicketsTaxi[i_PawnIndex] -= 1;
                break;

            case 2:  // Bus
                vec_PoliceTicketsBus[i_PawnIndex] -= 1;
                break;

            case 3:  // Metro
                vec_PoliceTicketsMetro[i_PawnIndex] -= 1;
                break;

            default:
                break;
        }

        int i_MrXPos = vec_Nodes[disMrX(gen)];
        if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) != vec_PolicePositions.end())
            return 1.0;

        double d_Value = 0.0;
        double d_Discount = 1.0;

        // --- Simulation loop ---
        for (int i_Depth = 0; i_Depth < i_kSimulationDepth; ++i_Depth) {
            // Mr. X random move
            auto vec_Connections = gameState.p_Graph->GetConnections(i_MrXPos);
            std::vector<PossibleMove> vec_MrXMoves;
            vec_MrXMoves.reserve(vec_Connections.size());
            for (const auto& conn : vec_Connections) {
                vec_MrXMoves.push_back({conn.i_NodeId, conn.i_TransportType});
            }
            if (!vec_MrXMoves.empty()) {
                std::uniform_int_distribution<> dis(0, static_cast<int>(vec_MrXMoves.size()) - 1);
                i_MrXPos = vec_MrXMoves[dis(gen)].i_DestinationNode;
            }

            // Police moves
        for (size_t i = 0; i < vec_PolicePositions.size(); ++i) {
            int i_Pos = vec_PolicePositions[i];
            auto& i_TicketsTaxi = vec_PoliceTicketsTaxi[i];
            auto& i_TicketsBus = vec_PoliceTicketsBus[i];
            auto& i_TicketsMetro = vec_PoliceTicketsMetro[i];

            auto vec_Connections = gameState.p_Graph->GetConnections(i_Pos);

            std::vector<PossibleMove> vec_Moves;
            vec_Moves.reserve(vec_Connections.size());

            for (const auto& conn : vec_Connections) {
                vec_Moves.push_back({conn.i_NodeId, conn.i_TransportType});
            }

            std::vector<PossibleMove> vec_ValidMoves;
            for (const auto& m : vec_Moves) {
                if (i_TicketsTaxi > 0 && m.i_TransportType == 1)
                    vec_ValidMoves.push_back(m);
                if (i_TicketsBus > 0 && m.i_TransportType == 2)
                    vec_ValidMoves.push_back(m);
                if (i_TicketsMetro > 0 && m.i_TransportType == 3)
                    vec_ValidMoves.push_back(m);
            }

            if (vec_ValidMoves.empty()) continue;

            std::uniform_real_distribution<> disChance(0.0, 1.0);
            PossibleMove chosenMove;

            if (disChance(gen) < 0.9) {
                chosenMove = *std::min_element(vec_ValidMoves.begin(), vec_ValidMoves.end(),
                    [&](const auto& a, const auto& b) {
                        return ShortestPathDistance(gameState.p_Graph, i_MrXPos, a.i_DestinationNode) <
                            ShortestPathDistance(gameState.p_Graph, i_MrXPos, b.i_DestinationNode);
                    });
            } else {
                std::uniform_int_distribution<> disRand(0, static_cast<int>(vec_ValidMoves.size()) - 1);
                chosenMove = vec_ValidMoves[disRand(gen)];
            }

            vec_PolicePositions[i] = chosenMove.i_DestinationNode;
            switch (chosenMove.i_TransportType) {
                case 1:  // Taxi
                    i_TicketsTaxi -= 1;
                    break;

                case 2:  // Bus
                    i_TicketsBus -= 1;
                    break;

                case 3:  // Metro
                    i_TicketsMetro -= 1;
                    break;

                default:
                    // Nieznany typ transportu — nic nie rób lub loguj błąd
                    break;
            }

        }


            // Capture check
            if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) != vec_PolicePositions.end()) {
                d_Value += d_Discount * 1.0;
                break;
            }

            d_Discount *= d_kCaptureDiscount;
        }

        return d_Value;
    };

    // --- Score calculation for all moves ---
    std::map<int, double> map_MoveScores;
    for (const auto& move : vec_PossibleMoves) {
        double d_Total = 0.0;
        for (int i = 0; i < i_kSimulationsPerOption; ++i) {
            d_Total += fn_SimulateOnce(move);
        }
        map_MoveScores[move.i_DestinationNode] = d_Total / static_cast<double>(i_kSimulationsPerOption);
    }

    // --- Select best move ---
    auto it_Best = std::max_element(map_MoveScores.begin(), map_MoveScores.end(),
        [](const auto& a, const auto& b) { return a.second < b.second; });

    if (it_Best != map_MoveScores.end()) {
        auto it_Move = std::find_if(vec_PossibleMoves.begin(), vec_PossibleMoves.end(),
            [&](const PossibleMove& m) { return m.i_DestinationNode == it_Best->first; });

        if (it_Move != vec_PossibleMoves.end()) {
            decision = { true, it_Move->i_DestinationNode, it_Move->i_TransportType };
        }
    } else {
        decision.b_HasDecision = false;
    }

    return decision;
}

struct BeliefCentroid
{
    double d_X = 0.0;
    double d_Y = 0.0;
    bool b_Valid = false;
};

struct FrontPlanEntry
{
    bool b_HasMove = false;
    PossibleMove move{};
    int i_TargetNode = -1;
    double d_TargetProbability = 0.0;
};

struct FrontPlanCache
{
    int i_Round = -1;
    int i_LastKnownRound = -1;
    int i_LastKnownPosition = -1;
    size_t i_DetectiveCount = 0U;
    bool b_MrXVisible = false;
    bool b_CentroidValid = false;
    std::pair<double, double> centroid{0.0, 0.0};
    std::map<int, double> mapBelief;
    std::map<int, FrontPlanEntry> mapEntries;
};

static std::mutex s_mtx_FrontPlan;
static std::map<int, int> s_mapFrontBlockedNodes;
static FrontPlanCache s_FrontPlanCache;

static void DecayFrontBlockedNodes(std::map<int, int>& mapNodes)
{
    for (auto it = mapNodes.begin(); it != mapNodes.end(); ) {
        if (it->second > 0) {
            --(it->second);
        }
        if (it->second <= 0) {
            it = mapNodes.erase(it);
        } else {
            ++it;
        }
    }
}

static void MarkFrontBlockedNode(std::map<int, int>& mapNodes, int i_Node)
{
    if (i_Node <= 0) {
        return;
    }
    constexpr int i_kMaxPenalty = 6;
    int& i_Entry = mapNodes[i_Node];
    i_Entry = std::min(i_Entry + 2, i_kMaxPenalty);
}

static std::unordered_map<int, int> ComputeBfsDistances(const GraphManager* p_Graph, int i_Start)
{
    std::unordered_map<int, int> mapDistances;
    if (!p_Graph || !p_Graph->IsValidNode(i_Start)) {
        return mapDistances;
    }

    std::queue<int> queueNodes;
    queueNodes.push(i_Start);
    mapDistances[i_Start] = 0;

    while (!queueNodes.empty()) {
        int i_Node = queueNodes.front();
        queueNodes.pop();
        int i_NextDist = mapDistances[i_Node] + 1;

        for (const auto& conn : p_Graph->GetConnections(i_Node)) {
            int i_Neighbor = conn.i_NodeId;
            if (!p_Graph->IsValidNode(i_Neighbor)) {
                continue;
            }
            if (mapDistances.find(i_Neighbor) != mapDistances.end()) {
                continue;
            }
            mapDistances[i_Neighbor] = i_NextDist;
            queueNodes.push(i_Neighbor);
        }
    }

    return mapDistances;
}

static std::vector<PossibleMove> ComputeLegalMovesForDetective(
    const PlayerInfo& info,
    const GameStateData& gameState,
    const std::set<int>& set_ForbiddenDestinations
)
{
    std::vector<PossibleMove> vec_Moves;
    const GraphManager* p_Graph = gameState.p_Graph;
    if (!p_Graph) {
        return vec_Moves;
    }

    for (const auto& conn : p_Graph->GetConnections(info.i_Position)) {
        int i_Dest = conn.i_NodeId;
        if (set_ForbiddenDestinations.count(i_Dest) != 0) {
            continue;
        }

        bool b_CanTravel = false;
        switch (conn.i_TransportType) {
            case Core::k_TransportTypeTaxi:
                b_CanTravel = info.i_TaxiTickets > 0;
                break;
            case Core::k_TransportTypeBus:
                b_CanTravel = info.i_BusTickets > 0;
                break;
            case Core::k_TransportTypeMetro:
                b_CanTravel = info.i_MetroTickets > 0;
                break;
            case Core::k_TransportTypeWater:
                b_CanTravel = info.i_BlackTickets > 0;
                break;
            default:
                b_CanTravel = false;
                break;
        }

        if (b_CanTravel) {
            vec_Moves.push_back({i_Dest, conn.i_TransportType});
        }
    }

    return vec_Moves;
}

static std::vector<int> SelectGuardNodes(
    const std::map<int, double>& map_Belief,
    const std::set<int>& set_UsedNodes,
    const GraphManager* p_Graph
)
{
    std::vector<int> vec_Result;
    std::set<int> set_Seen;
    std::set<int> set_Used = set_UsedNodes;

    std::set<int> set_FerryNodes;
    if (p_Graph) {
        for (const auto& [i_Node, _] : map_Belief) {
            bool b_HasWater = false;
            for (const auto& conn : p_Graph->GetConnections(i_Node)) {
                if (conn.i_TransportType == Core::k_TransportTypeWater) {
                    b_HasWater = true;
                    break;
                }
            }
            if (b_HasWater) {
                set_FerryNodes.insert(i_Node);
            }
        }
    }

    auto fn_AddCandidate = [&](int i_Node) {
        if (set_Used.count(i_Node) != 0) {
            return;
        }
        if (set_Seen.insert(i_Node).second) {
            vec_Result.push_back(i_Node);
        }
    };

    for (int i_Node : set_FerryNodes) {
        fn_AddCandidate(i_Node);
    }

    std::vector<std::pair<int, double>> vec_Sorted(map_Belief.begin(), map_Belief.end());
    std::sort(vec_Sorted.begin(), vec_Sorted.end(), [](const auto& a, const auto& b) {
        if (std::fabs(a.second - b.second) < 1e-9) {
            return a.first < b.first;
        }
        return a.second > b.second;
    });

    for (const auto& entry : vec_Sorted) {
        fn_AddCandidate(entry.first);
    }

    return vec_Result;
}

static BeliefCentroid ComputeBeliefCentroid(const std::map<int, double>& map_Belief, const GraphManager* p_Graph)
{
    BeliefCentroid result;
    if (!p_Graph || map_Belief.empty()) {
        return result;
    }

    double d_Total = 0.0;
    double d_SumX = 0.0;
    double d_SumY = 0.0;

    for (const auto& [i_Node, d_Prob] : map_Belief) {
        if (d_Prob <= 0.0) {
            continue;
        }
        const Node* p_Node = p_Graph->GetNode(i_Node);
        if (!p_Node) {
            continue;
        }
        d_Total += d_Prob;
        d_SumX += d_Prob * static_cast<double>(p_Node->i_X);
        d_SumY += d_Prob * static_cast<double>(p_Node->i_Y);
    }

    if (d_Total > 0.0) {
        result.d_X = d_SumX / d_Total;
        result.d_Y = d_SumY / d_Total;
        result.b_Valid = true;
    }

    return result;
}

static double DistanceToCentroid(int i_Node, const BeliefCentroid& centroid, const GraphManager* p_Graph)
{
    if (!centroid.b_Valid || !p_Graph) {
        return 0.0;
    }

    const Node* p_Node = p_Graph->GetNode(i_Node);
    if (!p_Node) {
        return 0.0;
    }

    double d_Dx = static_cast<double>(p_Node->i_X) - centroid.d_X;
    double d_Dy = static_cast<double>(p_Node->i_Y) - centroid.d_Y;
    return std::sqrt(d_Dx * d_Dx + d_Dy * d_Dy);
}

static std::optional<PossibleMove> SelectFallbackMove(
    const std::vector<PossibleMove>& vec_Moves,
    const std::map<int, double>& map_Belief,
    const BeliefCentroid& centroid,
    const GraphManager* p_Graph
)
{
    if (vec_Moves.empty()) {
        return std::nullopt;
    }

    constexpr double d_kEpsilon = 1e-6;
    std::optional<PossibleMove> opt_Best;
    double d_BestProb = -1.0;
    double d_BestDist = std::numeric_limits<double>::max();

    for (const auto& move : vec_Moves) {
        double d_Prob = 0.0;
        auto it = map_Belief.find(move.i_DestinationNode);
        if (it != map_Belief.end()) {
            d_Prob = it->second;
        }

        double d_Dist = DistanceToCentroid(move.i_DestinationNode, centroid, p_Graph);

        if (!opt_Best.has_value() || d_Prob > d_BestProb + d_kEpsilon ||
            (std::fabs(d_Prob - d_BestProb) <= d_kEpsilon && d_Dist < d_BestDist)) {
            opt_Best = move;
            d_BestProb = d_Prob;
            d_BestDist = d_Dist;
        }
    }

    return opt_Best;
}

static bool FindTicketAwarePath(
    const GraphManager* p_Graph,
    int i_Start,
    int i_Target,
    const TicketState& initialTickets,
    const std::set<int>& set_ForbiddenNodes,
    std::vector<int>& out_PathNodes,
    std::vector<int>& out_PathTransports,
    int i_MaxDepth = 6
)
{
    out_PathNodes.clear();
    out_PathTransports.clear();

    if (!p_Graph || !p_Graph->IsValidNode(i_Start) || !p_Graph->IsValidNode(i_Target)) {
        return false;
    }

    struct PathState {
        int i_Node;
        TicketState tickets;
        int i_ParentIndex;
        int i_UsedTransport;
        int i_Depth;
    };

    std::vector<PathState> vec_States;
    vec_States.push_back({i_Start, initialTickets, -1, 0, 0});
    std::queue<int> queue_Indices;
    queue_Indices.push(0);

    std::set<std::tuple<int, int, int, int, int>> set_Visited;
    set_Visited.insert(std::make_tuple(i_Start, initialTickets.taxi, initialTickets.bus, initialTickets.metro, initialTickets.black));

    while (!queue_Indices.empty()) {
        int i_Index = queue_Indices.front();
        queue_Indices.pop();
        const PathState& state = vec_States[i_Index];

        if (state.i_Node == i_Target) {
            std::vector<int> vec_ReversedNodes;
            std::vector<int> vec_ReversedTransports;
            int i_Cursor = i_Index;
            while (i_Cursor >= 0) {
                const PathState& nodeState = vec_States[i_Cursor];
                vec_ReversedNodes.push_back(nodeState.i_Node);
                vec_ReversedTransports.push_back(nodeState.i_UsedTransport);
                i_Cursor = nodeState.i_ParentIndex;
            }

            std::reverse(vec_ReversedNodes.begin(), vec_ReversedNodes.end());
            std::reverse(vec_ReversedTransports.begin(), vec_ReversedTransports.end());

            if (!vec_ReversedTransports.empty()) {
                vec_ReversedTransports.erase(vec_ReversedTransports.begin());
            }

            out_PathNodes = std::move(vec_ReversedNodes);
            out_PathTransports = std::move(vec_ReversedTransports);
            return true;
        }

        if (state.i_Depth >= i_MaxDepth) {
            continue;
        }

        for (const auto& conn : p_Graph->GetConnections(state.i_Node)) {
            int i_NextNode = conn.i_NodeId;
            if (set_ForbiddenNodes.count(i_NextNode) != 0 && i_NextNode != i_Target) {
                continue;
            }

            TicketState nextTickets = state.tickets;
            if (!ConsumeTicketForTransport(nextTickets, conn.i_TransportType)) {
                continue;
            }

            auto visitKey = std::make_tuple(
                i_NextNode,
                nextTickets.taxi,
                nextTickets.bus,
                nextTickets.metro,
                nextTickets.black
            );

            if (!set_Visited.insert(visitKey).second) {
                continue;
            }

            vec_States.push_back({i_NextNode, nextTickets, i_Index, conn.i_TransportType, state.i_Depth + 1});
            queue_Indices.push(static_cast<int>(vec_States.size()) - 1);
        }
    }

    return false;
}

static FrontPlanCache BuildFrontPlan(const GameStateData& gameState, std::map<int, int>& map_BlockedNodes)
{
    FrontPlanCache cache;
    cache.i_Round = gameState.i_CurrentRound;
    cache.i_LastKnownRound = gameState.i_MrXLastKnownRound;
    cache.i_LastKnownPosition = gameState.i_MrXLastKnownPosition;

    const GraphManager* p_Graph = gameState.p_Graph;
    if (!p_Graph) {
        return cache;
    }

    std::vector<int> vec_DetectiveIndices;
    std::set<int> set_OccupiedNodes;
    bool b_MrXVisibleNow = false;

    for (size_t i = 0; i < gameState.vec_AllPlayers.size(); ++i) {
        const PlayerInfo& info = gameState.vec_AllPlayers[i];
        if (info.b_IsMisterX) {
            b_MrXVisibleNow = info.b_IsVisible;
        } else {
            vec_DetectiveIndices.push_back(static_cast<int>(i));
            set_OccupiedNodes.insert(info.i_Position);
        }
    }

    cache.i_DetectiveCount = vec_DetectiveIndices.size();
    cache.b_MrXVisible = b_MrXVisibleNow;

    if (vec_DetectiveIndices.empty()) {
        return cache;
    }

    std::map<int, double> map_Belief = ComputeProbabilityMap(gameState, nullptr);
    if (map_Belief.empty()) {
        int i_NodeCount = p_Graph->GetNodeCount();
        if (i_NodeCount <= 0) {
            i_NodeCount = 1;
        }
        double d_Uniform = 1.0 / static_cast<double>(i_NodeCount);
        for (int i_Node = 1; i_Node <= i_NodeCount; ++i_Node) {
            map_Belief[i_Node] = d_Uniform;
        }
    }

    double d_Total = 0.0;
    for (auto& entry : map_Belief) {
        if (set_OccupiedNodes.count(entry.first) != 0) {
            entry.second = 0.0;
        } else {
            auto itBlocked = map_BlockedNodes.find(entry.first);
            if (itBlocked != map_BlockedNodes.end()) {
                double d_Penalty = 1.0 - std::min(0.6, 0.15 * static_cast<double>(itBlocked->second));
                entry.second *= std::max(0.0, d_Penalty);
            }
        }
        d_Total += entry.second;
    }

    if (d_Total <= 0.0) {
        map_Belief.clear();
        std::vector<int> vec_Candidates;
        int i_NodeCount = p_Graph->GetNodeCount();
        for (int i_Node = 1; i_Node <= i_NodeCount; ++i_Node) {
            if (set_OccupiedNodes.count(i_Node) == 0) {
                vec_Candidates.push_back(i_Node);
            }
        }

        if (vec_Candidates.empty()) {
            vec_Candidates.push_back(vec_DetectiveIndices.front() < static_cast<int>(gameState.vec_AllPlayers.size())
                                        ? gameState.vec_AllPlayers[vec_DetectiveIndices.front()].i_Position
                                        : 1);
        }

        double d_Uniform = 1.0 / static_cast<double>(vec_Candidates.size());
        for (int i_Node : vec_Candidates) {
            map_Belief[i_Node] = d_Uniform;
        }
    } else {
        for (auto& entry : map_Belief) {
            entry.second = (d_Total > 0.0) ? (entry.second / d_Total) : 0.0;
        }
    }

    cache.mapBelief = map_Belief;

    BeliefCentroid centroid = ComputeBeliefCentroid(map_Belief, p_Graph);
    cache.centroid = {centroid.d_X, centroid.d_Y};
    cache.b_CentroidValid = centroid.b_Valid;

    std::vector<std::pair<int, double>> vec_BeliefSorted;
    vec_BeliefSorted.reserve(map_Belief.size());
    for (const auto& entry : map_Belief) {
        if (entry.second > 0.0) {
            vec_BeliefSorted.push_back(entry);
        }
    }

    std::sort(vec_BeliefSorted.begin(), vec_BeliefSorted.end(), [](const auto& a, const auto& b) {
        if (std::fabs(a.second - b.second) < 1e-9) {
            return a.first < b.first;
        }
        return a.second > b.second;
    });

    size_t i_DetectiveCount = vec_DetectiveIndices.size();
    double d_Cumulative = 0.0;
    std::vector<std::pair<int, double>> vec_FrontNodes;
    for (const auto& entry : vec_BeliefSorted) {
        vec_FrontNodes.push_back(entry);
        d_Cumulative += entry.second;
        if (vec_FrontNodes.size() >= i_DetectiveCount || d_Cumulative >= 0.75) {
            break;
        }
    }

    std::unordered_map<int, std::unordered_map<int, int>> map_DistanceCache;
    for (int idx : vec_DetectiveIndices) {
        const PlayerInfo& info = gameState.vec_AllPlayers[idx];
        map_DistanceCache[idx] = ComputeBfsDistances(p_Graph, info.i_Position);
    }

    std::set<int> set_AvailableDetectives(vec_DetectiveIndices.begin(), vec_DetectiveIndices.end());
    std::map<int, int> map_Assignments;
    std::map<int, double> map_AssignmentProb;

    for (const auto& [i_Node, d_Prob] : vec_FrontNodes) {
        int i_SelectedDetective = -1;
        double d_BestScore = std::numeric_limits<double>::infinity();

        for (int idx : set_AvailableDetectives) {
            auto itDist = map_DistanceCache[idx].find(i_Node);
            if (itDist == map_DistanceCache[idx].end()) {
                continue;
            }

            double d_Score = static_cast<double>(itDist->second) - d_Prob * 2.0;
            if (d_Score < d_BestScore) {
                d_BestScore = d_Score;
                i_SelectedDetective = idx;
            }
        }

        if (i_SelectedDetective != -1) {
            map_Assignments[i_SelectedDetective] = i_Node;
            map_AssignmentProb[i_SelectedDetective] = d_Prob;
            set_AvailableDetectives.erase(i_SelectedDetective);
        }
    }

    std::set<int> set_UsedTargetNodes;
    for (const auto& entry : map_Assignments) {
        set_UsedTargetNodes.insert(entry.second);
    }

    std::vector<int> vec_GuardNodes = SelectGuardNodes(map_Belief, set_UsedTargetNodes, p_Graph);

    struct PlanningOrderEntry {
        int i_Index;
        double d_Primary;
        double d_Secondary;
    };

    std::vector<PlanningOrderEntry> vec_PlanningOrder;
    vec_PlanningOrder.reserve(vec_DetectiveIndices.size());
    for (int idx : vec_DetectiveIndices) {
        bool b_Assigned = map_Assignments.count(idx) > 0;
        double d_Primary = b_Assigned ? 0.0 : 1.0;
        double d_Secondary = b_Assigned ? -map_AssignmentProb[idx] : 0.0;
        vec_PlanningOrder.push_back({idx, d_Primary, d_Secondary});
    }

    std::sort(vec_PlanningOrder.begin(), vec_PlanningOrder.end(), [](const PlanningOrderEntry& a, const PlanningOrderEntry& b) {
        if (std::fabs(a.d_Primary - b.d_Primary) > 1e-6) {
            return a.d_Primary < b.d_Primary;
        }
        if (std::fabs(a.d_Secondary - b.d_Secondary) > 1e-6) {
            return a.d_Secondary < b.d_Secondary;
        }
        return a.i_Index < b.i_Index;
    });

    std::set<int> set_ReservedDestinations;
    std::set<int> set_CurrentOccupied = set_OccupiedNodes;

    auto fn_FetchGuardNode = [&]() -> int {
        while (!vec_GuardNodes.empty()) {
            int i_Candidate = vec_GuardNodes.front();
            vec_GuardNodes.erase(vec_GuardNodes.begin());
            if (set_CurrentOccupied.count(i_Candidate) != 0 || set_ReservedDestinations.count(i_Candidate) != 0) {
                continue;
            }
            return i_Candidate;
        }
        return -1;
    };

    for (const auto& orderEntry : vec_PlanningOrder) {
        int idx = orderEntry.i_Index;
        const PlayerInfo& info = gameState.vec_AllPlayers[idx];

        set_CurrentOccupied.erase(info.i_Position);

        int i_TargetNode = -1;
        if (map_Assignments.count(idx)) {
            i_TargetNode = map_Assignments[idx];
        } else {
            i_TargetNode = fn_FetchGuardNode();
        }

        std::set<int> set_Forbidden = set_CurrentOccupied;
        set_Forbidden.insert(set_ReservedDestinations.begin(), set_ReservedDestinations.end());
        std::vector<PossibleMove> vec_LegalMoves = ComputeLegalMovesForDetective(info, gameState, set_Forbidden);

        FrontPlanEntry entry;
        entry.i_TargetNode = i_TargetNode;
        entry.d_TargetProbability = (i_TargetNode >= 0 && map_Belief.count(i_TargetNode)) ? map_Belief[i_TargetNode] : 0.0;

        bool b_Planned = false;
        if (i_TargetNode > 0 && i_TargetNode != info.i_Position) {
            int i_Attempts = 0;
            int i_CurrentTarget = i_TargetNode;

            while (i_CurrentTarget > 0 && i_Attempts < 3 && !b_Planned) {
                std::vector<int> vec_PathNodes;
                std::vector<int> vec_PathTransports;
                TicketState tickets{
                    std::max(0, info.i_TaxiTickets),
                    std::max(0, info.i_BusTickets),
                    std::max(0, info.i_MetroTickets),
                    std::max(0, info.i_BlackTickets)
                };

                std::set<int> set_PathForbidden = set_Forbidden;
                if (FindTicketAwarePath(p_Graph, info.i_Position, i_CurrentTarget, tickets, set_PathForbidden, vec_PathNodes, vec_PathTransports)) {
                    if (vec_PathNodes.size() > 1 && !vec_PathTransports.empty()) {
                        entry.move.i_DestinationNode = vec_PathNodes[1];
                        entry.move.i_TransportType = vec_PathTransports[0];
                        entry.b_HasMove = true;
                        set_ReservedDestinations.insert(entry.move.i_DestinationNode);
                        set_CurrentOccupied.insert(entry.move.i_DestinationNode);
                        b_Planned = true;
                    }
                }

                if (!b_Planned) {
                    MarkFrontBlockedNode(map_BlockedNodes, i_CurrentTarget);
                    i_CurrentTarget = fn_FetchGuardNode();
                    ++i_Attempts;
                }
            }
        }

        if (!b_Planned) {
            std::optional<PossibleMove> opt_Fallback = SelectFallbackMove(vec_LegalMoves, map_Belief, centroid, p_Graph);
            if (opt_Fallback.has_value()) {
                entry.move = opt_Fallback.value();
                entry.b_HasMove = true;
                set_ReservedDestinations.insert(entry.move.i_DestinationNode);
                set_CurrentOccupied.insert(entry.move.i_DestinationNode);
            } else {
                set_CurrentOccupied.insert(info.i_Position);
            }
        }

        cache.mapEntries[idx] = entry;
    }

    return cache;
}

static MoveDecision FrontSearchEncirclementPoliceAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
)
{
    MoveDecision decision;
    decision.b_HasDecision = false;

    if (vec_PossibleMoves.empty()) {
        return decision;
    }

    const int i_PlayerIndex = gameState.i_CurrentPlayerIndex;
    if (i_PlayerIndex < 0 || i_PlayerIndex >= static_cast<int>(gameState.vec_AllPlayers.size())) {
        return GreedyShortestPathPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
    }

    bool b_ForceRebuild = false;
    constexpr int i_kMaxAttempts = 2;

    for (int i_Attempt = 0; i_Attempt < i_kMaxAttempts; ++i_Attempt) {
        FrontPlanCache cacheSnapshot;
        size_t i_DetectiveCount = 0;
        bool b_MrXVisibleNow = false;
        for (const auto& info : gameState.vec_AllPlayers) {
            if (info.b_IsMisterX) {
                b_MrXVisibleNow = info.b_IsVisible;
            } else {
                ++i_DetectiveCount;
            }
        }

        {
            std::lock_guard<std::mutex> lock(s_mtx_FrontPlan);

            bool b_NeedsRebuild = b_ForceRebuild ||
                s_FrontPlanCache.i_Round != gameState.i_CurrentRound ||
                s_FrontPlanCache.i_DetectiveCount != i_DetectiveCount ||
                s_FrontPlanCache.i_LastKnownRound != gameState.i_MrXLastKnownRound ||
                s_FrontPlanCache.i_LastKnownPosition != gameState.i_MrXLastKnownPosition ||
                s_FrontPlanCache.b_MrXVisible != b_MrXVisibleNow;

            if (b_NeedsRebuild) {
                DecayFrontBlockedNodes(s_mapFrontBlockedNodes);
                s_FrontPlanCache = BuildFrontPlan(gameState, s_mapFrontBlockedNodes);
            }

            cacheSnapshot = s_FrontPlanCache;
        }

        auto itEntry = cacheSnapshot.mapEntries.find(i_PlayerIndex);
        if (itEntry != cacheSnapshot.mapEntries.end() && itEntry->second.b_HasMove) {
            PossibleMove plannedMove = itEntry->second.move;

            const PossibleMove* p_MatchedMove = nullptr;
            for (const auto& move : vec_PossibleMoves) {
                if (move.i_DestinationNode == plannedMove.i_DestinationNode &&
                    move.i_TransportType == plannedMove.i_TransportType) {
                    p_MatchedMove = &move;
                    break;
                }
            }

            if (!p_MatchedMove) {
                for (const auto& move : vec_PossibleMoves) {
                    if (move.i_DestinationNode == plannedMove.i_DestinationNode) {
                        p_MatchedMove = &move;
                        plannedMove.i_TransportType = move.i_TransportType;
                        break;
                    }
                }
            }

            if (p_MatchedMove) {
                decision.b_HasDecision = true;
                decision.i_DestinationNode = p_MatchedMove->i_DestinationNode;
                decision.i_TransportType = p_MatchedMove->i_TransportType;
                return decision;
            }

            int i_TargetToBlock = itEntry->second.i_TargetNode;
            {
                std::lock_guard<std::mutex> lock(s_mtx_FrontPlan);
                MarkFrontBlockedNode(s_mapFrontBlockedNodes, i_TargetToBlock);
                s_FrontPlanCache.i_Round = -1;
            }

            b_ForceRebuild = true;
            continue;
        }

        BeliefCentroid centroid;
        centroid.d_X = cacheSnapshot.centroid.first;
        centroid.d_Y = cacheSnapshot.centroid.second;
        centroid.b_Valid = cacheSnapshot.b_CentroidValid;

        auto opt_Fallback = SelectFallbackMove(vec_PossibleMoves, cacheSnapshot.mapBelief, centroid, gameState.p_Graph);
        if (opt_Fallback.has_value()) {
            decision.b_HasDecision = true;
            decision.i_DestinationNode = opt_Fallback->i_DestinationNode;
            decision.i_TransportType = opt_Fallback->i_TransportType;
            return decision;
        }

        break;
    }

    return GreedyShortestPathPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
}

// === MINIMAX ALGORITHM FOR POLICE ===

static double EvaluateStateForMinimax(
    int i_MrXPos,
    const std::vector<int>& vec_PolicePositions,
    const std::vector<std::map<int, int>>& vec_PoliceTickets,
    const std::map<int, int>& map_MrXTickets,
    const GraphManager* p_Graph
) {
    // Distance average computation
    double d_DistSum = 0.0;
    int i_Count = 0;

    for (int i_PolicePos : vec_PolicePositions) {
        int i_Dist = GetCachedDistance(p_Graph, i_PolicePos, i_MrXPos);
        if (i_Dist >= 0) {
            d_DistSum += static_cast<double>(i_Dist);
            i_Count++;
        }
    }

    double d_AvgDist = (i_Count > 0) ? (d_DistSum / i_Count) : 50.0;

    // Clustering penalty - police too close to each other
    double d_ClusterPenalty = 0.0;
    for (size_t i = 0; i < vec_PolicePositions.size(); ++i) {
        for (size_t j = i + 1; j < vec_PolicePositions.size(); ++j) {
            int i_Dist = GetCachedDistance(p_Graph, vec_PolicePositions[i], vec_PolicePositions[j]);
            if (i_Dist >= 0 && i_Dist <= 2) {
                d_ClusterPenalty += (2.0 - i_Dist);
            }
        }
    }

    // Ticket advantage
    int i_PoliceTicketSum = 0;
    for (const auto& map_Tickets : vec_PoliceTickets) {
        for (const auto& [_, i_Count] : map_Tickets) {
            if (i_Count != std::numeric_limits<int>::max()) {
                i_PoliceTicketSum += i_Count;
            }
        }
    }

    int i_MrXTicketSum = 0;
    for (const auto& [_, i_Count] : map_MrXTickets) {
        if (i_Count != std::numeric_limits<int>::max()) {
            i_MrXTicketSum += i_Count;
        }
    }

    double d_TicketAdv = static_cast<double>(i_PoliceTicketSum - i_MrXTicketSum);

    // Lower score is better for police (negative of distance)
    return -(0.7 * d_AvgDist - 0.1 * d_TicketAdv + 0.3 * d_ClusterPenalty);
}

static std::vector<std::pair<int, int>> GetBestMovesForPolice(
    int i_PolicePos,
    int i_MrXPos,
    const std::map<int, int>& map_Tickets,
    const GraphManager* p_Graph,
    int i_N
) {
    std::vector<std::pair<int, int>> vec_Moves;
    auto vec_Connections = p_Graph->GetConnections(i_PolicePos);

    for (const auto& conn : vec_Connections) {
        auto it = map_Tickets.find(conn.i_TransportType);
        if (it != map_Tickets.end() && it->second > 0) {
            vec_Moves.push_back({conn.i_NodeId, conn.i_TransportType});
        }
    }

    if (vec_Moves.empty()) {
        return {{i_PolicePos, 0}};
    }

    // Score moves by distance to Mr. X
    std::vector<std::tuple<int, int, int>> vec_ScoredMoves;
    for (const auto& [i_Dest, i_Transport] : vec_Moves) {
        int i_Dist = GetCachedDistance(p_Graph, i_Dest, i_MrXPos);
        vec_ScoredMoves.push_back({i_Dest, i_Transport, i_Dist});
    }

    std::sort(vec_ScoredMoves.begin(), vec_ScoredMoves.end(),
              [](const auto& a, const auto& b) { return std::get<2>(a) < std::get<2>(b); });

    std::vector<std::pair<int, int>> vec_Result;
    int i_Limit = std::min(i_N, static_cast<int>(vec_ScoredMoves.size()));
    for (int i = 0; i < i_Limit; ++i) {
        vec_Result.push_back({std::get<0>(vec_ScoredMoves[i]), std::get<1>(vec_ScoredMoves[i])});
    }

    return vec_Result;
}

struct JointPoliceMove {
    std::vector<int> vec_Destinations;
    std::vector<int> vec_Transports;
    std::vector<int> vec_PoliceIndices;
};

static std::vector<JointPoliceMove> GenerateJointMoves(
    const std::vector<int>& vec_PolicePositions,
    const std::vector<std::map<int, int>>& vec_PoliceTickets,
    int i_MrXPos,
    const GraphManager* p_Graph,
    int i_N
) {
    std::vector<std::vector<std::tuple<int, int, int>>> vec_MovesPerPolice;

    for (size_t i = 0; i < vec_PolicePositions.size(); ++i) {
        auto vec_Moves = GetBestMovesForPolice(
            vec_PolicePositions[i], i_MrXPos, vec_PoliceTickets[i], p_Graph, i_N
        );

        std::vector<std::tuple<int, int, int>> vec_IndexedMoves;
        for (const auto& [i_Dest, i_Transport] : vec_Moves) {
            vec_IndexedMoves.push_back({i_Dest, i_Transport, static_cast<int>(i)});
        }
        vec_MovesPerPolice.push_back(vec_IndexedMoves);
    }

    // Generate all combinations
    std::vector<JointPoliceMove> vec_JointMoves;

    std::function<void(size_t, std::vector<int>&, std::vector<int>&, std::vector<int>&)> fn_Generate;
    fn_Generate = [&](size_t i_Depth, std::vector<int>& vec_Dests, 
                     std::vector<int>& vec_Trans, std::vector<int>& vec_Idx) {
        if (i_Depth == vec_MovesPerPolice.size()) {
            // Check for collisions
            std::set<int> set_UniquePos(vec_Dests.begin(), vec_Dests.end());
            if (set_UniquePos.size() == vec_Dests.size()) {
                // No collisions - valid joint move
                JointPoliceMove move;
                move.vec_Destinations = vec_Dests;
                move.vec_Transports = vec_Trans;
                move.vec_PoliceIndices = vec_Idx;
                vec_JointMoves.push_back(move);
            }
            return;
        }

        for (const auto& [i_Dest, i_Transport, i_Index] : vec_MovesPerPolice[i_Depth]) {
            // Check tickets
            auto it = vec_PoliceTickets[i_Depth].find(i_Transport);
            if (i_Transport != 0 && (it == vec_PoliceTickets[i_Depth].end() || it->second <= 0)) {
                continue;
            }

            vec_Dests.push_back(i_Dest);
            vec_Trans.push_back(i_Transport);
            vec_Idx.push_back(i_Index);

            fn_Generate(i_Depth + 1, vec_Dests, vec_Trans, vec_Idx);

            vec_Dests.pop_back();
            vec_Trans.pop_back();
            vec_Idx.pop_back();
        }
    };

    std::vector<int> vec_Dests, vec_Trans, vec_Idx;
    fn_Generate(0, vec_Dests, vec_Trans, vec_Idx);

    return vec_JointMoves;
}

static bool IsTerminalState(
    int i_MrXPos,
    const std::vector<int>& vec_PolicePositions,
    const std::map<int, int>& map_MrXTickets,
    const std::vector<std::map<int, int>>& vec_PoliceTickets,
    const GraphManager* p_Graph
) {
    // Mr. X caught
    if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) 
        != vec_PolicePositions.end()) {
        return true;
    }

    // Mr. X has no moves
    auto vec_Connections = p_Graph->GetConnections(i_MrXPos);
    bool b_HasMove = false;
    for (const auto& conn : vec_Connections) {
        auto it = map_MrXTickets.find(conn.i_TransportType);
        if (it != map_MrXTickets.end() && it->second > 0) {
            b_HasMove = true;
            break;
        }
    }
    if (!b_HasMove) {
        return true;
    }

    // Police have no moves (fast local check without generating joint combinations)
    bool b_PoliceHasAnyMove = false;
    for (size_t i = 0; i < vec_PolicePositions.size() && !b_PoliceHasAnyMove; ++i) {
        int i_Pos = vec_PolicePositions[i];
        const auto& map_Tickets = vec_PoliceTickets[i];
        auto vec_Conn = p_Graph->GetConnections(i_Pos);
        for (const auto& conn : vec_Conn) {
            auto it = map_Tickets.find(conn.i_TransportType);
            if (it != map_Tickets.end() && it->second > 0) {
                b_PoliceHasAnyMove = true;
                break;
            }
        }
    }
    if (!b_PoliceHasAnyMove) {
        return true;
    }

    return false;
}

static double MinimaxAlgorithmRecursive(
    int i_MrXPos,
    std::vector<int> vec_PolicePositions,
    std::map<int, int> map_MrXTickets,
    std::vector<std::map<int, int>> vec_PoliceTickets,
    int i_Depth,
    bool b_IsMrXTurn,
    double d_Alpha,
    double d_Beta,
    const GraphManager* p_Graph,
    const std::vector<std::tuple<int, int, double>>& vec_MrXPositionCandidates
) {
    // Terminal conditions
    if (i_Depth == 0 || IsTerminalState(i_MrXPos, vec_PolicePositions, map_MrXTickets, vec_PoliceTickets, p_Graph)) {
        return EvaluateStateForMinimax(i_MrXPos, vec_PolicePositions, vec_PoliceTickets, map_MrXTickets, p_Graph);
    }

    // Check if caught
    if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) 
        != vec_PolicePositions.end()) {
        return -100000.0;
    }

    if (b_IsMrXTurn) {
        // Mr. X turn - maximize
        double d_MaxEval = std::numeric_limits<double>::lowest();

        // Build a quick weight lookup from Mr X probability candidates (optional ordering heuristic)
        std::map<int, double> map_PosWeight;
        for (const auto& tup : vec_MrXPositionCandidates) {
            int i_Pos = std::get<0>(tup);
            double d_W = std::get<2>(tup);
            map_PosWeight[i_Pos] = d_W;
        }

        // Generate all legal neighbor moves for Mr X from the current position (filter by tickets)
        struct MrXMove { int dest; int transport; double weight; };
        std::vector<MrXMove> vec_LegalMoves;
        auto vec_Connections = p_Graph->GetConnections(i_MrXPos);
        vec_LegalMoves.reserve(vec_Connections.size());
        for (const auto& conn : vec_Connections) {
            auto itT = map_MrXTickets.find(conn.i_TransportType);
            if (itT != map_MrXTickets.end() && itT->second > 0) {
                double d_W = 0.0;
                auto itW = map_PosWeight.find(conn.i_NodeId);
                if (itW != map_PosWeight.end()) d_W = itW->second;
                vec_LegalMoves.push_back({conn.i_NodeId, conn.i_TransportType, d_W});
            }
        }

        // Order moves by descending probability weight to improve pruning (higher first)
        std::sort(vec_LegalMoves.begin(), vec_LegalMoves.end(), [](const MrXMove& a, const MrXMove& b){
            return a.weight > b.weight;
        });

        for (const auto& mv : vec_LegalMoves) {
            // Apply move
            std::map<int, int> map_NewTickets = map_MrXTickets;
            if (map_NewTickets[mv.transport] != std::numeric_limits<int>::max()) {
                map_NewTickets[mv.transport] -= 1;
            }

            double d_Eval = MinimaxAlgorithmRecursive(
                mv.dest, vec_PolicePositions, map_NewTickets, vec_PoliceTickets,
                i_Depth - 1, false, d_Alpha, d_Beta, p_Graph, vec_MrXPositionCandidates
            );

            d_MaxEval = std::max(d_MaxEval, d_Eval);
            d_Alpha = std::max(d_Alpha, d_Eval);

            if (d_Beta <= d_Alpha) {
                break; // Beta cutoff
            }
        }

        return (d_MaxEval != std::numeric_limits<double>::lowest()) ? d_MaxEval : -10000.0;

    } else {
        // Police turn - minimize
        double d_MinEval = std::numeric_limits<double>::max();

        auto vec_JointMoves = GenerateJointMoves(vec_PolicePositions, vec_PoliceTickets, i_MrXPos, p_Graph, 4);

        for (const auto& jointMove : vec_JointMoves) {
            // Apply joint move
            std::vector<int> vec_NewPositions = jointMove.vec_Destinations;
            std::vector<std::map<int, int>> vec_NewTickets = vec_PoliceTickets;

            bool b_Valid = true;
            for (size_t i = 0; i < jointMove.vec_PoliceIndices.size(); ++i) {
                int i_Idx = jointMove.vec_PoliceIndices[i];
                int i_Transport = jointMove.vec_Transports[i];

                if (i_Transport != 0) {
                    auto it = vec_NewTickets[i_Idx].find(i_Transport);
                    if (it == vec_NewTickets[i_Idx].end() || it->second <= 0) {
                        b_Valid = false;
                        break;
                    }
                    vec_NewTickets[i_Idx][i_Transport] -= 1;
                }
            }

            if (!b_Valid) {
                continue;
            }

            double d_Eval = MinimaxAlgorithmRecursive(
                i_MrXPos, vec_NewPositions, map_MrXTickets, vec_NewTickets,
                i_Depth - 1, true, d_Alpha, d_Beta, p_Graph, vec_MrXPositionCandidates
            );

            d_MinEval = std::min(d_MinEval, d_Eval);
            d_Beta = std::min(d_Beta, d_Eval);

            if (d_Beta <= d_Alpha) {
                break; // Alpha cutoff
            }
        }

        return (d_MinEval != std::numeric_limits<double>::max()) ? d_MinEval : 10000.0;
    }
}

static MoveDecision MinimaxPoliceAlgorithm(
    const Player* p_Player,
    const std::vector<PossibleMove>& vec_PossibleMoves,
    const GameStateData& gameState
) {
    MoveDecision decision;
    decision.b_HasDecision = false;

    if (vec_PossibleMoves.empty()) {
        return decision;
    }

    constexpr int i_kMaxDepth = 5;
    constexpr int i_kNBestPoliceMoves = 4;
    constexpr int i_kNBestMrXPositions = 4;

    const GraphManager* p_Graph = gameState.p_Graph;
    if (!p_Graph) {
        return decision;
    }

    // Get probability map for Mr. X positions
    std::map<int, double> map_ProbMap = ComputeProbabilityMap(gameState, p_Player);
    if (map_ProbMap.empty()) {
        map_ProbMap[gameState.i_MrXLastKnownPosition > 0 ? gameState.i_MrXLastKnownPosition : 1] = 1.0;
    }

    // Sort by probability and take top N
    std::vector<std::tuple<int, int, double>> vec_MrXPositions;
    for (const auto& [i_Node, f_Prob] : map_ProbMap) {
        vec_MrXPositions.push_back({i_Node, 0, f_Prob});
    }

    std::sort(vec_MrXPositions.begin(), vec_MrXPositions.end(),
              [](const auto& a, const auto& b) { return std::get<2>(a) > std::get<2>(b); });

    if (vec_MrXPositions.size() > i_kNBestMrXPositions) {
        vec_MrXPositions.resize(i_kNBestMrXPositions);
    }

    // Get police info
    std::vector<int> vec_PolicePositions;
    std::vector<std::map<int, int>> vec_PoliceTickets;

    for (const auto& info : gameState.vec_AllPlayers) {
        if (!info.b_IsMisterX) {
            vec_PolicePositions.push_back(info.i_Position);

            std::map<int, int> map_Tickets;
            map_Tickets[Core::k_TransportTypeTaxi] = info.i_TaxiTickets;
            map_Tickets[Core::k_TransportTypeBus] = info.i_BusTickets;
            map_Tickets[Core::k_TransportTypeMetro] = info.i_MetroTickets;
            vec_PoliceTickets.push_back(map_Tickets);
        }
    }

    // Get Mr. X tickets (estimated)
    std::map<int, int> map_MrXTickets;
    const PlayerInfo* p_MrXInfo = nullptr;
    for (const auto& info : gameState.vec_AllPlayers) {
        if (info.b_IsMisterX) {
            p_MrXInfo = &info;
            break;
        }
    }

    if (p_MrXInfo) {
        map_MrXTickets[Core::k_TransportTypeTaxi] = p_MrXInfo->i_TaxiTickets;
        map_MrXTickets[Core::k_TransportTypeBus] = p_MrXInfo->i_BusTickets;
        map_MrXTickets[Core::k_TransportTypeMetro] = p_MrXInfo->i_MetroTickets;
        map_MrXTickets[Core::k_TransportTypeWater] = p_MrXInfo->i_BlackTickets;
    } else {
        // Default values if no info
        map_MrXTickets[Core::k_TransportTypeTaxi] = 10;
        map_MrXTickets[Core::k_TransportTypeBus] = 10;
        map_MrXTickets[Core::k_TransportTypeMetro] = 10;
        map_MrXTickets[Core::k_TransportTypeWater] = 3;
    }

    int i_CurrentPlayerPos = gameState.vec_AllPlayers[gameState.i_CurrentPlayerIndex].i_Position;

    // Find current player index
    int i_PlayerIndex = -1;
    for (size_t i = 0; i < gameState.vec_AllPlayers.size(); ++i) {
        if (!gameState.vec_AllPlayers[i].b_IsMisterX) {
            if (gameState.vec_AllPlayers[i].i_Position == i_CurrentPlayerPos) {
                i_PlayerIndex = static_cast<int>(i) - 1; // Subtract 1 because Mr. X is at index 0
                for (size_t j = 0; j <= i; ++j) {
                    if (gameState.vec_AllPlayers[j].b_IsMisterX) {
                        i_PlayerIndex++;
                        break;
                    }
                }
                break;
            }
        }
    }

    if (i_PlayerIndex < 0 || i_PlayerIndex >= static_cast<int>(vec_PolicePositions.size())) {
        // Fallback: find by position matching
        for (size_t i = 0; i < vec_PolicePositions.size(); ++i) {
            if (vec_PolicePositions[i] == i_CurrentPlayerPos) {
                i_PlayerIndex = static_cast<int>(i);
                break;
            }
        }
    }

    if (i_PlayerIndex < 0) {
        // Ultimate fallback - random move
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, static_cast<int>(vec_PossibleMoves.size()) - 1);
        const auto& move = vec_PossibleMoves[dis(gen)];
        decision = { true, move.i_DestinationNode, move.i_TransportType };
        return decision;
    }

    // Evaluate each possible move
    double d_BestScore = std::numeric_limits<double>::infinity();
    PossibleMove bestMove = vec_PossibleMoves[0];

    for (const auto& move : vec_PossibleMoves) {
        // Apply move temporarily
        int i_PrevPos = vec_PolicePositions[i_PlayerIndex];
        std::map<int, int> map_PrevTickets = vec_PoliceTickets[i_PlayerIndex];

        vec_PolicePositions[i_PlayerIndex] = move.i_DestinationNode;

        if (vec_PoliceTickets[i_PlayerIndex].count(move.i_TransportType) && 
            vec_PoliceTickets[i_PlayerIndex][move.i_TransportType] > 0) {
            vec_PoliceTickets[i_PlayerIndex][move.i_TransportType] -= 1;
        }

        // Evaluate across all probable Mr. X positions
        double d_TotalScore = 0.0;
        for (const auto& [i_MrXPos, _, f_Weight] : vec_MrXPositions) {
            double d_Score = MinimaxAlgorithmRecursive(
                i_MrXPos, vec_PolicePositions, map_MrXTickets, vec_PoliceTickets,
                i_kMaxDepth, true,
                std::numeric_limits<double>::lowest(), std::numeric_limits<double>::max(),
                p_Graph, vec_MrXPositions
            );

            d_TotalScore += f_Weight * d_Score;
        }

        // Restore position
        vec_PolicePositions[i_PlayerIndex] = i_PrevPos;
        vec_PoliceTickets[i_PlayerIndex] = map_PrevTickets;

        if (d_TotalScore < d_BestScore) {
            d_BestScore = d_TotalScore;
            bestMove = move;
        }
    }

    decision.b_HasDecision = true;
    decision.i_DestinationNode = bestMove.i_DestinationNode;
    decision.i_TransportType = bestMove.i_TransportType;

    return decision;
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
        case Core::AIAlgorithm::DistanceMaximizationMrX:
            //decision = DistanceMaximizationAlgorithm(p_Player, vec_PossibleMoves, gameState);
            decision = ExternalPythonAlgorithmmrX(p_Player, vec_PossibleMoves, gameState);
            // todo trzeba co rundę wysyłać algorytmowi nagrodę za ruch i dawać mu znać (done) że gra się skończyła żeby na zdobytych danych się nauczył
            // trzeba też napisać skrypt który będzie uruchamiał wiele gier pod rząd żeby zebrać dane do nauki 
            // oraz napisać kod RL dla policjantów ale na innym porcie żeby nie było sprzeczności w danych
            break;
        case Core::AIAlgorithm::DecoyMovementMrX:
            decision = DecoyMovementAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::MonteCarloMrX:
            decision = MonteCarloMrXAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::DFSMrX:
            decision = DFSMrXAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::MonteCarloPolice:
            decision = MonteCarloPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::MinimaxPolice:
            decision = MinimaxPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::GreedyShortestPathPolice:
            decision = GreedyShortestPathPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::FrontSearchEncirclementPolice:
            decision = FrontSearchEncirclementPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        
        case Core::AIAlgorithm::NeuralNet:
        case Core::AIAlgorithm::PPOMrX:
        case Core::AIAlgorithm::MAPPOMrX:
            decision = ExternalPythonAlgorithmmrX(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::DiscreteSACMrX:
            decision = ExternalPythonAlgorithmmrX(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::NeuralNetPolice:
        case Core::AIAlgorithm::PPOPolice:
        case Core::AIAlgorithm::MAPPOPolice:
        case Core::AIAlgorithm::DiscreteSACPolice:
            decision = ExternalPythonAlgorithmPolice(p_Player, vec_PossibleMoves, gameState);
            break;
        
    }
    return decision;
}

} // namespace Core
} // namespace ScotlandYard
