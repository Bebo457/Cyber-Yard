#include "EmptyEnvironmentState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include "RoadGenerator.h"
#include "SampleMapDataGenerator.h"
#include "MapDataSerializer.h"
#include "MapGenerator.h"
#include "HighwayGenerator.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <limits>
#include <type_traits>
#include <nlohmann/json.hpp>

namespace {

struct BridgeVertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct AccessorView {
    const uint8_t* data = nullptr;
    size_t count = 0;
    size_t stride = 0;
    int componentType = 0;
    int components = 0;
};

size_t ComponentSize(int componentType) {
    switch (componentType) {
    case 5120: return sizeof(int8_t);
    case 5121: return sizeof(uint8_t);
    case 5122: return sizeof(int16_t);
    case 5123: return sizeof(uint16_t);
    case 5125: return sizeof(uint32_t);
    case 5126: return sizeof(float);
    default: return 0;
    }
}

int ComponentCount(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2") return 2;
    if (type == "VEC3") return 3;
    if (type == "VEC4") return 4;
    return 0;
}

bool ParseGlbFile(const std::string& path, nlohmann::json& outJson, std::vector<uint8_t>& outBin) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[Bridge] Cannot open GLB: " << path << std::endl;
        return false;
    }

    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size < 20) {
        std::cerr << "[Bridge] GLB too small: " << path << std::endl;
        return false;
    }

    std::vector<uint8_t> bytes(static_cast<size_t>(size));
    file.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
    if (!file) {
        std::cerr << "[Bridge] Failed to read GLB: " << path << std::endl;
        return false;
    }

    auto read32 = [&](size_t offset) {
        uint32_t value = 0;
        std::memcpy(&value, bytes.data() + offset, sizeof(uint32_t));
        return value;
    };

    const uint32_t magic = read32(0);
    const uint32_t version = read32(4);
    if (magic != 0x46546C67 || version != 2) {
        std::cerr << "[Bridge] Unsupported GLB header in " << path << std::endl;
        return false;
    }

    size_t offset = 12;
    bool jsonLoaded = false;
    bool binLoaded = false;

    while (offset + 8 <= bytes.size()) {
        const uint32_t chunkLen = read32(offset);
        const uint32_t chunkType = read32(offset + 4);
        offset += 8;

        if (offset + chunkLen > bytes.size()) {
            std::cerr << "[Bridge] Truncated GLB chunk in " << path << std::endl;
            return false;
        }

        if (chunkType == 0x4E4F534A) { // JSON
            try {
                outJson = nlohmann::json::parse(
                    std::string(reinterpret_cast<const char*>(bytes.data() + offset), chunkLen));
                jsonLoaded = true;
            }
            catch (const std::exception& e) {
                std::cerr << "[Bridge] JSON parse error: " << e.what() << std::endl;
                return false;
            }
        }
        else if (chunkType == 0x004E4942) { // BIN
            outBin.assign(bytes.begin() + offset, bytes.begin() + offset + chunkLen);
            binLoaded = true;
        }

        offset += chunkLen;
    }

    if (!jsonLoaded || !binLoaded) {
        std::cerr << "[Bridge] Missing JSON or BIN chunk in " << path << std::endl;
    }
    return jsonLoaded && binLoaded;
}

bool GetAccessorView(const nlohmann::json& root, const std::vector<uint8_t>& bin,
    int accessorIndex, AccessorView& out) {
    if (accessorIndex < 0) return false;
    if (!root.contains("accessors") || accessorIndex >= static_cast<int>(root["accessors"].size())) return false;

    const auto& accessor = root["accessors"][accessorIndex];
    const int bufferViewIndex = accessor.value("bufferView", -1);
    if (bufferViewIndex < 0) return false;

    if (!root.contains("bufferViews") || bufferViewIndex >= static_cast<int>(root["bufferViews"].size())) return false;
    const auto& bufferView = root["bufferViews"][bufferViewIndex];

    const int bufferIndex = bufferView.value("buffer", -1);
    if (bufferIndex != 0) return false; 

    const size_t bufferOffset = bufferView.value("byteOffset", 0);
    const size_t accessorOffset = accessor.value("byteOffset", 0);
    const size_t count = accessor.value("count", 0);
    const int componentType = accessor.value("componentType", 0);
    const std::string typeStr = accessor.value("type", "");

    const size_t compSize = ComponentSize(componentType);
    const int components = ComponentCount(typeStr);
    if (compSize == 0 || components == 0) return false;

    size_t stride = bufferView.value("byteStride", 0);
    if (stride == 0) {
        stride = compSize * static_cast<size_t>(components);
    }

    const size_t start = bufferOffset + accessorOffset;
    const size_t needed = stride * (count ? count - 1 : 0) + compSize * static_cast<size_t>(components);
    if (start + needed > bin.size()) return false;

    out.data = bin.data() + start;
    out.count = count;
    out.stride = stride;
    out.componentType = componentType;
    out.components = components;
    return true;
}

uint32_t ReadIndex(const AccessorView& view, size_t i) {
    const uint8_t* ptr = view.data + i * view.stride;
    switch (view.componentType) {
    case 5121: { // uint8
        return static_cast<uint32_t>(*ptr);
    }
    case 5123: { // uint16
        uint16_t v = 0;
        std::memcpy(&v, ptr, sizeof(uint16_t));
        return static_cast<uint32_t>(v);
    }
    case 5125: { // uint32
        uint32_t v = 0;
        std::memcpy(&v, ptr, sizeof(uint32_t));
        return v;
    }
    default:
        return 0;
    }
}

constexpr float k_MapWidth = 1200.0f;
constexpr float k_MapHeight = 900.0f;
constexpr float k_PlaneWidth = 24.0f;
constexpr float k_PlaneDepth = 18.0f;
constexpr float k_MapOffsetX = -1.0f;
constexpr float k_MapOffsetZ = -1.0f;

} // namespace

namespace ScotlandYard {
    namespace States {

        EmptyEnvironmentState::EmptyEnvironmentState() {}
        EmptyEnvironmentState::~EmptyEnvironmentState() = default;

        // --- IMPLEMENTACJA InjectMapData ---
        void EmptyEnvironmentState::InjectMapData(
            const std::vector<CityGen::Point>& vec_Nodes,
            const std::vector<CityGen::Road>& vec_Roads,
            const std::vector<MapGen::Park>& vec_Parks,
            const std::vector<MapGen::Point>& vec_RiverPath,
            const std::vector<CityGen::Highway>& vec_Highways
        )
        {
            std::cout << "[EmptyEnvironmentState] Injecting map data from Generator..." << std::endl;

            // 1. Reset danych
            m_MapData = MapGen::GeneratedMapData();

            m_MapData.vec_RiverPath = vec_RiverPath;
            m_MapData.vec_Parks = vec_Parks;

            m_MapData.vec_GraphNodes.clear();
            // Uzywamy decltype zeby uniknac bledu "niezadeklarowany identyfikator", 
            using GraphNodeType = typename std::remove_reference<decltype(m_MapData.vec_GraphNodes)>::type::value_type;

            for (size_t i = 0; i < vec_Nodes.size(); ++i) {
                GraphNodeType node;
                node.i_ID = (int)i;
                node.position = MapGen::Point{ vec_Nodes[i].x, vec_Nodes[i].y };


                m_MapData.vec_GraphNodes.push_back(node);
            }

            // 4. Konwersja Ulic (CityGen::Road -> MapGen::Street)
            m_MapData.vec_Streets.clear();
            // Podobnie dla Street/Edge
            using StreetType = typename std::remove_reference<decltype(m_MapData.vec_Streets)>::type::value_type;

            for (const auto& r : vec_Roads) {
                if (r.isDeleted) continue;

                StreetType street;
                street.i_Node1 = r.startNodeIdx;
                street.i_Node2 = r.endNodeIdx;

                // Mapowanie typu drogi na Tier (do wizualizacji)
                if (r.type == CityGen::RoadType::HIGHWAY) {
                    street.i_Tier = 0;
                    street.f_Width = 0.25f;
                }
                else {
                    street.i_Tier = 2; // Lokalna
                    street.f_Width = 0.25f;
                }

                street.b_IsInPark = false;

                m_MapData.vec_Streets.push_back(street);
            }

            m_vec_Highways = vec_Highways;
            m_vec_HighwayNodes = vec_Nodes;
            m_vec_HighwayRoads = vec_Roads;

            m_b_MapDataLoaded = true;
            std::cout << "[EmptyEnvironmentState] Injection complete. Nodes: " << m_MapData.vec_GraphNodes.size()
                << ", Streets: " << m_MapData.vec_Streets.size()
                << ", Parks: " << m_MapData.vec_Parks.size()
                << ", River points: " << m_MapData.vec_RiverPath.size() 
                << ", Highways: " << m_vec_Highways.size() << std::endl;
        }
        // -----------------------------------

