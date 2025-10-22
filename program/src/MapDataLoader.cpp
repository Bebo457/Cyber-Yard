#include "MapDataLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>

namespace ScotlandYard {
namespace Utils {

std::vector<StationData> MapDataLoader::LoadStations(const std::string& s_FilePath) {
    std::vector<StationData> vec_Stations;
    std::ifstream file(s_FilePath);

    if (!file.is_open()) {
        std::cerr << "[MapDataLoader] ERROR: Could not open file: " << s_FilePath << std::endl;
        return vec_Stations;
    }

    std::string s_Line;
    bool b_FirstLine = true;

    while (std::getline(file, s_Line)) {
        if (s_Line.empty()) continue;

        if (b_FirstLine) {
            b_FirstLine = false;
            continue;
        }

        StationData station;
        if (ParseStationLine(s_Line, station)) {
            vec_Stations.push_back(station);
        }
    }

    file.close();
    std::cout << "[MapDataLoader] Loaded " << vec_Stations.size() << " stations from " << s_FilePath << std::endl;
    return vec_Stations;
}

bool MapDataLoader::ParseStationLine(const std::string& s_Line, StationData& out_Station) {
    std::stringstream ss(s_Line);
    std::string s_IdStr, s_XStr, s_YStr, s_TypeStr;

    if (!std::getline(ss, s_IdStr, ',')) return false;
    if (!std::getline(ss, s_XStr, ',')) return false;
    if (!std::getline(ss, s_YStr, ',')) return false;
    if (!std::getline(ss, s_TypeStr, ',')) return false;

    try {
        out_Station.i_StationID = std::stoi(s_IdStr);
        float f_X = std::stof(s_XStr);
        float f_Y = std::stof(s_YStr);
        out_Station.vec2_Position = glm::vec2(f_X, f_Y);
        out_Station.vec_TransportTypes = SplitTransportTypes(s_TypeStr);
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "[MapDataLoader] WARNING: Invalid line format: " << s_Line << std::endl;
        return false;
    }
}

std::vector<std::string> MapDataLoader::SplitTransportTypes(const std::string& s_TypeString) {
    std::vector<std::string> vec_Types;
    std::stringstream ss(s_TypeString);
    std::string s_Transport;

    while (std::getline(ss, s_Transport, '_')) {
        if (!s_Transport.empty()) {
            vec_Types.push_back(s_Transport);
        }
    }

    return vec_Types;
}

} // namespace Utils
} // namespace ScotlandYard
