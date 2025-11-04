#include "MapGenerator.h"
#include <fstream>
#include <iostream>

namespace ScotlandYard {
namespace MapGen {

bool ExportMapToJSON(const std::string& s_Filename,
                     const GenerationParams& params,
                     const std::vector<GraphNode>& vec_Nodes,
                     const std::vector<Point>& vec_RiverPath,
                     const std::vector<Park>& vec_Parks) {
    
    std::ofstream file(s_Filename);
    if (!file.is_open()) return false;
    
    file << "{\n";
    
    // Parameters
    file << "  \"parameters\": {\n";
    file << "    \"width\": " << params.i_MapWidth << ",\n";
    file << "    \"height\": " << params.i_MapHeight << ",\n";
    file << "    \"numParks\": " << params.i_NumParks << ",\n";
    file << "    \"riverCurviness\": " << params.f_RiverCurviness << ",\n";
    file << "    \"taxiDensity\": " << params.f_TaxiDensity << ",\n";
    file << "    \"busDensity\": " << params.f_BusDensity << ",\n";
    file << "    \"metroDensity\": " << params.f_MetroDensity << "\n";
    file << "  },\n";
    
    // Nodes
    file << "  \"nodes\": [\n";
    for (size_t i = 0; i < vec_Nodes.size(); ++i) {
        const auto& node = vec_Nodes[i];
        file << "    {\n";
        file << "      \"id\": " << node.i_ID << ",\n";
        file << "      \"x\": " << node.m_Position.f_X << ",\n";
        file << "      \"y\": " << node.m_Position.f_Y << ",\n";
        file << "      \"isInPark\": " << (node.b_IsInPark ? "true" : "false") << ",\n";
        file << "      \"isNearRiver\": " << (node.b_IsNearRiver ? "true" : "false") << ",\n";
        
        // Connections
        file << "      \"connections\": {\n";
        
        file << "        \"taxi\": [";
        bool first = true;
        for (int id : node.set_TaxiConnections) {
            if (!first) file << ", ";
            file << id;
            first = false;
        }
        file << "],\n";
        
        file << "        \"bus\": [";
        first = true;
        for (int id : node.set_BusConnections) {
            if (!first) file << ", ";
            file << id;
            first = false;
        }
        file << "],\n";
        
        file << "        \"metro\": [";
        first = true;
        for (int id : node.set_MetroConnections) {
            if (!first) file << ", ";
            file << id;
            first = false;
        }
        file << "],\n";
        
        file << "        \"ferry\": [";
        first = true;
        for (int id : node.set_FerryConnections) {
            if (!first) file << ", ";
            file << id;
            first = false;
        }
        file << "]\n";
        
        file << "      }\n";
        file << "    }";
        if (i + 1 < vec_Nodes.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";
    
    // Parks
    file << "  \"parks\": [\n";
    for (size_t i = 0; i < vec_Parks.size(); ++i) {
        const auto& park = vec_Parks[i];
        file << "    {\n";
        file << "      \"centerX\": " << park.m_Center.f_X << ",\n";
        file << "      \"centerY\": " << park.m_Center.f_Y << ",\n";
        file << "      \"radius\": " << park.m_f_BaseRadius << "\n";
        file << "    }";
        if (i + 1 < vec_Parks.size()) file << ",";
        file << "\n";
    }
    file << "  ]\n";
    
    file << "}\n";
    file.close();
    
    return true;
}

} // namespace MapGen
} // namespace ScotlandYard
