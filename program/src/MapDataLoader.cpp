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

std::vector<StationData> MapDataLoader::LoadNodesWithStation(const std::string& s_FilePath) {
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

        std::stringstream ss(s_Line);
        std::string s_IdStr, s_XStr, s_YStr, s_TypeStr;

        if (!std::getline(ss, s_IdStr, ',')) continue;
        if (!std::getline(ss, s_XStr, ',')) continue;
        if (!std::getline(ss, s_YStr, ',')) continue;
        if (!std::getline(ss, s_TypeStr, ',')) continue;

        try {
            StationData station;
            station.i_StationID = std::stoi(s_IdStr);
            float f_X = std::stof(s_XStr);
            float f_Y = std::stof(s_YStr);
            station.vec2_Position = glm::vec2(f_X, f_Y);
            station.vec_TransportTypes = SplitTransportTypes(s_TypeStr);
            vec_Stations.push_back(station);
        }
        catch (const std::exception& e) {
            std::cerr << "[MapDataLoader] WARNING: Invalid line format: " << s_Line << std::endl;
        }
    }

    file.close();
    std::cout << "[MapDataLoader] Loaded " << vec_Stations.size() << " nodes with stations from " << s_FilePath << std::endl;
    return vec_Stations;
}

std::vector<EdgeGeometryData> MapDataLoader::LoadEdgesGeometry(const std::string& s_FilePath) {
    std::vector<EdgeGeometryData> vec_Edges;
    std::ifstream file(s_FilePath);

    if (!file.is_open()) {
        std::cerr << "[MapDataLoader] ERROR: Could not open file: " << s_FilePath << std::endl;
        return vec_Edges;
    }

    std::string s_Line;
    bool b_FirstLine = true;

    while (std::getline(file, s_Line)) {
        if (s_Line.empty()) continue;

        if (b_FirstLine) {
            b_FirstLine = false;
            continue;
        }

        std::stringstream ss(s_Line);
        std::string s_SourceStr, s_DestStr, s_Type, s_Format, s_Points;

        if (!std::getline(ss, s_SourceStr, ',')) continue;
        if (!std::getline(ss, s_DestStr, ',')) continue;
        if (!std::getline(ss, s_Type, ',')) continue;
        if (!std::getline(ss, s_Format, ',')) continue;
        if (!std::getline(ss, s_Points, ',')) continue;

        try {
            EdgeGeometryData edge;
            edge.i_Source = std::stoi(s_SourceStr);
            edge.i_Dest = std::stoi(s_DestStr);
            edge.s_Type = s_Type;
            edge.s_Format = s_Format;

            // Parse points if available (format: "x1 y1;x2 y2;...")
            if (!s_Points.empty()) {
                std::stringstream pointsStream(s_Points);
                std::string pointPair;
                while (std::getline(pointsStream, pointPair, ';')) {
                    std::stringstream pairStream(pointPair);
                    std::string xStr, yStr;
                    if (std::getline(pairStream, xStr, ' ') && std::getline(pairStream, yStr, ' ')) {
                        float x = std::stof(xStr);
                        float y = std::stof(yStr);
                        edge.vec_Points.push_back(glm::vec2(x, y));
                    }
                }
            }

            vec_Edges.push_back(edge);
        }
        catch (const std::exception& e) {
            std::cerr << "[MapDataLoader] WARNING: Invalid line format: " << s_Line << std::endl;
        }
    }

    file.close();
    std::cout << "[MapDataLoader] Loaded " << vec_Edges.size() << " edges from " << s_FilePath << std::endl;
    return vec_Edges;
}

std::vector<GameConnectionData> MapDataLoader::LoadGameConnections(const std::string& s_FilePath) {
    std::vector<GameConnectionData> vec_Connections;
    std::ifstream file(s_FilePath);

    if (!file.is_open()) {
        std::cerr << "[MapDataLoader] ERROR: Could not open file: " << s_FilePath << std::endl;
        return vec_Connections;
    }

    std::string s_Line;
    bool b_FirstLine = true;

    while (std::getline(file, s_Line)) {
        if (s_Line.empty()) continue;

        if (b_FirstLine) {
            b_FirstLine = false;
            continue;
        }

        std::stringstream ss(s_Line);
        std::string s_SourceStr, s_DestStr, s_Type;

        if (!std::getline(ss, s_SourceStr, ',')) continue;
        if (!std::getline(ss, s_DestStr, ',')) continue;
        if (!std::getline(ss, s_Type, ',')) continue;

        try {
            GameConnectionData conn;
            conn.i_Source = std::stoi(s_SourceStr);
            conn.i_Destination = std::stoi(s_DestStr);
            conn.s_ConnectionType = s_Type;
            vec_Connections.push_back(conn);
        }
        catch (const std::exception& e) {
            std::cerr << "[MapDataLoader] WARNING: Invalid line format: " << s_Line << std::endl;
        }
    }

    file.close();
    std::cout << "[MapDataLoader] Loaded " << vec_Connections.size() << " game connections from " << s_FilePath << std::endl;
    return vec_Connections;
}

} // namespace Utils
} // namespace ScotlandYard