        void EmptyEnvironmentState::CreateShaders() {
            // --- OLD SHADER (keep as is) ---
            const char* vsSrc = R"(#version 330 core
                layout(location=0) in vec3 aPos;
                layout(location=1) in vec3 aNormal;
                layout(location=2) in vec2 aUV;

                uniform mat4 uMVP;
                out vec2 vUV;

                void main(){
                    vUV = aUV;
                    gl_Position = uMVP * vec4(aPos,1.0);
                }
            )";

            const char* fsSrc = R"(#version 330 core
                in vec2 vUV;

                uniform sampler2D uSidewalk;
                uniform sampler2D uGrass;
                uniform sampler2D uMask;      
                uniform int uUseMask;         
                uniform vec2 uTileUV;         

                out vec4 FragColor;

                void main(){
                    vec2 tiledUV = vUV * uTileUV;

                    vec4 sidewalk = texture(uSidewalk, tiledUV);
                    vec4 grass    = texture(uGrass, tiledUV);

                    if(uUseMask == 1){
                        vec3 m = texture(uMask, vUV).rgb;

                        // parks = green = grass
                        float park = step(0.35, m.g);

                        FragColor = mix(sidewalk, grass, park);
                    } else {
                        FragColor = sidewalk;
                    }
                }
            )";

