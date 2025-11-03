#include "PlayerController.h"
#include "Player.h"
#include "Application.h"
#include "GameSettings.h"
#include <iostream>
#include <random>
#include "../../Graphs/graph_manage.h"
#include "GameConstants.h"
#include <queue>
#include <set>
#include <map>
#include <algorithm>            
#include <vector>      
#include <numeric>             
#include <utility>           
#include <cmath> 
#include <tuple>

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

    auto map_PredictPolicePositions = [&](int turns_ahead = 1) {
        std::map<int, std::set<int>> map_predicted;
        std::set<int> set_s0(vec_police_positions.begin(), vec_police_positions.end());
        map_predicted[0] = set_s0;

        for (int t = 1; t <= turns_ahead; ++t) {
            std::set<int> set_nextset;
            for (int police_pos : vec_police_positions) {
                std::set<int> set_frontier;
                set_frontier.insert(police_pos);
                for (int step = 0; step < t; ++step) {
                    std::set<int> set_new_front;
                    for (int pos : set_frontier) {
                        auto it = map_Graph.find(pos);
                        if (it == map_Graph.end()) continue;
                        for (const auto& nb : it->second) set_new_front.insert(nb.first);
                    }
                    if (!set_new_front.empty()) set_frontier.swap(set_new_front);
                }
                set_nextset.insert(set_frontier.begin(), set_frontier.end());
            }
            map_predicted[t] = std::move(set_nextset);
        }
        return map_predicted;
    };

    std::map<int, std::set<int>> predicted = map_PredictPolicePositions();

    auto fn_SafetyRisk = [&](int node) {
        int i_MinDist = 999999;
        for (const auto& map_Dist : vec_PoliceDists) {
            auto it = map_Dist.find(node);
            if (it != map_Dist.end()) {
                i_MinDist = std::min(i_MinDist, it->second);
            }
        }
        if (i_MinDist == 999999) i_MinDist = 8;
        int i_degree = static_cast<int>(map_Graph[node].size());
        return 1.0 / (i_MinDist + 1e-6) + 0.8 / std::max(1, i_degree);
    };

    auto fn_DeceptionScore = [&](int dest) {
        double d_f_Score = 0.0;
        int i_currentPos = p_Player ? p_Player->GetOccupiedNode() : 0;
        int i_lastKnown = gameState.i_MrXLastKnownPosition;

        if (i_lastKnown != 0 && i_lastKnown != i_currentPos && i_lastKnown != dest) {
            auto map_DistPrev = fn_BFS(i_lastKnown);
            int i_dDest = map_DistPrev.count(dest) ? map_DistPrev[dest] : 999999;
            int i_dCurr = map_DistPrev.count(i_currentPos) ? map_DistPrev[i_currentPos] : 999999;
            if (i_dDest > i_dCurr) {
                d_f_Score += 0.6;
            }
        }

        if (i_lastKnown != 0) {
            auto map_DistLast = fn_BFS(i_lastKnown);
            int i_Dist = map_DistLast.count(dest) ? map_DistLast[dest] : 999999;
            if (i_Dist != 999999) {
                d_f_Score += 0.5 / (i_Dist + 1.0);
            }
        }

        if (predicted.count(1) && !predicted[1].empty()) {
            int i_min_pred = 999999;
            for (int i_p : predicted[1]) {
                auto distMap = fn_BFS(i_p);
                auto it = distMap.find(dest);
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

static bool ConsumeTicketForTransport(TicketState& tickets, int transportType)
{
    switch (transportType) {
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
    int startNode,
    int turnsSinceReveal,
    const PlayerInfo& mrXInfo
)
{
    std::set<int> reachable;
    if (!p_Graph || !p_Graph->IsValidNode(startNode)) {
        return reachable;
    }

    turnsSinceReveal = std::max(0, turnsSinceReveal);

    TicketState initialTickets{
        std::max(0, mrXInfo.i_TaxiTickets),
        std::max(0, mrXInfo.i_BusTickets),
        std::max(0, mrXInfo.i_MetroTickets),
        std::max(0, mrXInfo.i_BlackTickets)
    };

    struct State {
        int node;
        int depth;
        TicketState tickets;
    };

    std::queue<State> queueStates;
    std::set<std::tuple<int, int, int, int, int>> visited;

    reachable.insert(startNode);
    queueStates.push({startNode, 0, initialTickets});
    visited.insert(std::make_tuple(startNode, initialTickets.taxi, initialTickets.bus, initialTickets.metro, initialTickets.black));

    while (!queueStates.empty()) {
        State state = queueStates.front();
        queueStates.pop();

        if (state.depth >= turnsSinceReveal) {
            continue;
        }

        auto connections = p_Graph->GetConnections(state.node);
        for (const auto& conn : connections) {
            TicketState nextTickets = state.tickets;
            if (!ConsumeTicketForTransport(nextTickets, conn.i_TransportType)) {
                continue;
            }

            int nextNode = conn.i_NodeId;
            int nextDepth = state.depth + 1;
            reachable.insert(nextNode);

            auto key = std::make_tuple(nextNode, nextTickets.taxi, nextTickets.bus, nextTickets.metro, nextTickets.black);
            if (visited.insert(key).second) {
                queueStates.push({nextNode, nextDepth, nextTickets});
            }
        }
    }

    if (reachable.empty()) {
        reachable.insert(startNode);
    }
    return reachable;
}

static int ShortestPathDistance(const GraphManager* p_Graph, int startNode, int goalNode)
{
    if (!p_Graph || !p_Graph->IsValidNode(startNode) || !p_Graph->IsValidNode(goalNode)) {
        return -1;
    }

    if (startNode == goalNode) {
        return 0;
    }

    int nodeCount = p_Graph->GetNodeCount();
    std::vector<int> dist(nodeCount + 1, -1);
    std::queue<int> queueNodes;
    dist[startNode] = 0;
    queueNodes.push(startNode);

    while (!queueNodes.empty()) {
        int node = queueNodes.front();
        queueNodes.pop();
        int nextDist = dist[node] + 1;

        auto connections = p_Graph->GetConnections(node);
        for (const auto& conn : connections) {
            int neighbor = conn.i_NodeId;
            if (!p_Graph->IsValidNode(neighbor)) {
                continue;
            }
            if (dist[neighbor] != -1) {
                continue;
            }
            dist[neighbor] = nextDist;
            if (neighbor == goalNode) {
                return dist[neighbor];
            }
            queueNodes.push(neighbor);
        }
    }

    return dist[goalNode];
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

    int lastKnownPos = gameState.i_MrXLastKnownPosition;
    int lastKnownRound = gameState.i_MrXLastKnownRound;

    if (lastKnownPos <= 0 || lastKnownRound < 0) {
        int nodeCount = p_Graph->GetNodeCount();
        if (nodeCount <= 0) {
            return probabilityMap;
        }
        double uniformProb = 1.0 / static_cast<double>(nodeCount);
        for (int node = 1; node <= nodeCount; ++node) {
            probabilityMap[node] = uniformProb;
        }
        return probabilityMap;
    }

    int turnsSinceReveal = std::max(0, gameState.i_CurrentRound - lastKnownRound);
    std::set<int> reachable = GenerateReachableNodes(p_Graph, lastKnownPos, turnsSinceReveal, *p_MrXInfo);

    if (reachable.empty()) {
        reachable.insert(lastKnownPos);
    }

    double baseProb = reachable.empty() ? 0.0 : 1.0 / static_cast<double>(reachable.size());
    for (int node : reachable) {
        probabilityMap[node] = baseProb;
    }

    constexpr double kAlpha = 0.15;
    for (auto& entry : probabilityMap) {
        int node = entry.first;
        double& probRef = entry.second;

        int minDistance = std::numeric_limits<int>::max();
        for (const auto* p_Police : vec_PoliceInfos) {
            int dist = ShortestPathDistance(p_Graph, p_Police->i_Position, node);
            if (dist >= 0) {
                minDistance = std::min(minDistance, dist);
            }
        }

        if (minDistance != std::numeric_limits<int>::max()) {
            probRef *= (1.0 + kAlpha * static_cast<double>(minDistance));
        }
    }

    double totalProb = 0.0;
    for (const auto& entry : probabilityMap) {
        totalProb += entry.second;
    }

    if (totalProb > 0.0) {
        for (auto& entry : probabilityMap) {
            entry.second /= totalProb;
        }
    }

    return probabilityMap;
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
    constexpr double f_kCaptureDiscount = 0.9;

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

    double f_Total = std::accumulate(vec_Probs.begin(), vec_Probs.end(), 0.0);
    if (f_Total > 0.0) {
        for (auto& f_P : vec_Probs) f_P /= f_Total;
    } else {
        double f_Uniform = 1.0 / static_cast<double>(vec_Nodes.size());
        vec_Probs.assign(vec_Nodes.size(), f_Uniform);
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

        double f_Value = 0.0;
        double f_Discount = 1.0;

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
            auto& map_TicketsTaxi = vec_PoliceTicketsTaxi[i];  // jeden nie wspólny słownik
            auto& map_TicketsBus = vec_PoliceTicketsBus[i];
            auto& map_TicketsMetro = vec_PoliceTicketsMetro[i];

            auto vec_Connections = gameState.p_Graph->GetConnections(gameState.vec_AllPlayers[i].i_Position);

            std::vector<PossibleMove> vec_Moves;
            vec_Moves.reserve(vec_Connections.size());

            for (const auto& conn : vec_Connections) {
                vec_Moves.push_back({conn.i_NodeId, conn.i_TransportType});
            }

            std::vector<PossibleMove> vec_ValidMoves;
            for (const auto& m : vec_Moves) {
                if (map_TicketsTaxi > 0 && i_TransportType == 1)
                    vec_ValidMoves.push_back(m);
                if (map_TicketsBus > 0 && i_TransportType == 2)
                    vec_ValidMoves.push_back(m);
                if (map_TicketsMetro > 0 && i_TransportType == 3)
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
                    map_TicketsTaxi -= 1;
                    break;

                case 2:  // Bus
                    map_TicketsBus -= 1;
                    break;

                case 3:  // Metro
                    map_TicketsMetro -= 1;
                    break;

                default:
                    // Nieznany typ transportu — nic nie rób lub loguj błąd
                    break;
            }

        }


            // Capture check
            if (std::find(vec_PolicePositions.begin(), vec_PolicePositions.end(), i_MrXPos) != vec_PolicePositions.end()) {
                f_Value += f_Discount * 1.0;
                break;
            }

            f_Discount *= f_kCaptureDiscount;
        }

        return f_Value;
    };

    // --- Score calculation for all moves ---
    std::map<int, double> map_MoveScores;
    for (const auto& move : vec_PossibleMoves) {
        double f_Total = 0.0;
        for (int i = 0; i < i_kSimulationsPerOption; ++i) {
            f_Total += fn_SimulateOnce(move);
        }
        map_MoveScores[move.i_DestinationNode] = f_Total / static_cast<double>(i_kSimulationsPerOption);
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
            //[[fallthrough]];
            decision = DecoyMovementAlgorithm(p_Player, vec_PossibleMoves, gameState);
            break;
        case Core::AIAlgorithm::NeuralNet:
            // TODO implementing algorithm here, rn fallback to Random
            {
                decision = MonteCarloPoliceAlgorithm(p_Player, vec_PossibleMoves, gameState);
                break;
            }
            break;
    }
    return decision;
}

} // namespace Core
} // namespace ScotlandYard
