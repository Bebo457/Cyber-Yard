#include "MapDataSerializer.h"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace ScotlandYard {
namespace MapGen {

// ============================================================================
// SAVE / LOAD
// ============================================================================

bool MapDataSerializer::SaveToFile(const GeneratedMapData& data, const std::string& filepath) {
    try {
        json j = ToJson(data);

        std::ofstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[MapDataSerializer] ERROR: Cannot open file for writing: " << filepath << std::endl;
            return false;
        }

        // Pretty print with 2-space indentation
        file << j.dump(2);
        file.close();

        std::cout << "[MapDataSerializer] Map saved successfully to: " << filepath << std::endl;
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[MapDataSerializer] ERROR saving map: " << e.what() << std::endl;
        return false;
    }
}

GeneratedMapData MapDataSerializer::LoadFromFile(const std::string& filepath) {
    try {
        std::ifstream file(filepath);
        if (!file.is_open()) {
            std::cerr << "[MapDataSerializer] ERROR: Cannot open file for reading: " << filepath << std::endl;
            return GeneratedMapData();
        }

        json j;
        file >> j;
        file.close();

        if (!ValidateJson(j)) {
            std::cerr << "[MapDataSerializer] ERROR: Invalid JSON format" << std::endl;
            return GeneratedMapData();
        }

        GeneratedMapData data = FromJson(j);
        std::cout << "[MapDataSerializer] Map loaded successfully from: " << filepath << std::endl;
        return data;
    }
    catch (const std::exception& e) {
        std::cerr << "[MapDataSerializer] ERROR loading map: " << e.what() << std::endl;
        return GeneratedMapData();
    }
}

// ============================================================================
// TO JSON
// ============================================================================

json MapDataSerializer::ToJson(const GeneratedMapData& data) {
    json j;

    // Metadata
    j["version"] = "1.0";
    j["format"] = "ScotlandYardMap";
    j["width"] = data.i_Width;
    j["height"] = data.i_Height;
    j["seed"] = data.ui_Seed;
    j["generationDate"] = data.s_GenerationDate;

    // River
    j["river"] = {
        {"corner", data.i_RiverCorner},
        {"path", json::array()}
    };
    for (const auto& pt : data.vec_RiverPath) {
        j["river"]["path"].push_back(PointToJson(pt));
    }
    for (const auto& pt : data.vec_RiverControlPoints) {
        j["river"]["controlPoints"] = json::array();
    }
    for (const auto& pt : data.vec_RiverControlPoints) {
        j["river"]["controlPoints"].push_back(PointToJson(pt));
    }

    // Parks
    j["parks"] = json::array();
    for (const auto& park : data.vec_Parks) {
        j["parks"].push_back(ParkToJson(park));
    }

    // Bridges
    j["bridges"] = json::array();
    for (const auto& bridge : data.vec_Bridges) {
        j["bridges"].push_back({
            {"point1", PointToJson(bridge.first)},
            {"point2", PointToJson(bridge.second)}
        });
    }

    // Streets
    j["streets"] = json::array();
    for (const auto& street : data.vec_Streets) {
        j["streets"].push_back(StreetToJson(street));
    }

    // Buildings
    j["buildings"] = json::array();
    for (const auto& building : data.vec_Buildings) {
        j["buildings"].push_back(BuildingToJson(building));
    }

    // Trees
    j["trees"] = json::array();
    for (const auto& tree : data.vec_Trees) {
        j["trees"].push_back(TreeToJson(tree));
    }

    // Graph nodes
    j["graph"] = {
        {"nodeCount", data.i_NumGraphNodes},
        {"nodes", json::array()}
    };
    for (const auto& node : data.vec_GraphNodes) {
        j["graph"]["nodes"].push_back(GraphNodeToJson(node));
    }

    return j;
}

// ============================================================================
// FROM JSON
// ============================================================================

