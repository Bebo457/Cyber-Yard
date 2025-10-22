#ifndef SCOTLANDYARD_CORE_GAMECONSTANTS_H
#define SCOTLANDYARD_CORE_GAMECONSTANTS_H

#include <string>

namespace ScotlandYard {
namespace Core {

// Game Rules
static constexpr int k_MaxRounds = 24;
static constexpr int k_DetectiveCount = 4;

// Initial Ticket Counts - Detectives
static constexpr int k_DetectiveTaxiTickets = 11;
static constexpr int k_DetectiveBusTickets = 8;
static constexpr int k_DetectiveMetroTickets = 4;
static constexpr int k_DetectiveWaterTickets = 0;

// Initial Ticket Counts - Mr. X
static constexpr int k_MrXTaxiTickets = 30;
static constexpr int k_MrXBusTickets = 30;
static constexpr int k_MrXMetroTickets = 30;
static constexpr int k_MrXWaterTickets = 30;
static constexpr int k_MrXBlackTickets = 5;
static constexpr int k_MrXDoubleMoveTickets = 2;

// Map Data Paths - use GetMapPath() to get full paths with ASSETS_DIR
static constexpr const char* k_NodeDataRelativePath = "maps/nodes_with_station.csv";
static constexpr const char* k_ConnectionsRelativePath = "maps/polaczenia.csv";

// Helper function to build full asset path (like GetAssetPath in Application)
inline std::string GetMapPath(const std::string& s_RelativePath) {
    #ifdef ASSETS_DIR
        return std::string(ASSETS_DIR) + "/" + s_RelativePath;
    #else
        return "assets/" + s_RelativePath;
    #endif
}

// Map Constants
static constexpr int k_MaxNodes = 200;
static constexpr float k_MapSizeMeters = 13.0f;

// Transport Types
static constexpr int k_TransportTypeTaxi = 1;
static constexpr int k_TransportTypeBus = 2;
static constexpr int k_TransportTypeMetro = 3;
static constexpr int k_TransportTypeWater = 4;

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_GAMECONSTANTS_H
