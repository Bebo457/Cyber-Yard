## System Danych Mapy i Środowiska 3D

> **WAŻNE:** Obecny system używa **tymczasowego generatora testowego** (`SampleMapDataGenerator`), który NIE jest połączony z istniejącym `MapGenerator.cpp` (Delaunay/triangulacja). Aby podłączyć prawdziwy generator, zobacz sekcję "Integracja z Istniejącym Generatorem" poniżej.

### Przegląd Architektury

System generowania i przechowywania map składa się z trzech głównych komponentów:

1. **Struktury danych** (`GeneratedMapData`) - format przechowywania wszystkich elementów mapy
2. **Generator map** (`SampleMapDataGenerator`) - generowanie proceduralnych map testowych
3. **Serializacja** (`MapDataSerializer`) - zapis/odczyt map w formacie `.symap` (JSON)

### Przepływ Danych

```
┌─────────────────────┐
│  MapGenerator.cpp   │ (Istniejący generator)
│  - Delaunay         │
│  - Triangulation    │
│  - Parks, River     │
└──────────┬──────────┘
           │
           ▼
┌─────────────────────────────────────────────────────┐
│         GeneratedMapData (Struktura danych)         │
│  ┌─────────────────────────────────────────────┐   │
│  │  • vec_RiverPath: vector<Point>             │   │
│  │  • vec_Parks: vector<Park>                  │   │
│  │  • vec_Streets: vector<StreetSegment>       │   │
│  │  • vec_Buildings: vector<BuildingData>      │   │
│  │  • vec_Trees: vector<TreeInstance>          │   │
│  │  • vec_GraphNodes: vector<GraphNodeData>    │   │
│  └─────────────────────────────────────────────┘   │
└──────────┬──────────────────────────────┬──────────┘
           │                               │
           ▼                               ▼
┌──────────────────────┐       ┌──────────────────────┐
│ MapDataSerializer    │       │ EmptyEnvironmentState│
│ - SaveToFile()       │       │ - LoadSampleMapData()│
│ - LoadFromFile()     │       │ - RenderMapData()    │
│ Format: .symap (JSON)│       │ (Rendering pipeline) │
└──────────────────────┘       └──────────────────────┘
```

---

## Struktury Danych

### GeneratedMapData

Główna struktura przechowująca wszystkie elementy wygenerowanej mapy.

**Lokalizacja:** `program/include/GeneratedMapData.h`

#### Pola struktury:

```cpp
struct GeneratedMapData {
    // Wymiary
    int i_Width;                    // Szerokość mapy (typowo 1200)
    int i_Height;                   // Wysokość mapy (typowo 900)

    // Rzeka
    vector<Point> vec_RiverPath;            // Wygładzone punkty krzywej rzeki
    vector<Point> vec_RiverControlPoints;   // Punkty kontrolne Bézier
    int i_RiverCorner;                      // 0-3: narożniki, 4: lewo-prawo

    // Parki
    vector<Park> vec_Parks;                 // Wielokąty parków (6-10 boków)

    // Mosty
    vector<pair<Point, Point>> vec_Bridges; // Para punktów mostów

    // Ulice
    vector<StreetSegment> vec_Streets;      // Segmenty ulic z geometrią

    // Budynki
    vector<BuildingData> vec_Buildings;     // Pełne dane budynków 3D

    // Drzewa
    vector<TreeInstance> vec_Trees;         // Instancje drzew (pos + seed)

    // Graf gry (logika)
    vector<GraphNodeData> vec_GraphNodes;   // Węzły z połączeniami
    int i_NumGraphNodes;

    // Metadata
    unsigned int ui_Seed;                   // Seed generowania
    string s_GenerationDate;                // Timestamp
};
```

### StreetSegment

Reprezentuje pojedynczy segment ulicy między dwoma węzłami grafu.

```cpp
struct StreetSegment {
    int i_Node1, i_Node2;           // IDs węzłów grafu (start-end)
    int i_Tier;                     // 0=arteria, 1=główna, 2=lokalna, 3=uliczka
    bool b_IsInPark;                // Czy to ścieżka w parku
    vector<Point> vec_Geometry;     // Punkty geometrii (może być krzywoliniowa)
    float f_Width;                  // Szerokość w metrach (4.0-12.0)
};
```

