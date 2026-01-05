#pragma once

#include "GeneratedMapData.h"
#include <random>

namespace ScotlandYard {
namespace MapGen {

/**
 * @class SampleMapDataGenerator
 * Generator przykładowych danych mapy dla celów testowych
 *
 * Używaj tego gdy chcesz przetestować renderowanie bez uruchamiania pełnego generatora.
 */
class SampleMapDataGenerator {
public:
    /**
     * Generuje minimalną mapę testową - prosty graf na siatce
     * - 9 węzłów w siatce 3x3
     * - Prosta rzeka po środku
     * - 2 małe parki
     * - Kilka ulic
     * - 5-10 budynków
     * - Kilka drzew w parkach
     */
    static GeneratedMapData GenerateSimpleTestMap(unsigned int seed = 42);

    /**
     * Generuje średnio złożoną mapę testową
     * - 20-30 węzłów
     * - Rzeka z zakrętami
     * - 3-4 parki
     * - Więcej ulic (różne tiers)
     * - 20-30 budynków
     * - Drzewa w parkach
     */
    static GeneratedMapData GenerateMediumTestMap(unsigned int seed = 123);

    /**
     * Generuje realistyczną mapę miasta
     * - Układ siatki ulic inspirowany prawdziwym miastem
     * - Gęsta zabudowa budynków
     * - Naturalne rozmieszczenie parków
     * - Realistyczny system transportu
     */
    static GeneratedMapData GenerateRealisticCityMap(unsigned int seed = 456);

    /**
     * Generuje pełną mapę używając prawdziwego generatora
     * (wrapper dla istniejących funkcji generatora)
     */
    static GeneratedMapData GenerateFullMap(
        int width = 1200,
        int height = 900,
        unsigned int seed = 0,
        int numNodes = 50,
        int numParks = 3,
        int maxStreets = 100
    );

    /**
     * Generuje i zapisuje mapę do pliku
     * @param filepath Ścieżka do pliku (np. "maps/city_name.symap")
     * @param seed Seed generatora
     * @return true jeśli sukces
     */
    static bool GenerateAndSave(const std::string& filepath, unsigned int seed = 456);

private:
    // Pomocnicze funkcje
    static void GenerateSimpleRiver(GeneratedMapData& data);
    static void GenerateSimpleParks(GeneratedMapData& data);
    static void GenerateSimpleGraph(GeneratedMapData& data);
    static void GenerateSimpleStreets(GeneratedMapData& data);
    static void GenerateSimpleBuildings(GeneratedMapData& data, std::mt19937& rng);
    static void GenerateSimpleTrees(GeneratedMapData& data, std::mt19937& rng);

    static float RandomFloat(std::mt19937& rng, float min, float max);
    static glm::vec3 RandomColor(std::mt19937& rng);
};

} // namespace MapGen
} // namespace ScotlandYard
