#ifndef SCOTLANDYARD_UTILS_MAPDATALOADER_H
#define SCOTLANDYARD_UTILS_MAPDATALOADER_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace ScotlandYard {
namespace Utils {

struct StationData {
    glm::vec2 vec2_Position;
    std::vector<std::string> vec_TransportTypes;
    int i_StationID;
};

struct EdgeGeometryData {
    int i_Source;
    int i_Dest;
    std::string s_Type;
    std::string s_Format;
    std::vector<glm::vec2> vec_Points;
};

struct GameConnectionData {
    int i_Source;
    int i_Destination;
    std::string s_ConnectionType;
};

class MapDataLoader {
public:
    static std::vector<StationData> LoadStations(const std::string& s_FilePath);
    static std::vector<StationData> LoadNodesWithStation(const std::string& s_FilePath);
    static std::vector<EdgeGeometryData> LoadEdgesGeometry(const std::string& s_FilePath);
    static std::vector<GameConnectionData> LoadGameConnections(const std::string& s_FilePath);

private:
    static bool ParseStationLine(const std::string& s_Line, StationData& out_Station);
    static std::vector<std::string> SplitTransportTypes(const std::string& s_TypeString);
};

} // namespace Utils
} // namespace ScotlandYard

#endif // SCOTLANDYARD_UTILS_MAPDATALOADER_H