**Szerokości według typu:**
- **Tier 0 (Arteria):** 12.0m - główne arterie, autostrady
- **Tier 1 (Główna):** 8.0m - ulice główne
- **Tier 2 (Lokalna):** 6.0m - ulice osiedlowe
- **Tier 3 (Uliczka):** 4.0m - małe uliczki, ścieżki

### BuildingData

Kompletne dane o budynku dla renderingu 3D.

```cpp
struct BuildingData {
    vector<glm::vec2> vec_BaseFootprint;  // Podstawa budynku (4+ punkty 2D)
    glm::vec3 vec3_Position;              // Pozycja środka (x, y=0, z)
    float f_Height;                       // Wysokość głównej bryły (5.0-25.0m)
    bool b_HasRoof;                       // Czy ma dach dwuspadowy
    float f_RoofHeight;                   // Wysokość dachu (jeśli ma)
    glm::vec3 vec3_WallColor;             // Kolor ścian (RGB 0-1)
    glm::vec3 vec3_RoofColor;             // Kolor dachu (RGB 0-1)
    int i_BuildingType;                   // 0=dom, 1=sklep, 2=biuro
};
```

**Typowe rozkłady:**
- **Wysokości:** 50% niskie (5-8m), 35% średnie (8-15m), 15% wysokie (15-25m)
- **Kolory ścian:** 40% cegła (reddish), 30% beż, 30% szary
- **Kolory dachów:** 80% czerwony/bordowy, 20% szary

### GraphNodeData

Węzeł grafu gry z wszystkimi połączeniami transportowymi.

```cpp
struct GraphNodeData {
    int i_ID;                           // Unikalny ID (0 do N-1)
    Point position;                     // Pozycja 2D na mapie

    // Połączenia (IDs innych węzłów)
    vector<int> vec_TaxiConnections;    // Taxi: krótkie, do 180m
    vector<int> vec_BusConnections;     // Autobus: średnie, do 280m
    vector<int> vec_MetroConnections;   // Metro: linie metra
    vector<int> vec_FerryConnections;   // Prom: przeprawy przez rzekę

    // Flagi
    bool b_IsInPark;                    // Węzeł w parku
    bool b_IsNearRiver;                 // Węzeł blisko rzeki (<30m)
    bool b_IsMetroStation;              // Stacja metra
    bool b_IsFerryStop;                 // Przystanek promowy
};
```

---

## Integracja z EmptyEnvironmentState

### Jak dane trafiają do środowiska 3D

**Plik:** `program/src/EmptyEnvironmentState.cpp`

#### Krok 1: Wczytanie danych (OnEnter)

```cpp
void EmptyEnvironmentState::OnEnter(Core::Application* p_App) {
    // ... inicjalizacja OpenGL ...

    LoadSampleMapData();  // ← Wczytanie/generowanie mapy

    // ... reszta inicjalizacji ...
}
```

#### Krok 2: Load/Generate (LoadSampleMapData)

```cpp
void EmptyEnvironmentState::LoadSampleMapData() {
    string mapPath = "maps/example_city.symap";

    // Próba wczytania z pliku
    if (filesystem::exists(mapPath)) {
        m_MapData = MapGen::MapDataSerializer::LoadFromFile(mapPath);
    }

    // Jeśli nie ma pliku lub błąd - generuj
    if (m_MapData.IsEmpty()) {
        m_MapData = MapGen::SampleMapDataGenerator::GenerateRealisticCityMap(42);

        // Zapisz dla następnego razu
        filesystem::create_directories("maps");
        MapGen::MapDataSerializer::SaveToFile(m_MapData, mapPath);
    }

    m_b_MapDataLoaded = true;
}
```

**Przy pierwszym uruchomieniu:**
1. Brak pliku `maps/example_city.symap`
2. Generuje mapę (`GenerateRealisticCityMap`)
3. Zapisuje do pliku JSON
4. Ładuje do `m_MapData`

**Przy kolejnych uruchomieniach:**
1. Plik istnieje
2. Deserializuje z JSON (szybsze!)
3. Ładuje do `m_MapData`

#### Krok 3: Dostęp do danych w kodzie

Po załadowaniu, wszystkie dane są dostępne przez `m_MapData`:

