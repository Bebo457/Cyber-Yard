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
    constexpr double f_kSurvivalDiscount = 0.95;

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
        double f_Value = 0.0;
        double f_Discount = 1.0;

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
                f_Value -= 1000.0 * f_Discount;
                return f_Value;  // game over - caught
            }

            //MrX move
            auto vec_MrMoves = fn_GetMovesForPosition(i_MrXPos, map_MrXTickets);

            if (vec_MrMoves.empty()) {
                f_Value -= 1000.0 * f_Discount;
                return f_Value;  //nie ma drogi, wszędzie policja
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

                    double f_Score = i_MinDist * 10.0 + i_Connectivity;
                    vec_MoveScores.push_back({f_Score, i_Neighbor, i_Trans});
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
                f_Value -= 1000.0 * f_Discount;
                return f_Value;
            }

            //score turn
            int i_MinDistance = std::numeric_limits<int>::max();
            int i_TotalDistance = 0;
            for (int i_PolicePos : vec_PolicePositions) {
                int i_Dist = fn_GetDistance(i_MrXPos, i_PolicePos);
                i_MinDistance = std::min(i_MinDistance, i_Dist);
                i_TotalDistance += i_Dist;
            }

            double f_AvgDistance = static_cast<double>(i_TotalDistance) / vec_PolicePositions.size();
            double f_TurnScore = i_MinDistance * 3.0 + f_AvgDistance * 0.5;
            f_Value += f_TurnScore * f_Discount;

            f_Discount *= f_kSurvivalDiscount;
        }

        //survived? - get your bonus:)
        int i_FinalMinDist = std::numeric_limits<int>::max();
        for (int i_PolicePos : vec_PolicePositions) {
            i_FinalMinDist = std::min(i_FinalMinDist, fn_GetDistance(i_MrXPos, i_PolicePos));
        }
        f_Value += 100.0 + i_FinalMinDist * 5.0;

        return f_Value;
    };

    //evaluation of all moves
    std::map<PossibleMove, double, decltype([](const PossibleMove& a, const PossibleMove& b) {
        return a.i_DestinationNode < b.i_DestinationNode || 
               (a.i_DestinationNode == b.i_DestinationNode && a.i_TransportType < b.i_TransportType);
    })> map_MoveScores;

    for (const auto& move : vec_PossibleMoves) {
        double f_TotalScore = 0.0;
        int i_Wins = 0;

        for (int i_Sim = 0; i_Sim < i_kSimulationsPerMove; ++i_Sim) {
            double f_Score = fn_SimulateOnce(move);
            f_TotalScore += f_Score;

            if (f_Score > 0.0) {
                i_Wins++;
            }
        }

        double f_AvgScore = f_TotalScore / i_kSimulationsPerMove;
        map_MoveScores[move] = f_AvgScore;
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

    // === HELPER FUNCTIONS ===

    // Get neighbors with caching
    auto fn_GetNeighbors = [&](int i_Node) -> std::vector<std::pair<int, int>> {
        if (s_NeighborsCache.count(i_Node)) {
            return s_NeighborsCache[i_Node];
        }

        std::vector<std::pair<int, int>> vec_Neighbors;
        if (map_Graph.count(i_Node)) {
            vec_Neighbors = map_Graph[i_Node];
        }

        s_NeighborsCache[i_Node] = vec_Neighbors;
        return vec_Neighbors;
    };

    // BFS distances with caching
    auto fn_BFSDistances = [&](int i_Start) -> std::map<int, int> {
        if (s_DistanceCache.count(i_Start)) {
            return s_DistanceCache[i_Start];
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

        s_DistanceCache[i_Start] = map_Distances;
        local_DistanceCache[i_Start] = map_Distances;
        return map_Distances;
    };

    // Get connectivity with caching
    auto fn_GetConnectivity = [&](int i_Node) -> int {
        if (s_ConnectivityCache.count(i_Node)) {
            return s_ConnectivityCache[i_Node];
        }

        int i_Connectivity = static_cast<int>(fn_GetNeighbors(i_Node).size());
        s_ConnectivityCache[i_Node] = i_Connectivity;
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
        
        if (s_ReachableCache.count(cache_Key)) {
            return s_ReachableCache[cache_Key];
        }

        std::map<int, int> map_Reachable;
        map_Reachable[i_Start] = 0;

        struct QueueItem {
            int pos;
            int steps;
            std::map<int, int> tickets;
        };

        std::queue<QueueItem> queue_Items;
        queue_Items.push({i_Start, 0, map_Tickets});

        while (!queue_Items.empty()) {
            auto item = queue_Items.front();
            queue_Items.pop();

            if (item.steps >= i_MaxSteps) {
                continue;
            }

            auto vec_Neighbors = fn_GetNeighbors(item.pos);
            for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
                auto it = item.tickets.find(i_Transport);
                if (it == item.tickets.end() || it->second <= 0) {
                    continue;
                }

                int i_NewSteps = item.steps + 1;

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

        s_ReachableCache[cache_Key] = map_Reachable;
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
            double f_BestScore = std::numeric_limits<double>::lowest();

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

                double f_Score = 0.0;

                int i_DistToMrX = map_MrXDistances.count(i_Neighbor) ? 
                                  map_MrXDistances[i_Neighbor] : 999;
                f_Score -= i_DistToMrX * 100.0;

                int i_Connectivity = fn_GetConnectivity(i_Neighbor);
                f_Score += i_Connectivity * 10.0;

                if (map_MrXDistances.count(i_Neighbor) && map_MrXDistances[i_Neighbor] == 1) {
                    f_Score += 500.0;
                }

                if (f_Score > f_BestScore) {
                    f_BestScore = f_Score;
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

        if (s_LastPolicePositions.empty() || s_LastPolicePositions == vec_CurrentPositions) {
            s_LastPolicePositions = vec_CurrentPositions;
            return fn_SimulateDetectiveMovesAdvanced(vec_Police, i_MrXPosition);
        }

        auto cache_Key = std::make_tuple(i_MrXPosition, 
                                         std::accumulate(vec_CurrentPositions.begin(), 
                                                        vec_CurrentPositions.end(), 0));

        if (s_PolicePredictions.count(cache_Key)) {
            s_LastPolicePositions = vec_CurrentPositions;
            return s_PolicePredictions[cache_Key];
        }

        s_LastPolicePositions = vec_CurrentPositions;
        auto vec_NewPredictions = fn_SimulateDetectiveMovesAdvanced(vec_Police, i_MrXPosition);
        s_PolicePredictions[cache_Key] = vec_NewPredictions;

        return vec_NewPredictions;
    };

    // Comprehensive position evaluation
    auto fn_EvaluatePositionComprehensive = [&](int i_Position, 
                                                const std::map<int, int>& map_Tickets,
                                                const std::vector<const PlayerInfo*>& vec_Police) 
        -> double {
        
        double f_Score = 0.0;

        // Distance calculations
        std::vector<int> vec_DistancesToPolice;
        for (const auto* p : vec_Police) {
            auto map_Dist = fn_BFSDistances(p->i_Position);
            int i_Dist = map_Dist.count(i_Position) ? map_Dist[i_Position] : 999;
            vec_DistancesToPolice.push_back(i_Dist);
        }

        int i_MinDistance = *std::min_element(vec_DistancesToPolice.begin(), 
                                             vec_DistancesToPolice.end());
        double f_AvgDistance = std::accumulate(vec_DistancesToPolice.begin(), 
                                              vec_DistancesToPolice.end(), 0.0) / 
                              vec_DistancesToPolice.size();

        // Distance scoring
        if (i_MinDistance <= 1) {
            f_Score -= 50000.0;
        } else if (i_MinDistance <= 2) {
            f_Score -= 10000.0;
        } else if (i_MinDistance <= 3) {
            f_Score -= 2000.0;
        }

        f_Score += i_MinDistance * 500.0;
        f_Score += f_AvgDistance * 100.0;

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

        f_Score += i_AvailableMoves * 150.0;
        f_Score += set_AvailableTransports.size() * 100.0;

        // Secondary mobility
        double f_SecondaryMobility = 0.0;
        for (const auto& [i_Neighbor, i_Transport] : vec_Neighbors) {
            auto it = map_Tickets.find(i_Transport);
            if (it != map_Tickets.end() && it->second > 0) {
                f_SecondaryMobility += fn_GetConnectivity(i_Neighbor);
            }
        }

        double f_AvgSecondary = vec_Neighbors.empty() ? 0.0 : 
                                f_SecondaryMobility / vec_Neighbors.size();
        f_Score += f_AvgSecondary * 30.0;

        // Strategic positioning
        if (i_AvailableMoves <= 2) {
            f_Score -= 500.0;
        } else if (i_AvailableMoves >= 5) {
            f_Score += 300.0;
        }

        // Police vicinity
        int i_PoliceInVicinity = std::count_if(vec_DistancesToPolice.begin(),
                                               vec_DistancesToPolice.end(),
                                               [](int d) { return d <= 3; });
        if (i_PoliceInVicinity >= 3) {
            f_Score -= 2000.0;
        } else if (i_PoliceInVicinity >= 2) {
            f_Score -= 800.0;
        }

        // Ticket management
        int i_TotalTickets = 0;
        for (const auto& [k, v] : map_Tickets) {
            if (v != std::numeric_limits<int>::max()) {
                i_TotalTickets += v;
            }
        }
        f_Score += i_TotalTickets * 20.0;

        auto it_Black = map_Tickets.find(Core::k_TransportTypeWater);
        if (it_Black != map_Tickets.end()) {
            f_Score += it_Black->second * 200.0;
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
            f_Score += 250.0;
        }

        // Police ticket depletion
        for (const auto* p : vec_Police) {
            int i_TotalPoliceTickets = p->i_TaxiTickets + p->i_BusTickets + p->i_MetroTickets;
            if (i_TotalPoliceTickets < 5) {
                f_Score += 400.0;
            }
            if (i_TotalPoliceTickets < 3) {
                f_Score += 1000.0;
            }
        }

        // Future mobility
        auto map_FutureReachable = fn_GetAllReachablePositions(i_Position, 2, map_Tickets);
        f_Score += map_FutureReachable.size() * 50.0;

        return f_Score;
    };

    // Forward declaration for minimax
    std::function<double(int, const std::map<int, int>&, const std::vector<const PlayerInfo*>&,
                        int, bool, double, double, int)> fn_MinimaxEvaluatePosition;

    // Minimax evaluation
    fn_MinimaxEvaluatePosition = [&](int i_Position, 
                                     const std::map<int, int>& map_Tickets,
                                     const std::vector<const PlayerInfo*>& vec_Police,
                                     int i_Depth, bool b_IsMrXTurn,
                                     double f_Alpha, double f_Beta, int i_MrXStart) 
        -> double {
        
        // Create cache key
        std::string str_CacheKey = std::to_string(i_Position) + "_" + std::to_string(i_Depth) + "_" + 
                                   std::to_string(b_IsMrXTurn);
        
        if (local_EvalCache.count(str_CacheKey)) {
            return local_EvalCache[str_CacheKey];
        }

        // Terminal conditions
        if (i_Depth == 0) {
            double f_Result = fn_EvaluatePositionComprehensive(i_Position, map_Tickets, vec_Police);
            local_EvalCache[str_CacheKey] = f_Result;
            return f_Result;
        }

        // Check if caught
        for (const auto* p : vec_Police) {
            if (i_Position == p->i_Position) {
                return -100000.0;
            }
        }

        if (b_IsMrXTurn) {
            double f_MaxEval = std::numeric_limits<double>::lowest();
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

                double f_EvalScore = fn_MinimaxEvaluatePosition(
                    i_Neighbor, map_NewTickets, vec_Police, i_Depth - 1,
                    false, f_Alpha, f_Beta, i_MrXStart
                );

                f_MaxEval = std::max(f_MaxEval, f_EvalScore);
                f_Alpha = std::max(f_Alpha, f_EvalScore);

                if (f_Beta <= f_Alpha) {
                    break;
                }
            }

            double f_Result = (f_MaxEval != std::numeric_limits<double>::lowest()) ? 
                             f_MaxEval : -10000.0;
            local_EvalCache[str_CacheKey] = f_Result;
            return f_Result;

        } else {
            // Police turn
            auto vec_NewPolicePositions = fn_UpdatePolicePredictions(vec_Police, i_Position);

            std::vector<const PlayerInfo*> vec_NewPolice;
            for (size_t i = 0; i < vec_Police.size(); ++i) {
                // Create temporary police info (we can't modify the original)
                // In practice, we just use the positions for evaluation
                vec_NewPolice.push_back(vec_Police[i]);
            }

            double f_EvalScore = fn_MinimaxEvaluatePosition(
                i_Position, map_Tickets, vec_NewPolice, i_Depth - 1,
                true, f_Alpha, f_Beta, i_MrXStart
            );

            local_EvalCache[str_CacheKey] = f_EvalScore;
            return f_EvalScore;
        }
    };

    // Strategic move evaluation
    auto fn_EvaluateMoveStrategic = [&](int i_Position, int i_Transport,
                                        const std::map<int, int>& map_Tickets,
                                        const std::vector<const PlayerInfo*>& vec_Police,
                                        int i_TurnNumber) 
        -> double {
        
        double f_Bonus = 0.0;

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
                f_Bonus += 300.0;
            }
            
            auto it = map_Tickets.find(Core::k_TransportTypeWater);
            if (i_TurnNumber < 8 && it != map_Tickets.end() && it->second > 3) {
                f_Bonus -= 150.0;
            }
        } else if (i_Transport == Core::k_TransportTypeMetro) {
            f_Bonus += 150.0;
        }

        // Game phase strategy
        double f_GameProgress = static_cast<double>(i_TurnNumber) / 24.0;

        if (f_GameProgress < 0.3) {
            int i_Connectivity = fn_GetConnectivity(i_Position);
            f_Bonus += i_Connectivity * 50.0;
        } else if (f_GameProgress < 0.7) {
            int i_Dist = fn_GetMinDistance();
            f_Bonus += i_Dist * 80.0;
        } else {
            int i_Dist = fn_GetMinDistance();
            f_Bonus += i_Dist * 200.0;
            if (i_Dist <= 2) {
                f_Bonus -= 1000.0;
            }
        }

        // Hub control
        int i_Connectivity = fn_GetConnectivity(i_Position);
        if (i_Connectivity >= 6) {
            f_Bonus += 400.0;
        }

        // Escape corridors
        int i_EscapeDirections = 0;
        auto vec_Neighbors = fn_GetNeighbors(i_Position);
        for (const auto& [i_Neighbor, _] : vec_Neighbors) {
            if (fn_GetConnectivity(i_Neighbor) >= 4) {
                i_EscapeDirections++;
            }
        }

        f_Bonus += i_EscapeDirections * 60.0;

        return f_Bonus;
    };

    // Path quality calculation
    auto fn_CalculatePathQuality = [&](const std::vector<std::pair<int, int>>& vec_Path,
                                       const std::vector<const PlayerInfo*>& vec_Police,
                                       int i_MrXStart,
                                       const std::map<int, int>& map_InitialTickets,
                                       int i_TurnNumber) 
        -> double {
        
        double f_Score = 0.0;
        std::map<int, int> map_CurrentTickets = map_InitialTickets;

        for (size_t i = 0; i < vec_Path.size(); ++i) {
            int i_Position = vec_Path[i].first;
            int i_Transport = vec_Path[i].second;

            if (map_CurrentTickets[i_Transport] != std::numeric_limits<int>::max()) {
                map_CurrentTickets[i_Transport] -= 1;
            }

            int i_Depth = std::min({2, i_kMinimaxDepth, static_cast<int>(vec_Path.size() - i)});

            double f_MinimaxScore = fn_MinimaxEvaluatePosition(
                i_Position, map_CurrentTickets, vec_Police,
                i_Depth, true,
                std::numeric_limits<double>::lowest(),
                std::numeric_limits<double>::max(), i_MrXStart
            );
            f_Score += f_MinimaxScore;

            double f_StrategicBonus = fn_EvaluateMoveStrategic(
                i_Position, i_Transport, map_CurrentTickets, vec_Police, i_TurnNumber + i
            );
            f_Score += f_StrategicBonus;
            f_Score += i * 50.0;
        }

        // Final position evaluation
        double f_FinalScore = fn_EvaluatePositionComprehensive(
            vec_Path.back().first, map_CurrentTickets, vec_Police
        );
        f_Score += f_FinalScore * 2.0;

        return f_Score;
    };

    // === MAIN DFS FUNCTION ===

    std::vector<std::pair<int, int>> vec_BestPath;
    double f_BestScore = std::numeric_limits<double>::lowest();

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
            double f_CurrentScore = fn_CalculatePathQuality(
                vec_Path, vec_PoliceInfos, p_Player->GetOccupiedNode(),
                map_Tickets, gameState.i_CurrentRound
            );

            if (f_CurrentScore > f_BestScore) {
                f_BestScore = f_CurrentScore;
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

            double f_Score = 0.0;

            int i_MinDist = 999;
            for (const auto* p : vec_PoliceInfos) {
                auto map_Dist = fn_BFSDistances(p->i_Position);
                int i_Dist = map_Dist.count(i_Neighbor) ? map_Dist[i_Neighbor] : 999;
                i_MinDist = std::min(i_MinDist, i_Dist);
            }

            f_Score += i_MinDist * 100.0;
            f_Score += fn_GetConnectivity(i_Neighbor) * 50.0;

            vec_NeighborPriorities.push_back({i_Neighbor, i_Transport, f_Score});
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

    int i_MrXPos = p_Player->GetOccupiedNode();
    int i_CurrentTurn = gameState.i_CurrentRound;

    // Get Mr. X tickets
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

    std::map<int, int> map_MrXTickets;
    map_MrXTickets[Core::k_TransportTypeTaxi] = p_MrXInfo->i_TaxiTickets;
    map_MrXTickets[Core::k_TransportTypeBus] = p_MrXInfo->i_BusTickets;
    map_MrXTickets[Core::k_TransportTypeMetro] = p_MrXInfo->i_MetroTickets;
    map_MrXTickets[Core::k_TransportTypeWater] = p_MrXInfo->i_BlackTickets;

    // Clean old cache entries
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

    // Initial move evaluation
    std::vector<std::tuple<int, int, double>> vec_InitialMoves;

    for (const auto& move : vec_PossibleMoves) {
        std::map<int, int> map_TempTickets = map_MrXTickets;
        if (map_TempTickets[move.i_TransportType] != std::numeric_limits<int>::max()) {
            map_TempTickets[move.i_TransportType] -= 1;
        }

        double f_QuickScore = 0.0;
        for (const auto* p : vec_PoliceInfos) {
            auto map_Dist = fn_BFSDistances(p->i_Position);
            f_QuickScore += (map_Dist.count(move.i_DestinationNode) ? 
                            map_Dist[move.i_DestinationNode] : 0) * 100.0;
        }

        f_QuickScore += fn_GetConnectivity(move.i_DestinationNode) * 50.0;

        vec_InitialMoves.push_back({move.i_DestinationNode, move.i_TransportType, f_QuickScore});
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

        double f_Score = fn_MinimaxEvaluatePosition(
            i_Dest, map_TempTickets, vec_PoliceInfos, i_kMinimaxDepth,
            false, std::numeric_limits<double>::lowest(),
            std::numeric_limits<double>::max(), i_MrXPos
        );

        double f_StrategicScore = fn_EvaluateMoveStrategic(
            i_Dest, i_Transport, map_TempTickets, vec_PoliceInfos, i_CurrentTurn
        );

        double f_TotalScore = f_Score + f_StrategicScore;
        vec_EvaluatedMoves.push_back({i_Dest, i_Transport, f_TotalScore});
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
            // decision = DecoyMovementAlgorithm(p_Player, vec_PossibleMoves, gameState);
            //decision = MonteCarloMrXAlgorithm(p_Player, vec_PossibleMoves, gameState);
            decision = DFSMrXAlgorithm(p_Player, vec_PossibleMoves, gameState);
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