            GLuint vs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vs, 1, &vsSrc, nullptr);
            glCompileShader(vs);

            GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fs, 1, &fsSrc, nullptr);
            glCompileShader(fs);

            m_ShaderProgram = glCreateProgram();
            glAttachShader(m_ShaderProgram, vs);
            glAttachShader(m_ShaderProgram, fs);
            glLinkProgram(m_ShaderProgram);

            glDeleteShader(vs);
            glDeleteShader(fs);

            // --- NEW HARD-MASK SHADER FOR ROAD ---
            const char* roadVsSrc = R"(#version 330 core
                layout(location=0) in vec3 aPos;
                layout(location=1) in vec3 aNormal;
                layout(location=2) in vec2 aUV;

                uniform mat4 uMVP;
                out vec2 vUV;

                void main() {
                    vUV = aUV;
                    gl_Position = uMVP * vec4(aPos, 1.0);
                }
            )";

            const char* roadFsSrc = R"(#version 330 core
                in vec2 vUV;

                uniform sampler2D uRoad;
                uniform vec2 uTileUV;

                out vec4 FragColor;

                void main() {
                    vec2 tiledUV = vUV * uTileUV;
                    FragColor = texture(uRoad, tiledUV);
                }
            )";

            GLuint roadVs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(roadVs, 1, &roadVsSrc, nullptr);
            glCompileShader(roadVs);

            GLuint roadFs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(roadFs, 1, &roadFsSrc, nullptr);
            glCompileShader(roadFs);

            m_ShaderRoad = glCreateProgram();  // <-- new member variable GLuint m_ShaderRoad
            glAttachShader(m_ShaderRoad, roadVs);
            glAttachShader(m_ShaderRoad, roadFs);
            glLinkProgram(m_ShaderRoad);

            glDeleteShader(roadVs);
            glDeleteShader(roadFs);

            // --- Simple bridge shader --
            const char* modelVsSrc = R"(#version 330 core
        layout(location=0) in vec3 aPos;
        layout(location=1) in vec3 aNormal;
        layout(location=2) in vec2 aUV;

        uniform mat4 uMVP;
        uniform mat4 uModel;
        out float vLight;

        out vec2 vUV;
        void main() {
            vec3 normalWS = normalize(mat3(uModel) * aNormal);
            vec3 lightDir = normalize(vec3(0.3, 1.0, 0.2));
            vLight = max(dot(normalWS, lightDir), 0.2);
            vUV = aUV;
            gl_Position = uMVP * vec4(aPos, 1.0);
        }
    )";

            const char* modelFsSrc = R"(#version 330 core
        in float vLight;
        uniform vec3 uColor;
        uniform sampler2D uTex;
        uniform int uHasTex;
        in vec2 vUV;
        out vec4 FragColor;
        void main() {
            vec3 base = uColor;
            if (uHasTex == 1) {
                vec4 tex = texture(uTex, vUV);
                base = tex.rgb;
            }
            FragColor = vec4(base * vLight, 1.0);
        }
    )";

            GLuint modelVs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(modelVs, 1, &modelVsSrc, nullptr);
            glCompileShader(modelVs);

            GLuint modelFs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(modelFs, 1, &modelFsSrc, nullptr);
            glCompileShader(modelFs);

            m_ShaderBridge = glCreateProgram();
            glAttachShader(m_ShaderBridge, modelVs);
            glAttachShader(m_ShaderBridge, modelFs);
            glLinkProgram(m_ShaderBridge);

            glDeleteShader(modelVs);
            glDeleteShader(modelFs);
        }

        void EmptyEnvironmentState::CreatePlane() {
            float f_PlaneVertices[] = {
                // pos                 // normal       // uv
                -1.0f, 0.0f, 17.0f,     0.0f,1.0f,0.0f,  0.0f,1.0f,
                 23.0f,0.0f, 17.0f,     0.0f,1.0f,0.0f,  1.0f,1.0f,
                 23.0f,0.0f,-1.0f,      0.0f,1.0f,0.0f,  1.0f,0.0f,
                -1.0f, 0.0f, 17.0f,     0.0f,1.0f,0.0f,  0.0f,1.0f,
                 23.0f,0.0f,-1.0f,      0.0f,1.0f,0.0f,  1.0f,0.0f,
                -1.0f, 0.0f,-1.0f,      0.0f,1.0f,0.0f,  0.0f,0.0f
            };

            glGenVertexArrays(1, &m_VAO_Plane);
            glGenBuffers(1, &m_VBO_Plane);
            glBindVertexArray(m_VAO_Plane);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Plane);
            glBufferData(GL_ARRAY_BUFFER, sizeof(f_PlaneVertices), f_PlaneVertices, GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glBindVertexArray(0);
        }

        void EmptyEnvironmentState::OnEnter(Core::Application* p_App) {
            bool b_EnableTestEnvironment = true;
            // CreateTestRoad(p_App);

            if (p_App && !p_App->IsTrainingMode() && b_EnableTestEnvironment) {
                const_cast<Core::Application*>(p_App)->UpdateUIScaling();

                glEnable(GL_DEPTH_TEST);
                CreateShaders();
                CreatePlane();
                m_TexSidewalk = p_App->LoadTexture(p_App->GetAssetPath("textures/sidewalk.jpg"));
                m_TexGrass = p_App->LoadTexture(p_App->GetAssetPath("textures/grass.png"));
                TryLoadGeneratedMap(p_App);
                LoadBridgeModel(p_App);
                SetBridgeLength(4.0f);
                
                // water renderer
                m_p_WaterRenderer = std::make_unique<Rendering::WaterRenderer>();
                m_p_WaterRenderer->Initialize();
                m_p_WaterRenderer->SetWaterHeight(0.002f);
                m_mat4_GlobalScaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(0.1f, 0.1f, 0.1f));

                m_b_RiverStripLoaded = false;
                LoadPolygonData(p_App);

                if (!m_b_MapDataLoaded) {
                    LoadSampleMapData();
                    BuildRiverFromMapData();
                    BuildHighwaysFromMapData(p_App);
                }
                else {
                    std::cout << "[EmptyEnvironmentState] Skipping sample data load (Map injected)." << std::endl;
                    BuildRiverFromMapData();
                    BuildHighwaysFromMapData(p_App);
                }

                // HUD setup: load camera icon and hook toggle
                std::string s_IconPath = p_App->GetAssetPath("icons/camera_icon.png");
                UI::LoadCameraIconPNG(s_IconPath.c_str(), p_App);
                UI::SetCameraToggleCallback([this]() { this->m_b_Camera3D = !this->m_b_Camera3D; });

                // Initialize top bar with labels and hide all counts (no players here)
                std::vector<std::string> vec_Labels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };
                std::vector<UI::Color> vec_Colors = {
                    {0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f},      // Black
                    {0xE2 / 255.0f, 0x70 / 255.0f, 0x3F / 255.0f, 1.0f},      // Taxi (orange)
                    {0xED / 255.0f, 0xD1 / 255.0f, 0x00 / 255.0f, 1.0f},      // Double (yellow)
                    {0xF5 / 255.0f, 0x51 / 255.0f, 0xAE / 255.0f, 1.0f},      // Metro (pink/magenta)
                    {0x41 / 255.0f, 0x84 / 255.0f, 0x3D / 255.0f, 1.0f},      // Bus (green)
                };
                std::vector<int> vec_Counts = { -1, -1, -1, -1, -1, -1 };
                UI::SetTopBar(vec_Labels, vec_Colors, vec_Counts);
                UI::SetMrXButtonsVisible(false);
                UI::SetMrXButtonsEnabled(false, false);

                // Initialize default ticket slots and round for full HUD visuals
                std::vector<UI::TicketSlot> vec_Slots(UI::k_TicketSlotCount);
                UI::SetTicketStates(vec_Slots);
                UI::SetRound(1);

            }

            UI::SetPauseCallback([this]() {
                UI::ShowPausedModal(true);
                });

            UI::SetPausedResumeCallback([this]() {
                UI::ShowPausedModal(false);
                this->m_b_GameActive = true;
                });

            UI::SetPausedDebugCallback([this]() {
                });

            UI::SetPausedMenuCallback([this, p_App]() {
                UI::ShowPausedModal(false);
                if (p_App && p_App->GetStateManager()) {
                    p_App->GetStateManager()->ChangeState("menu", p_App);
                }
                });
        }

        void EmptyEnvironmentState::OnExit(Core::Application* p_App) {
            if (m_VBO_Plane) {
                glDeleteBuffers(1, &m_VBO_Plane);
                m_VBO_Plane = 0;
            }
            if (m_VAO_Plane) {
                glDeleteVertexArrays(1, &m_VAO_Plane);
                m_VAO_Plane = 0;
            }
            if (m_ShaderProgram) {
                glDeleteProgram(m_ShaderProgram);
                m_ShaderProgram = 0;
            }
            if (m_TexSidewalk) { p_App->UnloadTexture(m_TexSidewalk); m_TexSidewalk = 0; }
            if (m_TexGrass) { p_App->UnloadTexture(m_TexGrass);    m_TexGrass = 0; }
            if (m_TexMask) { p_App->UnloadTexture(m_TexMask);     m_TexMask = 0; }
            m_b_UseMask = false;

            if (m_p_WaterRenderer) {
                m_p_WaterRenderer->Cleanup();
                m_p_WaterRenderer.reset();
            }

            if (m_VBO_Road) { glDeleteBuffers(1, &m_VBO_Road); m_VBO_Road = 0; }
            if (m_EBO_Road) { glDeleteBuffers(1, &m_EBO_Road); m_EBO_Road = 0; }
            if (m_VAO_Road) { glDeleteVertexArrays(1, &m_VAO_Road); m_VAO_Road = 0; }
            if (m_TexRoad) { p_App->UnloadTexture(m_TexRoad); m_TexRoad = 0; }
            if (m_VBO_Bridge) { glDeleteBuffers(1, &m_VBO_Bridge); m_VBO_Bridge = 0; }
            if (m_EBO_Bridge) { glDeleteBuffers(1, &m_EBO_Bridge); m_EBO_Bridge = 0; }
            if (m_VAO_Bridge) { glDeleteVertexArrays(1, &m_VAO_Bridge); m_VAO_Bridge = 0; }
            if (m_ShaderBridge) { glDeleteProgram(m_ShaderBridge); m_ShaderBridge = 0; }
            if (m_TexBridge) { p_App->UnloadTexture(m_TexBridge); m_TexBridge = 0; m_b_BridgeHasTexture = false; }

        }

        void EmptyEnvironmentState::OnPause() {
            m_b_GameActive = false;
        }

        void EmptyEnvironmentState::OnResume() {
            m_b_GameActive = true;
        }

        void EmptyEnvironmentState::TryLoadGeneratedMap(Core::Application* p_App) {
            std::string s_Path = "generated_map.bmp";
            if (std::filesystem::exists(s_Path)) {
                m_TexMask = p_App->LoadTexture(s_Path);
                m_b_UseMask = (m_TexMask != 0);
            }
            else {
                m_b_UseMask = false;
                m_TexMask = 0;
            }
        }

        void EmptyEnvironmentState::LoadPolygonData(Core::Application* p_App) {
            // PRIORITY 1 - injected map data
            if (m_b_MapDataLoaded && !m_MapData.vec_Parks.empty()) {
                BuildParksFromMapData();
                return;
            }

            // PRIORITY 2 - Load from JSON file
            std::string s_JsonPath = "generated_map.json";
            if (!std::filesystem::exists(s_JsonPath)) {
                BuildFallbackRiver();
                return;
            }

            const float f_ScaleX = k_PlaneWidth / k_MapWidth;
            const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
            const float f_OffsetX = k_MapOffsetX;
            const float f_OffsetZ = k_MapOffsetZ;

            try {
                std::ifstream file(s_JsonPath);
                nlohmann::json json;
                file >> json;

                if (json.contains("parks") && json["parks"].is_array()) {
                    for (const auto& parkJson : json["parks"]) {
                        if (parkJson.contains("vertices") && parkJson["vertices"].is_array()) {
                            std::vector<glm::vec2> vec_Vertices;
                            for (const auto& jsonVertex : parkJson["vertices"]) {
                                float f_X = jsonVertex["x"].get<float>();
                                float f_Y = jsonVertex["y"].get<float>();
                                float f_ScaledX = f_X * f_ScaleX + f_OffsetX;
                                float f_ScaledZ = f_Y * f_ScaleZ + f_OffsetZ;
                                vec_Vertices.emplace_back(f_ScaledX, f_ScaledZ);
                            }

                            if (vec_Vertices.size() >= 3) {
                                auto parkRenderer = std::make_unique<Rendering::PolygonRenderer>();
                                if (parkRenderer->Initialize()) {
                                    parkRenderer->SetPolygon(vec_Vertices, 0.001f);
                                    m_vec_ParkRenderers.push_back(std::move(parkRenderer));
                                }
                            }
                        }
                    }
                    std::cout << "[PolygonLoader] Loaded " << m_vec_ParkRenderers.size() << " park polygons from JSON." << std::endl;
                }

                if (json.contains("river") && json["river"].contains("path") && json["river"]["path"].is_array()) {
                    std::vector<glm::vec2> vec_RiverPath;
                    for (const auto& jsonPoint : json["river"]["path"]) {
                        float f_X = jsonPoint["x"].get<float>();
                        float f_Y = jsonPoint["y"].get<float>();
                        float f_ScaledX = f_X * f_ScaleX + f_OffsetX;
                        float f_ScaledZ = f_Y * f_ScaleZ + f_OffsetZ;
                        vec_RiverPath.emplace_back(f_ScaledX, f_ScaledZ);
                    }

                    if (vec_RiverPath.size() >= 2) {
                        float f_RiverWidth = 50.0f * f_ScaleX;

                        m_p_RiverRenderer = std::make_unique<Rendering::PolygonRenderer>();
                        if (m_p_RiverRenderer->Initialize()) {
                            m_p_RiverRenderer->SetRiverStrip(vec_RiverPath, f_RiverWidth, 0.001f);
                        }

                        if (m_p_WaterRenderer) {
                            m_p_WaterRenderer->SetRiverStrip(vec_RiverPath, f_RiverWidth);
                        }

                        m_b_RiverStripLoaded = true;
                        std::cout << "[PolygonLoader] Loaded river strip with " << vec_RiverPath.size() << " path points from JSON." << std::endl;
                    }
                }

            }
            catch (const std::exception& e) {
                std::cerr << "[PolygonLoader] Error loading polygon data: " << e.what() << std::endl;
            }

            if (!m_b_RiverStripLoaded) {
                BuildFallbackRiver();
            }
        }

        void EmptyEnvironmentState::Update(float f_DeltaTime) {
            m_f_Time += f_DeltaTime;

            if (!m_b_GameActive) return;
            if (!m_b_Camera3D) return;

            // Angle friction and limits
            m_f_CameraAngleVelocity *= k_CameraScrollFriction;
            if (fabs(m_f_CameraAngleVelocity) < 0.0001f) {
                m_f_CameraAngleVelocity = 0.0f;
            }

            m_f_CameraAngle += m_f_CameraAngleVelocity;

            if (m_f_CameraAngle < k_MinCameraAngle) {
                m_f_CameraAngle = k_MinCameraAngle;
                m_f_CameraAngleVelocity = 0.0f;
            }
            if (m_f_CameraAngle > k_MaxCameraAngle) {
                m_f_CameraAngle = k_MaxCameraAngle;
                m_f_CameraAngleVelocity = 0.0f;
            }

            // Update front vector from angle
            m_vec3_CameraFront.x = 0.0f;
            m_vec3_CameraFront.y = sin(m_f_CameraAngle);
            m_vec3_CameraFront.z = -cos(m_f_CameraAngle);
            m_vec3_CameraFront = glm::normalize(m_vec3_CameraFront);

            // Scroll-to-forward acceleration
            if (fabs(m_f_CameraAngleVelocity) > 0.0001f) {
                glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
                float f_ForwardAccel = -m_f_CameraAngleVelocity * k_CameraScrollToForwardRatio;
                m_vec3_CameraVelocity += vec3_Forward * f_ForwardAccel;

                float f_Speed = glm::length(m_vec3_CameraVelocity);
                if (f_Speed > k_MaxCameraSpeed) {
                    m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
                }
            }

            // Keyboard (WASD)
            const Uint8* p_KeyState = SDL_GetKeyboardState(nullptr);
            if (p_KeyState[SDL_SCANCODE_W]) AccelerateCameraForward(f_DeltaTime);
            if (p_KeyState[SDL_SCANCODE_S]) AccelerateCameraBackward(f_DeltaTime);
            if (p_KeyState[SDL_SCANCODE_A]) AccelerateCameraLeft(f_DeltaTime);
            if (p_KeyState[SDL_SCANCODE_D]) AccelerateCameraRight(f_DeltaTime);

            // Physics integration
            UpdateCameraPhysics(f_DeltaTime);
        }

        void EmptyEnvironmentState::Render(Core::Application* p_App) {
            if (p_App->IsTrainingMode()) return;

            glClearColor(0.12f, 0.13f, 0.15f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            int i_W = p_App->GetVirtualWidth();
            int i_H = p_App->GetVirtualHeight();

            glm::mat4 mat4_View, mat4_Projection;

            if (m_b_Camera3D) {
                m_vec3_CameraPosition = m_vec3_Saved3DCameraPosition;
                glm::vec3 vec3_Target = m_vec3_CameraPosition + m_vec3_CameraFront;
                mat4_View = glm::lookAt(m_vec3_CameraPosition, vec3_Target, m_vec3_CameraUp);
                mat4_Projection = glm::perspective(glm::radians(45.0f), (float)i_W / (float)i_H, 0.1f, 100.0f);
            }
            else {
                // Simple top-down ortho similar to GameState 2D mode
                glm::vec3 vec3_CamPos = glm::vec3(11.0f, 5.0f, 8.0f);
                glm::vec3 vec3_Target = glm::vec3(11.0f, 0.0f, 8.0f);
                glm::vec3 vec3_Up = glm::vec3(0.0f, 0.0f, -1.0f);
                mat4_View = glm::lookAt(vec3_CamPos, vec3_Target, vec3_Up);

                float f_Aspect = (float)i_W / (float)i_H;
                float f_HalfHeight = 10.0f;
                float f_HalfWidth = f_HalfHeight * f_Aspect;
                mat4_Projection = glm::ortho(-f_HalfWidth, f_HalfWidth, -f_HalfHeight, f_HalfHeight, 0.1f, 10.0f);
            }

            if (m_p_WaterRenderer) {
                m_p_WaterRenderer->RenderRiverStrip(mat4_Projection * mat4_View, m_f_Time, glm::mat4(1.0f));
            }

            glm::mat4 mat4_Model(1.0f);
            glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;

            glUseProgram(m_ShaderProgram);

            GLint mvpLoc = glGetUniformLocation(m_ShaderProgram, "uMVP");
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

            // Tiling
            GLint tileLoc = glGetUniformLocation(m_ShaderProgram, "uTileUV");
            glUniform2f(tileLoc, 12.0f, 9.0f);

            // Bind sidewalk
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_TexSidewalk);
            glUniform1i(glGetUniformLocation(m_ShaderProgram, "uSidewalk"), 0);

            // Bind grass
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, m_TexGrass);
            glUniform1i(glGetUniformLocation(m_ShaderProgram, "uGrass"), 1);

            // Mask stays enabled to keep fallback grass where polygon data is absent
            glUniform1i(glGetUniformLocation(m_ShaderProgram, "uUseMask"), m_b_UseMask ? 1 : 0);
            if (m_b_UseMask) {
                glActiveTexture(GL_TEXTURE2);
                glBindTexture(GL_TEXTURE_2D, m_TexMask);
                glUniform1i(glGetUniformLocation(m_ShaderProgram, "uMask"), 2);
            }

            glBindVertexArray(m_VAO_Plane);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            // --- Render road ---
           if (!m_RoadMeshes.empty() && m_TexRoad) {
                glUseProgram(m_ShaderRoad);
                GLint mvpLoc = glGetUniformLocation(m_ShaderRoad, "uMVP");
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View));

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_TexRoad);
                glUniform1i(glGetUniformLocation(m_ShaderRoad, "uRoad"), 0);

                GLint tileLoc = glGetUniformLocation(m_ShaderRoad, "uTileUV");
                glUniform2f(tileLoc, 1.0f, 1.0f);

                for (const auto& road : m_RoadMeshes) {
                    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glBindVertexArray(road.VAO);
                    glDrawElements(GL_TRIANGLES, road.indexCount, GL_UNSIGNED_INT, 0);
                }
                glBindVertexArray(0);
            }

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            // Bridge model
            if (m_VAO_Bridge && m_ShaderBridge && m_BridgeIndexCount > 0) {
                glm::mat4 mat4_BridgeModel = glm::translate(glm::mat4(1.0f), m_vec3_BridgePosition);
                mat4_BridgeModel = glm::scale(mat4_BridgeModel, m_vec3_BridgeScale);
                glm::mat4 mat4_BridgeMVP = mat4_Projection * mat4_View * mat4_BridgeModel;

                glUseProgram(m_ShaderBridge);
                glUniformMatrix4fv(glGetUniformLocation(m_ShaderBridge, "uMVP"), 1, GL_FALSE, glm::value_ptr(mat4_BridgeMVP));
                glUniformMatrix4fv(glGetUniformLocation(m_ShaderBridge, "uModel"), 1, GL_FALSE, glm::value_ptr(mat4_BridgeModel));
                glUniform3f(glGetUniformLocation(m_ShaderBridge, "uColor"), 0.82f, 0.82f, 0.78f);
                glUniform1i(glGetUniformLocation(m_ShaderBridge, "uHasTex"), m_b_BridgeHasTexture ? 1 : 0);
                if (m_b_BridgeHasTexture && m_TexBridge) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, m_TexBridge);
                    glUniform1i(glGetUniformLocation(m_ShaderBridge, "uTex"), 0);
                }

                glBindVertexArray(m_VAO_Bridge);
                glDrawElements(GL_TRIANGLES, m_BridgeIndexCount, GL_UNSIGNED_INT, 0);
                glBindVertexArray(0);
            }

            if (!m_vec_ParkRenderers.empty() && m_TexGrass) {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(-1.0f, -1.0f);

                glm::mat4 mat4_MVP_Parks = mat4_Projection * mat4_View;
                glm::vec2 tileScale(3.0f, 2.25f);

                for (const auto& parkRenderer : m_vec_ParkRenderers) {
                    parkRenderer->Render(mat4_MVP_Parks, m_TexGrass, tileScale);
                }

                glDisable(GL_POLYGON_OFFSET_FILL);
            }

            // HUD
            UI::SetViewport(p_App->GetVirtualWidth(), p_App->GetVirtualHeight());
            UI::RenderHUD(p_App);

            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void EmptyEnvironmentState::RenderText(const std::string& s_Text, float f_X, float f_Y, float f_Scale,
            float f_R, float f_G, float f_B, Core::Application* p_App) {
            const auto& characters = p_App->GetCharacterMap();
            GLuint i_Program = p_App->GetTextShaderProgram();
            GLuint i_VAO = p_App->GetTextVAO();
            GLuint i_VBO = p_App->GetTextVBO();

            glUseProgram(i_Program);
            glUniform3f(glGetUniformLocation(i_Program, "textColor"), f_R, f_G, f_B);
            glUniformMatrix4fv(glGetUniformLocation(i_Program, "projection"), 1, GL_FALSE,
                glm::value_ptr(glm::ortho(0.0f, (float)p_App->GetVirtualWidth(), 0.0f, (float)p_App->GetVirtualHeight())));
            glActiveTexture(GL_TEXTURE0);
            glBindVertexArray(i_VAO);

            for (auto c : s_Text) {
                auto it = characters.find(c);
                if (it == characters.end()) continue;

                const Core::Character& ch = it->second;
                float f_Xpos = f_X + ch.m_i_BearingX * f_Scale;
                float f_Ypos = f_Y + ch.m_i_BearingY * f_Scale;
                float f_W = ch.m_i_Width * f_Scale;
                float f_H = ch.m_i_Height * f_Scale;

                float f_Vertices[6][4] = {
                    {f_Xpos,         f_Ypos - f_H, 0.0f, 1.0f},
                    {f_Xpos,         f_Ypos,       0.0f, 0.0f},
                    {f_Xpos + f_W,   f_Ypos,       1.0f, 0.0f},
                    {f_Xpos,         f_Ypos - f_H, 0.0f, 1.0f},
                    {f_Xpos + f_W,   f_Ypos,       1.0f, 0.0f},
                    {f_Xpos + f_W,   f_Ypos - f_H, 1.0f, 1.0f}
                };

                glBindTexture(GL_TEXTURE_2D, ch.m_TextureID);
                glBindBuffer(GL_ARRAY_BUFFER, i_VBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(f_Vertices), f_Vertices);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
                glDrawArrays(GL_TRIANGLES, 0, 6);

                f_X += (ch.m_i_Advance >> 6) * f_Scale;
            }

            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        void EmptyEnvironmentState::AccelerateCameraForward(float f_DeltaTime) {
            if (!m_b_Camera3D) return;

            glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
            m_vec3_CameraVelocity += vec3_Forward * k_CameraAcceleration * f_DeltaTime;

            float f_Speed = glm::length(m_vec3_CameraVelocity);
            if (f_Speed > k_MaxCameraSpeed) {
                m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
            }
        }

        void EmptyEnvironmentState::AccelerateCameraBackward(float f_DeltaTime) {
            if (!m_b_Camera3D) return;

            glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
            m_vec3_CameraVelocity += -vec3_Forward * k_CameraAcceleration * f_DeltaTime;

            float f_Speed = glm::length(m_vec3_CameraVelocity);
            if (f_Speed > k_MaxCameraSpeed) {
                m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
            }
        }

        void EmptyEnvironmentState::AccelerateCameraLeft(float f_DeltaTime) {
            if (!m_b_Camera3D) return;

            glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
            glm::vec3 vec3_Left = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), vec3_Forward));
            m_vec3_CameraVelocity += vec3_Left * k_CameraAcceleration * f_DeltaTime;

            float f_Speed = glm::length(m_vec3_CameraVelocity);
            if (f_Speed > k_MaxCameraSpeed) {
                m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
            }
        }

        void EmptyEnvironmentState::AccelerateCameraRight(float f_DeltaTime) {
            if (!m_b_Camera3D) return;

            glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
            glm::vec3 vec3_Right = glm::normalize(glm::cross(vec3_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            m_vec3_CameraVelocity += vec3_Right * k_CameraAcceleration * f_DeltaTime;

            float f_Speed = glm::length(m_vec3_CameraVelocity);
            if (f_Speed > k_MaxCameraSpeed) {
                m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
            }
        }

        void EmptyEnvironmentState::UpdateCameraPhysics(float f_DeltaTime) {
            if (!m_b_Camera3D) return;

            m_vec3_CameraVelocity *= k_CameraFriction;
            if (glm::length(m_vec3_CameraVelocity) < 0.05f) {
                m_vec3_CameraVelocity = glm::vec3(0.0f);
            }

            float f_OriginalY = m_vec3_Saved3DCameraPosition.y;
            m_vec3_Saved3DCameraPosition += m_vec3_CameraVelocity * f_DeltaTime;
            m_vec3_Saved3DCameraPosition.y = f_OriginalY;
        }

        void EmptyEnvironmentState::HandleEvent(const SDL_Event& ev, Core::Application* p_App) {
            if (ev.type == SDL_KEYDOWN) {
                switch (ev.key.keysym.sym) {
                case SDLK_g:
                    p_App->GetStateManager()->PushState("mapgen", p_App);
                    break;
                case SDLK_l:
                    TryLoadGeneratedMap(p_App);
                    break;
                case SDLK_ESCAPE:
                    p_App->GetStateManager()->ChangeState("menu", p_App);
                    break;
                }
            }

            if (ev.type == SDL_MOUSEWHEEL && m_b_Camera3D) {
                float f_ScrollInput = ev.wheel.y * k_CameraScrollAcceleration;
                m_f_CameraAngleVelocity -= f_ScrollInput;
            }

            if (ev.type == SDL_MOUSEBUTTONDOWN) {
                UI::HandleMouseClick(ev.button.x, ev.button.y);
            }

            if (ev.type == SDL_MOUSEMOTION) {
                UI::HandleMouseMotion(ev.motion.x, ev.motion.y);
            }
        }

        void EmptyEnvironmentState::CreateTestRoad(Core::Application* p_App) {
            std::vector<glm::vec2> vec_RoadPoints;

            int i_Segments = 20;
            float f_Radius = 10.0f; 
            glm::vec2 vec2_Center(0.0f, 0.0f);
            for (int i = 0; i <= i_Segments; ++i) {
                float f_Angle = glm::half_pi<float>() * i / i_Segments; 
                float f_X = vec2_Center.x + f_Radius * cos(f_Angle);
                float f_Y = vec2_Center.y + f_Radius * sin(f_Angle);
                vec_RoadPoints.emplace_back(f_X, f_Y);
            }

            std::vector<float> vec_RoadWidths;
            for (int i = 0; i <= i_Segments; ++i) {
                // float f_WidthTemp = 2.0f + 2.0f * sin((float)i / i_Segments * glm::half_pi<float>()); // road widens along the curve
                // float f_WidthTemp = 1.0f; // constant width
                float f_Width = 1.0f - 0.05f * i;
                vec_RoadWidths.push_back(f_Width);
            }

            ScotlandYard::Core::RoadMesh mesh_Road = ScotlandYard::Core::RoadGenerator::GenerateRoad(
                vec_RoadPoints,
                vec_RoadWidths,
                2.0f // texture repeats every 2 meters
            );
           

            for (auto& vertex : mesh_Road.vertices) {
                vertex.y = 0.5f;
            }

            struct RoadVertex { glm::vec3 pos; glm::vec3 normal; glm::vec2 uv; };
            std::vector<RoadVertex> vec_Vertices;
            for (size_t i = 0; i < mesh_Road.vertices.size(); ++i) {
                vec_Vertices.push_back({ mesh_Road.vertices[i], mesh_Road.normals[i], mesh_Road.texCoords[i] });
            }

            glGenVertexArrays(1, &m_VAO_Road);
            glGenBuffers(1, &m_VBO_Road);
            glGenBuffers(1, &m_EBO_Road);

            glBindVertexArray(m_VAO_Road);

            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Road);
            glBufferData(GL_ARRAY_BUFFER, vec_Vertices.size() * sizeof(RoadVertex), vec_Vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_Road);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh_Road.indices.size() * sizeof(unsigned int),
                mesh_Road.indices.data(), GL_STATIC_DRAW);

            // Vertex attributes
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RoadVertex), (void*)0); // position
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RoadVertex), (void*)offsetof(RoadVertex, normal)); // normal
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RoadVertex), (void*)offsetof(RoadVertex, uv)); // uv
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);

            m_RoadIndexCount = static_cast<int>(mesh_Road.indices.size());

            // Load road texture
            m_TexRoad = p_App->LoadTexture(p_App->GetAssetPath("textures/road.jpg"));

            if (m_TexRoad == 0) {
                std::cerr << "[Road] Failed to load road texture!" << std::endl;
            }
            else {
                std::cout << "[Road] Road texture loaded successfully." << std::endl;
            }
        }

        void EmptyEnvironmentState::LoadBridgeModel(Core::Application* p_App) {
            const std::string s_ModelPath = p_App->GetAssetPath("models/bridge.glb");

            nlohmann::json gltf;
            std::vector<uint8_t> bin;
            if (!ParseGlbFile(s_ModelPath, gltf, bin)) {
                std::cerr << "[Bridge] Failed to parse GLB: " << s_ModelPath << std::endl;
                return;
            }

            if (!gltf.contains("meshes") || !gltf["meshes"].is_array() || gltf["meshes"].empty()) {
                std::cerr << "[Bridge] No meshes in GLB" << std::endl;
                return;
            }

            std::vector<BridgeVertex> vertices;
            std::vector<uint32_t> indices;

            auto appendPrimitive = [&](const nlohmann::json& prim) {
                const auto& attrs = prim.value("attributes", nlohmann::json::object());
                const int posAccessor = attrs.value("POSITION", -1);
                const int normalAccessor = attrs.value("NORMAL", -1);
                const int uvAccessor = attrs.value("TEXCOORD_0", -1);
                const int idxAccessor = prim.value("indices", -1);

                AccessorView posView, normView, uvView, idxView;
                if (!GetAccessorView(gltf, bin, posAccessor, posView)) {
                    std::cerr << "[Bridge] Skip primitive: POSITION missing/invalid" << std::endl;
                    return;
                }
                if (posView.componentType != 5126 || posView.components != 3) {
                    std::cerr << "[Bridge] Skip primitive: POSITION not float3" << std::endl;
                    return;
                }

                bool hasNormals = normalAccessor >= 0 && GetAccessorView(gltf, bin, normalAccessor, normView) &&
                    normView.componentType == 5126 && normView.components >= 3;
                bool hasUV = uvAccessor >= 0 && GetAccessorView(gltf, bin, uvAccessor, uvView) &&
                    uvView.componentType == 5126 && uvView.components >= 2;

                const size_t baseVertex = vertices.size();
                vertices.reserve(vertices.size() + posView.count);
                for (size_t i = 0; i < posView.count; ++i) {
                    const float* pPos = reinterpret_cast<const float*>(posView.data + i * posView.stride);
                    glm::vec3 pos{ pPos[0], pPos[1], pPos[2] };

                    glm::vec3 normal{ 0.0f, 1.0f, 0.0f };
                    if (hasNormals) {
                        const float* pNorm = reinterpret_cast<const float*>(normView.data + i * normView.stride);
                        normal = glm::vec3{ pNorm[0], pNorm[1], pNorm[2] };
                    }

                    glm::vec2 uv{ 0.0f, 0.0f };
                    if (hasUV) {
                        const float* pUV = reinterpret_cast<const float*>(uvView.data + i * uvView.stride);
                        uv = glm::vec2{ pUV[0], pUV[1] };
                    }

                    vertices.push_back({ pos, normal, uv });
                }

                std::vector<uint32_t> localIndices;
                if (GetAccessorView(gltf, bin, idxAccessor, idxView) &&
                    (idxView.componentType == 5121 || idxView.componentType == 5123 || idxView.componentType == 5125)) {
                    localIndices.reserve(idxView.count);
                    for (size_t i = 0; i < idxView.count; ++i) {
                        localIndices.push_back(ReadIndex(idxView, i));
                    }
                }
                else {
                    localIndices.reserve(posView.count);
                    for (uint32_t i = 0; i < posView.count; ++i) {
                        localIndices.push_back(i);
                    }
                }

                indices.reserve(indices.size() + localIndices.size());
                for (uint32_t idx : localIndices) {
                    indices.push_back(idx + static_cast<uint32_t>(baseVertex));
                }
            };

            for (const auto& mesh : gltf["meshes"]) {
                if (!mesh.contains("primitives") || !mesh["primitives"].is_array()) continue;
                for (const auto& prim : mesh["primitives"]) {
                    appendPrimitive(prim);
                }
            }

            if (vertices.empty() || indices.empty()) {
                std::cerr << "[Bridge] Failed to assemble mesh data (vertices: " << vertices.size()
                    << ", indices: " << indices.size() << ")" << std::endl;
                return;
            }

            // Capture original model length along X for length-only scaling
            float f_MinX = std::numeric_limits<float>::max();
            float f_MaxX = std::numeric_limits<float>::lowest();
            for (const auto& v : vertices) {
                f_MinX = std::min(f_MinX, v.pos.x);
                f_MaxX = std::max(f_MaxX, v.pos.x);
            }
            m_f_BridgeModelLength = std::max(0.0f, f_MaxX - f_MinX);
            m_vec3_BridgeScale = m_vec3_BridgeBaseScale;
            if (m_f_BridgeModelLength <= 0.0f) {
                std::cerr << "[Bridge] Model length is zero; length scaling disabled" << std::endl;
            }

            const std::string s_BridgeTex = p_App->GetAssetPath("textures/bridge.png");
            if (std::filesystem::exists(s_BridgeTex)) {
                m_TexBridge = p_App->LoadTexture(s_BridgeTex);
                m_b_BridgeHasTexture = (m_TexBridge != 0);
                if (!m_b_BridgeHasTexture) {
                    std::cerr << "[Bridge] Failed to load bridge texture: " << s_BridgeTex << std::endl;
                }
            }
            else {
                m_b_BridgeHasTexture = false;
            }

            glGenVertexArrays(1, &m_VAO_Bridge);
            glGenBuffers(1, &m_VBO_Bridge);
            glGenBuffers(1, &m_EBO_Bridge);

            glBindVertexArray(m_VAO_Bridge);

            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Bridge);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(BridgeVertex), vertices.data(), GL_STATIC_DRAW);

            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_Bridge);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(BridgeVertex), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(BridgeVertex), (void*)offsetof(BridgeVertex, normal));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(BridgeVertex), (void*)offsetof(BridgeVertex, uv));
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);

            m_BridgeIndexCount = static_cast<int>(indices.size());
            std::cout << "[Bridge] Loaded bridge.glb: " << vertices.size() << " vertices, "
                << m_BridgeIndexCount << " indices (merged primitives)" << std::endl;
        }

        void EmptyEnvironmentState::SetBridgeLength(float f_LengthWorld) {
            if (m_f_BridgeModelLength <= 0.0f) return;
            if (f_LengthWorld <= 0.0f) return;

            float f_ScaleX = f_LengthWorld / m_f_BridgeModelLength;
            m_vec3_BridgeScale.x = f_ScaleX;
            m_vec3_BridgeScale.y = m_vec3_BridgeBaseScale.y;
            m_vec3_BridgeScale.z = m_vec3_BridgeBaseScale.z;
        }

        // Load sample map data
        void EmptyEnvironmentState::LoadSampleMapData() {
            std::cout << "[EmptyEnvironmentState] Loading sample map data..." << std::endl;

            // Try to load from saved file first
            std::string mapPath = "maps/example_city.symap";

            if (std::filesystem::exists(mapPath)) {
                std::cout << "[EmptyEnvironmentState] Found saved map file, loading..." << std::endl;
                m_MapData = MapGen::MapDataSerializer::LoadFromFile(mapPath);
            }

            // If loading failed or file doesn't exist, generate new map
            if (m_MapData.IsEmpty()) {
                std::cout << "[EmptyEnvironmentState] No saved map found or loading failed, generating new map..." << std::endl;
                m_MapData = MapGen::SampleMapDataGenerator::GenerateRealisticCityMap(42);

                // Create maps directory if it doesn't exist
                std::filesystem::create_directories("maps");

                // Save the generated map for future use
                std::cout << "[EmptyEnvironmentState] Saving generated map to: " << mapPath << std::endl;
                MapGen::MapDataSerializer::SaveToFile(m_MapData, mapPath);
            }
            else {
                std::cout << "[EmptyEnvironmentState] Successfully loaded map from file!" << std::endl;
            }

            m_b_MapDataLoaded = true;

            // Print stats
            auto stats = m_MapData.GetStats();
            std::cout << "[EmptyEnvironmentState] Map loaded:" << std::endl;
            std::cout << "  - Graph nodes: " << stats.nodes << std::endl;
            std::cout << "  - Streets: " << stats.streets << std::endl;
            std::cout << "  - Buildings: " << stats.buildings << std::endl;
            std::cout << "  - Trees: " << stats.trees << std::endl;
            std::cout << "  - Parks: " << stats.parks << std::endl;
            std::cout << "  - Bridges: " << stats.bridges << std::endl;

            // DEBUG: Print some graph nodes
            // std::cout << "[EmptyEnvironmentState] First 3 graph nodes:" << std::endl;
            // for (int i = 0; i < std::min(3, static_cast<int>(m_MapData.vec_GraphNodes.size())); ++i) {
            //     const auto& node = m_MapData.vec_GraphNodes[i];
            //     std::cout << "  Node " << node.i_ID << ": pos=(" << node.position.x << ", " << node.position.y << ")"
            //               << ", taxi_connections=" << node.vec_TaxiConnections.size()
            //               << ", bus=" << node.vec_BusConnections.size()
            //               << ", metro=" << node.vec_MetroConnections.size()
            //               << std::endl;
            // }

            // DEBUG: Print some streets
            // std::cout << "[EmptyEnvironmentState] First 3 streets:" << std::endl;
            // for (int i = 0; i < std::min(3, static_cast<int>(m_MapData.vec_Streets.size())); ++i) {
            //     const auto& street = m_MapData.vec_Streets[i];
            //     std::cout << "  Street " << i << ": " << street.i_Node1 << " <-> " << street.i_Node2
            //               << ", tier=" << street.i_Tier
            //               << ", width=" << street.f_Width
            //               << ", inPark=" << (street.b_IsInPark ? "yes" : "no")
            //               << std::endl;
            // }
        }

        void EmptyEnvironmentState::BuildRiverFromMapData() {
            if (!m_p_WaterRenderer) return;
            if (!m_b_MapDataLoaded || m_MapData.vec_RiverPath.empty()) {
                BuildFallbackRiver();
                return;
            }

            const float f_ScaleX = k_PlaneWidth / k_MapWidth;
            const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
            const float f_OffsetX = k_MapOffsetX;
            const float f_OffsetZ = k_MapOffsetZ;

            std::vector<glm::vec2> vec_RiverPath;
            vec_RiverPath.reserve(m_MapData.vec_RiverPath.size());
            for (const auto& point : m_MapData.vec_RiverPath) {
                float f_ScaledX = point.x * f_ScaleX + f_OffsetX;
                float f_ScaledZ = point.y * f_ScaleZ + f_OffsetZ;
                vec_RiverPath.emplace_back(f_ScaledX, f_ScaledZ);
            }

            if (vec_RiverPath.size() < 2) {
                BuildFallbackRiver();
                return;
            }

            float f_RiverWidth = 50.0f * f_ScaleX;
            m_p_WaterRenderer->SetRiverStrip(vec_RiverPath, f_RiverWidth);

            m_p_RiverRenderer = std::make_unique<Rendering::PolygonRenderer>();
            if (m_p_RiverRenderer->Initialize()) {
                m_p_RiverRenderer->SetRiverStrip(vec_RiverPath, f_RiverWidth, 0.001f);
            }

            m_b_RiverStripLoaded = true;
            std::cout << "[EmptyEnvironmentState] River path built from map data (" << vec_RiverPath.size()
                << " points)." << std::endl;
        }

        void EmptyEnvironmentState::BuildParksFromMapData() {
            if (!m_b_MapDataLoaded || m_MapData.vec_Parks.empty()) {
                return;
            }

            const float f_ScaleX = k_PlaneWidth / k_MapWidth;
            const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
            const float f_OffsetX = k_MapOffsetX;
            const float f_OffsetZ = k_MapOffsetZ;

            m_vec_ParkRenderers.clear();

            for (const auto& park : m_MapData.vec_Parks) {
                if (park.vec_Vertices.empty()) continue;

                std::vector<glm::vec2> vec_ScaledVertices;
                vec_ScaledVertices.reserve(park.vec_Vertices.size());

                for (const auto& vertex : park.vec_Vertices) {
                    float f_ScaledX = vertex.x * f_ScaleX + f_OffsetX;
                    float f_ScaledZ = vertex.y * f_ScaleZ + f_OffsetZ;
                    vec_ScaledVertices.emplace_back(f_ScaledX, f_ScaledZ);
                }

                if (vec_ScaledVertices.size() >= 3) {
                    auto parkRenderer = std::make_unique<Rendering::PolygonRenderer>();
                    if (parkRenderer->Initialize()) {
                        parkRenderer->SetPolygon(vec_ScaledVertices, 0.001f);
                        m_vec_ParkRenderers.push_back(std::move(parkRenderer));
                    }
                }
            }
        }

        void EmptyEnvironmentState::BuildFallbackRiver() {
            if (!m_p_WaterRenderer || m_b_RiverStripLoaded) return;

            std::vector<glm::vec2> vec_FallbackPath = {
                { -1.0f, 4.0f },
                { 1.5f, 4.6f },
                { 4.0f, 5.3f },
                { 7.0f, 6.1f },
                { 10.5f, 6.9f },
                { 14.0f, 7.6f },
                { 17.5f, 8.4f },
                { 21.0f, 9.1f },
                { 24.0f, 9.8f }
            };

            float f_FallbackWidth = 2.0f;
            m_p_WaterRenderer->SetRiverStrip(vec_FallbackPath, f_FallbackWidth);

            m_p_RiverRenderer = std::make_unique<Rendering::PolygonRenderer>();
            if (m_p_RiverRenderer->Initialize()) {
                m_p_RiverRenderer->SetRiverStrip(vec_FallbackPath, f_FallbackWidth, 0.001f);
            }

            m_b_RiverStripLoaded = true;
            std::cout << "[EmptyEnvironmentState] Using fallback river strip (" << vec_FallbackPath.size()
                << " points)." << std::endl;
        }

        // render map data (placeholder - to be implemented by graphics team)
        void EmptyEnvironmentState::RenderMapData(Core::Application* p_App) {
            if (!m_b_MapDataLoaded) return;

            // TODO: Implement rendering for:
            // 1. River - use WaterRenderer with m_MapData.vec_RiverPath
            // 2. Parks - render park polygons with grass texture
            // 3. Streets - use RoadGenerator for each street segment
            // 4. Buildings - use BuildingGenerator to create meshes
            // 5. Trees - use TreeGenerator for each tree instance
            // 6. Graph nodes - render as spheres/cubes for debugging

            // For now, just log that we would render
            // (Uncomment this when actually implementing rendering)
            /*
            std::cout << "[EmptyEnvironmentState] Rendering map data..." << std::endl;
            std::cout << "  - Would render " << m_MapData.vec_GraphNodes.size() << " graph nodes" << std::endl;
            std::cout << "  - Would render " << m_MapData.vec_Streets.size() << " streets" << std::endl;
            std::cout << "  - Would render " << m_MapData.vec_Buildings.size() << " buildings" << std::endl;
            std::cout << "  - Would render " << m_MapData.vec_Trees.size() << " trees" << std::endl;
            */
        }

        void EmptyEnvironmentState::BuildHighwaysFromMapData(Core::Application* p_App)
        {
            if (!m_b_MapDataLoaded || m_vec_Highways.empty()) return;
            

            std::cout << "[EmptyEnvironmentState] Building highways from map data..." << std::endl;

            // Najpierw wyczyść poprzednie meshe
            for (auto& roadMesh : m_RoadMeshes) {
                if (roadMesh.VAO) glDeleteVertexArrays(1, &roadMesh.VAO);
                if (roadMesh.VBO) glDeleteBuffers(1, &roadMesh.VBO);
                if (roadMesh.EBO) glDeleteBuffers(1, &roadMesh.EBO);
            }
            m_RoadMeshes.clear();

            // Iteracja po wszystkich highwayach zwróconych przez generator mapy m_vec_Highways.size()
            for (size_t hwIdx = 0; hwIdx < m_vec_Highways.size(); ++hwIdx)
            {
                const auto& highway = m_vec_Highways[hwIdx];
                std::vector<glm::vec2> points;
                std::vector<float> widths;

                // Zbieramy wszystkie punkty dla highwaya
                for (size_t i = 0; i < highway.roadIndices.size(); ++i)
                {   
                    int roadIdx = highway.roadIndices[i];
                    const auto& road = m_vec_HighwayRoads[roadIdx];
                    const auto& n1 = m_vec_HighwayNodes[road.startNodeIdx];
                    const auto& n2 = m_vec_HighwayNodes[road.endNodeIdx];

                    // std::cout << "  [Segment " << i << "/" << highway.roadIndices.size()
                    //         << "] Road " << roadIdx
                    //         << ": (" << (int)n1.x << "," << (int)n1.y << ") -> (" << (int)n2.x << "," << (int)n2.y << ")"
                    //         << " type=" << (road.type == CityGen::RoadType::HIGHWAY ? "HIGHWAY" : "STREET") << std::endl;
                    
                    if(i == 0)
                    {
                        points.emplace_back(n1.x, n1.y);
                        widths.push_back(0.25f);
                    }
                    points.emplace_back(n2.x, n2.y);
                    widths.push_back(0.25f);
                }

                if (points.size() < 2) continue; // pomijamy za krótkie

                const float f_ScaleX = k_PlaneWidth / k_MapWidth;
                const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
                const float f_OffsetX = k_MapOffsetX;
                const float f_OffsetZ = k_MapOffsetZ;

                for (size_t i = 0; i < points.size(); ++i) {
                    // std::cout << "Point " << i << ": (" << points[i].x << ", " << points[i].y << ")" << std::endl;
                    points[i].x = (points[i].x * 0.02f) - 1.0f;
                    points[i].y = (points[i].y * 0.02f) - 1.0f;
                    // std::cout << "Point " << i << ": (" << points[i].x << ", " << points[i].y << ")" << std::endl;
                }

                // Generujemy mesh drogi
                auto mesh = ScotlandYard::Core::RoadGenerator::GenerateRoad(points, widths, 2.0f);

                // std::cout << "Verts: " << mesh.vertices.size()
                //     << " Indices: " << mesh.indices.size() << std::endl;

                // Podnosimy wszystkie wierzchołki na y = 0.5f
                for (auto& vertex : mesh.vertices)
                {
                    vertex.y = 0.01f + hwIdx * 0.001f; // unieś nad ziemię, aby uniknąć z-fightingu przy wielu highwayach
                }

                // Tworzymy VAO/VBO/EBO
                struct V { glm::vec3 p; glm::vec3 n; glm::vec2 uv; };
                std::vector<V> verts;
                for (size_t i = 0; i < mesh.vertices.size(); ++i)
                    verts.push_back({ mesh.vertices[i], mesh.normals[i], mesh.texCoords[i] });

                RoadMesh roadMesh{};
                glGenVertexArrays(1, &roadMesh.VAO);
                glGenBuffers(1, &roadMesh.VBO);
                glGenBuffers(1, &roadMesh.EBO);

                glBindVertexArray(roadMesh.VAO);

                glBindBuffer(GL_ARRAY_BUFFER, roadMesh.VBO);
                glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, roadMesh.EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int),
                            mesh.indices.data(), GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
                glEnableVertexAttribArray(2);

                glBindVertexArray(0);

                roadMesh.indexCount = static_cast<int>(mesh.indices.size());
                m_RoadMeshes.push_back(roadMesh);

                std::cout << "[Highway " << hwIdx << "] Built highway with " 
                        << highway.roadIndices.size() << " segments, length: "
                        << (int)highway.totalLength << std::endl;
            }

            // Załaduj teksturę drogi
            m_TexRoad = p_App->LoadTexture(p_App->GetAssetPath("textures/road.jpg"));

            std::cout << "[EmptyEnvironmentState] Built " << m_RoadMeshes.size() << " highway meshes." << std::endl;
        }

    } // namespace States
} // namespace ScotlandYard