```cpp
void EmptyEnvironmentState::Render(Core::Application* p_App) {
    if (!m_b_MapDataLoaded) return;

    // Przykład: renderuj wszystkie budynki
    for (const auto& building : m_MapData.vec_Buildings) {
        // building.vec3_Position - pozycja
        // building.f_Height - wysokość
        // building.vec_BaseFootprint - podstawa
        // building.vec3_WallColor - kolor
        // ... renderuj mesh ...
    }

    // Przykład: renderuj ulice
    for (const auto& street : m_MapData.vec_Streets) {
        // street.vec_Geometry - punkty drogi
        // street.f_Width - szerokość
        // street.i_Tier - typ (tekstura)
        // ... użyj RoadGenerator ...
    }
}
```

---

## Rendering Pipeline

### Jak renderować elementy mapy

#### 1. Rzeka (WaterRenderer)

```cpp
// Dostęp do danych rzeki
const auto& riverPath = m_MapData.vec_RiverPath;  // vector<Point>

// WaterRenderer już istnieje w EmptyEnvironmentState
m_p_WaterRenderer->SetRiverPath(riverPath);
m_p_WaterRenderer->Render(viewProjectionMatrix, time, scaleMatrix);
```

#### 2. Parki (Grass texture)

```cpp
for (const auto& park : m_MapData.vec_Parks) {
    // park.vertices - vector<Point> (wielokąt)

    // Triangulacja wielokąta
    auto triangles = TriangulatePark(park.vertices);

    // Renderuj z teksturą trawy
    RenderTriangles(triangles, grassTexture);
}
```

#### 3. Ulice (RoadGenerator)

```cpp
#include "RoadGenerator.h"

for (const auto& street : m_MapData.vec_Streets) {
    // Konwersja Point -> glm::vec2
    vector<glm::vec2> roadPoints;
    for (const auto& pt : street.vec_Geometry) {
        roadPoints.push_back(glm::vec2(pt.x, pt.y));
    }

    // Stała szerokość lub zmienna
    vector<float> widths(roadPoints.size(), street.f_Width);

    // Generuj mesh drogi
    auto roadMesh = RoadGenerator::GenerateRoad(
        roadPoints,
        widths,
        2.0f  // texture repeats
    );

    // Wybierz teksturę według tier
    GLuint texture = GetTextureForTier(street.i_Tier);

    // Renderuj mesh
    RenderRoadMesh(roadMesh, texture);
}
```

#### 4. Budynki (Manual mesh creation)

```cpp
for (const auto& building : m_MapData.vec_Buildings) {
    // Podstawa: building.vec_BaseFootprint (vector<glm::vec2>)
    // Wysokość: building.f_Height
    // Pozycja: building.vec3_Position

    // Stwórz ściany
    auto wallMesh = CreateWallMesh(
        building.vec_BaseFootprint,
        building.f_Height,
        building.vec3_Position
    );

    // Stwórz dach (jeśli ma)
    if (building.b_HasRoof) {
        auto roofMesh = CreateRoofMesh(
            building.vec_BaseFootprint,
            building.f_Height,
            building.f_RoofHeight,
            building.vec3_Position
        );
        RenderMesh(roofMesh, building.vec3_RoofColor);
    }

    RenderMesh(wallMesh, building.vec3_WallColor);
}
```

#### 5. Drzewa (TreeInstance)

```cpp
for (const auto& tree : m_MapData.vec_Trees) {
    // tree.position - glm::vec3
    // tree.scale - float (0.8-1.5)
    // tree.seed - unsigned int (dla proceduralnego generowania)

    // Renderuj instancję drzewa
    RenderTreeInstance(tree.position, tree.scale, tree.seed);
}
```

#### 6. Graf gry (Debug visualization)

```cpp
for (const auto& node : m_MapData.vec_GraphNodes) {
    // node.position - Point (2D)
    // node.vec_TaxiConnections - vector<int>

    glm::vec3 pos3D(node.position.x, 0.5f, node.position.y);

    // Renderuj węzeł jako sphere/cube
    RenderNodeMarker(pos3D, node.i_ID);

    // Renderuj połączenia jako linie
    for (int connectedID : node.vec_TaxiConnections) {
        const auto& otherNode = m_MapData.vec_GraphNodes[connectedID];
        glm::vec3 otherPos(otherNode.position.x, 0.5f, otherNode.position.y);
        RenderLine(pos3D, otherPos, COLOR_TAXI);
    }
}
```

