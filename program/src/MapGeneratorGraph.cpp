#include "MapGenerator.h"
#include <random>
#include <algorithm>
#include <iostream>
#include <cmath>

namespace ScotlandYard {
namespace MapGen {

// Helper: Calculate distance between two points
float Distance(const Point& p1, const Point& p2) {
    float dx = p1.x - p2.x;
    float dy = p1.y - p2.y;
    return sqrt(dx * dx + dy * dy);
}

// Helper: Check if connection crosses river
bool ConnectionCrossesRiver(const Point& p1, const Point& p2, 
                           const std::vector<Point>& riverPath) {
    // Simple check: if midpoint is very close to river, assume crossing
    Point mid((p1.x + p2.x) / 2.0f, (p1.y + p2.y) / 2.0f);
    return GetDistanceToRiver(mid, riverPath) < Config::RIVER_WIDTH * 0.6f;
}

std::vector<GraphNode> GenerateGraph(const std::vector<Point>& vec_GridPoints,
                                     const std::vector<Point>& vec_RiverPath,
                                     const std::vector<Park>& vec_Parks,
                                     const GenerationParams& params) {
    std::vector<GraphNode> nodes;
    
    // Create nodes from grid points
    int nodeID = 1;
    for (const auto& pt : vec_GridPoints) {
        GraphNode node(nodeID++, pt);
        
        // Mark if in park
        for (const auto& park : vec_Parks) {
            if (park.ContainsPoint(pt.x, pt.y)) {
                node.b_IsInPark = true;
                break;
            }
        }
        
        // Mark if near river
        float distToRiver = GetDistanceToRiver(pt, vec_RiverPath);
        node.b_IsNearRiver = (distToRiver < Config::MIN_RIVER_DISTANCE);
        
        nodes.push_back(node);
    }
    
    std::cout << "Created " << nodes.size() << " graph nodes" << std::endl;
    
    // Generate connections
    GenerateTaxiConnections(nodes, params.f_TaxiDensity);
    GenerateBusConnections(nodes, params.f_BusDensity);
    GenerateMetroConnections(nodes, params.f_MetroDensity);
    GenerateFerryConnections(nodes, vec_RiverPath, params.i_NumFerries);
    
    return nodes;
}

void GenerateTaxiConnections(std::vector<GraphNode>& vec_Nodes, float f_Density) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int connectionsAdded = 0;
    
    for (auto& node : vec_Nodes) {
        // Find potential neighbors
        std::vector<int> candidates;
        
        for (auto& other : vec_Nodes) {
            if (node.i_ID == other.i_ID) continue;
            
            float dist = Distance(node.position, other.position);
            if (dist <= Config::TAXI_MAX_DISTANCE * f_Density) {
                candidates.push_back(other.i_ID);
            }
        }
        
        if (candidates.empty()) continue;
        
        // Shuffle first for randomness
        std::shuffle(candidates.begin(), candidates.end(), gen);
        
        // Then sort by distance with some randomness
        std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
            if (a < 1 || a > (int)vec_Nodes.size() || b < 1 || b > (int)vec_Nodes.size()) {
                return a < b; // Fallback for invalid IDs
            }
            
            float distA = Distance(node.position, vec_Nodes[a-1].position);
            float distB = Distance(node.position, vec_Nodes[b-1].position);
            
            // Add randomness: 70% prefer closer, 30% random
            if (std::uniform_real_distribution<float>(0, 1)(gen) > 0.7f) {
                return std::uniform_int_distribution<>(0, 1)(gen) == 0;
            }
            return distA < distB;
        });
        
        // Determine target connections with randomness
        int minConnections = node.b_IsInPark ? 2 : Config::MIN_CONNECTIONS_PER_NODE;
        int maxConnections = node.b_IsInPark ? 4 : Config::MAX_CONNECTIONS_PER_NODE;
        
        std::uniform_int_distribution<> connDis(minConnections, maxConnections);
        int targetConnections = std::min(connDis(gen), (int)candidates.size());
        
