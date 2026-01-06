#pragma once

#include "GeneratedMapData.h"
#include <string>
#include <nlohmann/json.hpp>

namespace ScotlandYard {
namespace MapGen {

/**
 * @class MapDataSerializer
 * Serializacja i deserializacja danych mapy do/z formatu JSON
 *
 * Format pliku .symap (Scotland Yard Map):
 * - JSON z czytelną strukturą
 * - Łatwy do edycji ręcznie
 * - Kompatybilny z narzędziami zewnętrznymi
 * - Możliwość importu z OSM
 */
class MapDataSerializer {
public:
    /**
     * Zapisuje mapę do pliku JSON
     * @param data Dane mapy do zapisania
     * @param filepath Ścieżka do pliku (np. "maps/london.symap")
     * @return true jeśli sukces
     */
    static bool SaveToFile(const GeneratedMapData& data, const std::string& filepath);

    /**
     * Wczytuje mapę z pliku JSON
     * @param filepath Ścieżka do pliku
     * @return Dane mapy (puste jeśli błąd)
     */
    static GeneratedMapData LoadFromFile(const std::string& filepath);

    /**
     * Konwertuje GeneratedMapData do JSON
     */
    static nlohmann::json ToJson(const GeneratedMapData& data);

    /**
     * Konwertuje JSON do GeneratedMapData
     */
    static GeneratedMapData FromJson(const nlohmann::json& j);

    /**
     * Waliduje poprawność danych JSON
     */
    static bool ValidateJson(const nlohmann::json& j);

private:
    // Helper functions
    static nlohmann::json PointToJson(const Point& p);
    static Point PointFromJson(const nlohmann::json& j);

    static nlohmann::json ParkToJson(const Park& park);
    static Park ParkFromJson(const nlohmann::json& j);

    static nlohmann::json StreetToJson(const StreetSegment& street);
    static StreetSegment StreetFromJson(const nlohmann::json& j);

    static nlohmann::json BuildingToJson(const BuildingData& building);
    static BuildingData BuildingFromJson(const nlohmann::json& j);

    static nlohmann::json TreeToJson(const Core::TreeInstance& tree);
    static Core::TreeInstance TreeFromJson(const nlohmann::json& j);

    static nlohmann::json GraphNodeToJson(const GraphNodeData& node);
    static GraphNodeData GraphNodeFromJson(const nlohmann::json& j);
};

} // namespace MapGen
} // namespace ScotlandYard