---

## System Zapisu/Odczytu (.symap)

### Format pliku

**Rozszerzenie:** `.symap` (Scotland Yard Map)
**Format:** JSON (human-readable)
**Lokalizacja:** `maps/*.symap`

### Przykładowa struktura pliku

```json
{
  "version": "1.0",
  "format": "ScotlandYardMap",
  "width": 1200,
  "height": 900,
  "seed": 42,

  "river": {
    "corner": 4,
    "path": [
      {"x": 0.0, "y": 450.0},
      {"x": 50.0, "y": 455.0}
    ],
    "controlPoints": [...]
  },

  "parks": [
    {
      "center": {"x": 300.0, "y": 400.0},
      "radius": 80.0,
      "sides": 8,
      "vertices": [...]
    }
  ],

  "streets": [
    {
      "node1": 0,
      "node2": 1,
      "tier": 2,
      "width": 6.0,
      "isInPark": false,
      "geometry": [...]
    }
  ],

  "buildings": [...],
  "trees": [...],
  "graph": {
    "nodeCount": 32,
    "nodes": [...]
  }
}
```

### API Serializacji

**Plik:** `program/include/MapDataSerializer.h`

```cpp
#include "MapDataSerializer.h"

// Zapis
GeneratedMapData data = /* ... */;
bool success = MapDataSerializer::SaveToFile(data, "maps/city.symap");

// Odczyt
GeneratedMapData loaded = MapDataSerializer::LoadFromFile("maps/city.symap");
if (!loaded.IsEmpty()) {
    // Dane załadowane poprawnie
}

// Konwersja do/z JSON
nlohmann::json j = MapDataSerializer::ToJson(data);
GeneratedMapData data2 = MapDataSerializer::FromJson(j);

// Walidacja
bool valid = MapDataSerializer::ValidateJson(j);
```

---

## Integracja z Istniejącym Generatorem

### Jak podłączyć MapGenerator.cpp

Obecny generator (`MapGenerator.cpp`) tworzy dane w starym formacie. Aby zintegrować:

#### Opcja 1: Wrapper (Recommended)

```cpp
// W MapGenerator.cpp - dodaj metodę eksportu
GeneratedMapData MapGenerator::ExportToGeneratedMapData() {
    GeneratedMapData data;

    // Wymiary
    data.i_Width = width;
    data.i_Height = height;

    // Rzeka
    for (const auto& pt : riverBezierPath) {
        data.vec_RiverPath.push_back(pt);
    }

    // Parki
    for (const auto& park : parks) {
        data.vec_Parks.push_back(park);
    }

    // Triangulacja -> Ulice
    for (const auto& edge : delaunayEdges) {
        StreetSegment street;
        street.i_Node1 = edge.nodeA;
        street.i_Node2 = edge.nodeB;
        street.i_Tier = DetermineTier(edge);
        street.vec_Geometry = {edge.pointA, edge.pointB};
        street.f_Width = StreetSegment::GetWidthForTier(street.i_Tier);
        data.vec_Streets.push_back(street);
    }

    // Graf gry
    for (int i = 0; i < numNodes; ++i) {
        GraphNodeData node;
        node.i_ID = i;
        node.position = nodePositions[i];
        node.vec_TaxiConnections = taxiGraph[i];
        node.vec_BusConnections = busGraph[i];
        // ... etc
        data.vec_GraphNodes.push_back(node);
    }

    return data;
}
```

#### Opcja 2: Bezpośrednia integracja

Zmodyfikuj `MapGenerator` aby bezpośrednio zapisywał do `GeneratedMapData` zamiast własnych struktur.

---

## Typowe Wartości i Rozkłady

### Realistyczna mapa miasta (seed=42)

```
Węzły grafu:        32 (siatka 5×4 + pośrednie)
Ulice:              ~47 segmentów
Budynki:            50-80 (gęsta zabudowa)
Drzewa:             ~40 (w parkach)
Parki:              3 (wielokąty 6-10 boków)
Rzeka:              1 (wije się przez mapę)
Mosty:              2-4 (zależnie od rzeki)

Połączenia taxi:    max 6 na węzeł, dystans ≤180m
Połączenia bus:     max 3 na węzeł, dystans ≤280m
Połączenia metro:   2 linie (pionowe)
Połączenia ferry:   nad rzeką
```

