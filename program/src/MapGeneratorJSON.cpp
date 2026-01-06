#include "MapGenerator.h"
#include <fstream>
#include <iostream>

namespace ScotlandYard {
namespace MapGen {

// If needed in the future, add proper type definitions (GenerationParams, GraphNode)
/*
bool ExportMapToJSON(const std::string& s_Filename,
                     const GenerationParams& params,
                     const std::vector<GraphNode>& vec_Nodes,
                     const std::vector<Point>& vec_RiverPath,
                     const std::vector<Park>& vec_Parks) {
}
*/

bool ExportPolygonsToJSON(const std::string& s_Filename,
                          const std::vector<Park>& vec_Parks,
                          const std::vector<Point>& vec_RiverPath) {

    std::ofstream file(s_Filename);
    if (!file.is_open()) {
        std::cerr << "[JSON Export] Failed to open file: " << s_Filename << std::endl;
        return false;
    }

    file << "{\n";

    file << "  \"parks\": [\n";
    for (size_t i = 0; i < vec_Parks.size(); ++i) {
        const auto& park = vec_Parks[i];
        file << "    {\n";
        file << "      \"centerX\": " << park.center.x << ",\n";
        file << "      \"centerY\": " << park.center.y << ",\n";
        file << "      \"radius\": " << park.f_BaseRadius << ",\n";
        file << "      \"vertices\": [\n";

        if (park.vec_Vertices.empty()) {
            std::cerr << "[JSON Export] Warning: Park has no vertices!" << std::endl;
        }

        for (size_t j = 0; j < park.vec_Vertices.size(); ++j) {
            file << "        {\"x\": " << park.vec_Vertices[j].x
                 << ", \"y\": " << park.vec_Vertices[j].y << "}";
            if (j + 1 < park.vec_Vertices.size()) file << ",";
            file << "\n";
        }
        file << "      ]\n";
        file << "    }";
        if (i + 1 < vec_Parks.size()) file << ",";
        file << "\n";
    }
    file << "  ],\n";

    // River path
    file << "  \"river\": {\n";
    file << "    \"path\": [\n";
    for (size_t i = 0; i < vec_RiverPath.size(); ++i) {
        file << "      {\"x\": " << vec_RiverPath[i].x
             << ", \"y\": " << vec_RiverPath[i].y << "}";
        if (i + 1 < vec_RiverPath.size()) file << ",";
        file << "\n";
    }
    file << "    ]\n";
    file << "  }\n";

    file << "}\n";
    file.close();

    std::cout << "[JSON Export] Exported " << vec_Parks.size()
              << " parks and " << vec_RiverPath.size()
              << " river points to " << s_Filename << std::endl;

    return true;
}

} // namespace MapGen
} // namespace ScotlandYard