        for (int i = 0; i < targetConnections && i < (int)candidates.size(); ++i) {
            int otherID = candidates[i];
            
            // Validate ID
            if (otherID < 1 || otherID > (int)vec_Nodes.size()) continue;
            
            // Skip if already connected
            if (node.set_TaxiConnections.count(otherID) > 0) continue;
            
            node.set_TaxiConnections.insert(otherID);
            vec_Nodes[otherID - 1].set_TaxiConnections.insert(node.i_ID);
            connectionsAdded++;
        }
    }
    
    std::cout << "Generated " << connectionsAdded << " taxi connections" << std::endl;
}

void GenerateBusConnections(std::vector<GraphNode>& vec_Nodes, float f_Density) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int connectionsAdded = 0;
    
    for (auto& node : vec_Nodes) {
        // Parks: ONLY taxi connections
        if (node.b_IsInPark) continue;
        
        // Add randomness: not all nodes get bus connections
        if (std::uniform_real_distribution<float>(0, 1)(gen) > f_Density * 0.7f) continue;
        
        // Find potential neighbors
        std::vector<int> candidates;
        
        for (auto& other : vec_Nodes) {
            if (node.i_ID == other.i_ID || other.b_IsInPark) continue;
            
            float dist = Distance(node.position, other.position);
            if (dist > Config::TAXI_MAX_DISTANCE && 
                dist <= Config::BUS_MAX_DISTANCE * f_Density) {
                candidates.push_back(other.i_ID);
            }
        }
        
        if (candidates.empty()) continue;
        
        // Shuffle for randomness
        std::shuffle(candidates.begin(), candidates.end(), gen);
        
        // Random number of connections (1-3)
        std::uniform_int_distribution<> connDis(1, 3);
        int numConnections = std::min(connDis(gen), (int)candidates.size());
        
        for (int i = 0; i < numConnections; ++i) {
            int otherID = candidates[i];
            
            // Validate ID
            if (otherID < 1 || otherID > (int)vec_Nodes.size()) continue;
            
            // Skip if already connected
            if (node.set_BusConnections.count(otherID) > 0) continue;
            
            node.set_BusConnections.insert(otherID);
            vec_Nodes[otherID - 1].set_BusConnections.insert(node.i_ID);
            connectionsAdded++;
        }
    }
    
    std::cout << "Generated " << connectionsAdded << " bus connections" << std::endl;
}

void GenerateMetroConnections(std::vector<GraphNode>& vec_Nodes, float f_Density) {
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int connectionsAdded = 0;
    
    // Create metro lines (long-distance connections)
    int numMetroStations = std::max(4, (int)(vec_Nodes.size() * f_Density * 0.25f));
    std::vector<int> metroStations;
    
    // Select stations that are NOT in parks, with randomness
    std::vector<int> eligibleNodes;
    for (auto& node : vec_Nodes) {
        if (!node.b_IsInPark) {
            eligibleNodes.push_back(node.i_ID);
        }
    }
    
    if (eligibleNodes.empty()) {
        std::cout << "No eligible nodes for metro (all in parks?)" << std::endl;
        return;
    }
    
    std::shuffle(eligibleNodes.begin(), eligibleNodes.end(), gen);
    
    for (int i = 0; i < numMetroStations && i < (int)eligibleNodes.size(); ++i) {
        metroStations.push_back(eligibleNodes[i]);
    }
    
    std::cout << "Selected " << metroStations.size() << " metro stations" << std::endl;
    
    // Connect metro stations - create 2-3 lines with some branching
    if (metroStations.size() >= 4) {
        std::shuffle(metroStations.begin(), metroStations.end(), gen);
        
        // Create main line
        size_t mainLineLength = (size_t)(metroStations.size() * 0.6f);
        for (size_t i = 0; i + 1 < mainLineLength; ++i) {
            int id1 = metroStations[i];
            int id2 = metroStations[i + 1];
            
            // Validate IDs
            if (id1 < 1 || id1 > (int)vec_Nodes.size()) continue;
            if (id2 < 1 || id2 > (int)vec_Nodes.size()) continue;
            
            vec_Nodes[id1 - 1].set_MetroConnections.insert(id2);
            vec_Nodes[id2 - 1].set_MetroConnections.insert(id1);
            connectionsAdded++;
        }
        
        // Add some random cross-connections
        if (metroStations.size() > 3) {
            std::uniform_int_distribution<> crossDis(0, metroStations.size() - 1);
            int numCrossConnections = std::min(3, (int)metroStations.size() / 3);
            
            for (int i = 0; i < numCrossConnections; ++i) {
                int id1 = metroStations[crossDis(gen)];
                int id2 = metroStations[crossDis(gen)];
                
                // Validate IDs
                if (id1 < 1 || id1 > (int)vec_Nodes.size()) continue;
                if (id2 < 1 || id2 > (int)vec_Nodes.size()) continue;
                
                if (id1 != id2 && vec_Nodes[id1-1].set_MetroConnections.count(id2) == 0) {
                    vec_Nodes[id1 - 1].set_MetroConnections.insert(id2);
                    vec_Nodes[id2 - 1].set_MetroConnections.insert(id1);
                    connectionsAdded++;
                }
            }
        }
    }
    
    std::cout << "Generated " << connectionsAdded << " metro connections (" 
              << metroStations.size() << " stations)" << std::endl;
}