### Rozkład wysokości budynków

```
50% - Niskie    (5.0 - 8.0m)   - domy jednorodzinne
35% - Średnie   (8.0 - 15.0m)  - kamienice
15% - Wysokie   (15.0 - 25.0m) - wieżowce
```

### Rozkład kolorów budynków

```
Ściany:
  40% - Cegła     (0.85, 0.65, 0.55)
  30% - Beż       (0.92, 0.88, 0.75)
  30% - Szary     (0.75, 0.75, 0.75)

Dachy:
  80% - Czerwony  (0.6-0.8, 0.2-0.3, 0.2-0.3)
  20% - Szary     (0.4-0.5, 0.4-0.5, 0.45-0.55)
```

---

## Quick Start dla Nowego Developera

### 1. Znajdź dane mapy

```cpp
// W EmptyEnvironmentState masz dostęp do:
m_MapData          // GeneratedMapData - główna struktura
m_b_MapDataLoaded  // bool - czy załadowano
```

### 2. Iteruj po elementach

```cpp
// Budynki
for (const auto& building : m_MapData.vec_Buildings) {
    // Renderuj building
}

// Ulice
for (const auto& street : m_MapData.vec_Streets) {
    // Renderuj street używając RoadGenerator
}

// Drzewa
for (const auto& tree : m_MapData.vec_Trees) {
    // Renderuj tree instance
}
```

### 3. Użyj istniejących narzędzi

```cpp
#include "RoadGenerator.h"      // Generowanie mesh'y dróg
#include "WaterRenderer.h"      // Rendering wody/rzeki
#include "BuildingGenerator.h"  // (jeśli istnieje) Generator budynków
#include "TreeGenerator.h"      // (jeśli istnieje) Generator drzew
```

### 4. Zapisz/Wczytaj własną mapę

```cpp
#include "MapDataSerializer.h"

// Zapisz
MapDataSerializer::SaveToFile(m_MapData, "maps/my_map.symap");

// Wczytaj
m_MapData = MapDataSerializer::LoadFromFile("maps/my_map.symap");
```

---

## Pliki Referencyjne

### Header files
- `program/include/GeneratedMapData.h` - Struktury danych
- `program/include/SampleMapDataGenerator.h` - Generator testowych map
- `program/include/MapDataSerializer.h` - Serializacja JSON
- `program/include/EmptyEnvironmentState.h` - Integracja z 3D

### Source files
- `program/src/SampleMapDataGenerator.cpp` - Implementacja generatora (500+ linii)
- `program/src/MapDataSerializer.cpp` - Implementacja serializacji (460+ linii)
- `program/src/EmptyEnvironmentState.cpp` - Wczytywanie i rendering

### Documentation
- `program/docs/SYMAP_FORMAT.md` - Pełna specyfikacja formatu .symap
- `program/docs/MAP_DATA_USAGE.md` - Szczegółowa dokumentacja API
- `program/docs/GENERATED_MAP_DATA_README.md` - Przegląd systemu

---

## Kompilacja

System jest w pełni zintegrowany z CMake:

```bash
cd program
cmake --build build

# Uruchom
./build/bin/ScotlandYardPlusPlus
```

Przy pierwszym uruchomieniu:
1. Wygeneruje mapę
2. Zapisze do `maps/example_city.symap`
3. Wyświetli statystyki w konsoli

---

## Punkty Rozszerzenia

### Dla zespołu graficznego
- Implementacja `RenderMapData()` w `EmptyEnvironmentState.cpp` (obecnie placeholder)
- Dodanie shader'ów dla różnych typów ulic (tier 0-3)
- Tworzenie mesh'y budynków z `BuildingData`
- System LOD dla drzew i budynków

### Dla zespołu AI
- Graf gry dostępny przez `m_MapData.vec_GraphNodes`
- Wszystkie połączenia: taxi, bus, metro, ferry
- Flagi: `b_IsInPark`, `b_IsNearRiver`, etc.
- Pathfinding wykorzystujący graf

### Dla zespołu generatora
- Integracja `MapGenerator.cpp` z `GeneratedMapData`
- Import danych z OpenStreetMap
- Proceduralne generowanie wariantów map
- System seed'ów dla reprodukowalności
