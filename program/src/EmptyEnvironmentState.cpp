#include "EmptyEnvironmentState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"
#include "RoadGenerator.h"
#include "SampleMapDataGenerator.h"
#include "MapDataSerializer.h"
#include "MapGenerator.h"
#include "HighwayGenerator.h"
#include "MapDataLoader.h"
#include "GameConstants.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>
#include <unordered_map>
#include <map>
#include <iterator>
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

    bool HasSupportedTextureExtension(const std::filesystem::path& path) {
        std::string ext = path.extension().string();
        if (ext.empty()) {
            return false;
        }

        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
            });

        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga";
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
    constexpr float k_ShowcaseBuildingScale = 1.0f / 20.0f;
    constexpr float k_RoofBaseUVScale = 3.0f;

    const char* k_BuildingVertexShaderSrc = R"(#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

out vec3 FragPos;
out vec3 Normal;
out vec2 TexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = normalize(mat3(transpose(inverse(model))) * aNormal);
    TexCoord = aTexCoord;
    gl_Position = projection * view * vec4(FragPos, 1.0);
}
)";

    const char* k_BuildingFragmentShaderSrc = R"(#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord;

uniform sampler2D texture1;
uniform vec3 objColor;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform bool useTexture;
uniform bool isRoof;
uniform float roofUVScale;
uniform float textureExposure;

vec3 srgbToLinear(vec3 c) {
    return pow(c, vec3(2.2));
}

vec3 linearToSrgb(vec3 c) {
    return pow(max(c, vec3(0.0)), vec3(1.0 / 2.2));
}

void main()
{
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * lightColor;

    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * lightColor;

    vec2 sampleUV = TexCoord;
    if (isRoof) {
        sampleUV *= roofUVScale;
    }
    vec4 texSample = texture(texture1, sampleUV);
    vec3 texLinear = srgbToLinear(texSample.rgb) * textureExposure;
    vec3 baseColor = useTexture ? texLinear : objColor;
    float alpha = useTexture ? texSample.a : 1.0;
    if (alpha < 0.05)
        discard;
    vec3 resultLinear = (ambient + diffuse + specular) * baseColor;
    vec3 result = useTexture ? linearToSrgb(resultLinear) : resultLinear;
    FragColor = vec4(result, alpha);
}
)";

    glm::vec3 ConvertBuildingVector(const glm::vec3& vec) {
        return glm::vec3(vec.x, vec.z, vec.y);
    }

    glm::vec3 ConvertBuildingNormal(const glm::vec3& normal) {
        glm::vec3 converted = ConvertBuildingVector(normal);
        float len = glm::length(converted);
        if (len > 1e-4f) {
            converted /= len;
        }
        else {
            converted = glm::vec3(0.0f, 1.0f, 0.0f);
        }
        return converted;
    }

    GLuint CompileBuildingShader(GLenum shaderType, const char* source) {
        GLuint shader = glCreateShader(shaderType);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "[Building] Shader compilation failed: " << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint CreateBuildingShaderProgram() {
        GLuint vs = CompileBuildingShader(GL_VERTEX_SHADER, k_BuildingVertexShaderSrc);
        GLuint fs = CompileBuildingShader(GL_FRAGMENT_SHADER, k_BuildingFragmentShaderSrc);
        if (!vs || !fs) {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return 0;
        }

        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "[Building] Shader program link failed: " << infoLog << std::endl;
            glDeleteProgram(program);
            program = 0;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return program;
    }

    constexpr float k_PlayerScale = 0.5f;
    constexpr float k_PlayerHeightScale = 2.5f;
    constexpr float k_PlayerHover = 0.01f;
    constexpr float k_PlayerHeadOffset = 0.125f;
    constexpr float k_PlayerMultiRadius = 0.03f;
    constexpr float k_TokenClickRadiusPx = 32.0f;
    constexpr float k_DestinationClickRadiusPx = 28.0f;
    constexpr float k_TransportButtonClickRadiusPx = 52.0f;
    constexpr float k_TransportButtonRadius = 16.0f;
    constexpr float k_TransportButtonOrbitScale = 1.2f;
    constexpr float k_HighlightBaseScale = 18.0f;
    constexpr float k_HighlightPulseScale = 6.0f;
    constexpr float k_HighlightInnerScale = 11.0f;
    constexpr float k_HighlightPulseSpeed = 4.0f;

} // namespace

namespace ScotlandYard {
    namespace States {

        EmptyEnvironmentState::EmptyEnvironmentState()
            : m_Rng(std::random_device{}()), m_graph(Core::k_MaxNodes) {
        }

        EmptyEnvironmentState::~EmptyEnvironmentState() = default;

        void EmptyEnvironmentState::InjectMapData(
            const std::vector<CityGen::Point>& vec_Nodes,
            const std::vector<CityGen::Road>& vec_Roads,
            const std::vector<MapGen::Park>& vec_Parks,
            const std::vector<MapGen::Point>& vec_RiverPath,
            const std::vector<CityGen::Highway>& vec_Highways,
            const std::vector<MapGen::BuildingData>& vec_Buildings
        ) {

            m_MapData.Clear();
            m_MapData.i_Width = static_cast<int>(k_MapWidth);
            m_MapData.i_Height = static_cast<int>(k_MapHeight);

            m_MapData.vec_Parks = vec_Parks;
            m_MapData.vec_RiverPath = vec_RiverPath;
            m_MapData.vec_Buildings = vec_Buildings;

            m_vec_Highways = vec_Highways;
            m_vec_HighwayNodes = vec_Nodes;
            m_vec_HighwayRoads = vec_Roads;

            std::cout << "[EmptyEnvironmentState] Injected map data: "
                << vec_Nodes.size() << " nodes, "
                << vec_Roads.size() << " roads, "
                << vec_Buildings.size() << " buildings."
                << ", Highways: " << m_vec_Highways.size() << std::endl;
            m_b_RenderTestRoad = false;

            m_MapData.vec_GraphNodes.clear();
            m_MapData.vec_GraphNodes.reserve(vec_Nodes.size());
            for (size_t i = 0; i < vec_Nodes.size(); ++i) {
                MapGen::GraphNodeData node;
                node.i_ID = static_cast<int>(i);
                node.position = MapGen::Point(vec_Nodes[i].x, vec_Nodes[i].y);
                node.b_IsInPark = false;
                node.b_IsNearRiver = false;
                m_MapData.vec_GraphNodes.push_back(std::move(node));
            }

            auto appendConnection = [&](int from, int to, CityGen::RoadType type) {
                if (from < 0 || to < 0) {
                    return;
                }
                if (from >= static_cast<int>(m_MapData.vec_GraphNodes.size()) ||
                    to >= static_cast<int>(m_MapData.vec_GraphNodes.size())) {
                    return;
                }
                auto& src = m_MapData.vec_GraphNodes[static_cast<size_t>(from)];
                src.vec_TaxiConnections.push_back(to);
                if (type == CityGen::RoadType::HIGHWAY) {
                    src.vec_BusConnections.push_back(to);
                }
                };

            m_MapData.vec_Streets.clear();
            m_MapData.vec_Streets.reserve(vec_Roads.size());
            for (const auto& road : vec_Roads) {
                if (road.startNodeIdx < 0 || road.endNodeIdx < 0) {
                    continue;
                }
                if (road.startNodeIdx >= static_cast<int>(vec_Nodes.size()) ||
                    road.endNodeIdx >= static_cast<int>(vec_Nodes.size())) {
                    continue;
                }

                appendConnection(road.startNodeIdx, road.endNodeIdx, road.type);
                appendConnection(road.endNodeIdx, road.startNodeIdx, road.type);

                int tier = (road.type == CityGen::RoadType::HIGHWAY) ? 0 : 2;
                MapGen::StreetSegment segment(road.startNodeIdx, road.endNodeIdx, tier);
                segment.b_IsInPark = false;
                segment.vec_Geometry.push_back(MapGen::Point(vec_Nodes[static_cast<size_t>(road.startNodeIdx)].x,
                    vec_Nodes[static_cast<size_t>(road.startNodeIdx)].y));
                segment.vec_Geometry.push_back(MapGen::Point(vec_Nodes[static_cast<size_t>(road.endNodeIdx)].x,
                    vec_Nodes[static_cast<size_t>(road.endNodeIdx)].y));
                segment.f_Width = MapGen::StreetSegment::GetWidthForTier(tier);
                m_MapData.vec_Streets.push_back(std::move(segment));
            }

            m_MapData.i_NumGraphNodes = static_cast<int>(m_MapData.vec_GraphNodes.size());
            m_b_MapDataLoaded = true;
            m_b_RiverStripLoaded = false;
        }

        void EmptyEnvironmentState::InjectTreeData(const std::vector<Core::TreeInstance>& vec_Trees) {
            m_vec_TreeData = vec_Trees;
            std::cout << "[EmptyEnvironmentState] Injected tree data: " << m_vec_TreeData.size() << " trees." << std::endl;
        }

        void EmptyEnvironmentState::CreateShaders() {
            const char* vsSrc = R"(#version 330 core
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

            const char* fsSrc = R"(#version 330 core
        in vec2 vUV;

        uniform sampler2D uSidewalk;
        uniform sampler2D uGrass;
        uniform sampler2D uMask;
        uniform vec2 uTileUV;
        uniform int uUseMask;

        out vec4 FragColor;

        void main() {
            vec2 tiledUV = vUV * uTileUV;
            vec4 sidewalk = texture(uSidewalk, tiledUV);
            vec4 grass = texture(uGrass, tiledUV);

            if (uUseMask == 1) {
                vec3 m = texture(uMask, vUV).rgb;
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

                uniform vec3 uLaneColor;     // kolor pasów
                uniform vec3 uLineColor;     // kolor linii
                uniform float uLineWidth;    // np. 0.02

                out vec4 FragColor;

                void main() {
                    vec2 uv = vUV * uTileUV;

                    float center = abs(vUV.x - 0.5);

                    // --- Linia rozdzielająca ---
                    if (center < uLineWidth) {
                        FragColor = vec4(uLineColor, 1.0);
                        return;
                    }

                    // --- Pas lewy / prawy ---
                    vec4 road = texture(uRoad, uv);

                    // opcjonalnie różne odcienie pasów
                    if (vUV.x < 0.5)
                        road.rgb *= 1.0;
                    else
                        road.rgb *= 0.95;

                    FragColor = road;
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

            // --- Circle shader for transport stations ---
            const char* circleVsSrc = R"(#version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 MVP;
        void main() {
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

            const char* circleFsSrc = R"(#version 330 core
        uniform vec3 circleColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(circleColor, 1.0);
        }
    )";

            GLuint circleVs = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(circleVs, 1, &circleVsSrc, nullptr);
            glCompileShader(circleVs);

            GLuint circleFs = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(circleFs, 1, &circleFsSrc, nullptr);
            glCompileShader(circleFs);

            m_ShaderCircle = glCreateProgram();
            glAttachShader(m_ShaderCircle, circleVs);
            glAttachShader(m_ShaderCircle, circleFs);
            glLinkProgram(m_ShaderCircle);

            glDeleteShader(circleVs);
            glDeleteShader(circleFs);
        }

        GLuint EmptyEnvironmentState::Create1x1Texture(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            uint8_t pixel[4] = { r, g, b, a };
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, 0);
            return tex;
        }

        void EmptyEnvironmentState::CreateTreeShader() {
            const char* vsSrc = R"(
                #version 330 core
                layout (location = 0) in vec3 aPos;
                layout (location = 1) in vec3 aNormal;
                layout (location = 2) in vec2 aUV;

                out vec3 vPos;
                out vec3 vNormal;
                out vec2 vUV;

                uniform mat4 uMVP;
                uniform mat4 uModel;

                void main() {
                    vPos = vec3(uModel * vec4(aPos, 1.0));
                    vNormal = normalize(mat3(transpose(inverse(uModel))) * aNormal);
                    vUV = aUV;
                    gl_Position = uMVP * vec4(aPos, 1.0);
                }
            )";

            const char* fsSrc = R"(
                #version 330 core
                out vec4 FragColor;

                in vec3 vPos;
                in vec3 vNormal;
                in vec2 vUV;

                uniform sampler2D uTex;
                uniform vec3 uLightPos;
                uniform vec3 uViewPos;

                void main() {
                    vec3 color = texture(uTex, vUV).rgb;
                    
                    vec3 norm = normalize(vNormal);
                    vec3 lightDir = normalize(uLightPos - vPos);
                    float diff = max(dot(norm, lightDir), 0.2); 
                    
                    vec3 result = diff * color;
                    FragColor = vec4(result, 1.0);
                }
            )";

            GLuint vs = CompileBuildingShader(GL_VERTEX_SHADER, vsSrc);
            GLuint fs = CompileBuildingShader(GL_FRAGMENT_SHADER, fsSrc);

            m_ShaderTree = glCreateProgram();
            glAttachShader(m_ShaderTree, vs);
            glAttachShader(m_ShaderTree, fs);
            glLinkProgram(m_ShaderTree);

            glDeleteShader(vs);
            glDeleteShader(fs);
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

        void EmptyEnvironmentState::CreateFrame() {
            float frameWidth = 0.8f;
            float frameHeight = 0.12f;
            float innerLip = 0.0f;
            float frameY = 0.01f;

            std::vector<float> frameVertices;
            auto addQuad = [&](float x1, float y1, float z1, float x2, float y2, float z2, float x3, float y3, float z3, float x4, float y4, float z4) {
                frameVertices.insert(frameVertices.end(), { x1, y1, z1, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f });
                frameVertices.insert(frameVertices.end(), { x2, y2, z2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f });
                frameVertices.insert(frameVertices.end(), { x3, y3, z3, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f });
                frameVertices.insert(frameVertices.end(), { x1, y1, z1, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f });
                frameVertices.insert(frameVertices.end(), { x3, y3, z3, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f });
                frameVertices.insert(frameVertices.end(), { x4, y4, z4, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f });
                };

            float boardLeft = -1.0f;
            float boardRight = 23.0f;
            float boardTop = 17.0f;
            float boardBottom = -1.0f;
            // === TOP EDGE ===
            addQuad(boardLeft - frameWidth, frameY, boardTop + frameWidth,
                boardRight + frameWidth, frameY, boardTop + frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardTop + frameWidth,
                boardLeft - frameWidth, frameY + frameHeight, boardTop + frameWidth);

            addQuad(boardLeft - frameWidth, frameY + frameHeight, boardTop - innerLip,
                boardRight + frameWidth, frameY + frameHeight, boardTop - innerLip,
                boardRight + frameWidth, frameY + frameHeight, boardTop + frameWidth,
                boardLeft - frameWidth, frameY + frameHeight, boardTop + frameWidth);

            addQuad(boardLeft - innerLip, frameY, boardTop - innerLip,
                boardRight + innerLip, frameY, boardTop - innerLip,
                boardRight + innerLip, frameY + frameHeight, boardTop - innerLip,
                boardLeft - innerLip, frameY + frameHeight, boardTop - innerLip);

            // === LEFT EDGE ===
            addQuad(boardLeft - frameWidth, frameY, boardBottom - frameWidth,
                boardLeft - frameWidth, frameY, boardTop + frameWidth,
                boardLeft - frameWidth, frameY + frameHeight, boardTop + frameWidth,
                boardLeft - frameWidth, frameY + frameHeight, boardBottom - frameWidth);

            addQuad(boardLeft - frameWidth, frameY + frameHeight, boardBottom - frameWidth,
                boardLeft + innerLip, frameY + frameHeight, boardBottom - frameWidth,
                boardLeft + innerLip, frameY + frameHeight, boardTop + frameWidth,
                boardLeft - frameWidth, frameY + frameHeight, boardTop + frameWidth);

            addQuad(boardLeft + innerLip, frameY, boardBottom - frameWidth,
                boardLeft + innerLip, frameY, boardTop + frameWidth,
                boardLeft + innerLip, frameY + frameHeight, boardTop + frameWidth,
                boardLeft + innerLip, frameY + frameHeight, boardBottom - frameWidth);

            // === RIGHT EDGE ===
            addQuad(boardRight + frameWidth, frameY, boardTop + frameWidth,
                boardRight + frameWidth, frameY, boardBottom - frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardBottom - frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardTop + frameWidth);

            addQuad(boardRight - innerLip, frameY + frameHeight, boardBottom - frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardBottom - frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardTop + frameWidth,
                boardRight - innerLip, frameY + frameHeight, boardTop + frameWidth);

            addQuad(boardRight - innerLip, frameY, boardTop + frameWidth,
                boardRight - innerLip, frameY, boardBottom - frameWidth,
                boardRight - innerLip, frameY + frameHeight, boardBottom - frameWidth,
                boardRight - innerLip, frameY + frameHeight, boardTop + frameWidth);

            // === BOTTOM EDGE ===
            addQuad(boardRight + frameWidth, frameY, boardBottom,
                boardLeft - frameWidth, frameY, boardBottom,
                boardLeft - frameWidth, frameY + frameHeight, boardBottom,
                boardRight + frameWidth, frameY + frameHeight, boardBottom);

            addQuad(boardLeft - frameWidth, frameY + frameHeight, boardBottom - frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardBottom - frameWidth,
                boardRight + frameWidth, frameY + frameHeight, boardBottom + innerLip,
                boardLeft - frameWidth, frameY + frameHeight, boardBottom + innerLip);

            addQuad(boardRight + frameWidth, frameY, boardBottom + innerLip,
                boardLeft - frameWidth, frameY, boardBottom + innerLip,
                boardLeft - frameWidth, frameY + frameHeight, boardBottom + innerLip,
                boardRight + frameWidth, frameY + frameHeight, boardBottom + innerLip);

            glGenVertexArrays(1, &m_VAO_Frame);
            glGenBuffers(1, &m_VBO_Frame);
            glBindVertexArray(m_VAO_Frame);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Frame);
            glBufferData(GL_ARRAY_BUFFER, frameVertices.size() * sizeof(float), frameVertices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glBindVertexArray(0);
        }