void GenerateFerryConnections(std::vector<GraphNode>& vec_Nodes, 
                               const std::vector<Point>& vec_RiverPath,
                               int i_NumFerries) {
    if (vec_RiverPath.empty()) {
        std::cout << "No river path for ferry connections" << std::endl;
        return;
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int connectionsAdded = 0;
    
    // Generate only ONE ferry line across the river
    std::vector<int> ferryChain;
    
    // Divide river into segments - i_NumFerries determines number of stops on the ferry line
    int numStops = std::max(2, i_NumFerries); // At least 2 stops to make a connection
    std::vector<Point> sampledRiverPoints;
    
    // Sample points along the river at equal intervals
    for (int stop = 0; stop < numStops; ++stop) {
        float t = (float)stop / (float)(numStops - 1);
        size_t idx = (size_t)(t * (vec_RiverPath.size() - 1));
        sampledRiverPoints.push_back(vec_RiverPath[idx]);
    }
    
    // For each point on the river, find the nearest node
    for (const auto& riverPoint : sampledRiverPoints) {
        std::vector<std::pair<int, float>> candidates; // (nodeID, distance)
        
        for (auto& node : vec_Nodes) {
            if (node.b_IsInPark) continue; // Parks: taxi only
            
            float dist = Distance(node.position, riverPoint);
            if (dist < 200.0f) { // Wider search radius
                candidates.push_back({node.i_ID, dist});
            }
        }
        
        // Select nearest node
        if (!candidates.empty()) {
            std::sort(candidates.begin(), candidates.end(), 
                [](const auto& a, const auto& b) { return a.second < b.second; });
            
            int nodeID = candidates[0].first;
            
            // Make sure node is not already in the chain
            if (std::find(ferryChain.begin(), ferryChain.end(), nodeID) == ferryChain.end()) {
                ferryChain.push_back(nodeID);
            }
        }
    }
    
    // Connect nodes in a chain
    if (ferryChain.size() >= 2) {
        std::cout << "Ferry line: ";
        for (size_t i = 0; i + 1 < ferryChain.size(); ++i) {
            int id1 = ferryChain[i];
            int id2 = ferryChain[i + 1];
            
            std::cout << id1 << " <-> " << id2;
            if (i + 2 < ferryChain.size()) std::cout << ", ";
            
            // Validate IDs
            if (id1 < 1 || id1 > (int)vec_Nodes.size()) continue;
            if (id2 < 1 || id2 > (int)vec_Nodes.size()) continue;
            
            vec_Nodes[id1 - 1].set_FerryConnections.insert(id2);
            vec_Nodes[id2 - 1].set_FerryConnections.insert(id1);
            connectionsAdded++;
        }
        std::cout << std::endl;
    } else {
        std::cout << "Could not generate ferry line (only " << ferryChain.size() << " nodes found)" << std::endl;
    }
    
    std::cout << "Generated " << connectionsAdded << " ferry connections with " 
              << ferryChain.size() << " stops" << std::endl;
}

} // namespace MapGen
} // namespace ScotlandYard