GeneratedMapData MapDataSerializer::FromJson(const json& j) {
    GeneratedMapData data;

    // Metadata
    data.i_Width = j.value("width", 1200);
    data.i_Height = j.value("height", 900);
    data.ui_Seed = j.value("seed", 0u);
    data.s_GenerationDate = j.value("generationDate", "");

    // River
    if (j.contains("river")) {
        data.i_RiverCorner = j["river"].value("corner", 4);
        if (j["river"].contains("path")) {
            for (const auto& pt : j["river"]["path"]) {
                data.vec_RiverPath.push_back(PointFromJson(pt));
            }
        }
        if (j["river"].contains("controlPoints")) {
            for (const auto& pt : j["river"]["controlPoints"]) {
                data.vec_RiverControlPoints.push_back(PointFromJson(pt));
            }
        }
    }

    // Parks
    if (j.contains("parks")) {
        for (const auto& parkJson : j["parks"]) {
            data.vec_Parks.push_back(ParkFromJson(parkJson));
        }
    }

    // Bridges
    if (j.contains("bridges")) {
        for (const auto& bridgeJson : j["bridges"]) {
            Point p1 = PointFromJson(bridgeJson["point1"]);
            Point p2 = PointFromJson(bridgeJson["point2"]);
            data.vec_Bridges.push_back({p1, p2});
        }
    }

    // Streets
    if (j.contains("streets")) {
        for (const auto& streetJson : j["streets"]) {
            data.vec_Streets.push_back(StreetFromJson(streetJson));
        }
    }

    // Buildings
    if (j.contains("buildings")) {
        for (const auto& buildingJson : j["buildings"]) {
            data.vec_Buildings.push_back(BuildingFromJson(buildingJson));
        }
    }

    // Trees
    if (j.contains("trees")) {
        for (const auto& treeJson : j["trees"]) {
            data.vec_Trees.push_back(TreeFromJson(treeJson));
        }
    }

    // Graph
    if (j.contains("graph")) {
        data.i_NumGraphNodes = j["graph"].value("nodeCount", 0);
        if (j["graph"].contains("nodes")) {
            for (const auto& nodeJson : j["graph"]["nodes"]) {
                data.vec_GraphNodes.push_back(GraphNodeFromJson(nodeJson));
            }
        }
    }

    return data;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool MapDataSerializer::ValidateJson(const json& j) {
    // Check required fields
    if (!j.contains("version") || !j.contains("format")) {
        std::cerr << "[MapDataSerializer] Missing version or format field" << std::endl;
        return false;
    }

    if (j["format"] != "ScotlandYardMap") {
        std::cerr << "[MapDataSerializer] Invalid format: " << j["format"] << std::endl;
        return false;
    }

    // Version check (currently only 1.0)
    std::string version = j["version"];
    if (version != "1.0") {
        std::cerr << "[MapDataSerializer] Unsupported version: " << version << std::endl;
        return false;
    }

    return true;
}

// ============================================================================
// HELPER CONVERSIONS
// ============================================================================

json MapDataSerializer::PointToJson(const Point& p) {
    return {{"x", p.x}, {"y", p.y}};
}

Point MapDataSerializer::PointFromJson(const json& j) {
    return Point(j.value("x", 0.0f), j.value("y", 0.0f));
}

json MapDataSerializer::ParkToJson(const Park& park) {
    json j;
    j["center"] = PointToJson(park.center);
    j["radius"] = park.f_BaseRadius;
    j["sides"] = park.i_NumSides;
    j["vertices"] = json::array();
    for (const auto& v : park.vec_Vertices) {
        j["vertices"].push_back(PointToJson(v));
    }
    return j;
}

Park MapDataSerializer::ParkFromJson(const json& j) {
    Point center = PointFromJson(j["center"]);
    float radius = j.value("radius", 50.0f);
    Park park(center, radius);

    // Override vertices if provided
    if (j.contains("vertices")) {
        park.vec_Vertices.clear();
        for (const auto& v : j["vertices"]) {
            park.vec_Vertices.push_back(PointFromJson(v));
        }
    }

    return park;
}

json MapDataSerializer::StreetToJson(const StreetSegment& street) {
    json j;
    j["node1"] = street.i_Node1;
    j["node2"] = street.i_Node2;
    j["tier"] = street.i_Tier;
    j["width"] = street.f_Width;
    j["isInPark"] = street.b_IsInPark;
    j["geometry"] = json::array();
    for (const auto& pt : street.vec_Geometry) {
        j["geometry"].push_back(PointToJson(pt));
    }
    return j;
}

StreetSegment MapDataSerializer::StreetFromJson(const json& j) {
    StreetSegment street;
    street.i_Node1 = j.value("node1", -1);
    street.i_Node2 = j.value("node2", -1);
    street.i_Tier = j.value("tier", 3);
    street.f_Width = j.value("width", 4.0f);
    street.b_IsInPark = j.value("isInPark", false);

    if (j.contains("geometry")) {
        for (const auto& pt : j["geometry"]) {
            street.vec_Geometry.push_back(PointFromJson(pt));
        }
    }

    return street;
}

json MapDataSerializer::BuildingToJson(const BuildingData& building) {
    json j;
    j["position"] = {
        {"x", building.vec3_Position.x},
        {"y", building.vec3_Position.y},
        {"z", building.vec3_Position.z}
    };
    j["height"] = building.f_Height;
    j["hasRoof"] = building.b_HasRoof;
    j["roofHeight"] = building.f_RoofHeight;
    j["wallColor"] = {
        {"r", building.vec3_WallColor.r},
        {"g", building.vec3_WallColor.g},
        {"b", building.vec3_WallColor.b}
    };
    j["roofColor"] = {
        {"r", building.vec3_RoofColor.r},
        {"g", building.vec3_RoofColor.g},
        {"b", building.vec3_RoofColor.b}
    };
    j["type"] = building.i_BuildingType;
    j["footprint"] = json::array();
    for (const auto& pt : building.vec_BaseFootprint) {
        j["footprint"].push_back({{"x", pt.x}, {"y", pt.y}});
    }
    return j;
}

BuildingData MapDataSerializer::BuildingFromJson(const json& j) {
    BuildingData building;

    auto pos = j["position"];
    building.vec3_Position = glm::vec3(
        pos.value("x", 0.0f),
        pos.value("y", 0.0f),
        pos.value("z", 0.0f)
    );

    building.f_Height = j.value("height", 8.0f);
    building.b_HasRoof = j.value("hasRoof", true);
    building.f_RoofHeight = j.value("roofHeight", 2.5f);

    if (j.contains("wallColor")) {
        auto wc = j["wallColor"];
        building.vec3_WallColor = glm::vec3(
            wc.value("r", 0.9f),
            wc.value("g", 0.85f),
            wc.value("b", 0.7f)
        );
    }

    if (j.contains("roofColor")) {
        auto rc = j["roofColor"];
        building.vec3_RoofColor = glm::vec3(
            rc.value("r", 0.8f),
            rc.value("g", 0.2f),
            rc.value("b", 0.2f)
        );
    }

    building.i_BuildingType = j.value("type", 0);

    if (j.contains("footprint")) {
        for (const auto& pt : j["footprint"]) {
            building.vec_BaseFootprint.push_back(glm::vec2(
                pt.value("x", 0.0f),
                pt.value("y", 0.0f)
            ));
        }
    }

    return building;
}

json MapDataSerializer::TreeToJson(const Core::TreeInstance& tree) {
    return {
        {"position", {
            {"x", tree.position.x},
            {"y", tree.position.y},
            {"z", tree.position.z}
        }},
        {"scale", tree.scale},
        {"seed", tree.seed}
    };
}

Core::TreeInstance MapDataSerializer::TreeFromJson(const json& j) {
    Core::TreeInstance tree;
    auto pos = j["position"];
    tree.position = glm::vec3(
        pos.value("x", 0.0f),
        pos.value("y", 0.0f),
        pos.value("z", 0.0f)
    );
    tree.scale = j.value("scale", 1.0f);
    tree.seed = j.value("seed", 0u);
    return tree;
}

json MapDataSerializer::GraphNodeToJson(const GraphNodeData& node) {
    json j;
    j["id"] = node.i_ID;
    j["position"] = PointToJson(node.position);
    j["taxiConnections"] = node.vec_TaxiConnections;
    j["busConnections"] = node.vec_BusConnections;
    j["metroConnections"] = node.vec_MetroConnections;
    j["ferryConnections"] = node.vec_FerryConnections;
    j["flags"] = {
        {"isInPark", node.b_IsInPark},
        {"isNearRiver", node.b_IsNearRiver},
        {"isMetroStation", node.b_IsMetroStation},
        {"isFerryStop", node.b_IsFerryStop}
    };
    return j;
}

GraphNodeData MapDataSerializer::GraphNodeFromJson(const json& j) {
    GraphNodeData node;
    node.i_ID = j.value("id", -1);
    node.position = PointFromJson(j["position"]);

    if (j.contains("taxiConnections")) {
        node.vec_TaxiConnections = j["taxiConnections"].get<std::vector<int>>();
    }
    if (j.contains("busConnections")) {
        node.vec_BusConnections = j["busConnections"].get<std::vector<int>>();
    }
    if (j.contains("metroConnections")) {
        node.vec_MetroConnections = j["metroConnections"].get<std::vector<int>>();
    }
    if (j.contains("ferryConnections")) {
        node.vec_FerryConnections = j["ferryConnections"].get<std::vector<int>>();
    }

    if (j.contains("flags")) {
        auto flags = j["flags"];
        node.b_IsInPark = flags.value("isInPark", false);
        node.b_IsNearRiver = flags.value("isNearRiver", false);
        node.b_IsMetroStation = flags.value("isMetroStation", false);
        node.b_IsFerryStop = flags.value("isFerryStop", false);
    }

    return node;
}

} // namespace MapGen
} // namespace ScotlandYard