        void EmptyEnvironmentState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {

            switch (event.type) {
            case SDL_MOUSEBUTTONDOWN: {
                float f_VirtualX = static_cast<float>(event.button.x);
                float f_VirtualY = static_cast<float>(event.button.y);
                if (p_App) {
                    p_App->TransformMouseToVirtual(event.button.x, event.button.y, f_VirtualX, f_VirtualY);
                }

                // First, check if UI was clicked
                UI::HandleMouseClick(static_cast<int>(f_VirtualX), static_cast<int>(f_VirtualY));

                if (event.button.button == SDL_BUTTON_LEFT && m_b_Camera3D) {
                    int i_WindowW = p_App ? p_App->GetWidth() : 1280;
                    int i_WindowH = p_App ? p_App->GetHeight() : 720;
                    int i_VirtualW = p_App ? p_App->GetVirtualWidth() : 1280;
                    int i_VirtualH = p_App ? p_App->GetVirtualHeight() : 720;

                    glm::vec3 vec3_Target = m_vec3_CameraPosition + m_vec3_CameraFront;
                    glm::mat4 mat4_View = glm::lookAt(m_vec3_CameraPosition, vec3_Target, m_vec3_CameraUp);
                    glm::mat4 mat4_Projection = glm::perspective(glm::radians(45.0f), (float)i_VirtualW / (float)i_VirtualH, 0.1f, 100.0f);

                    int i_ButtonIdx = FindTransportButtonAtScreenPos(event.button.x, event.button.y, mat4_View, mat4_Projection, i_WindowW, i_WindowH);
                    if (i_ButtonIdx >= 0) {
                        const auto& btn = m_vec_TransportButtons[i_ButtonIdx];
                        if (btn.b_Available) {
                            HandleTransportButtonClick(btn.i_TransportType);
                        } else {
                            std::cout << "[EmptyEnvironmentState] No tickets available for this transport option." << std::endl;
                        }
                        break;
                    }

                    int i_DestinationNode = FindDestinationAtScreenPos(event.button.x, event.button.y, mat4_View, mat4_Projection, i_WindowW, i_WindowH);
                    if (i_DestinationNode >= 0) {
                        HandleDestinationSelection(i_DestinationNode);
                        break;
                    }

                    int i_TokenIndex = FindPlayerTokenAtScreenPos(event.button.x, event.button.y, mat4_View, mat4_Projection, i_WindowW, i_WindowH);
                    if (i_TokenIndex >= 0) {
                        SelectPlayerToken(i_TokenIndex);
                        break;
                    }

                    int i_ClickedStation = FindStationAtScreenPos(event.button.x, event.button.y, mat4_View, mat4_Projection, i_WindowW, i_WindowH);
                    if (i_ClickedStation >= 0) {
                        ClearMovementSelection();
                        m_i_SelectedStationID = i_ClickedStation;
                        std::cout << "[EmptyEnvironmentState] Selected station ID: " << i_ClickedStation << std::endl;

                        m_vec_HighlightedStations.clear();
                        if (m_b_GraphLoaded) {
                            auto connections = m_graph.GetConnections(i_ClickedStation);
                            for (const auto& conn : connections) {
                                m_vec_HighlightedStations.push_back(conn.i_NodeId);
                            }
                            std::cout << "[EmptyEnvironmentState] Found " << m_vec_HighlightedStations.size()
                                << " connected stations" << std::endl;
                        }
                        break;
                    }

                    ClearMovementSelection();
                    m_i_SelectedStationID = -1;
                    m_vec_HighlightedStations.clear();
                }
                break;
            }
            case SDL_MOUSEMOTION:
                UI::HandleMouseMotion(event.motion.x, event.motion.y);
                break;
            case SDL_MOUSEWHEEL:
                if (m_b_Camera3D) {
                    m_f_CameraAngleVelocity += static_cast<float>(event.wheel.y) * k_CameraScrollAcceleration;
                }
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_SPACE) {
                    m_b_Camera3D = !m_b_Camera3D;
                    if (m_b_Camera3D) {
                        m_vec3_CameraPosition = m_vec3_Saved3DCameraPosition;
                    }
                }
                break;
            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                    m_b_GameActive = false;
                }
                else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                    m_b_GameActive = true;
                }
                break;
            default:
                break;
            }
        }

        void EmptyEnvironmentState::OnEnter(Core::Application* p_App) {
            if (m_b_RenderTestRoad) {
                CreateTestRoad(p_App);
            }

            if (p_App && !p_App->IsTrainingMode()) {
                const_cast<Core::Application*>(p_App)->UpdateUIScaling();

                glEnable(GL_DEPTH_TEST);
                CreateShaders();
                CreateTreeShader(); // Create tree shaders
                CreatePlane();
                CreateFrame();

                m_TexSidewalk = p_App->LoadTexture(p_App->GetAssetPath("textures/sidewalk.jpg"));
                m_TexGrass = p_App->LoadTexture(p_App->GetAssetPath("textures/grass.png"));

                // Create basic tree textures
                m_TexTreeTrunk = Create1x1Texture(100, 60, 30);  // Brown
                m_TexTreeCrown = Create1x1Texture(30, 120, 40);  // Green

                TryLoadGeneratedMap(p_App);
                LoadBridgeModel(p_App);
                SetBridgeLength(4.0f);
                LoadBuildingTextures(p_App);

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
                    GenerateRoadsFromMapData(p_App);
                }
                else {
                    std::cout << "[EmptyEnvironmentState] Skipping sample data load (Map injected)." << std::endl;
                    BuildRiverFromMapData();
                    BuildBuildingsFromMapData();
                    BuildHighwaysFromMapData(p_App);
                    BuildParkPathsFromMapData();
                    BuildTreesFromMapData(); // Build trees if map injected
                    GenerateRoadsFromMapData(p_App);
                }

                std::string s_IconPath = p_App->GetAssetPath("icons/camera_icon.png");
                UI::LoadCameraIconPNG(s_IconPath.c_str(), p_App);
                UI::SetCameraToggleCallback([this]() { this->m_b_Camera3D = !this->m_b_Camera3D; });

                std::vector<std::string> vec_Labels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };
                std::vector<UI::Color> vec_Colors = {
                    {0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f},
                    {0xE2 / 255.0f, 0x70 / 255.0f, 0x3F / 255.0f, 1.0f},
                    {0xED / 255.0f, 0xD1 / 255.0f, 0x00 / 255.0f, 1.0f},
                    {0xF5 / 255.0f, 0x51 / 255.0f, 0xAE / 255.0f, 1.0f},
                    {0x41 / 255.0f, 0x84 / 255.0f, 0x3D / 255.0f, 1.0f},
                };
                std::vector<int> vec_Counts = { -1, -1, -1, -1, -1, -1 };
                UI::SetTopBar(vec_Labels, vec_Colors, vec_Counts);
                UI::SetMrXButtonsVisible(false);
                UI::SetMrXButtonsEnabled(false, false);

                std::vector<UI::TicketSlot> vec_Slots(UI::k_TicketSlotCount);
                UI::SetTicketStates(vec_Slots);
                UI::SetRound(1);

                InitializeShowcaseBuilding(p_App);

                // Initialize circle geometry for transport stations
                float f_Radius = 0.02f;
                int i_Segments = 30;
                std::vector<float> vec_CircleVertices = generateCircleVertices(f_Radius, i_Segments);
                m_i_CircleVertexCount = static_cast<int>(vec_CircleVertices.size() / 3);

                glGenVertexArrays(1, &m_VAO_Circle);
                glGenBuffers(1, &m_VBO_Circle);

                glBindVertexArray(m_VAO_Circle);
                glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Circle);
                glBufferData(GL_ARRAY_BUFFER, vec_CircleVertices.size() * sizeof(float), vec_CircleVertices.data(), GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);

                glBindVertexArray(0);

                InitializePlayerTokenGeometry();

                // Load station data from generated CSV files in build/ folder
                m_vec_CircleStations.clear();

                // Try to load from build/ folder first
                // Use ASSETS_DIR as base and go up to find build folder
                std::string s_BuildDir;
                #ifdef ASSETS_DIR
                    s_BuildDir = std::string(ASSETS_DIR) + "/../build/";
                #else
                    s_BuildDir = "build/";
                #endif

                std::string s_BuildNodesPath = s_BuildDir + "nodes_with_station.csv";
                std::string s_BuildEdgesPath = s_BuildDir + "edges_geometry.csv";
                std::string s_BuildConnectionsPath = s_BuildDir + "game_connections.csv";

                std::cout << "[EmptyEnvironmentState] Attempting to load from CSV files..." << std::endl;
                std::cout << "[EmptyEnvironmentState] Build directory: " << s_BuildDir << std::endl;
                std::cout << "[EmptyEnvironmentState] Nodes path: " << s_BuildNodesPath << std::endl;

                auto vec_StationData = Utils::MapDataLoader::LoadNodesWithStation(s_BuildNodesPath);

                if (!vec_StationData.empty()) {
                    std::cout << "[EmptyEnvironmentState] Loading stations from build/ CSV files..." << std::endl;

                    // NOTE: CSV data is in HighwayGenerator coordinate space (600x450)
                    // which is 0.5x scale of the original map (1200x900).
                    // We need to scale up by 2.0 first, THEN transform to world coordinates.
                    const float streetGenScale = 0.5f;
                    const float scaleBack = 1.0f / streetGenScale; // 2.0

                    for (const auto& sd : vec_StationData) {
                        StationCircle sc;

                        // Step 1: Scale back from HighwayGenerator space (600x450) to map space (1200x900)
                        float mapX = sd.vec2_Position.x * scaleBack;
                        float mapY = sd.vec2_Position.y * scaleBack;

                        // Step 2: Transform from map coordinates (0-1200) to world coordinates (-1 to 23)
                        sc.position.x = (mapX * 0.02f) - 1.0f;
                        sc.position.y = (mapY * 0.02f) - 1.0f;

                        sc.transportTypes = sd.vec_TransportTypes;
                        sc.stationID = sd.i_StationID;
                        m_vec_CircleStations.push_back(sc);
                    }

                    std::cout << "[EmptyEnvironmentState] Loaded " << m_vec_CircleStations.size()
                        << " transport stations from build/ CSV" << std::endl;

                    if (!m_vec_CircleStations.empty()) {
                        const auto& first = vec_StationData.front();
                        std::cout << "[EmptyEnvironmentState] First station: CSV pos=("
                            << first.vec2_Position.x << ", " << first.vec2_Position.y
                            << ") -> World pos=(" << m_vec_CircleStations[0].position.x
                            << ", " << m_vec_CircleStations[0].position.y << ")" << std::endl;
                    }

                    // Load graph connections from build/ CSV
                    try {
                        std::cout << "[EmptyEnvironmentState] Loading graph from build/ CSV files..." << std::endl;

                        m_graph.LoadNodeData(s_BuildNodesPath, true);
                        m_graph.LoadConnections(s_BuildConnectionsPath, true);

                        m_b_GraphLoaded = true;
                        std::cout << "[EmptyEnvironmentState] Graph loaded successfully from build/ CSV" << std::endl;
                    }
                    catch (const std::exception& e) {
                        std::cerr << "[EmptyEnvironmentState] Failed to load graph from build/ CSV: " << e.what() << std::endl;
                        m_b_GraphLoaded = false;
                    }
                }
                else {
                    std::cout << "[EmptyEnvironmentState] Could not load from build/ folder, trying fallback..." << std::endl;

                    // Fallback to old method
                    if (m_b_MapDataLoaded && !m_MapData.vec_GraphNodes.empty()) {
                        LoadGraphDataFromGeneratedMap();
                    }
                    else {
                        auto vec_FallbackData = Utils::MapDataLoader::LoadStations(Core::GetMapPath(Core::k_NodeDataRelativePath));

                        for (const auto& sd : vec_FallbackData) {
                            StationCircle sc;
                            sc.position.x = sd.vec2_Position.x;
                            sc.position.y = sd.vec2_Position.y;
                            sc.transportTypes = sd.vec_TransportTypes;
                            sc.stationID = sd.i_StationID;
                            m_vec_CircleStations.push_back(sc);
                        }

                        LoadGraphData();
                    }
                }

                InitializeDebugPlayerTokens();
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

        void EmptyEnvironmentState::DestroyShowcaseBuilding(Core::Application* p_App) {
            (void)p_App;
            DestroyBuildingInstance(m_ShowcaseBuilding);

            if (m_ShaderBuilding) {
                glDeleteProgram(m_ShaderBuilding);
                m_ShaderBuilding = 0;
            }

            m_ShowcaseBuilding.ready = false;
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
            if (m_VBO_Frame) {
                glDeleteBuffers(1, &m_VBO_Frame);
                m_VBO_Frame = 0;
            }
            if (m_VAO_Frame) {
                glDeleteVertexArrays(1, &m_VAO_Frame);
                m_VAO_Frame = 0;
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
            for (auto& mesh : m_ParkPathMeshes) {
                if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
                if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
                if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
            }
            m_ParkPathMeshes.clear();

            DestroyGeneratedBuildings();
            DestroyShowcaseBuilding(p_App);
            DestroyBuildingTextures(p_App);

            // Destroy trees
            DestroyTrees();
            if (m_ShaderTree) { glDeleteProgram(m_ShaderTree); m_ShaderTree = 0; }
            if (m_TexTreeTrunk) { glDeleteTextures(1, &m_TexTreeTrunk); m_TexTreeTrunk = 0; }
            if (m_TexTreeCrown) { glDeleteTextures(1, &m_TexTreeCrown); m_TexTreeCrown = 0; }

            DestroyPlayerTokenGeometry();
            m_vec_PlayerTokens.clear();

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
            glm::vec3 vec3_ViewPosition(0.0f);

            if (m_b_Camera3D) {
                m_vec3_CameraPosition = m_vec3_Saved3DCameraPosition;
                glm::vec3 vec3_Target = m_vec3_CameraPosition + m_vec3_CameraFront;
                mat4_View = glm::lookAt(m_vec3_CameraPosition, vec3_Target, m_vec3_CameraUp);
                mat4_Projection = glm::perspective(glm::radians(45.0f), (float)i_W / (float)i_H, 0.1f, 100.0f);
                vec3_ViewPosition = m_vec3_CameraPosition;
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
                vec3_ViewPosition = vec3_CamPos;
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

            // --- Render highways ---
           if (!m_HighwayMeshes.empty() && m_TexHighway) {
                glUseProgram(m_ShaderRoad);
                
                GLint mvpLoc = glGetUniformLocation(m_ShaderRoad, "uMVP");
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View));

                
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_TexHighway);
                glUniform1i(glGetUniformLocation(m_ShaderRoad, "uRoad"), 0);

                GLint tileLoc = glGetUniformLocation(m_ShaderRoad, "uTileUV");
                glUniform2f(tileLoc, 1.0f, 1.0f);

                for (const auto& road : m_HighwayMeshes) {
                    // glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glBindVertexArray(road.VAO);
                    glDrawElements(GL_TRIANGLES, road.indexCount, GL_UNSIGNED_INT, 0);
                }
                glBindVertexArray(0);
            }

            // --- Render roads ---
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

            if (!m_ParkPathMeshes.empty() && m_TexSidewalk) { // Używamy tekstury chodnika
                glUseProgram(m_ShaderRoad); // Ten sam shader co drogi
                GLint mvpLoc = glGetUniformLocation(m_ShaderRoad, "uMVP");
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View));

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, m_TexSidewalk); // <--- TEKSTURA CHODNIKA
                glUniform1i(glGetUniformLocation(m_ShaderRoad, "uRoad"), 0);

                // Gęstsze kafelkowanie dla węższych ścieżek
                GLint tileLoc = glGetUniformLocation(m_ShaderRoad, "uTileUV");
                glUniform2f(tileLoc, 3.0f, 3.0f);

                for (const auto& path : m_ParkPathMeshes) {
                    glBindVertexArray(path.VAO);
                    glDrawElements(GL_TRIANGLES, path.indexCount, GL_UNSIGNED_INT, 0);
                }
                glBindVertexArray(0);
            }

            // Bridge model
            if (m_b_DrawBridge && m_VAO_Bridge && m_ShaderBridge && m_BridgeIndexCount > 0) {
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

            if (m_b_ShowcaseBuildingVisible) {
                RenderShowcaseBuilding(mat4_View, mat4_Projection, vec3_ViewPosition);
            }
            RenderGeneratedBuildings(mat4_View, mat4_Projection, vec3_ViewPosition);
            RenderTrees(mat4_View, mat4_Projection, vec3_ViewPosition);

            // Render transport stations
            RenderStations(mat4_View, mat4_Projection);
            RenderPlayerTokens(mat4_View, mat4_Projection);

            // Render highlighted stations (connected to selected)
            RenderHighlightedStations(mat4_View, mat4_Projection);

            // Render connection lines from selected station
            RenderConnectionLines(mat4_View, mat4_Projection);
            RenderTransportButtons(mat4_View, mat4_Projection);

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

            if (m_VAO_Frame) {
                GLint previousProgram = 0;
                GLint previousVAO = 0;
                glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
                glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
                GLboolean wasCullingEnabled = glIsEnabled(GL_CULL_FACE);
                glDisable(GL_CULL_FACE);

                glUseProgram(m_ShaderBridge);
                glm::mat4 mat4_MVP_Frame = mat4_Projection * mat4_View;
                glUniformMatrix4fv(glGetUniformLocation(m_ShaderBridge, "uMVP"), 1, GL_FALSE, glm::value_ptr(mat4_MVP_Frame));
                glUniformMatrix4fv(glGetUniformLocation(m_ShaderBridge, "uModel"), 1, GL_FALSE, glm::value_ptr(glm::mat4(1.0f)));
                glUniform3f(glGetUniformLocation(m_ShaderBridge, "uColor"), 0.545f, 0.353f, 0.169f);
                glUniform1i(glGetUniformLocation(m_ShaderBridge, "uHasTex"), 0);
                glBindVertexArray(m_VAO_Frame);
                glDrawArrays(GL_TRIANGLES, 0, 72);

                glBindVertexArray(previousVAO);
                glUseProgram(previousProgram);
                if (wasCullingEnabled) {
                    glEnable(GL_CULL_FACE);
                }
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

        GLuint EmptyEnvironmentState::LoadShowcaseTexture(Core::Application* p_App, const std::string& s_RelativePath) {
            if (!p_App) {
                return 0;
            }
            return p_App->LoadTexture(p_App->GetAssetPath(s_RelativePath));
        }

        void EmptyEnvironmentState::LoadTextureSetFromDirectory(Core::Application* p_App,
            const std::string& s_RelativeDir, std::vector<GLuint>& vec_Target) {
            if (!p_App) {
                return;
            }
            if (!vec_Target.empty()) {
                return;
            }

            const std::string s_AbsoluteDir = p_App->GetAssetPath(s_RelativeDir);
            if (!std::filesystem::exists(s_AbsoluteDir)) {
                std::cerr << "[Building] Texture directory missing: " << s_AbsoluteDir << std::endl;
                return;
            }

            try {
                for (const auto& entry : std::filesystem::directory_iterator(s_AbsoluteDir)) {
                    if (!entry.is_regular_file()) {
                        continue;
                    }
                    if (!HasSupportedTextureExtension(entry.path())) {
                        continue;
                    }

                    GLuint texture = p_App->LoadTexture(entry.path().string());
                    if (texture != 0) {
                        vec_Target.push_back(texture);
                    }
                    else {
                        std::cerr << "[Building] Failed to load texture: " << entry.path() << std::endl;
                    }
                }
            }
            catch (const std::exception& e) {
                std::cerr << "[Building] Directory iteration failed for " << s_AbsoluteDir << ": " << e.what() << std::endl;
            }
        }

        void EmptyEnvironmentState::LoadBuildingTextures(Core::Application* p_App) {
            if (!p_App) {
                return;
            }

            LoadTextureSetFromDirectory(p_App, "textures/facade", m_vecFacadeTextures);
            LoadTextureSetFromDirectory(p_App, "textures/windows", m_vecWindowTextures);
            LoadTextureSetFromDirectory(p_App, "textures/door", m_vecDoorTextures);

            auto ensureFallback = [this, p_App](std::vector<GLuint>& vec_Target, const std::string& s_Fallback) {
                if (!vec_Target.empty()) {
                    return;
                }
                GLuint fallback = LoadShowcaseTexture(p_App, s_Fallback);
                if (fallback != 0) {
                    vec_Target.push_back(fallback);
                }
                else {
                    std::cerr << "[Building] Failed to load fallback texture: " << s_Fallback << std::endl;
                }
                };

            ensureFallback(m_vecFacadeTextures, "textures/facade/white.jpg");
            ensureFallback(m_vecWindowTextures, "textures/windows/window1.png");
            ensureFallback(m_vecDoorTextures, "textures/door/door1.jpg");

            if (m_TexBuildingRoof == 0) {
                m_TexBuildingRoof = LoadShowcaseTexture(p_App, "textures/210_clay roof texture seamless.jpg");
            }
        }

        GLuint EmptyEnvironmentState::PickRandomTexture(const std::vector<GLuint>& vec_Textures) {
            if (vec_Textures.empty()) {
                return 0;
            }

            std::uniform_int_distribution<size_t> dist(0, vec_Textures.size() - 1);
            return vec_Textures[dist(m_Rng)];
        }

        void EmptyEnvironmentState::AssignRandomTextures(BuildingRenderInstance& instance) {
            instance.facadeTexture = PickRandomTexture(m_vecFacadeTextures);
            instance.windowTexture = PickRandomTexture(m_vecWindowTextures);
            instance.doorTexture = PickRandomTexture(m_vecDoorTextures);
        }

        void EmptyEnvironmentState::DestroyBuildingTextures(Core::Application* p_App) {
            auto unloadSet = [p_App](std::vector<GLuint>& vec_Textures) {
                for (auto& tex : vec_Textures) {
                    if (!tex) {
                        continue;
                    }
                    if (p_App) {
                        p_App->UnloadTexture(tex);
                    }
                    else {
                        glDeleteTextures(1, &tex);
                    }
                    tex = 0;
                }
                vec_Textures.clear();
                };

            unloadSet(m_vecFacadeTextures);
            unloadSet(m_vecWindowTextures);
            unloadSet(m_vecDoorTextures);

            if (m_TexBuildingRoof) {
                if (p_App) {
                    p_App->UnloadTexture(m_TexBuildingRoof);
                }
                else {
                    glDeleteTextures(1, &m_TexBuildingRoof);
                }
                m_TexBuildingRoof = 0;
            }
        }

        void EmptyEnvironmentState::ConvertBuildingMeshForEnvironment(Core::BuildingMesh& mesh) {
            for (auto& vertex : mesh.vertices) {
                vertex = ConvertBuildingVector(vertex);
            }
            for (auto& normal : mesh.normals) {
                normal = ConvertBuildingNormal(normal);
            }
            for (auto& wall : mesh.windowWalls) {
                for (auto& vertex : wall.vertices) {
                    vertex = ConvertBuildingVector(vertex);
                }
                for (auto& normal : wall.normals) {
                    normal = ConvertBuildingNormal(normal);
                }
                wall.wallCenter = ConvertBuildingVector(wall.wallCenter);
                wall.wallNormal = ConvertBuildingNormal(wall.wallNormal);
            }
            for (auto& door : mesh.doors) {
                for (auto& vertex : door.vertices) {
                    vertex = ConvertBuildingVector(vertex);
                }
                for (auto& normal : door.normals) {
                    normal = ConvertBuildingNormal(normal);
                }
                door.wallCenter = ConvertBuildingVector(door.wallCenter);
                door.wallNormal = ConvertBuildingNormal(door.wallNormal);
            }
        }

        void EmptyEnvironmentState::ScaleBuildingMesh(Core::BuildingMesh& mesh, float f_Scale) {
            if (f_Scale <= 0.0f) {
                return;
            }

            auto scaleVec3 = [f_Scale](glm::vec3& vec) {
                vec *= f_Scale;
                };

            for (auto& vertex : mesh.vertices) {
                scaleVec3(vertex);
            }
            for (auto& tex : mesh.texCoords) {
                tex *= f_Scale;
            }

            for (auto& wall : mesh.windowWalls) {
                for (auto& vertex : wall.vertices) {
                    scaleVec3(vertex);
                }
                wall.wallCenter *= f_Scale;
            }

            for (auto& door : mesh.doors) {
                for (auto& vertex : door.vertices) {
                    scaleVec3(vertex);
                }
                door.wallCenter *= f_Scale;
            }
        }

        void EmptyEnvironmentState::ReleaseSurfaceBuffers(SurfaceBuffers& buffers) {
            if (buffers.vboPos) {
                glDeleteBuffers(1, &buffers.vboPos);
                buffers.vboPos = 0;
            }
            if (buffers.vboNormal) {
                glDeleteBuffers(1, &buffers.vboNormal);
                buffers.vboNormal = 0;
            }
            if (buffers.vboUV) {
                glDeleteBuffers(1, &buffers.vboUV);
                buffers.vboUV = 0;
            }
            if (buffers.ebo) {
                glDeleteBuffers(1, &buffers.ebo);
                buffers.ebo = 0;
            }
            if (buffers.vao) {
                glDeleteVertexArrays(1, &buffers.vao);
                buffers.vao = 0;
            }
        }

        bool EmptyEnvironmentState::UploadSurfaceBuffersFromData(
            const std::vector<glm::vec3>& positions,
            const std::vector<glm::vec3>& normals,
            const std::vector<glm::vec2>& uvs,
            const std::vector<unsigned int>& indices,
            SurfaceBuffers& outBuffers) {

            ReleaseSurfaceBuffers(outBuffers);

            if (positions.empty() || indices.empty()) {
                return false;
            }

            glGenVertexArrays(1, &outBuffers.vao);
            glBindVertexArray(outBuffers.vao);

            glGenBuffers(1, &outBuffers.vboPos);
            glBindBuffer(GL_ARRAY_BUFFER, outBuffers.vboPos);
            glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3), positions.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(0);

            const std::vector<glm::vec3>* normalsPtr = &normals;
            std::vector<glm::vec3> fallbackNormals;
            if (normals.size() != positions.size()) {
                fallbackNormals.assign(positions.size(), glm::vec3(0.0f, 1.0f, 0.0f));
                normalsPtr = &fallbackNormals;
            }

            glGenBuffers(1, &outBuffers.vboNormal);
            glBindBuffer(GL_ARRAY_BUFFER, outBuffers.vboNormal);
            glBufferData(GL_ARRAY_BUFFER, normalsPtr->size() * sizeof(glm::vec3), normalsPtr->data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
            glEnableVertexAttribArray(1);

            const std::vector<glm::vec2>* uvPtr = &uvs;
            std::vector<glm::vec2> fallbackUVs;
            if (uvs.size() != positions.size()) {
                fallbackUVs.assign(positions.size(), glm::vec2(0.0f));
                uvPtr = &fallbackUVs;
            }

            glGenBuffers(1, &outBuffers.vboUV);
            glBindBuffer(GL_ARRAY_BUFFER, outBuffers.vboUV);
            glBufferData(GL_ARRAY_BUFFER, uvPtr->size() * sizeof(glm::vec2), uvPtr->data(), GL_STATIC_DRAW);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
            glEnableVertexAttribArray(2);

            glGenBuffers(1, &outBuffers.ebo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, outBuffers.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

            return true;
        }

        void EmptyEnvironmentState::UploadWindowSurfaces(
            const std::vector<Core::BuildingMesh::WindowWall>& walls,
            std::vector<SurfaceBuffers>& outBuffers) {
            outBuffers.resize(walls.size());
            for (size_t i = 0; i < walls.size(); ++i) {
                UploadSurfaceBuffersFromData(
                    walls[i].vertices,
                    walls[i].normals,
                    walls[i].texCoords,
                    walls[i].indices,
                    outBuffers[i]);
            }
        }

        void EmptyEnvironmentState::UploadDoorSurfaces(
            const std::vector<Core::BuildingMesh::Door>& doors,
            std::vector<SurfaceBuffers>& outBuffers) {
            outBuffers.resize(doors.size());
            for (size_t i = 0; i < doors.size(); ++i) {
                UploadSurfaceBuffersFromData(
                    doors[i].vertices,
                    doors[i].normals,
                    doors[i].texCoords,
                    doors[i].indices,
                    outBuffers[i]);
            }
        }

        void EmptyEnvironmentState::ReleaseInstanceBuffers(BuildingRenderInstance& instance) {
            ReleaseSurfaceBuffers(instance.meshBuffers);
            for (auto& buffers : instance.windowSurfaces) {
                ReleaseSurfaceBuffers(buffers);
            }
            for (auto& buffers : instance.doorSurfaces) {
                ReleaseSurfaceBuffers(buffers);
            }
            instance.windowSurfaces.clear();
            instance.doorSurfaces.clear();
        }

        bool EmptyEnvironmentState::UploadBuildingInstance(BuildingRenderInstance& instance) {
            ReleaseInstanceBuffers(instance);

            bool b_MainUploaded = UploadSurfaceBuffersFromData(
                instance.mesh.vertices,
                instance.mesh.normals,
                instance.mesh.texCoords,
                instance.mesh.indices,
                instance.meshBuffers);

            if (!b_MainUploaded) {
                instance.ready = false;
                return false;
            }

            UploadWindowSurfaces(instance.mesh.windowWalls, instance.windowSurfaces);
            UploadDoorSurfaces(instance.mesh.doors, instance.doorSurfaces);

            instance.ready = (instance.meshBuffers.vao != 0);
            return instance.ready;
        }

        void EmptyEnvironmentState::DestroyBuildingInstance(BuildingRenderInstance& instance) {
            ReleaseInstanceBuffers(instance);
            instance.mesh = Core::BuildingMesh();
            instance.unitScale = 1.0f;
            instance.position = glm::vec3(0.0f);
            instance.ready = false;
        }

        void EmptyEnvironmentState::InitializeShowcaseBuilding(Core::Application* p_App) {
            DestroyShowcaseBuilding(p_App);

            if (!p_App || p_App->IsTrainingMode()) {
                return;
            }

            const float f_BuildingWidth = 8.0f;
            const float f_BuildingDepth = 5.0f;
            const float f_BuildingHeight = 9.0f;
            const float f_RoofHeight = 2.5f;

            m_ShowcaseBuilding.position = glm::vec3(11.0f, 0.0f, 8.0f);
            m_ShowcaseBuilding.unitScale = k_ShowcaseBuildingScale;

            std::vector<glm::vec2> vec_BasePoints = {
                { -f_BuildingWidth * 0.5f, -f_BuildingDepth * 0.5f },
                {  f_BuildingWidth * 0.5f, -f_BuildingDepth * 0.5f },
                {  f_BuildingWidth * 0.5f,  f_BuildingDepth * 0.5f },
                { -f_BuildingWidth * 0.5f,  f_BuildingDepth * 0.5f }
            };

            try {
                m_ShowcaseBuilding.mesh = Core::BuildingGenerator::GenerateBuilding(
                    vec_BasePoints, f_BuildingHeight, true, f_RoofHeight);
            }
            catch (const std::exception& e) {
                std::cerr << "[Building] Generation failed: " << e.what() << std::endl;
                m_ShowcaseBuilding.ready = false;
                return;
            }

            ConvertBuildingMeshForEnvironment(m_ShowcaseBuilding.mesh);
            ScaleBuildingMesh(m_ShowcaseBuilding.mesh, k_ShowcaseBuildingScale);

            if (!UploadBuildingInstance(m_ShowcaseBuilding)) {
                std::cerr << "[Building] Upload failed." << std::endl;
            }

            m_ShaderBuilding = CreateBuildingShaderProgram();
            if (!m_ShaderBuilding) {
                std::cerr << "[Building] Shader creation failed." << std::endl;
                m_ShowcaseBuilding.ready = false;
                return;
            }
            LoadBuildingTextures(p_App);
            AssignRandomTextures(m_ShowcaseBuilding);

            m_ShowcaseBuilding.ready = (m_ShowcaseBuilding.meshBuffers.vao != 0 && m_ShaderBuilding != 0);
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
            glm::vec3 vec3_Right = glm::normalize(glm::cross(vec3_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
            m_vec3_CameraVelocity += -vec3_Right * k_CameraAcceleration * f_DeltaTime;

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

            glm::vec3 vec3_HorizontalVelocity(m_vec3_CameraVelocity.x, 0.0f, m_vec3_CameraVelocity.z);
            m_vec3_Saved3DCameraPosition += vec3_HorizontalVelocity * f_DeltaTime;

            m_vec3_CameraVelocity *= k_CameraFriction;
            if (glm::length(m_vec3_CameraVelocity) < 0.01f) {
                m_vec3_CameraVelocity = glm::vec3(0.0f);
            }
        }

        void EmptyEnvironmentState::RenderShowcaseBuilding(const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection, const glm::vec3& vec3_CameraPos) {
            RenderBuildingInstance(m_ShowcaseBuilding, mat4_View, mat4_Projection, vec3_CameraPos);
        }

        void EmptyEnvironmentState::RenderBuildingInstance(const BuildingRenderInstance& instance,
            const glm::mat4& mat4_View, const glm::mat4& mat4_Projection,
            const glm::vec3& vec3_CameraPos) const {

            if (!instance.ready || !m_ShaderBuilding || instance.meshBuffers.vao == 0) {
                return;
            }

            glUseProgram(m_ShaderBuilding);

            glm::mat4 model = glm::translate(glm::mat4(1.0f), instance.position + glm::vec3(0.0f, 0.01f, 0.0f));
            glUniformMatrix4fv(glGetUniformLocation(m_ShaderBuilding, "model"), 1, GL_FALSE, glm::value_ptr(model));
            glUniformMatrix4fv(glGetUniformLocation(m_ShaderBuilding, "view"), 1, GL_FALSE, glm::value_ptr(mat4_View));
            glUniformMatrix4fv(glGetUniformLocation(m_ShaderBuilding, "projection"), 1, GL_FALSE, glm::value_ptr(mat4_Projection));

            glm::vec3 lightPos = vec3_CameraPos + glm::vec3(6.0f, 8.0f, 4.0f);
            glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
            glUniform3fv(glGetUniformLocation(m_ShaderBuilding, "lightPos"), 1, glm::value_ptr(lightPos));
            glUniform3fv(glGetUniformLocation(m_ShaderBuilding, "viewPos"), 1, glm::value_ptr(vec3_CameraPos));
            glUniform3fv(glGetUniformLocation(m_ShaderBuilding, "lightColor"), 1, glm::value_ptr(lightColor));
            glUniform1f(glGetUniformLocation(m_ShaderBuilding, "textureExposure"), 1.0f);

            GLboolean b_CullWasEnabledBase = glIsEnabled(GL_CULL_FACE);
            if (b_CullWasEnabledBase) {
                glDisable(GL_CULL_FACE);
            }

            float f_Scale = instance.unitScale > 1e-4f ? instance.unitScale : 1.0f;

            glBindVertexArray(instance.meshBuffers.vao);
            for (const auto& material : instance.mesh.materials) {
                bool b_IsRoof = (material.name == "roof") && (m_TexBuildingRoof != 0);
                GLuint texture = b_IsRoof ? m_TexBuildingRoof : instance.facadeTexture;
                bool b_UseTexture = (texture != 0);

                if (b_UseTexture) {
                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, texture);
                    glUniform1i(glGetUniformLocation(m_ShaderBuilding, "texture1"), 0);
                }
                glUniform1i(glGetUniformLocation(m_ShaderBuilding, "useTexture"), b_UseTexture ? 1 : 0);
                glUniform1i(glGetUniformLocation(m_ShaderBuilding, "isRoof"), b_IsRoof ? 1 : 0);
                float f_RoofUVScale = b_IsRoof ? (k_RoofBaseUVScale / f_Scale) : 1.0f;
                glUniform1f(glGetUniformLocation(m_ShaderBuilding, "roofUVScale"), f_RoofUVScale);
                glUniform3fv(glGetUniformLocation(m_ShaderBuilding, "objColor"), 1, glm::value_ptr(material.color));

                glDrawElements(GL_TRIANGLES, material.indexCount, GL_UNSIGNED_INT,
                    (void*)(material.firstIndex * sizeof(unsigned int)));
            }
            glBindVertexArray(0);

            if (b_CullWasEnabledBase) {
                glEnable(GL_CULL_FACE);
            }

            bool b_HasWindows = (instance.windowTexture != 0) && !instance.mesh.windowWalls.empty();
            bool b_HasDoors = (instance.doorTexture != 0) && !instance.mesh.doors.empty();

            if (!(b_HasWindows || b_HasDoors)) {
                return;
            }

            glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));

            GLboolean b_CullWasEnabled = glIsEnabled(GL_CULL_FACE);
            GLboolean b_DepthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            GLint i_PrevDepthFunc = GL_LESS;
            glGetIntegerv(GL_DEPTH_FUNC, &i_PrevDepthFunc);

            glDepthMask(GL_FALSE);
            glDisable(GL_CULL_FACE);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            if (b_HasWindows) {
                size_t windowCount = std::min(instance.mesh.windowWalls.size(), instance.windowSurfaces.size());
                for (size_t i = 0; i < windowCount; ++i) {
                    const auto& wall = instance.mesh.windowWalls[i];
                    const auto& buffers = instance.windowSurfaces[i];
                    if (!buffers.vao) {
                        continue;
                    }

                    glm::vec3 wallCenterWorld = glm::vec3(model * glm::vec4(wall.wallCenter, 1.0f));
                    glm::vec3 wallNormalWorld = glm::normalize(normalMatrix * wall.wallNormal);
                    glm::vec3 viewDir = glm::normalize(vec3_CameraPos - wallCenterWorld);
                    if (glm::dot(wallNormalWorld, viewDir) <= 0.0f) {
                        continue;
                    }

                    glActiveTexture(GL_TEXTURE0);
                    glBindTexture(GL_TEXTURE_2D, instance.windowTexture);
                    glUniform1i(glGetUniformLocation(m_ShaderBuilding, "texture1"), 0);
                    glUniform1i(glGetUniformLocation(m_ShaderBuilding, "useTexture"), 1);
                    glUniform1i(glGetUniformLocation(m_ShaderBuilding, "isRoof"), 0);
                    glUniform1f(glGetUniformLocation(m_ShaderBuilding, "roofUVScale"), 1.0f);
                    glUniform1f(glGetUniformLocation(m_ShaderBuilding, "textureExposure"), 1.0f);

                    glBindVertexArray(buffers.vao);
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(wall.indices.size()), GL_UNSIGNED_INT, 0);
                }
            }

            if (b_HasDoors) {
                size_t doorCount = std::min(instance.mesh.doors.size(), instance.doorSurfaces.size());
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, instance.doorTexture);
                glUniform1i(glGetUniformLocation(m_ShaderBuilding, "texture1"), 0);
                glUniform1i(glGetUniformLocation(m_ShaderBuilding, "useTexture"), 1);
                glUniform1i(glGetUniformLocation(m_ShaderBuilding, "isRoof"), 0);
                glUniform1f(glGetUniformLocation(m_ShaderBuilding, "roofUVScale"), 1.0f);
                glUniform1f(glGetUniformLocation(m_ShaderBuilding, "textureExposure"), 1.0f);

                for (size_t i = 0; i < doorCount; ++i) {
                    const auto& door = instance.mesh.doors[i];
                    const auto& buffers = instance.doorSurfaces[i];
                    if (!buffers.vao) {
                        continue;
                    }

                    glm::vec3 doorCenterWorld = glm::vec3(model * glm::vec4(door.wallCenter, 1.0f));
                    glm::vec3 doorNormalWorld = glm::normalize(normalMatrix * door.wallNormal);
                    glm::vec3 viewDir = glm::normalize(vec3_CameraPos - doorCenterWorld);
                    if (glm::dot(doorNormalWorld, viewDir) <= 0.0f) {
                        continue;
                    }

                    glBindVertexArray(buffers.vao);
                    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(door.indices.size()), GL_UNSIGNED_INT, 0);
                }
            }

            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glDepthFunc(i_PrevDepthFunc);
            if (!b_DepthWasEnabled) {
                glDisable(GL_DEPTH_TEST);
            }
            if (b_CullWasEnabled) {
                glEnable(GL_CULL_FACE);
            }
        }

        void EmptyEnvironmentState::RenderGeneratedBuildings(const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection, const glm::vec3& vec3_CameraPos) {
            for (const auto& building : m_GeneratedBuildings) {
                RenderBuildingInstance(building, mat4_View, mat4_Projection, vec3_CameraPos);
            }
        }

        void EmptyEnvironmentState::DestroyGeneratedBuildings() {
            for (auto& building : m_GeneratedBuildings) {
                DestroyBuildingInstance(building);
            }
            m_GeneratedBuildings.clear();
        }

        void EmptyEnvironmentState::BuildBuildingsFromMapData() {
            DestroyGeneratedBuildings();

            if (!m_b_MapDataLoaded || m_MapData.vec_Buildings.empty()) {
                return;
            }

            const float f_ScaleX = k_PlaneWidth / k_MapWidth;
            const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
            const float f_OffsetX = k_MapOffsetX;
            const float f_OffsetZ = k_MapOffsetZ;
            const float f_HeightScale = 0.5f * (f_ScaleX + f_ScaleZ);

            size_t builtCount = 0;
            m_GeneratedBuildings.reserve(m_MapData.vec_Buildings.size());

            for (const auto& data : m_MapData.vec_Buildings) {
                if (data.vec_BaseFootprint.size() != 4) {
                    continue;
                }

                std::vector<glm::vec2> vec_Footprint = data.vec_BaseFootprint;

                float f_Height = data.f_Height;
                float f_RoofHeight = data.f_RoofHeight;

                BuildingRenderInstance instance;
                instance.unitScale = f_HeightScale;
                instance.position = glm::vec3(
                    data.vec3_Position.x * f_ScaleX + f_OffsetX,
                    0.0f,
                    data.vec3_Position.y * f_ScaleZ + f_OffsetZ);

                try {
                    instance.mesh = Core::BuildingGenerator::GenerateBuilding(
                        vec_Footprint,
                        f_Height,
                        data.b_HasRoof,
                        f_RoofHeight,
                        data.vec3_WallColor,
                        data.vec3_RoofColor);
                }
                catch (const std::exception& e) {
                    std::cerr << "[Building] Lot generation failed: " << e.what() << std::endl;
                    continue;
                }

                ConvertBuildingMeshForEnvironment(instance.mesh);
                ScaleBuildingMesh(instance.mesh, f_HeightScale);
                instance.unitScale = f_HeightScale;

                if (!UploadBuildingInstance(instance)) {
                    std::cerr << "[Building] Failed to upload generated building." << std::endl;
                    continue;
                }

                AssignRandomTextures(instance);
                m_GeneratedBuildings.push_back(std::move(instance));
                ++builtCount;
            }

            std::cout << "[EmptyEnvironmentState] Prepared " << builtCount << " generated buildings." << std::endl;
            if (!m_GeneratedBuildings.empty()) {
                const auto& sample = m_GeneratedBuildings.front();
                std::cout << "[EmptyEnvironmentState] First building position (world): "
                    << sample.position.x << ", " << sample.position.z << std::endl;
            }
        }

        void EmptyEnvironmentState::CreateTestRoad(Core::Application* p_App) {
            if (!p_App) {
                return;
            }

            if (m_VAO_Road) { glDeleteVertexArrays(1, &m_VAO_Road); m_VAO_Road = 0; }
            if (m_VBO_Road) { glDeleteBuffers(1, &m_VBO_Road); m_VBO_Road = 0; }
            if (m_EBO_Road) { glDeleteBuffers(1, &m_EBO_Road); m_EBO_Road = 0; }
            m_RoadIndexCount = 0;

            struct RoadVertex {
                glm::vec3 pos;
                glm::vec3 normal;
                glm::vec2 uv;
            };

            const float halfWidth = k_PlaneWidth * 0.5f;
            const float startZ = -2.0f;
            const float endZ = 12.0f;

            std::vector<RoadVertex> vertices = {
                {{-halfWidth, 0.001f, startZ}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                {{ halfWidth, 0.001f, startZ}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                {{-halfWidth, 0.001f, endZ},   {0.0f, 1.0f, 0.0f}, {0.0f, 4.0f}},
                {{ halfWidth, 0.001f, endZ},   {0.0f, 1.0f, 0.0f}, {1.0f, 4.0f}},
            };

            std::vector<uint32_t> indices = { 0, 1, 2, 2, 1, 3 };

            glGenVertexArrays(1, &m_VAO_Road);
            glBindVertexArray(m_VAO_Road);

            glGenBuffers(1, &m_VBO_Road);
            glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Road);
            glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(RoadVertex), vertices.data(), GL_STATIC_DRAW);

            glGenBuffers(1, &m_EBO_Road);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO_Road);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RoadVertex), (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RoadVertex), (void*)offsetof(RoadVertex, normal));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(RoadVertex), (void*)offsetof(RoadVertex, uv));
            glEnableVertexAttribArray(2);

            glBindVertexArray(0);

            m_RoadIndexCount = static_cast<int>(indices.size());

            if (!m_TexRoad) {
                m_TexRoad = p_App->LoadTexture(p_App->GetAssetPath("textures/road.jpg"));
                if (m_TexRoad == 0) {
                    std::cerr << "[Road] Failed to load road texture!" << std::endl;
                }
                else {
                    std::cout << "[Road] Road texture loaded successfully." << std::endl;
                }
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

            BuildBuildingsFromMapData();
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

            float f_FallbackWidth = 2.8f;
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
        }

        void EmptyEnvironmentState::BuildHighwaysFromMapData(Core::Application* p_App)
        {
            if (!m_b_MapDataLoaded || m_vec_Highways.empty()) return;


            std::cout << "[EmptyEnvironmentState] Building highways from map data..." << std::endl;

            // Najpierw wyczyść poprzednie meshe
            for (auto& roadMesh : m_HighwayMeshes) {
                if (roadMesh.VAO) glDeleteVertexArrays(1, &roadMesh.VAO);
                if (roadMesh.VBO) glDeleteBuffers(1, &roadMesh.VBO);
                if (roadMesh.EBO) glDeleteBuffers(1, &roadMesh.EBO);
            }
            m_HighwayMeshes.clear();

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

                    if (i == 0)
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
                    points[i].x = (points[i].x * 0.02f) - 1.0f;
                    points[i].y = (points[i].y * 0.02f) - 1.0f;
                }

                // Generujemy mesh drogi
                auto mesh = ScotlandYard::Core::RoadGenerator::GenerateRoad(points, widths, 2.0f);

                // Podnosimy wszystkie wierzchołki na y = 0.5f
                for (auto& vertex : mesh.vertices)
                {
                    vertex.y = 0.012f + hwIdx * 0.001f; // unieś nad ziemię, aby uniknąć z-fightingu przy wielu highwayach
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
                m_HighwayMeshes.push_back(roadMesh);

                std::cout << "[Highway " << hwIdx << "] Built highway with "
                    << highway.roadIndices.size() << " segments, length: "
                    << (int)highway.totalLength << std::endl;
            }

            // Załaduj teksturę drogi
            m_TexHighway = p_App->LoadTexture(p_App->GetAssetPath("textures/road.jpg"));

            std::cout << "[EmptyEnvironmentState] Built " << m_HighwayMeshes.size() << " highway meshes." << std::endl;
        }

        void EmptyEnvironmentState::GenerateRoadsFromMapData(Core::Application* p_App)
        {
            if (m_MapData.vec_GraphNodes.empty() || m_MapData.vec_Streets.empty()) return;

            std::cout << "[EmptyEnvironmentState] Generating roads from map data..." << std::endl;

            // --- wyczyść poprzednie meshe ---
            for (auto& roadMesh : m_RoadMeshes) {
                if (roadMesh.VAO) glDeleteVertexArrays(1, &roadMesh.VAO);
                if (roadMesh.VBO) glDeleteBuffers(1, &roadMesh.VBO);
                if (roadMesh.EBO) glDeleteBuffers(1, &roadMesh.EBO);
            }
            m_RoadMeshes.clear();

            const float textureRepeat = 2.0f;

            // --- skalowanie mapy ---
            const float f_ScaleX = k_PlaneWidth / k_MapWidth;
            const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
            const float f_OffsetX = k_MapOffsetX;
            const float f_OffsetZ = k_MapOffsetZ;
            float Height = 0.01f;
        
            
            // --- generowanie segmentów ulic ---
            for (size_t segIdx = 0; segIdx < m_MapData.vec_Streets.size(); ++segIdx)
            {
                const auto& street = m_MapData.vec_Streets[segIdx];
                const auto& node1 = m_MapData.vec_GraphNodes[street.i_Node1];
                const auto& node2 = m_MapData.vec_GraphNodes[street.i_Node2];
                std::vector<glm::vec2> points;

                float accumulatedLength = 0.0f;
                float halfWidth = 0.06f;

                glm::vec2 pos1(
                    node1.position.x *  f_ScaleX + f_OffsetX,
                    node1.position.y *  f_ScaleZ + f_OffsetZ
                );
                glm::vec2 pos2(
                    node2.position.x *  f_ScaleX + f_OffsetX,
                    node2.position.y *  f_ScaleZ + f_OffsetZ
                );

                auto segMesh = Core::RoadGenerator::GenerateRoadSegment(
                    pos1,
                    pos2,
                    halfWidth,
                    textureRepeat,
                    accumulatedLength
                );

                // ustaw wysokość drogi
                for (auto& vertex : segMesh.vertices)
                {
                    vertex.y = Height;
                    // std::cout << "Vertex position: (" << vertex.x << ", " << vertex.y << ", " << vertex.z << ")\n";
                }
                    
                // --- VAO/VBO/EBO ---
                struct V { glm::vec3 p; glm::vec3 n; glm::vec2 uv; };
                std::vector<V> verts;
                for (size_t i = 0; i < segMesh.vertices.size(); ++i)
                    verts.push_back({ segMesh.vertices[i], segMesh.normals[i], segMesh.texCoords[i] });

                RoadMesh roadMesh{};
                glGenVertexArrays(1, &roadMesh.VAO);
                glGenBuffers(1, &roadMesh.VBO);
                glGenBuffers(1, &roadMesh.EBO);

                glBindVertexArray(roadMesh.VAO);
                glBindBuffer(GL_ARRAY_BUFFER, roadMesh.VBO);
                glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);

                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, roadMesh.EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, segMesh.indices.size() * sizeof(unsigned int),
                            segMesh.indices.data(), GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
                glEnableVertexAttribArray(2);

                glBindVertexArray(0);

                roadMesh.indexCount = static_cast<int>(segMesh.indices.size());
                m_RoadMeshes.push_back(roadMesh);
            }

            // --- generowanie round joinów (skrzyżowań) ---
            for (auto& node : m_MapData.vec_GraphNodes)
            {
                std::vector<glm::vec2> neighborPositions;

                for (const auto& street : m_MapData.vec_Streets)
                {
                    if (street.i_Node1 == node.i_ID)
                        neighborPositions.push_back(glm::vec2(
                            m_MapData.vec_GraphNodes[street.i_Node2].position.x,
                            m_MapData.vec_GraphNodes[street.i_Node2].position.y
                        ));
                    else if (street.i_Node2 == node.i_ID)
                        neighborPositions.push_back(glm::vec2(
                            m_MapData.vec_GraphNodes[street.i_Node1].position.x,
                            m_MapData.vec_GraphNodes[street.i_Node1].position.y
                        ));
                }

                if (neighborPositions.size() >= 2)
                {
                    // --- skalowanie węzła ---
                    glm::vec2 nodePosWorld(
                        node.position.x * f_ScaleX + f_OffsetX,
                        node.position.y * f_ScaleZ + f_OffsetZ
                    );

                    // --- skalowanie sąsiadów ---
                    std::vector<glm::vec2> neighborPositionsWorld;
                    for (const auto& pos : neighborPositions)
                        neighborPositionsWorld.push_back(glm::vec2(
                            pos.x * f_ScaleX + f_OffsetX,
                            pos.y * f_ScaleZ + f_OffsetZ
                        ));

                    auto joinMesh = Core::RoadGenerator::GenerateRoundJoin(
                        nodePosWorld,
                        neighborPositionsWorld,
                        0.06f, // połowa szerokości pasa
                        6       // segmenty łuku
                    );

                    // ustaw wysokość drogi
                    for (auto& vertex : joinMesh.vertices)
                    {
                        vertex.y = Height+0.001f;;
                    }
                       
                    struct V { glm::vec3 p; glm::vec3 n; glm::vec2 uv; };
                    std::vector<V> verts;
                    for (size_t i = 0; i < joinMesh.vertices.size(); ++i)
                        verts.push_back({ joinMesh.vertices[i], joinMesh.normals[i], joinMesh.texCoords[i] });

                    RoadMesh roadMesh{};
                    glGenVertexArrays(1, &roadMesh.VAO);
                    glGenBuffers(1, &roadMesh.VBO);
                    glGenBuffers(1, &roadMesh.EBO);

                    glBindVertexArray(roadMesh.VAO);
                    glBindBuffer(GL_ARRAY_BUFFER, roadMesh.VBO);
                    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);

                    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, roadMesh.EBO);
                    glBufferData(GL_ELEMENT_ARRAY_BUFFER, joinMesh.indices.size() * sizeof(unsigned int),
                                joinMesh.indices.data(), GL_STATIC_DRAW);

                    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
                    glEnableVertexAttribArray(2);

                    glBindVertexArray(0);

                    roadMesh.indexCount = static_cast<int>(joinMesh.indices.size());
                    m_RoadMeshes.push_back(roadMesh);
                }
            }

            // --- załaduj teksturę drogi ---
            m_TexRoad = p_App->LoadTexture(p_App->GetAssetPath("textures/gravel.jpg"));

            std::cout << "[EmptyEnvironmentState] Generated " << m_RoadMeshes.size() << " road meshes." << std::endl;

        }

        void EmptyEnvironmentState::BuildParkPathsFromMapData() {
            // Czyścimy stare meshe
            for (auto& mesh : m_ParkPathMeshes) {
                if (mesh.VAO) glDeleteVertexArrays(1, &mesh.VAO);
                if (mesh.VBO) glDeleteBuffers(1, &mesh.VBO);
                if (mesh.EBO) glDeleteBuffers(1, &mesh.EBO);
            }
            m_ParkPathMeshes.clear();

            if (!m_b_MapDataLoaded) return;

            std::cout << "[EmptyEnvironmentState] Building park paths..." << std::endl;

            for (const auto& road : m_vec_HighwayRoads) {
                // Interesują nas tylko ścieżki parkowe
                if (road.isDeleted || road.type != CityGen::RoadType::PARK_PATH) continue;
                if (road.startNodeIdx < 0 || road.endNodeIdx < 0) continue;

                const auto& n1 = m_vec_HighwayNodes[road.startNodeIdx];
                const auto& n2 = m_vec_HighwayNodes[road.endNodeIdx];

                std::vector<glm::vec2> points;
                points.emplace_back(n1.x, n1.y);
                points.emplace_back(n2.x, n2.y);

                // Transformacja do świata 3D (taka sama jak w BuildHighways)
                for (auto& p : points) {
                    p.x = (p.x * 0.02f) - 1.0f;
                    p.y = (p.y * 0.02f) - 1.0f;
                }

                std::vector<float> widths = { 0.08f, 0.08f }; // Węższe niż ulice (0.25f)

                // Generowanie mesha
                auto mesh = ScotlandYard::Core::RoadGenerator::GenerateRoad(points, widths, 1.0f);

                // Podnieś lekko nad ziemię, ale niżej/wyżej niż trawa?
                // Trawa jest na 0.0, ulice na 0.01+.
                // Dajmy ścieżki na 0.015f, żeby leżały na trawie.
                for (auto& vertex : mesh.vertices) {
                    vertex.y = 0.015f;
                }

                // Upload do GPU (kopiuj-wklej z BuildHighways logic)
                struct V { glm::vec3 p; glm::vec3 n; glm::vec2 uv; };
                std::vector<V> verts;
                for (size_t i = 0; i < mesh.vertices.size(); ++i)
                    verts.push_back({ mesh.vertices[i], mesh.normals[i], mesh.texCoords[i] });

                RoadMesh pathMesh{};
                glGenVertexArrays(1, &pathMesh.VAO);
                glGenBuffers(1, &pathMesh.VBO);
                glGenBuffers(1, &pathMesh.EBO);

                glBindVertexArray(pathMesh.VAO);
                glBindBuffer(GL_ARRAY_BUFFER, pathMesh.VBO);
                glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);
                glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pathMesh.EBO);
                glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * sizeof(unsigned int), mesh.indices.data(), GL_STATIC_DRAW);

                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)0);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, n));
                glEnableVertexAttribArray(1);
                glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
                glEnableVertexAttribArray(2);
                glBindVertexArray(0);

                pathMesh.indexCount = static_cast<int>(mesh.indices.size());
                m_ParkPathMeshes.push_back(pathMesh);
            }
            std::cout << "[EmptyEnvironmentState] Built " << m_ParkPathMeshes.size() << " park path meshes." << std::endl;
        }

        void EmptyEnvironmentState::BuildTreesFromMapData() {
            DestroyTrees();

            if (m_vec_TreeData.empty()) return;

            std::cout << "[EmptyEnvironmentState] Building " << m_vec_TreeData.size() << " 3D trees..." << std::endl;

            const float f_ScaleX = k_PlaneWidth / k_MapWidth;
            const float f_ScaleZ = k_PlaneDepth / k_MapHeight;
            const float f_OffsetX = k_MapOffsetX;
            const float f_OffsetZ = k_MapOffsetZ;

            Core::TreeParams params;
            params.trunkHeight = 3.0f;
            params.crownRadius = 1.5f;

            for (const auto& treeInstance : m_vec_TreeData) {
                TreeRenderObj renderObj;

                // 1. Oblicz pozycję w świecie 3D
                float worldX = treeInstance.position.x * f_ScaleX + f_OffsetX;
                float worldZ = treeInstance.position.y * f_ScaleZ + f_OffsetZ;

                float globalTreeScale = 0.06f * treeInstance.scale; 

                // 2. Budowanie macierzy modelu
                renderObj.modelMatrix = glm::translate(glm::mat4(1.0f), glm::vec3(worldX, 0.0f, worldZ));


                renderObj.modelMatrix = glm::rotate(renderObj.modelMatrix, glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));


                renderObj.modelMatrix = glm::scale(renderObj.modelMatrix, glm::vec3(globalTreeScale));

                // 3. Generuj mesh (bez zmian)
                renderObj.mesh = Core::TreeGenerator::GenerateTree(params, treeInstance.seed);

                bool success = UploadSurfaceBuffersFromData(
                    renderObj.mesh.vertices,
                    renderObj.mesh.normals,
                    renderObj.mesh.texCoords,
                    renderObj.mesh.indices,
                    renderObj.buffers
                );

                if (success) {
                    renderObj.materialCount = (int)renderObj.mesh.materials.size();
                    m_RenderTrees.push_back(std::move(renderObj));
                }
            }
        }

        void EmptyEnvironmentState::RenderTrees(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection, const glm::vec3& vec3_CameraPos) {
            if (m_RenderTrees.empty() || !m_ShaderTree) return;

            glUseProgram(m_ShaderTree);

            glUniform3fv(glGetUniformLocation(m_ShaderTree, "uLightPos"), 1, glm::value_ptr(vec3_CameraPos + glm::vec3(5.0f, 10.0f, 5.0f)));
            glUniform3fv(glGetUniformLocation(m_ShaderTree, "uViewPos"), 1, glm::value_ptr(vec3_CameraPos));

            GLint locMVP = glGetUniformLocation(m_ShaderTree, "uMVP");
            GLint locModel = glGetUniformLocation(m_ShaderTree, "uModel");
            GLint locTex = glGetUniformLocation(m_ShaderTree, "uTex");

            glActiveTexture(GL_TEXTURE0);

            for (const auto& tree : m_RenderTrees) {
                if (!tree.buffers.vao) continue;

                glm::mat4 mvp = mat4_Projection * mat4_View * tree.modelMatrix;
                glUniformMatrix4fv(locMVP, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniformMatrix4fv(locModel, 1, GL_FALSE, glm::value_ptr(tree.modelMatrix));

                glBindVertexArray(tree.buffers.vao);

                for (const auto& mat : tree.mesh.materials) {
                    if (mat.name == "trunk") {
                        glBindTexture(GL_TEXTURE_2D, m_TexTreeTrunk);
                    }
                    else {
                        glBindTexture(GL_TEXTURE_2D, m_TexTreeCrown);
                    }
                    glUniform1i(locTex, 0);

                    glDrawElements(GL_TRIANGLES, mat.indexCount, GL_UNSIGNED_INT,
                        (void*)(mat.firstIndex * sizeof(unsigned int)));
                }
            }
            glBindVertexArray(0);
        }

        void EmptyEnvironmentState::DestroyTrees() {
            for (auto& tree : m_RenderTrees) {
                ReleaseSurfaceBuffers(tree.buffers);
            }
            m_RenderTrees.clear();
        }

        std::vector<float> EmptyEnvironmentState::generateCircleVertices(float f_Radius, int i_Segments) {
            std::vector<float> vec_Vertices;

            // Center point
            vec_Vertices.push_back(0.0f);
            vec_Vertices.push_back(0.01f);
            vec_Vertices.push_back(0.0f);

            // Perimeter points
            for (int i = 0; i <= i_Segments; i++) {
                float f_Theta = 2.0f * glm::pi<float>() * i / i_Segments;
                float f_X = f_Radius * cos(f_Theta);
                float f_Z = f_Radius * sin(f_Theta);
                vec_Vertices.push_back(f_X);
                vec_Vertices.push_back(0.01f);
                vec_Vertices.push_back(f_Z);
            }

            return vec_Vertices;
        }

        std::vector<float> EmptyEnvironmentState::generateCylinderVertices(float radius, float height, int segments) {
            std::vector<float> verts;
            const float k_TwoPi = 2.0f * glm::pi<float>();

            for (int i = 0; i < segments; ++i) {
                float theta1 = (float)i / segments * k_TwoPi;
                float theta2 = (float)(i + 1) / segments * k_TwoPi;

                float x1 = radius * cos(theta1);
                float z1 = radius * sin(theta1);
                float x2 = radius * cos(theta2);
                float z2 = radius * sin(theta2);

                verts.push_back(x1); verts.push_back(0.0f); verts.push_back(z1);
                verts.push_back(x2); verts.push_back(0.0f); verts.push_back(z2);
                verts.push_back(x2); verts.push_back(height); verts.push_back(z2);

                verts.push_back(x1); verts.push_back(0.0f); verts.push_back(z1);
                verts.push_back(x2); verts.push_back(height); verts.push_back(z2);
                verts.push_back(x1); verts.push_back(height); verts.push_back(z1);

                verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(0.0f);
                verts.push_back(x2); verts.push_back(0.0f); verts.push_back(z2);
                verts.push_back(x1); verts.push_back(0.0f); verts.push_back(z1);

                verts.push_back(0.0f); verts.push_back(height); verts.push_back(0.0f);
                verts.push_back(x1); verts.push_back(height); verts.push_back(z1);
                verts.push_back(x2); verts.push_back(height); verts.push_back(z2);
            }

            return verts;
        }

        std::vector<float> EmptyEnvironmentState::generateHemisphereVertices(float radius, int segments) {
            std::vector<float> verts;
            const float k_Pi = glm::pi<float>();
            const float k_TwoPi = 2.0f * k_Pi;

            for (int i = 0; i < segments / 2; ++i) {
                float theta1 = k_Pi * i / segments;
                float theta2 = k_Pi * (i + 1) / segments;

                for (int j = 0; j < segments; ++j) {
                    float phi1 = k_TwoPi * j / segments;
                    float phi2 = k_TwoPi * (j + 1) / segments;

                    float x1 = radius * sin(theta1) * cos(phi1);
                    float y1 = radius * cos(theta1);
                    float z1 = radius * sin(theta1) * sin(phi1);

                    float x2 = radius * sin(theta2) * cos(phi1);
                    float y2 = radius * cos(theta2);
                    float z2 = radius * sin(theta2) * sin(phi1);

                    float x3 = radius * sin(theta2) * cos(phi2);
                    float y3 = radius * cos(theta2);
                    float z3 = radius * sin(theta2) * sin(phi2);

                    float x4 = radius * sin(theta1) * cos(phi2);
                    float y4 = radius * cos(theta1);
                    float z4 = radius * sin(theta1) * sin(phi2);

                    verts.push_back(x1); verts.push_back(y1); verts.push_back(z1);
                    verts.push_back(x2); verts.push_back(y2); verts.push_back(z2);
                    verts.push_back(x3); verts.push_back(y3); verts.push_back(z3);

                    verts.push_back(x1); verts.push_back(y1); verts.push_back(z1);
                    verts.push_back(x3); verts.push_back(y3); verts.push_back(z3);
                    verts.push_back(x4); verts.push_back(y4); verts.push_back(z4);
                }
            }

            return verts;
        }

        void EmptyEnvironmentState::InitializePlayerTokenGeometry() {
            DestroyPlayerTokenGeometry();

            std::vector<float> vec_CylinderVerts = generateCylinderVertices(0.1f, 0.2f, 20);
            m_i_PlayerCylinderVertexCount = static_cast<int>(vec_CylinderVerts.size() / 3);
            if (!vec_CylinderVerts.empty()) {
                glGenVertexArrays(1, &m_VAO_PlayerCylinder);
                glGenBuffers(1, &m_VBO_PlayerCylinder);
                glBindVertexArray(m_VAO_PlayerCylinder);
                glBindBuffer(GL_ARRAY_BUFFER, m_VBO_PlayerCylinder);
                glBufferData(GL_ARRAY_BUFFER, vec_CylinderVerts.size() * sizeof(float), vec_CylinderVerts.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);
                glBindVertexArray(0);
            }

            std::vector<float> vec_HemisphereVerts = generateHemisphereVertices(0.1f, 30);
            m_i_PlayerHemisphereVertexCount = static_cast<int>(vec_HemisphereVerts.size() / 3);
            if (!vec_HemisphereVerts.empty()) {
                glGenVertexArrays(1, &m_VAO_PlayerHemisphere);
                glGenBuffers(1, &m_VBO_PlayerHemisphere);
                glBindVertexArray(m_VAO_PlayerHemisphere);
                glBindBuffer(GL_ARRAY_BUFFER, m_VBO_PlayerHemisphere);
                glBufferData(GL_ARRAY_BUFFER, vec_HemisphereVerts.size() * sizeof(float), vec_HemisphereVerts.data(), GL_STATIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);
                glBindVertexArray(0);
            }
        }

        void EmptyEnvironmentState::DestroyPlayerTokenGeometry() {
            if (m_VBO_PlayerCylinder) {
                glDeleteBuffers(1, &m_VBO_PlayerCylinder);
                m_VBO_PlayerCylinder = 0;
            }
            if (m_VAO_PlayerCylinder) {
                glDeleteVertexArrays(1, &m_VAO_PlayerCylinder);
                m_VAO_PlayerCylinder = 0;
            }
            m_i_PlayerCylinderVertexCount = 0;

            if (m_VBO_PlayerHemisphere) {
                glDeleteBuffers(1, &m_VBO_PlayerHemisphere);
                m_VBO_PlayerHemisphere = 0;
            }
            if (m_VAO_PlayerHemisphere) {
                glDeleteVertexArrays(1, &m_VAO_PlayerHemisphere);
                m_VAO_PlayerHemisphere = 0;
            }
            m_i_PlayerHemisphereVertexCount = 0;
        }

        const EmptyEnvironmentState::StationCircle* EmptyEnvironmentState::FindStationCircle(int stationID) const {
            auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                [stationID](const StationCircle& sc) { return sc.stationID == stationID; });
            if (it == m_vec_CircleStations.end()) {
                return nullptr;
            }
            return &(*it);
        }

        void EmptyEnvironmentState::InitializeDebugPlayerTokens() {
            m_vec_PlayerTokens.clear();
            if (m_vec_CircleStations.empty()) {
                return;
            }

            constexpr size_t k_DetectiveCount = 4;
            const glm::vec3 vec3_DetectiveColor(0.0f, 0.0f, 1.0f);
            std::vector<int> vec_ConnectedStationIDs;
            if (m_b_GraphLoaded) {
                vec_ConnectedStationIDs.reserve(m_vec_CircleStations.size());
                for (const auto& station : m_vec_CircleStations) {
                    auto connections = m_graph.GetConnections(station.stationID);
                    bool b_HasConnection = std::any_of(connections.begin(), connections.end(),
                        [](const auto& edge) {
                            return edge.i_NodeId >= 0;
                        });
                    if (b_HasConnection) {
                        vec_ConnectedStationIDs.push_back(station.stationID);
                    }
                }
            }

            if (!vec_ConnectedStationIDs.empty()) {
                std::shuffle(vec_ConnectedStationIDs.begin(), vec_ConnectedStationIDs.end(), m_Rng);
            }
            else {
                std::cout << "[EmptyEnvironmentState] Warning: Using all stations for debug tokens (graph not loaded or no connected stations)." << std::endl;
            }

            auto pickStationId = [&](size_t index) -> int {
                if (!vec_ConnectedStationIDs.empty()) {
                    return vec_ConnectedStationIDs[index % vec_ConnectedStationIDs.size()];
                }
                return m_vec_CircleStations[index % m_vec_CircleStations.size()].stationID;
            };

            size_t i_AvailableStations = !vec_ConnectedStationIDs.empty() ? vec_ConnectedStationIDs.size() : m_vec_CircleStations.size();
            size_t i_Stride = std::max<size_t>(1, i_AvailableStations / (k_DetectiveCount + 1));
            size_t i_Cursor = 0;

            for (size_t i_Index = 0; i_Index < k_DetectiveCount && i_Index < i_AvailableStations; ++i_Index) {
                int i_StationId = pickStationId(i_Cursor);
                PlayerToken token{};
                token.i_StationID = i_StationId;
                token.vec3_Color = vec3_DetectiveColor;
                token.b_IsMrX = false;
                token.taxiTickets = Core::k_DetectiveTaxiTickets;
                token.busTickets = Core::k_DetectiveBusTickets;
                token.metroTickets = Core::k_DetectiveMetroTickets;
                token.blackTickets = 0;
                token.doubleTickets = 0;
                m_vec_PlayerTokens.push_back(token);
                i_Cursor += i_Stride;
            }

            int i_MrXStationId = pickStationId(i_Cursor);
            PlayerToken tokenMrX{};
            tokenMrX.i_StationID = i_MrXStationId;
            tokenMrX.vec3_Color = glm::vec3(0.0f, 0.0f, 0.0f);
            tokenMrX.b_IsMrX = true;
            tokenMrX.taxiTickets = Core::k_MrXTaxiTickets;
            tokenMrX.busTickets = Core::k_MrXBusTickets;
            tokenMrX.metroTickets = Core::k_MrXMetroTickets;
            tokenMrX.blackTickets = Core::k_MrXBlackTickets;
            tokenMrX.doubleTickets = Core::k_MrXDoubleMoveTickets;
            m_vec_PlayerTokens.push_back(tokenMrX);

            m_i_MrXTokenIndex = static_cast<int>(m_vec_PlayerTokens.size()) - 1;
            m_vec_TokenMovedThisRound.assign(m_vec_PlayerTokens.size(), false);
            m_i_CurrentRound = 1;
            m_b_IsMrXTurn = true;
            m_b_MrXSecondMovePending = false;

            ClearMovementSelection();
            m_i_SelectedStationID = -1;
            m_vec_HighlightedStations.clear();
            UI::ClearMrXSelections();
            UI::SetRound(m_i_CurrentRound);
            UpdateMrXButtonStates();
        }

        void EmptyEnvironmentState::RenderPlayerTokens(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
            if (m_vec_PlayerTokens.empty() || !m_ShaderCircle || !m_VAO_PlayerCylinder || !m_VAO_PlayerHemisphere) {
                return;
            }

            glUseProgram(m_ShaderCircle);
            GLuint mvpLoc = glGetUniformLocation(m_ShaderCircle, "MVP");
            GLuint colorLoc = glGetUniformLocation(m_ShaderCircle, "circleColor");

            std::unordered_map<int, std::vector<size_t>> map_TokensByStation;
            for (size_t i_Index = 0; i_Index < m_vec_PlayerTokens.size(); ++i_Index) {
                map_TokensByStation[m_vec_PlayerTokens[i_Index].i_StationID].push_back(i_Index);
            }

            for (const auto& pair_TokenEntry : map_TokensByStation) {
                const StationCircle* p_Station = FindStationCircle(pair_TokenEntry.first);
                if (!p_Station) {
                    continue;
                }

                const auto& vec_TokenIndices = pair_TokenEntry.second;
                for (size_t i_Order = 0; i_Order < vec_TokenIndices.size(); ++i_Order) {
                    const auto& token = m_vec_PlayerTokens[vec_TokenIndices[i_Order]];

                    float f_OffsetX = 0.0f;
                    float f_OffsetZ = 0.0f;
                    if (vec_TokenIndices.size() > 1) {
                        float f_Angle = (2.0f * glm::pi<float>() * static_cast<float>(i_Order)) / static_cast<float>(vec_TokenIndices.size());
                        f_OffsetX = k_PlayerMultiRadius * cos(f_Angle);
                        f_OffsetZ = k_PlayerMultiRadius * sin(f_Angle);
                    }

                    glm::mat4 mat4_Model = glm::translate(glm::mat4(1.0f),
                        glm::vec3(p_Station->position.x + f_OffsetX, k_PlayerHover,
                            p_Station->position.y + f_OffsetZ));
                    mat4_Model = glm::scale(mat4_Model, glm::vec3(k_PlayerScale, k_PlayerScale * k_PlayerHeightScale, k_PlayerScale));

                    glm::vec3 vec3_DisplayColor = token.b_IsMrX ? glm::vec3(0.0f, 0.0f, 0.0f) : token.vec3_Color;
                    glUniform3fv(colorLoc, 1, glm::value_ptr(vec3_DisplayColor));

                    glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
                    glBindVertexArray(m_VAO_PlayerCylinder);
                    glDrawArrays(GL_TRIANGLES, 0, m_i_PlayerCylinderVertexCount);

                    glm::mat4 mat4_HemiModel = glm::translate(mat4_Model, glm::vec3(0.0f, k_PlayerHeadOffset, 0.0f));
                    glm::mat4 mat4_HemiMVP = mat4_Projection * mat4_View * mat4_HemiModel;
                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_HemiMVP));
                    glBindVertexArray(m_VAO_PlayerHemisphere);
                    glDrawArrays(GL_TRIANGLES, 0, m_i_PlayerHemisphereVertexCount);
                }
            }

            glBindVertexArray(0);
            glUseProgram(0);
        }

        glm::vec3 EmptyEnvironmentState::GetTokenWorldPosition(size_t i_TokenIndex) const {
            if (i_TokenIndex >= m_vec_PlayerTokens.size()) {
                return glm::vec3(0.0f);
            }

            const auto& token = m_vec_PlayerTokens[i_TokenIndex];
            const StationCircle* p_Station = FindStationCircle(token.i_StationID);
            if (!p_Station) {
                return glm::vec3(0.0f);
            }

            std::vector<size_t> vec_TokenIndices;
            vec_TokenIndices.reserve(m_vec_PlayerTokens.size());
            for (size_t i = 0; i < m_vec_PlayerTokens.size(); ++i) {
                if (m_vec_PlayerTokens[i].i_StationID == token.i_StationID) {
                    vec_TokenIndices.push_back(i);
                }
            }

            size_t i_Order = 0;
            auto it = std::find(vec_TokenIndices.begin(), vec_TokenIndices.end(), i_TokenIndex);
            if (it != vec_TokenIndices.end()) {
                i_Order = static_cast<size_t>(std::distance(vec_TokenIndices.begin(), it));
            }

            float f_OffsetX = 0.0f;
            float f_OffsetZ = 0.0f;
            if (vec_TokenIndices.size() > 1) {
                float f_Angle = (2.0f * glm::pi<float>() * static_cast<float>(i_Order)) / static_cast<float>(vec_TokenIndices.size());
                f_OffsetX = k_PlayerMultiRadius * cos(f_Angle);
                f_OffsetZ = k_PlayerMultiRadius * sin(f_Angle);
            }

            return glm::vec3(p_Station->position.x + f_OffsetX, k_PlayerHover,
                p_Station->position.y + f_OffsetZ);
        }

        bool EmptyEnvironmentState::ProjectToScreen(const glm::vec3& vec3_WorldPos,
            const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection,
            int i_WindowW,
            int i_WindowH,
            glm::vec2& out_ScreenPos) const {

            glm::vec4 vec4_Clip = mat4_Projection * mat4_View * glm::vec4(vec3_WorldPos, 1.0f);
            if (std::abs(vec4_Clip.w) < 1e-5f) {
                return false;
            }

            glm::vec3 vec3_NDC = glm::vec3(vec4_Clip) / vec4_Clip.w;
            if (vec3_NDC.x < -1.0f || vec3_NDC.x > 1.0f ||
                vec3_NDC.y < -1.0f || vec3_NDC.y > 1.0f ||
                vec3_NDC.z < -1.0f || vec3_NDC.z > 1.0f) {
                return false;
            }

            float f_ScreenXGL = (vec3_NDC.x + 1.0f) * 0.5f * static_cast<float>(i_WindowW);
            float f_ScreenYGL = (vec3_NDC.y + 1.0f) * 0.5f * static_cast<float>(i_WindowH);
            out_ScreenPos.x = f_ScreenXGL;
            out_ScreenPos.y = static_cast<float>(i_WindowH) - f_ScreenYGL;
            return true;
        }

        int EmptyEnvironmentState::FindPlayerTokenAtScreenPos(int i_ScreenX, int i_ScreenY,
            const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection,
            int i_WindowW,
            int i_WindowH) const {

            float f_ClosestDist = std::numeric_limits<float>::max();
            int i_Selected = -1;

            for (size_t i = 0; i < m_vec_PlayerTokens.size(); ++i) {
                glm::vec3 vec3_World = GetTokenWorldPosition(i);
                glm::vec2 vec2_Screen;
                if (!ProjectToScreen(vec3_World, mat4_View, mat4_Projection, i_WindowW, i_WindowH, vec2_Screen)) {
                    continue;
                }

                float f_Dist = glm::distance(vec2_Screen, glm::vec2(static_cast<float>(i_ScreenX), static_cast<float>(i_ScreenY)));
                if (f_Dist < k_TokenClickRadiusPx && f_Dist < f_ClosestDist) {
                    f_ClosestDist = f_Dist;
                    i_Selected = static_cast<int>(i);
                }
            }

            return i_Selected;
        }

        int EmptyEnvironmentState::FindDestinationAtScreenPos(int i_ScreenX, int i_ScreenY,
            const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection,
            int i_WindowW,
            int i_WindowH) const {

            float f_ClosestDist = std::numeric_limits<float>::max();
            int i_Selected = -1;

            for (const auto& destination : m_vec_DestinationOptions) {
                glm::vec3 vec3_World(destination.vec2_Position.x, 0.03f, destination.vec2_Position.y);
                glm::vec2 vec2_Screen;
                if (!ProjectToScreen(vec3_World, mat4_View, mat4_Projection, i_WindowW, i_WindowH, vec2_Screen)) {
                    continue;
                }

                float f_Dist = glm::distance(vec2_Screen, glm::vec2(static_cast<float>(i_ScreenX), static_cast<float>(i_ScreenY)));
                if (f_Dist < k_DestinationClickRadiusPx && f_Dist < f_ClosestDist) {
                    f_ClosestDist = f_Dist;
                    i_Selected = destination.i_NodeID;
                }
            }

            return i_Selected;
        }

        int EmptyEnvironmentState::FindTransportButtonAtScreenPos(int i_ScreenX, int i_ScreenY,
            const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection,
            int i_WindowW,
            int i_WindowH) const {

            float f_ClosestDist = std::numeric_limits<float>::max();
            int i_Selected = -1;

            for (size_t i = 0; i < m_vec_TransportButtons.size(); ++i) {
                const auto& button = m_vec_TransportButtons[i];
                glm::vec3 vec3_World(button.vec2_Position.x, 0.12f, button.vec2_Position.y);
                glm::vec2 vec2_Screen;
                if (!ProjectToScreen(vec3_World, mat4_View, mat4_Projection, i_WindowW, i_WindowH, vec2_Screen)) {
                    continue;
                }

                float f_Dist = glm::distance(vec2_Screen, glm::vec2(static_cast<float>(i_ScreenX), static_cast<float>(i_ScreenY)));
                if (f_Dist < k_TransportButtonClickRadiusPx && f_Dist < f_ClosestDist) {
                    f_ClosestDist = f_Dist;
                    i_Selected = static_cast<int>(i);
                }
            }

            return i_Selected;
        }

        void EmptyEnvironmentState::RenderStations(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
            if (m_vec_CircleStations.empty() || !m_VAO_Circle || !m_ShaderCircle) {
                static bool s_b_Warned = false;
                if (!s_b_Warned) {
                    std::cout << "[RenderStations] Skipping: empty=" << m_vec_CircleStations.empty()
                        << " VAO=" << m_VAO_Circle << " Shader=" << m_ShaderCircle << std::endl;
                    s_b_Warned = true;
                }
                return;
            }

            static bool s_b_DebugOnce = false;
            if (!s_b_DebugOnce) {
                std::cout << "[RenderStations] Rendering " << m_vec_CircleStations.size() << " stations" << std::endl;
                if (!m_vec_CircleStations.empty()) {
                    std::cout << "[RenderStations] First station pos: ("
                        << m_vec_CircleStations[0].position.x << ", "
                        << m_vec_CircleStations[0].position.y << ")" << std::endl;
                }
                s_b_DebugOnce = true;
            }

            glUseProgram(m_ShaderCircle);
            glBindVertexArray(m_VAO_Circle);

            GLint mvpLoc = glGetUniformLocation(m_ShaderCircle, "MVP");
            GLint colorLoc = glGetUniformLocation(m_ShaderCircle, "circleColor");

            for (const auto& station : m_vec_CircleStations) {
                // Render each transport type as a separate circle with different size
                float f_BaseY = 0.02f;  // Higher than roads to be visible
                float f_YOffset = 0.001f;  // Small vertical offset between overlapping circles
                int i_RingIdx = 0;

                for (const auto& transportType : station.transportTypes) {
                    glm::vec3 vec3_Color(1.0f, 1.0f, 1.0f); // Default white
                    float f_TransportScale = 5.0f; // Default scale

                    // Different sizes for different transport types: taxi < bus < metro < water
                    if (transportType == "taxi") {
                        vec3_Color = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow
                        f_TransportScale = 3.0f; // Smallest
                    }
                    else if (transportType == "bus") {
                        vec3_Color = glm::vec3(0.0f, 1.0f, 0.0f); // Green
                        f_TransportScale = 4.0f; // Medium-small
                    }
                    else if (transportType == "metro") {
                        vec3_Color = glm::vec3(1.0f, 0.0f, 0.0f); // Red
                        f_TransportScale = 5.0f; // Medium-large
                    }
                    else if (transportType == "water") {
                        vec3_Color = glm::vec3(0.0f, 0.4f, 1.0f); // Light blue
                        f_TransportScale = 6.0f; // Largest
                    }

                    // Enlarge selected station by 1.5x
                    bool b_IsSelected = (station.stationID == m_i_SelectedStationID);
                    if (b_IsSelected) {
                        f_TransportScale *= 1.5f;
                    }

                    float f_Y = f_BaseY + (i_RingIdx)*f_YOffset;

                    glm::mat4 mat4_Model = glm::translate(glm::mat4(1.0f),
                        glm::vec3(station.position.x, f_Y, station.position.y));
                    mat4_Model = glm::scale(mat4_Model, glm::vec3(f_TransportScale, 1.0f, f_TransportScale));
                    glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;

                    glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
                    glUniform3f(colorLoc, vec3_Color.r, vec3_Color.g, vec3_Color.b);
                    glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);

                    i_RingIdx++;
                }

                // No top white circle - we show different sizes per transport type
                // (removed to keep visual clarity of different transport sizes)
            }

            glBindVertexArray(0);
            glUseProgram(0);
        }

        int EmptyEnvironmentState::FindStationAtScreenPos(int i_ScreenX, int i_ScreenY,
            const glm::mat4& mat4_View,
            const glm::mat4& mat4_Projection,
            int i_WindowW,
            int i_WindowH) {

            float f_ClosestDist = std::numeric_limits<float>::max();
            int i_ClosestStation = -1;

            for (const auto& station : m_vec_CircleStations) {
                glm::vec3 vec3_World(station.position.x, 0.02f, station.position.y);
                glm::vec2 vec2_Screen;
                if (!ProjectToScreen(vec3_World, mat4_View, mat4_Projection, i_WindowW, i_WindowH, vec2_Screen)) {
                    continue;
                }

                float f_Dist = glm::distance(vec2_Screen, glm::vec2(static_cast<float>(i_ScreenX), static_cast<float>(i_ScreenY)));
                const float k_ClickRadius = 30.0f;
                if (f_Dist < k_ClickRadius && f_Dist < f_ClosestDist) {
                    f_ClosestDist = f_Dist;
                    i_ClosestStation = station.stationID;
                }
            }

            return i_ClosestStation;
        }

        void EmptyEnvironmentState::ClearMovementSelection() {
            m_i_SelectedTokenIndex = -1;
            m_i_SelectedDestinationNode = -1;
            m_vec_DestinationOptions.clear();
            m_vec_TransportButtons.clear();
            m_vec_HighlightedStations.clear();
        }

        bool EmptyEnvironmentState::IsTokenSelectable(size_t i_TokenIndex) const {
            if (i_TokenIndex >= m_vec_PlayerTokens.size()) {
                return false;
            }

            const auto& token = m_vec_PlayerTokens[i_TokenIndex];
            if (token.i_StationID < 0) {
                return false;
            }

            bool b_HasMoved = (i_TokenIndex < m_vec_TokenMovedThisRound.size()) ?
                m_vec_TokenMovedThisRound[i_TokenIndex] : false;

            if (token.b_IsMrX) {
                if (!m_b_IsMrXTurn) {
                    return false;
                }
                if (b_HasMoved && !m_b_MrXSecondMovePending) {
                    return false;
                }
                return true;
            }

            if (m_i_CurrentRound < 2) {
                return false;
            }

            if (m_b_IsMrXTurn) {
                return false;
            }

            return !b_HasMoved;
        }

        void EmptyEnvironmentState::AdvanceRoundIfNeeded() {
            if (m_vec_PlayerTokens.empty()) {
                return;
            }

            if (m_vec_TokenMovedThisRound.size() != m_vec_PlayerTokens.size()) {
                m_vec_TokenMovedThisRound.resize(m_vec_PlayerTokens.size(), false);
            }

            bool b_AllMoved = true;
            for (size_t i = 0; i < m_vec_PlayerTokens.size(); ++i) {
                const auto& token = m_vec_PlayerTokens[i];
                bool b_ShouldMove = token.b_IsMrX || m_i_CurrentRound >= 2;
                if (!b_ShouldMove) {
                    continue;
                }

                if (!m_vec_TokenMovedThisRound[i]) {
                    b_AllMoved = false;
                    break;
                }
            }

            if (!b_AllMoved) {
                return;
            }

            ++m_i_CurrentRound;
            m_b_IsMrXTurn = true;
            m_b_MrXSecondMovePending = false;
            std::fill(m_vec_TokenMovedThisRound.begin(), m_vec_TokenMovedThisRound.end(), false);
            ClearMovementSelection();
            m_i_SelectedStationID = -1;
            UI::ClearMrXSelections();
            UI::SetRound(m_i_CurrentRound);
            UpdateMrXButtonStates();

            std::cout << "[EmptyEnvironmentState] Starting round " << m_i_CurrentRound << std::endl;
            if (m_i_CurrentRound == 2) {
                std::cout << "[EmptyEnvironmentState] Detectives are now allowed to move." << std::endl;
            }
        }

        void EmptyEnvironmentState::UpdateMrXButtonStates() {
            if (m_i_MrXTokenIndex < 0 ||
                m_i_MrXTokenIndex >= static_cast<int>(m_vec_PlayerTokens.size())) {
                UI::SetMrXButtonsVisible(false);
                UI::SetMrXButtonsEnabled(false, false);
                return;
            }

            const auto& mrX = m_vec_PlayerTokens[static_cast<size_t>(m_i_MrXTokenIndex)];
            bool b_ShowButtons = m_b_IsMrXTurn;
            UI::SetMrXButtonsVisible(b_ShowButtons);

            if (!b_ShowButtons) {
                UI::SetMrXButtonsEnabled(false, false);
                return;
            }

            bool b_BlackEnabled = mrX.blackTickets > 0;
            bool b_DoubleEnabled = mrX.doubleTickets > 0 && !m_b_MrXSecondMovePending;
            UI::SetMrXButtonsEnabled(b_BlackEnabled, b_DoubleEnabled);
        }

        void EmptyEnvironmentState::SelectPlayerToken(int i_TokenIndex) {
            if (i_TokenIndex < 0 || i_TokenIndex >= static_cast<int>(m_vec_PlayerTokens.size())) {
                return;
            }

            if (!IsTokenSelectable(static_cast<size_t>(i_TokenIndex))) {
                const auto& token = m_vec_PlayerTokens[static_cast<size_t>(i_TokenIndex)];
                if (token.b_IsMrX && !m_b_IsMrXTurn) {
                    std::cout << "[EmptyEnvironmentState] Mr X already moved this phase." << std::endl;
                }
                else if (!token.b_IsMrX && m_i_CurrentRound < 2) {
                    std::cout << "[EmptyEnvironmentState] Detectives become active starting from round 2." << std::endl;
                }
                else {
                    std::cout << "[EmptyEnvironmentState] This token cannot move right now." << std::endl;
                }
                return;
            }

            m_i_SelectedTokenIndex = i_TokenIndex;
            m_i_SelectedDestinationNode = -1;
            m_vec_TransportButtons.clear();

            const auto& token = m_vec_PlayerTokens[i_TokenIndex];
            m_i_SelectedStationID = token.i_StationID;
            std::cout << "[EmptyEnvironmentState] Selected token at station " << token.i_StationID << std::endl;

            UpdateDestinationsForSelectedToken();
        }

        void EmptyEnvironmentState::UpdateDestinationsForSelectedToken() {
            m_vec_DestinationOptions.clear();
            m_vec_HighlightedStations.clear();

            if (!m_b_GraphLoaded ||
                m_i_SelectedTokenIndex < 0 ||
                m_i_SelectedTokenIndex >= static_cast<int>(m_vec_PlayerTokens.size())) {
                return;
            }

            if (!IsTokenSelectable(static_cast<size_t>(m_i_SelectedTokenIndex))) {
                ClearMovementSelection();
                return;
            }

            const auto& token = m_vec_PlayerTokens[static_cast<size_t>(m_i_SelectedTokenIndex)];
            if (token.i_StationID < 0) {
                return;
            }

            const auto vec_Connections = m_graph.GetConnections(token.i_StationID);
            std::map<int, std::vector<int>> map_NodeToTransports;
            for (const auto& conn : vec_Connections) {
                if (!TokenHasTicket(token, conn.i_TransportType)) {
                    continue;
                }
                map_NodeToTransports[conn.i_NodeId].push_back(conn.i_TransportType);
            }

            for (const auto& entry : map_NodeToTransports) {
                const StationCircle* p_Destination = FindStationCircle(entry.first);
                if (!p_Destination) {
                    continue;
                }

                DestinationOption option;
                option.i_NodeID = entry.first;
                option.vec2_Position = p_Destination->position;
                option.vec_AvailableTransports = entry.second;
                m_vec_DestinationOptions.push_back(option);
                m_vec_HighlightedStations.push_back(entry.first);
            }

            std::cout << "[EmptyEnvironmentState] " << m_vec_DestinationOptions.size()
                << " destinations available." << std::endl;
        }

        void EmptyEnvironmentState::UpdateTransportButtons(int i_DestinationNode) {
            m_vec_TransportButtons.clear();

            if (m_i_SelectedTokenIndex < 0 ||
                m_i_SelectedTokenIndex >= static_cast<int>(m_vec_PlayerTokens.size())) {
                return;
            }

            auto it = std::find_if(m_vec_DestinationOptions.begin(), m_vec_DestinationOptions.end(),
                [i_DestinationNode](const DestinationOption& option) {
                    return option.i_NodeID == i_DestinationNode;
                });
            if (it == m_vec_DestinationOptions.end()) {
                return;
            }

            int i_ButtonCount = static_cast<int>(it->vec_AvailableTransports.size());
            if (i_ButtonCount <= 0) {
                return;
            }

            float f_OrbitRadius = k_TransportButtonOrbitScale * m_f_GlobalScale;
            const auto& token = m_vec_PlayerTokens[static_cast<size_t>(m_i_SelectedTokenIndex)];

            for (int i = 0; i < i_ButtonCount; ++i) {
                float f_Angle = (2.0f * glm::pi<float>() * i) / std::max(1, i_ButtonCount) - glm::half_pi<float>();
                glm::vec2 vec2_Offset(cosf(f_Angle) * f_OrbitRadius, sinf(f_Angle) * f_OrbitRadius);

                TransportButton button;
                button.vec2_Position = it->vec2_Position + vec2_Offset;
                button.f_Radius = k_TransportButtonRadius;
                button.i_TransportType = it->vec_AvailableTransports[static_cast<size_t>(i)];
                button.b_Available = TokenHasTicket(token, button.i_TransportType);
                m_vec_TransportButtons.push_back(button);
            }
        }

        void EmptyEnvironmentState::HandleDestinationSelection(int i_DestinationNode) {
            if (m_i_SelectedTokenIndex < 0) {
                return;
            }

            auto it = std::find_if(m_vec_DestinationOptions.begin(), m_vec_DestinationOptions.end(),
                [i_DestinationNode](const DestinationOption& option) {
                    return option.i_NodeID == i_DestinationNode;
                });
            if (it == m_vec_DestinationOptions.end()) {
                return;
            }

            m_i_SelectedDestinationNode = i_DestinationNode;

            if (it->vec_AvailableTransports.size() == 1) {
                int i_Transport = it->vec_AvailableTransports.front();
                if (TokenHasTicket(m_vec_PlayerTokens[static_cast<size_t>(m_i_SelectedTokenIndex)], i_Transport)) {
                    HandleTransportButtonClick(i_Transport);
                    return;
                }
            }

            UpdateTransportButtons(i_DestinationNode);
        }

        bool EmptyEnvironmentState::TokenHasTicket(const PlayerToken& token, int i_TransportType) const {
            switch (i_TransportType) {
            case Core::k_TransportTypeTaxi:
                return token.taxiTickets > 0;
            case Core::k_TransportTypeBus:
                return token.busTickets > 0;
            case Core::k_TransportTypeMetro:
                return token.metroTickets > 0;
            case Core::k_TransportTypeWater:
                return token.b_IsMrX && token.blackTickets > 0;
            default:
                return false;
            }
        }

        bool EmptyEnvironmentState::SpendTicket(PlayerToken& token, int i_TransportType) {
            if (!TokenHasTicket(token, i_TransportType)) {
                return false;
            }

            switch (i_TransportType) {
            case Core::k_TransportTypeTaxi:
                --token.taxiTickets;
                return true;
            case Core::k_TransportTypeBus:
                --token.busTickets;
                return true;
            case Core::k_TransportTypeMetro:
                --token.metroTickets;
                return true;
            case Core::k_TransportTypeWater:
                --token.blackTickets;
                return true;
            default:
                return false;
            }
        }

        void EmptyEnvironmentState::HandleTransportButtonClick(int i_TransportType) {
            if (m_i_SelectedTokenIndex < 0 || m_i_SelectedDestinationNode < 0) {
                return;
            }

            ExecuteTokenMove(i_TransportType);
        }

        void EmptyEnvironmentState::ExecuteTokenMove(int i_TransportType) {
            if (m_i_SelectedTokenIndex < 0 ||
                m_i_SelectedDestinationNode < 0 ||
                m_i_SelectedTokenIndex >= static_cast<int>(m_vec_PlayerTokens.size())) {
                return;
            }

            PlayerToken& token = m_vec_PlayerTokens[static_cast<size_t>(m_i_SelectedTokenIndex)];
            const bool b_IsMrX = token.b_IsMrX;
            const bool b_ForceBlack = (i_TransportType == Core::k_TransportTypeWater);

            if (!b_IsMrX && b_ForceBlack) {
                std::cout << "[EmptyEnvironmentState] Detectives cannot use water transport." << std::endl;
                return;
            }

            if (m_vec_TokenMovedThisRound.size() != m_vec_PlayerTokens.size()) {
                m_vec_TokenMovedThisRound.resize(m_vec_PlayerTokens.size(), false);
            }

            bool b_SpentTicket = false;
            bool b_MrXSecondMoveWasPending = m_b_MrXSecondMovePending;

            if (b_IsMrX) {
                bool b_UIBlack = UI::IsMrXBlackSelected();
                if ((b_UIBlack || b_ForceBlack)) {
                    if (token.blackTickets <= 0) {
                        std::cout << "[EmptyEnvironmentState] Mr X does not have a black ticket for this move." << std::endl;
                        return;
                    }
                    --token.blackTickets;
                    b_SpentTicket = true;
                    std::cout << "[EmptyEnvironmentState] Mr X spent a black ticket." << std::endl;
                }
            }

            if (!b_SpentTicket) {
                if (!SpendTicket(token, i_TransportType)) {
                    std::cout << "[EmptyEnvironmentState] No tickets for transport type " << i_TransportType << std::endl;
                    return;
                }
            }

            token.i_StationID = m_i_SelectedDestinationNode;
            m_i_SelectedStationID = token.i_StationID;
            std::cout << "[EmptyEnvironmentState] Token moved to node " << token.i_StationID << std::endl;

            const size_t tokenIndex = static_cast<size_t>(m_i_SelectedTokenIndex);

            if (b_IsMrX) {
                if (!b_MrXSecondMoveWasPending) {
                    bool b_UIDouble = UI::IsMrXDoubleSelected();
                    if (b_UIDouble && token.doubleTickets > 0) {
                        --token.doubleTickets;
                        m_b_MrXSecondMovePending = true;
                        std::cout << "[EmptyEnvironmentState] Mr X activated a double move." << std::endl;
                    }
                }
                else {
                    m_b_MrXSecondMovePending = false;
                }

                if (m_b_MrXSecondMovePending) {
                    m_vec_TokenMovedThisRound[tokenIndex] = false;
                    m_b_IsMrXTurn = true;
                }
                else {
                    m_vec_TokenMovedThisRound[tokenIndex] = true;
                    m_b_IsMrXTurn = false;
                }
            }
            else {
                m_vec_TokenMovedThisRound[tokenIndex] = true;
            }

            m_i_SelectedDestinationNode = -1;
            m_vec_TransportButtons.clear();

            if (b_IsMrX && m_b_MrXSecondMovePending) {
                UpdateDestinationsForSelectedToken();
            }
            else {
                ClearMovementSelection();
            }

            AdvanceRoundIfNeeded();
            UpdateMrXButtonStates();
        }

        void EmptyEnvironmentState::RenderTransportButtons(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
            if (m_vec_TransportButtons.empty() || !m_VAO_Circle || !m_ShaderCircle) {
                return;
            }

            glUseProgram(m_ShaderCircle);
            glBindVertexArray(m_VAO_Circle);

            GLuint mvpLoc = glGetUniformLocation(m_ShaderCircle, "MVP");
            GLuint colorLoc = glGetUniformLocation(m_ShaderCircle, "circleColor");

            for (const auto& button : m_vec_TransportButtons) {
                glm::vec3 vec3_Color(0.5f, 0.5f, 0.5f);
                switch (button.i_TransportType) {
                case Core::k_TransportTypeTaxi:
                    vec3_Color = glm::vec3(1.0f, 1.0f, 0.0f);
                    break;
                case Core::k_TransportTypeBus:
                    vec3_Color = glm::vec3(0.0f, 1.0f, 0.0f);
                    break;
                case Core::k_TransportTypeMetro:
                    vec3_Color = glm::vec3(1.0f, 0.0f, 0.0f);
                    break;
                case Core::k_TransportTypeWater:
                    vec3_Color = glm::vec3(0.0f, 0.4f, 1.0f);
                    break;
                default:
                    break;
                }

                if (!button.b_Available) {
                    vec3_Color *= 0.3f;
                }

                glm::mat4 mat4_ModelOutline = glm::translate(glm::mat4(1.0f),
                    glm::vec3(button.vec2_Position.x, 0.13f, button.vec2_Position.y));
                mat4_ModelOutline = glm::scale(mat4_ModelOutline, glm::vec3(button.f_Radius * 1.3f, 0.2f, button.f_Radius * 1.3f));
                mat4_ModelOutline = mat4_ModelOutline * m_mat4_GlobalScaleMatrix;

                glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_ModelOutline;
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
                glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
                glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);

                glm::mat4 mat4_Model = glm::translate(glm::mat4(1.0f),
                    glm::vec3(button.vec2_Position.x, 0.14f, button.vec2_Position.y));
                mat4_Model = glm::scale(mat4_Model, glm::vec3(button.f_Radius, 0.2f, button.f_Radius));
                mat4_Model = mat4_Model * m_mat4_GlobalScaleMatrix;

                mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
                glUniform3fv(colorLoc, 1, glm::value_ptr(vec3_Color));
                glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
            }

            glBindVertexArray(0);
            glUseProgram(0);
        }

        void EmptyEnvironmentState::RenderStationInfo(Core::Application* p_App) {
            if (m_i_SelectedStationID < 0 || !p_App) return;

            // Find the selected station
            auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                [this](const StationCircle& sc) { return sc.stationID == m_i_SelectedStationID; });

            if (it == m_vec_CircleStations.end()) return;

            const auto& station = *it;

            // Build info string
            std::string s_Info = "Station " + std::to_string(station.stationID) + ": ";
            for (size_t i = 0; i < station.transportTypes.size(); ++i) {
                s_Info += station.transportTypes[i];
                if (i < station.transportTypes.size() - 1) s_Info += ", ";
            }

            std::cout << "[Selected] " << s_Info << std::endl;

            // TODO: Render actual UI panel with text
            // For now, selection is just logged to console
        }

        void EmptyEnvironmentState::LoadGraphDataFromGeneratedMap() {
            try {
                std::cout << "[EmptyEnvironmentState] Loading graph from generated map data..." << std::endl;

                if (m_MapData.vec_GraphNodes.empty()) {
                    std::cerr << "[EmptyEnvironmentState] No graph nodes in generated map data!" << std::endl;
                    m_b_GraphLoaded = false;
                    return;
                }

                // Initialize nodes in GraphManager
                // GraphManager nodes are already initialized in constructor, we just need to set their data
                for (const auto& graphNode : m_MapData.vec_GraphNodes) {
                    Node* pNode = m_graph.GetNode(graphNode.i_ID);
                    if (pNode) {
                        pNode->i_Id = graphNode.i_ID;
                        // Store map coordinates (0-1200) as grid coordinates (divide by ~50 to get 0-24 range)
                        pNode->i_X = static_cast<int>(graphNode.position.x / 50.0f);
                        pNode->i_Y = static_cast<int>(graphNode.position.y / 50.0f);
                    }
                }

                // Create edges from connections
                int connectionCount = 0;
                for (const auto& graphNode : m_MapData.vec_GraphNodes) {
                    int srcId = graphNode.i_ID;
                    Node* pSrcNode = m_graph.GetNode(srcId);
                    if (!pSrcNode) continue;

                    // Add taxi connections
                    for (int dstId : graphNode.vec_TaxiConnections) {
                        if (srcId < dstId) { // Avoid duplicates
                            Node* pDstNode = m_graph.GetNode(dstId);
                            if (pDstNode) {
                                pSrcNode->ConnectTo(pDstNode, Core::k_TransportTypeTaxi);
                                connectionCount++;
                            }
                        }
                    }

                    // Add bus connections
                    for (int dstId : graphNode.vec_BusConnections) {
                        if (srcId < dstId) {
                            Node* pDstNode = m_graph.GetNode(dstId);
                            if (pDstNode) {
                                pSrcNode->ConnectTo(pDstNode, Core::k_TransportTypeBus);
                                connectionCount++;
                            }
                        }
                    }

                    // Add metro connections
                    for (int dstId : graphNode.vec_MetroConnections) {
                        if (srcId < dstId) {
                            Node* pDstNode = m_graph.GetNode(dstId);
                            if (pDstNode) {
                                pSrcNode->ConnectTo(pDstNode, Core::k_TransportTypeMetro);
                                connectionCount++;
                            }
                        }
                    }

                    // Add water connections
                    for (int dstId : graphNode.vec_FerryConnections) {
                        if (srcId < dstId) {
                            Node* pDstNode = m_graph.GetNode(dstId);
                            if (pDstNode) {
                                pSrcNode->ConnectTo(pDstNode, Core::k_TransportTypeWater);
                                connectionCount++;
                            }
                        }
                    }
                }

                m_b_GraphLoaded = true;

                std::cout << "[EmptyEnvironmentState] Graph loaded from generated map: "
                    << m_MapData.vec_GraphNodes.size() << " nodes, "
                    << connectionCount << " connections" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[EmptyEnvironmentState] Failed to load graph from generated map: " << e.what() << std::endl;
                m_b_GraphLoaded = false;
            }
        }

        void EmptyEnvironmentState::LoadGraphData() {
            try {
                std::string s_NodeFile = Core::GetMapPath(Core::k_NodeDataRelativePath);
                std::string s_ConnFile = Core::GetMapPath(Core::k_ConnectionsRelativePath);

                std::cout << "[EmptyEnvironmentState] Loading graph from:" << std::endl;
                std::cout << "  Nodes: " << s_NodeFile << std::endl;
                std::cout << "  Connections: " << s_ConnFile << std::endl;

                m_graph.LoadData(s_NodeFile, s_ConnFile, true);
                m_b_GraphLoaded = true;

                std::cout << "[EmptyEnvironmentState] Graph loaded successfully!" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "[EmptyEnvironmentState] Failed to load graph: " << e.what() << std::endl;
                m_b_GraphLoaded = false;
            }
        }

        void EmptyEnvironmentState::RenderConnectionLines(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
            if (m_i_SelectedStationID < 0 || !m_b_GraphLoaded) return;

            // Find selected station position
            auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                [this](const StationCircle& sc) { return sc.stationID == m_i_SelectedStationID; });

            if (it == m_vec_CircleStations.end()) return;

            glm::vec2 vec2_StartPos = it->position;

            // Get connections for selected station
            auto connections = m_graph.GetConnections(m_i_SelectedStationID);

            const float k_LineHeight = 0.05f;
            const float k_ArrowSize = 0.15f; // Size of arrow head
            const float k_ArrowAngle = 0.4f; // Angle of arrow wings in radians (~23 degrees)

            // Setup OpenGL state for rendering lines and arrows
            glDisable(GL_TEXTURE_2D);
            glUseProgram(0);
            glMatrixMode(GL_PROJECTION);
            glLoadMatrixf(glm::value_ptr(mat4_Projection));
            glMatrixMode(GL_MODELVIEW);
            glLoadMatrixf(glm::value_ptr(mat4_View));
            glLineWidth(2.5f);

            // Render lines and arrows to each connected station
            for (const auto& conn : connections) {
                // Find connected station
                auto connIt = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                    [&conn](const StationCircle& sc) { return sc.stationID == conn.i_NodeId; });

                if (connIt == m_vec_CircleStations.end()) continue;

                glm::vec2 vec2_EndPos = connIt->position;

                // Color based on transport type
                glm::vec3 vec3_Color;
                switch (conn.i_TransportType) {
                    case 0: // Taxi
                        vec3_Color = glm::vec3(1.0f, 1.0f, 0.0f); // Yellow
                        break;
                    case 1: // Bus
                        vec3_Color = glm::vec3(0.0f, 1.0f, 0.0f); // Green
                        break;
                    case 2: // Metro
                        vec3_Color = glm::vec3(1.0f, 0.0f, 0.0f); // Red
                        break;
                    case 3: // Water
                        vec3_Color = glm::vec3(0.0f, 0.4f, 1.0f); // Blue
                        break;
                    default:
                        vec3_Color = glm::vec3(1.0f, 1.0f, 1.0f); // White
                        break;
                }

                glColor3f(vec3_Color.r, vec3_Color.g, vec3_Color.b);

                // Draw line slightly above ground to avoid z-fighting
                glBegin(GL_LINES);
                glVertex3f(vec2_StartPos.x, k_LineHeight, vec2_StartPos.y);
                glVertex3f(vec2_EndPos.x, k_LineHeight, vec2_EndPos.y);
                glEnd();

                // Draw arrow at the end pointing to destination
                glm::vec2 vec2_Direction = glm::normalize(vec2_EndPos - vec2_StartPos);
                glm::vec2 vec2_ArrowTip = vec2_EndPos - vec2_Direction * 0.3f; // Arrow tip slightly before station

                // Calculate perpendicular vector for arrow wings
                glm::vec2 vec2_Perp(-vec2_Direction.y, vec2_Direction.x);

                // Arrow wing points
                glm::vec2 vec2_Wing1 = vec2_ArrowTip - vec2_Direction * k_ArrowSize + vec2_Perp * k_ArrowSize * 0.5f;
                glm::vec2 vec2_Wing2 = vec2_ArrowTip - vec2_Direction * k_ArrowSize - vec2_Perp * k_ArrowSize * 0.5f;

                // Draw arrow head
                glBegin(GL_TRIANGLES);
                glVertex3f(vec2_ArrowTip.x, k_LineHeight, vec2_ArrowTip.y);
                glVertex3f(vec2_Wing1.x, k_LineHeight, vec2_Wing1.y);
                glVertex3f(vec2_Wing2.x, k_LineHeight, vec2_Wing2.y);
                glEnd();
            }

            // Restore OpenGL state
            glLineWidth(1.0f);
        }

        void EmptyEnvironmentState::RenderHighlightedStations(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
            if (m_vec_HighlightedStations.empty()) return;

            float f_Pulse = 0.5f + 0.5f * sinf(m_f_Time * k_HighlightPulseSpeed);
            float f_OuterScale = m_f_GlobalScale * (k_HighlightBaseScale + f_Pulse * k_HighlightPulseScale);
            float f_InnerScale = m_f_GlobalScale * (k_HighlightInnerScale + f_Pulse * (k_HighlightPulseScale * 0.5f));

            glUseProgram(m_ShaderCircle);

            GLint i_LocMVP = glGetUniformLocation(m_ShaderCircle, "MVP");
            GLint i_LocColor = glGetUniformLocation(m_ShaderCircle, "circleColor");

            glBindVertexArray(m_VAO_Circle);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            for (int i_StationID : m_vec_HighlightedStations) {
                auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                    [i_StationID](const StationCircle& sc) { return sc.stationID == i_StationID; });
                if (it == m_vec_CircleStations.end()) {
                    continue;
                }

                const auto& station = *it;
                glm::vec3 vec3_Position(station.position.x, 0.03f, station.position.y);

                glm::mat4 mat4_ModelOuter = glm::translate(glm::mat4(1.0f), vec3_Position);
                mat4_ModelOuter = glm::scale(mat4_ModelOuter, glm::vec3(f_OuterScale));
                glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_ModelOuter;
                glUniformMatrix4fv(i_LocMVP, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
                glUniform3f(i_LocColor, 1.0f, 0.95f, 0.2f + 0.4f * f_Pulse);
                glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);

                glm::mat4 mat4_ModelInner = glm::translate(glm::mat4(1.0f), vec3_Position);
                mat4_ModelInner = glm::scale(mat4_ModelInner, glm::vec3(f_InnerScale));
                mat4_MVP = mat4_Projection * mat4_View * mat4_ModelInner;
                glUniformMatrix4fv(i_LocMVP, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
                glUniform3f(i_LocColor, 1.0f, 0.5f + 0.5f * f_Pulse, 0.0f);
                glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
            }

            glDisable(GL_BLEND);
            glBindVertexArray(0);
            glUseProgram(0);
        }

    } // namespace States
} // namespace ScotlandYard