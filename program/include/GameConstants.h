#ifndef SCOTLANDYARD_CORE_GAMECONSTANTS_H
#define SCOTLANDYARD_CORE_GAMECONSTANTS_H

#include <string>

namespace ScotlandYard {
namespace Core {

// Game Rules
// Base rounds in the game (not counting Mr. X double-move extra turns)
// Per requirement, the basic number of rounds is 22.
static constexpr int k_MaxRounds = 22;
static constexpr int k_DetectiveCount = 4;

// Mr X Reveal Rounds
static constexpr int k_RevealRounds[] = {3, 8, 13, 18, 24};
static constexpr int k_RevealRoundsCount = sizeof(k_RevealRounds) / sizeof(k_RevealRounds[0]);
inline bool IsRevealRound(int i_Round) {
    for (int i = 0; i < k_RevealRoundsCount; ++i) {
        if (k_RevealRounds[i] == i_Round) return true;
    }
    return false;
}

// Initial Ticket Counts - Detectives
static constexpr int k_DetectiveTaxiTickets = 11;
static constexpr int k_DetectiveBusTickets = 8;
static constexpr int k_DetectiveMetroTickets = 4;

// Initial Ticket Counts - Mr. X
static constexpr int k_MrXTaxiTickets = 30;
static constexpr int k_MrXBusTickets = 30;
static constexpr int k_MrXMetroTickets = 30;
static constexpr int k_MrXBlackTickets = 5;
static constexpr int k_MrXDoubleMoveTickets = 2;

// Map Data Paths - use GetMapPath() to get full paths with ASSETS_DIR
static constexpr const char* k_NodeDataRelativePath = "maps/nodes_original.csv";
static constexpr const char* k_ConnectionsRelativePath = "maps/polaczenia.csv";
// Edge geometry (normalized polylines or curves)
static constexpr const char* k_EdgeGeometryRelativePath = "maps/edges_geometry.csv";

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
// Grid extents used to denormalize edge geometry points from [0..1] to board units
static constexpr float k_MapGridMaxX = 22.0f; // matches nodes_original.csv max pos_x
static constexpr float k_MapGridMaxY = 15.0f; // matches nodes_original.csv max pos_y

// Edge rendering thickness (before applying global scale matrix)
static constexpr float k_EdgeThicknessTaxi  = 0.15f; // thinnest
static constexpr float k_EdgeThicknessBus   = 0.22f; // middle
static constexpr float k_EdgeThicknessMetro = 0.35f; // thickest
static constexpr float k_EdgeThicknessWater = 0.25f; // thicker for visibility

// Edge colors (RGB in [0..1])
static constexpr float k_EdgeColorTaxi[3]  = {1.0f, 1.0f, 0.0f}; // yellow
static constexpr float k_EdgeColorBus[3]   = {0.0f, 1.0f, 0.0f}; // green
static constexpr float k_EdgeColorMetro[3] = {1.0f, 0.0f, 0.0f}; // red
static constexpr float k_EdgeColorWater[3] = {0.2f, 0.2f, 0.2f}; // dark gray

// Metro dashed rendering (lengths in world units after applying node scale; tuned visually)
static constexpr float k_MetroDashLen = 0.02f;
static constexpr float k_MetroGapLen  = 0.02f;

// Water dashed rendering (separate from metro for independent tuning)
static constexpr float k_WaterDashLen = 0.035f;
static constexpr float k_WaterGapLen  = 0.025f;

// Transport Types
static constexpr int k_TransportTypeTaxi = 1;
static constexpr int k_TransportTypeBus = 2;
static constexpr int k_TransportTypeMetro = 3;
static constexpr int k_TransportTypeWater = 4;

} // namespace Core

namespace UI {

// Arrow Geometry (3x enlarged)
static constexpr float k_ArrowLength = 0.60f;
static constexpr float k_ArrowWidth = 0.36f;

// Transport Orbital Radii (distance from station center for direction arrows)
// Same orbit for taxi/bus/metro to keep visual grouping
static constexpr float k_TaxiWaterOrbitalRadius = 0.15f;
static constexpr float k_BusOrbitalRadius = 0.75f;
static constexpr float k_MetroOrbitalRadius = 1.35f;

} // namespace UI

} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_GAMECONSTANTS_H
