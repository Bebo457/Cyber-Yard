#include "GameState.h"
#include "Application.h"
#include "StateManager.h"
#include "GameConstants.h"
#include "GameSettings.h"
#include <GL/glew.h>

#include <random>
#include <algorithm>
#include "../../Graphs/graph_manage.h"
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include "HUDOverlay.h"
#include <unordered_map>

namespace ScotlandYard {
namespace States {

GameState::GameState()
    : m_b_GameActive(false)
    , m_b_Camera3D(true)
    , m_b_TexturesLoaded(false)
    , m_VAO_Plane(0)
    , m_VBO_Plane(0)
    , m_ShaderProgram_Plane(0)
    , m_ShaderProgram_Circle(0)
    , m_VAO_Circle(0)
    , m_VBO_Circle(0)
    , m_i_CircleVertexCount(0)
    , m_p_Window(nullptr)
    , m_f_Rotation(0.0f)
    , m_i_Width(800)
    , m_i_Height(600)
    , m_TextureID(0)
    , m_vec3_CameraPosition(0.0f, 1.5f, 4.0f)
    , m_vec3_CameraVelocity(0.0f, 0.0f, 0.0f)
    , m_vec3_CameraFront(0.0f, -0.3f, -1.0f)
    , m_vec3_CameraUp(0.0f, 1.0f, 0.0f)
    , m_f_CameraAngle(k_MaxCameraAngle)
    , m_f_CameraAngleVelocity(0.0f)
    , m_vec3_Saved3DCameraPosition(0.0f, 1.5f, 4.0f)
    , m_graph(200)
    , m_FBO_Picking(0)
    , m_TextureID_Picking(0)
    , m_RBO_PickingDepth(0)
    , m_FBO_PickingDilated(0)
    , m_TextureID_PickingDilated(0)
    , m_VAO_Arrow(0)
    , m_VBO_Arrow(0)
    , m_i_ArrowVertexCount(0)
    , m_ShaderProgram_Picking(0)
    , m_ShaderProgram_Dilation(0)
    , m_VAO_FullscreenQuad(0)
    , m_VBO_FullscreenQuad(0)
    , m_i_SelectedPlayerIndex(-1)
    , m_ui_NextPickingID(0)
{
}

GameState::~GameState() {}

void GameState::OnEnter() {
    m_b_GameActive = true;
    m_mat4_GlobalScaleMatrix = glm::scale(glm::mat4(1.0f), glm::vec3(m_f_GlobalScale));

    // Dane wierzchołków planszy (pozycja, kolor, UV)
    float planeVertices[] = {
        // pozycja xyz           // normalne        // uv (odwrócone pionowo)
        -1.0f, 0.0f, 17.0f,      0.0f, 1.0f, 0.0f,   0.0f, 1.0f,   // left_down
        23.0f, 0.0f, 17.0f,      0.0f, 1.0f, 0.0f,   1.0f, 1.0f,   // right_down
        23.0f, 0.0f,  -1.0f,      0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // right_up

        -1.0f, 0.0f, 17.0f,      0.0f, 1.0f, 0.0f,   0.0f, 1.0f,   // left_down
        23.0f, 0.0f,  -1.0f,      0.0f, 1.0f, 0.0f,   1.0f, 0.0f,   // right_up
        -1.0f, 0.0f,  -1.0f,      0.0f, 1.0f, 0.0f,   0.0f, 0.0f    // left_up
    };



    // Pozycje kółek z pliku CSV
    auto vec_StationData = Utils::MapDataLoader::LoadStations(Core::GetMapPath(Core::k_NodeDataRelativePath));
        for (auto& station : vec_StationData) {
        station.vec2_Position.x *= m_f_GlobalScale;
        station.vec2_Position.y *= m_f_GlobalScale;
    }

    if (vec_StationData.empty()) {
        std::cerr << "[GameState] Warning: No positions loaded from CSV, using defaults.\n";

        m_vec_CircleStations = {
            { glm::vec2(-0.9f, -0.9f), {"taxi"}, 1 },
            { glm::vec2(-0.5f, -0.9f), {"bus"}, 2 },
            { glm::vec2( 0.0f, -0.9f), {"metro"}, 3 },
            { glm::vec2( 0.5f, -0.9f), {"bus", "metro"}, 4 },
            { glm::vec2( 0.9f, -0.9f), {"taxi", "bus", "metro"}, 5 }
        };
    } else {
        // Convert StationData to StationCircle format
        for (const auto& station : vec_StationData) {
            StationCircle circle;
            circle.position = station.vec2_Position;
            circle.transportTypes = station.vec_TransportTypes;
            circle.stationID = station.i_StationID;
            m_vec_CircleStations.push_back(circle);
        }
    }

    // VAO/VBO planszy
    glGenVertexArrays(1, &m_VAO_Plane);
    glGenBuffers(1, &m_VBO_Plane);

    glBindVertexArray(m_VAO_Plane);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Plane);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    // VAO/VBO kółek
    float f_Radius = 0.05f;
    int i_Segments = 30;
    std::vector<float> vec_CircleVertices = generateCircleVertices(f_Radius, i_Segments);
    m_i_CircleVertexCount = static_cast<int>(vec_CircleVertices.size() / 3);

    glGenVertexArrays(1, &m_VAO_Circle);
    glGenBuffers(1, &m_VBO_Circle);

    glBindVertexArray(m_VAO_Circle);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Circle);
    glBufferData(GL_ARRAY_BUFFER, vec_CircleVertices.size() * sizeof(float), vec_CircleVertices.data(), GL_STATIC_DRAW);

    // tylko pozycja
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // VAO/VBO cylindra i półkuli pionka
    std::vector<float> cylVerts = generateCylinderVertices(0.05f, 0.1f, 20); // radius, height, segments
    m_i_CylinderVertexCount = static_cast<int>(cylVerts.size() / 3);
    glGenVertexArrays(1, &m_VAO_Cylinder);
    glGenBuffers(1, &m_VBO_Cylinder);
    glBindVertexArray(m_VAO_Cylinder);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Cylinder);
    glBufferData(GL_ARRAY_BUFFER, cylVerts.size() * sizeof(float), cylVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    std::vector<float> hemiVerts = generateHemisphereVertices(0.05f, 30); // radius, segments
    m_i_HemisphereVertexCount = static_cast<int>(hemiVerts.size() / 3);
    glGenVertexArrays(1, &m_VAO_Hemisphere);
    glGenBuffers(1, &m_VBO_Hemisphere);
    glBindVertexArray(m_VAO_Hemisphere);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Hemisphere);
    glBufferData(GL_ARRAY_BUFFER, hemiVerts.size() * sizeof(float), hemiVerts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // Shadery planszy
    const char* vertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        layout(location = 1) in vec3 aColor;
        layout(location = 2) in vec2 aTexCoord;
        uniform mat4 MVP;
        out vec3 vertexColor;
        out vec2 TexCoord;
        void main() {
            vertexColor = aColor;
            TexCoord = aTexCoord;
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* fragmentShaderSrc = R"(
        #version 330 core
        in vec3 vertexColor;
        in vec2 TexCoord;
        uniform sampler2D ourTexture;
        out vec4 FragColor;
        void main() {
            FragColor = texture(ourTexture, TexCoord);
        }
    )";

    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSrc, nullptr);
    glCompileShader(vertexShader);

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentShaderSrc, nullptr);
    glCompileShader(fragmentShader);

    m_ShaderProgram_Plane = glCreateProgram();
    glAttachShader(m_ShaderProgram_Plane, vertexShader);
    glAttachShader(m_ShaderProgram_Plane, fragmentShader);
    glLinkProgram(m_ShaderProgram_Plane);

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Shadery kółek
    const char* circleVertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 MVP;
        void main() {
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* circleFragmentShaderSrc = R"(
        #version 330 core
        uniform vec3 circleColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(circleColor, 1.0);
        }
    )";

    GLuint cVertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(cVertexShader, 1, &circleVertexShaderSrc, nullptr);
    glCompileShader(cVertexShader);

    GLuint cFragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(cFragmentShader, 1, &circleFragmentShaderSrc, nullptr);
    glCompileShader(cFragmentShader);

    m_ShaderProgram_Circle = glCreateProgram();
    glAttachShader(m_ShaderProgram_Circle, cVertexShader);
    glAttachShader(m_ShaderProgram_Circle, cFragmentShader);
    glLinkProgram(m_ShaderProgram_Circle);

    glDeleteShader(cVertexShader);
    glDeleteShader(cFragmentShader);

    // Shader for color picking
    const char* pickingVertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec3 aPos;
        uniform mat4 MVP;
        void main() {
            gl_Position = MVP * vec4(aPos, 1.0);
        }
    )";

    const char* pickingFragmentShaderSrc = R"(
        #version 330 core
        uniform vec3 pickingColor;
        out vec4 FragColor;
        void main() {
            FragColor = vec4(pickingColor, 1.0);
        }
    )";

    GLuint pickingVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(pickingVS, 1, &pickingVertexShaderSrc, nullptr);
    glCompileShader(pickingVS);

    GLuint pickingFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(pickingFS, 1, &pickingFragmentShaderSrc, nullptr);
    glCompileShader(pickingFS);

    m_ShaderProgram_Picking = glCreateProgram();
    glAttachShader(m_ShaderProgram_Picking, pickingVS);
    glAttachShader(m_ShaderProgram_Picking, pickingFS);
    glLinkProgram(m_ShaderProgram_Picking);

    glDeleteShader(pickingVS);
    glDeleteShader(pickingFS);

    // Framebuffer for color picking
    glGenFramebuffers(1, &m_FBO_Picking);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_Picking);

    glGenTextures(1, &m_TextureID_Picking);
    glBindTexture(GL_TEXTURE_2D, m_TextureID_Picking);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_i_Width, m_i_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID_Picking, 0);

    glGenRenderbuffers(1, &m_RBO_PickingDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, m_RBO_PickingDepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_i_Width, m_i_Height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_RBO_PickingDepth);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[GameState] ERROR: Picking framebuffer is not complete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Framebuffer for dilated picking
    glGenFramebuffers(1, &m_FBO_PickingDilated);
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_PickingDilated);

    glGenTextures(1, &m_TextureID_PickingDilated);
    glBindTexture(GL_TEXTURE_2D, m_TextureID_PickingDilated);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_i_Width, m_i_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_TextureID_PickingDilated, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::cerr << "[GameState] ERROR: Dilated picking framebuffer is not complete!\n";
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Shader for dilation
    const char* dilationVertexShaderSrc = R"(
        #version 330 core
        layout(location = 0) in vec2 aPos;
        out vec2 TexCoords;
        void main() {
            TexCoords = aPos * 0.5 + 0.5;
            gl_Position = vec4(aPos, 0.0, 1.0);
        }
    )";

    const char* dilationFragmentShaderSrc = R"(
        #version 330 core
        in vec2 TexCoords;
        uniform sampler2D idBuffer;
        uniform vec2 texelSize;
        out vec4 fragColor;

        void main() {
            ivec2 offsets[9] = ivec2[9](
                ivec2(-1, -1), ivec2(0, -1), ivec2(1, -1),
                ivec2(-1,  0), ivec2(0,  0), ivec2(1,  0),
                ivec2(-1,  1), ivec2(0,  1), ivec2(1,  1)
            );

            int bestID = 0;
            float bestDist = 99999.0;

            for (int i = 0; i < 9; i++) {
                vec2 uv = TexCoords + vec2(offsets[i]) * texelSize;
                vec3 color = texture(idBuffer, uv).rgb;
                int id = int(color.r * 255.0) |
                         (int(color.g * 255.0) << 8) |
                         (int(color.b * 255.0) << 16);

                if (id != 0) {
                    float weight = length(vec2(offsets[i]));
                    if (weight < bestDist) {
                        bestDist = weight;
                        bestID = id;
                    }
                }
            }

            fragColor = vec4(float(bestID & 255) / 255.0,
                             float((bestID >> 8) & 255) / 255.0,
                             float((bestID >> 16) & 255) / 255.0,
                             1.0);
        }
    )";

    GLuint dilationVS = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(dilationVS, 1, &dilationVertexShaderSrc, nullptr);
    glCompileShader(dilationVS);

    GLuint dilationFS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(dilationFS, 1, &dilationFragmentShaderSrc, nullptr);
    glCompileShader(dilationFS);

    m_ShaderProgram_Dilation = glCreateProgram();
    glAttachShader(m_ShaderProgram_Dilation, dilationVS);
    glAttachShader(m_ShaderProgram_Dilation, dilationFS);
    glLinkProgram(m_ShaderProgram_Dilation);

    glDeleteShader(dilationVS);
    glDeleteShader(dilationFS);

    // Fullscreen quad for dilation
    float quadVertices[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f
    };

    glGenVertexArrays(1, &m_VAO_FullscreenQuad);
    glGenBuffers(1, &m_VBO_FullscreenQuad);

    glBindVertexArray(m_VAO_FullscreenQuad);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_FullscreenQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // VAO/VBO for arrows
    std::vector<float> arrowVerts = generateArrowVertices();
    m_i_ArrowVertexCount = static_cast<int>(arrowVerts.size() / 3);

    glGenVertexArrays(1, &m_VAO_Arrow);
    glGenBuffers(1, &m_VBO_Arrow);

    glBindVertexArray(m_VAO_Arrow);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Arrow);
    glBufferData(GL_ARRAY_BUFFER, arrowVerts.size() * sizeof(float), arrowVerts.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // Quad for thick line segments (edge rendering)
    {
        float quadLine[] = {
            0.0f, 0.0f, -0.5f,
            1.0f, 0.0f, -0.5f,
            1.0f, 0.0f,  0.5f,
            0.0f, 0.0f, -0.5f,
            1.0f, 0.0f,  0.5f,
            0.0f, 0.0f,  0.5f
        };
        m_i_LineVertexCount = 6;
        glGenVertexArrays(1, &m_VAO_Line);
        glGenBuffers(1, &m_VBO_Line);
        glBindVertexArray(m_VAO_Line);
        glBindBuffer(GL_ARRAY_BUFFER, m_VBO_Line);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadLine), quadLine, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glBindVertexArray(0);
    }

    UI::SetCameraToggleCallback([this]() {
        m_b_Camera3D = !m_b_Camera3D;
        });

    UI::SetPauseCallback([this]() {
        UI::ShowPausedModal(true);
    });

    UI::SetPausedResumeCallback([this]() {
        UI::ShowPausedModal(false);
        this->m_b_GameActive = true;
    });

    UI::SetPausedDebugCallback([this]() {
        this->m_b_DebuggingMode.store(!this->m_b_DebuggingMode.load());
        // inform HUD so the debug button label updates
        UI::SetPausedDebugState(this->m_b_DebuggingMode.load());
    });

    UI::SetPausedMenuCallback([this]() {
        UI::ShowPausedModal(false);
        this->m_b_RequestMenuChange.store(true);
    });

    // Initialize HUD with current debug state
    UI::SetPausedDebugState(this->m_b_DebuggingMode.load());

    glEnable(GL_DEPTH_TEST);

    // Initialize players from graph data (random distinct nodes)
    m_vec_Players.clear();

    GraphManager gm(Core::k_MaxNodes);
    gm.LoadData(Core::GetMapPath(Core::k_NodeDataRelativePath), Core::GetMapPath(Core::k_ConnectionsRelativePath), false);

    int i_NodeCount = gm.GetNodeCount();
    if (i_NodeCount <= 0) {
        // fallback to simple hardcoded values if graph failed to load
        m_vec_Players.emplace_back(Core::PlayerType::MisterX, 10);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 1);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 2);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 3);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 4);
    }
    else
    {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::uniform_int_distribution<int> dist(1, i_NodeCount);

        auto pick_unique = [&](std::vector<int>& vec_Used) {
            int i_V;
            do { i_V = dist(rng); } while (std::find(vec_Used.begin(), vec_Used.end(), i_V) != vec_Used.end());
            vec_Used.push_back(i_V);
            return i_V;
        };

        std::vector<int> vec_Used;
        int i_MrXNode = pick_unique(vec_Used);
        m_vec_Players.emplace_back(Core::PlayerType::MisterX, i_MrXNode);

        for (int i = 0; i < Core::k_DetectiveCount; ++i) {
            int i_DNode = pick_unique(vec_Used);
            m_vec_Players.emplace_back(Core::PlayerType::Detective, i_DNode);
        }
    }

    m_vec_MovedThisRound.assign(m_vec_Players.size(), false);
    m_i_PlayersRemainingThisRound.store(static_cast<int>(m_vec_Players.size()));
    m_i_Round.store(1);

    // Ensure MisterX is the active player at the start of the game/rounds
    {
        std::lock_guard<std::mutex> lock(m_mtx_Players);
        for (auto& p : m_vec_Players) {
            if (p.GetType() == Core::PlayerType::MisterX) {
                p.SetActive(true);
            } else {
                p.SetActive(false);
            }
        }
    }

    // Initialize player controllers (human/AI)
    InitializePlayerControllers();

    // Reset HUD / ticket marks so previous game's marks don't persist
    {
        std::vector<ScotlandYard::UI::TicketSlot> emptySlots(ScotlandYard::UI::k_TicketSlotCount);
        ScotlandYard::UI::SetTicketStates(emptySlots);
        ScotlandYard::UI::SetRound(m_i_Round.load());
    }

    // Note: previously used debug modal here was removed so the end-game modal
    // only appears when CheckEndOfGame() sets m_b_ShowEndGameModal.


    // Print player positions to console
    for (const auto& player : m_vec_Players) {
        printf("Player: %s\n", player.ToString().c_str());
    }

    // --- Console interaction in background thread: allow moving a player to a connected node ---
    m_graph.LoadData(Core::GetMapPath(Core::k_NodeDataRelativePath), Core::GetMapPath(Core::k_ConnectionsRelativePath), false);
    // Load optional edge geometry (normalized polylines)
    m_graph.LoadEdgeGeometryCSV(Core::GetMapPath(Core::k_EdgeGeometryRelativePath), true);

    // Launch console input loop as a dedicated joinable thread so we don't occupy a ThreadPool worker
    m_b_ConsoleThreadRunning.store(true);
    m_t_ConsoleThread = std::thread([this]() {
        while (m_b_ConsoleThreadRunning.load()) {
            // Print snapshot once and then block waiting for user input (no spamming)
            {
                std::lock_guard<std::mutex> lock(m_mtx_Players);
                std::cout << "\n[Console] Current player positions:\n";
                for (size_t i = 0; i < m_vec_Players.size(); ++i) {
                    std::cout << i << ": " << m_vec_Players[i].ToString() << "\n";
                }
            }

            std::cout << "[Console] Select player index to move (or 'q' to skip): ";
            std::string sel;
            if (!std::getline(std::cin, sel)) {
                // EOF or stream closed - exit thread
                m_b_ConsoleThreadRunning.store(false);
                break;
            }

            if (sel.empty()) {
                continue;
            }

            if (sel == "q" || sel == "Q") {
                continue;
            }

            int idx = -1;
            try { idx = std::stoi(sel); }
            catch (...) { std::cout << "[Console] Invalid selection\n"; continue; }

            int curNode = -1;
            {
                std::lock_guard<std::mutex> lock(m_mtx_Players);
                if (idx < 0 || idx >= static_cast<int>(m_vec_Players.size())) { std::cout << "[Console] Index out of range\n"; continue; }
                curNode = m_vec_Players[idx].GetOccupiedNode();
            }

            // Enforce Mr X active-first move policy:
            // If MrX is active this round, only he may move; if he's not active, he may not move.
            {
                std::lock_guard<std::mutex> lock(m_mtx_Players);
                int i_MrXIndex = -1;
                bool b_MrXActive = false;
                for (size_t i = 0; i < m_vec_Players.size(); ++i) {
                    if (m_vec_Players[i].GetType() == Core::PlayerType::MisterX) {
                        i_MrXIndex = static_cast<int>(i);
                        b_MrXActive = m_vec_Players[i].IsActive();
                        break;
                    }
                }
                if (i_MrXIndex != -1) {
                    if (b_MrXActive && idx != i_MrXIndex) {
                        std::cout << "[Console] Mr X is active this round — only he may move now.\n";
                        continue;
                    }
                    if (!b_MrXActive && idx == i_MrXIndex) {
                        std::cout << "[Console] Mr X already moved this round — wait for other players.\n";
                        continue;
                    }
                }
            }

            std::cout << "[Console] Player " << idx << " is at node " << curNode << "\n";
            auto conns = m_graph.GetConnections(curNode);
            if (conns.empty()) { std::cout << "[Console] No connections from this node.\n"; continue; }

            std::cout << "[Console] Available moves:\n";
            for (size_t i = 0; i < conns.size(); ++i) {
                std::string s_TransportName;
                if (conns[i].i_TransportType == Core::k_TransportTypeTaxi) s_TransportName = "taxi";
                else if (conns[i].i_TransportType == Core::k_TransportTypeBus) s_TransportName = "bus";
                else if (conns[i].i_TransportType == Core::k_TransportTypeMetro) s_TransportName = "metro";
                else if (conns[i].i_TransportType == Core::k_TransportTypeWater) s_TransportName = "water";
                else s_TransportName = "unknown";
                std::cout << i << ": to node " << conns[i].i_NodeId << " via " << s_TransportName << "\n";
            }

            std::cout << "[Console] Choose move index: ";
            std::string s_MoveSelection;
            if (!std::getline(std::cin, s_MoveSelection)) { m_b_ConsoleThreadRunning.store(false); break; }
            if (s_MoveSelection.empty()) { std::cout << "[Console] Empty selection\n"; continue; }
            int i_MoveIndex = -1;
            try { i_MoveIndex = std::stoi(s_MoveSelection); }
            catch (...) { std::cout << "[Console] Invalid move index\n"; continue; }
            if (i_MoveIndex < 0 || i_MoveIndex >= static_cast<int>(conns.size())) { std::cout << "[Console] Move index out of range\n"; continue; }


            int i_DestinationNode = conns[i_MoveIndex].i_NodeId;
            int i_TransportType = conns[i_MoveIndex].i_TransportType;
            bool b_Moved = false;

            // 1 move per round guard - protected by m_mtx_GameState
            {
                std::lock_guard<std::mutex> lock(m_mtx_GameState);
                if (idx < 0 || idx >= (int)m_vec_MovedThisRound.size()) {
                    std::cout << "[Console] Invalid player index.\n";
                    continue;
                }
                if (m_vec_MovedThisRound[idx]) {
                    std::cout << "[Console] Player " << idx
                        << " has already moved in Round " << m_i_Round.load()
                        << ". Wait for next round.\n";
                    continue;
                }
            }

            {
                std::lock_guard<std::mutex> lock(m_mtx_Players);
                auto& player = m_vec_Players[idx];
                bool b_TicketAvailable = true;

                if (i_TransportType == Core::k_TransportTypeTaxi) {
                    b_TicketAvailable = player.SpendTaxiTicket();
                }
                else if (i_TransportType == Core::k_TransportTypeBus) {
                    b_TicketAvailable = player.SpendBusTicket();
                }
                else if (i_TransportType == Core::k_TransportTypeMetro) {
                    b_TicketAvailable = player.SpendMetroTicket();
                }
                else if (i_TransportType == Core::k_TransportTypeWater) {
                    b_TicketAvailable = player.SpendWaterTicket();
                }

                if (!b_TicketAvailable) {
                    std::cout << "[Console] Player " << idx << " does not have required ticket for this transport.\n";
                }
                else {
                    player.MoveTo(i_DestinationNode);
                    b_Moved = true;
                }
            }

            if (b_Moved) {
                std::cout << "[Console] Moved player " << idx << " to node " << i_DestinationNode << "\n";

                // After moving, check for capture (detective landed on MisterX)
                bool b_Captured = false;
                {
                    std::lock_guard<std::mutex> lock(m_mtx_Players);
                    b_Captured = CheckCapture();
                }
                if (b_Captured) {
                    // End game with detectives as winners
                    CheckEndOfGame(Winner::Detectives);
                    // break out of console loop if game inactive
                    if (!m_b_GameActive) {
                        m_b_ConsoleThreadRunning.store(false);
                        break;
                    }
                }

                // Track Mr X ticket usage for HUD and clear his active flag after he moves
                using UI::TicketMark;
                {
                    std::lock_guard<std::mutex> lock(m_mtx_Players);
                    auto& ref_Player = m_vec_Players[idx];
                    if (ref_Player.GetType() == ScotlandYard::Core::PlayerType::MisterX) {
                        TicketMark mark = TicketMark::None;
                        if (i_TransportType == Core::k_TransportTypeTaxi) mark = TicketMark::Taxi;
                        else if (i_TransportType == Core::k_TransportTypeBus) mark = TicketMark::Bus;
                        else if (i_TransportType == Core::k_TransportTypeMetro) mark = TicketMark::Metro;
                        else if (i_TransportType == Core::k_TransportTypeWater) mark = TicketMark::Water;
                        UI::SetSlotMark(m_i_Round.load(), mark, true);
                        // Mr X moved — clear his active flag so other players can move
                        ref_Player.SetActive(false);
                    }
                }

                // Update round state - protected by m_mtx_GameState
                {
                    std::lock_guard<std::mutex> lock(m_mtx_GameState);
                    if (idx >= 0 && idx < (int)m_vec_MovedThisRound.size() && !m_vec_MovedThisRound[idx]) {
                        m_vec_MovedThisRound[idx] = true;
                        int i_Remaining = m_i_PlayersRemainingThisRound.load();
                        if (i_Remaining > 0) {
                            m_i_PlayersRemainingThisRound.store(i_Remaining - 1);
                        }
                    }

                    // If everyone made a move -> either advance to next round or end the game if max reached
                    if (m_i_PlayersRemainingThisRound.load() == 0) {
                        int i_CurrentRound = m_i_Round.load();
                        if (i_CurrentRound >= Core::k_MaxRounds) {
                            // We have completed the final allowed round -> check end conditions
                            std::cout << "[Console] === Completed final Round " << i_CurrentRound << " ===\n";
                            CheckEndOfGame();
                        } else {
                            // advance to next round
                            m_i_Round.store(i_CurrentRound + 1);
                            std::fill(m_vec_MovedThisRound.begin(), m_vec_MovedThisRound.end(), false);
                            m_i_PlayersRemainingThisRound.store(static_cast<int>(m_vec_Players.size()));
                            std::cout << "[Console] === Round " << m_i_Round.load() << " begins ===\n";
                            // At the start of each round, Mr X is the active player
                            {
                                std::lock_guard<std::mutex> lock(m_mtx_Players);
                                for (auto& p : m_vec_Players) {
                                    if (p.GetType() == Core::PlayerType::MisterX) p.SetActive(true);
                                    else p.SetActive(false);
                                }
                            }
                        }
                    }
                }
            }
            // (previously cleared active selection here) - removed per request
        }

    });
}

void GameState::LoadTextures(Core::Application* p_App) {
    if (m_b_TexturesLoaded) return;

    std::string s_TexturePath = p_App->GetAssetPath("textures/Scotland_Yard_schematic.png");
    m_TextureID = p_App->LoadTexture(s_TexturePath);
    if (m_TextureID == 0) {
        printf("Failed to load board texture\n");
    }

    std::string s_IconPath = p_App->GetAssetPath("icons/camera_icon.png");
    UI::LoadCameraIconPNG(s_IconPath.c_str(), p_App);

    m_b_TexturesLoaded = true;
}

void GameState::OnExit() {
    if (m_VAO_Plane) {
        glDeleteVertexArrays(1, &m_VAO_Plane);
        m_VAO_Plane = 0;
    }
    if (m_VBO_Plane) {
        glDeleteBuffers(1, &m_VBO_Plane);
        m_VBO_Plane = 0;
    }
    if (m_VAO_Circle) {
        glDeleteVertexArrays(1, &m_VAO_Circle);
        m_VAO_Circle = 0;
    }
    if (m_VBO_Circle) {
        glDeleteBuffers(1, &m_VBO_Circle);
        m_VBO_Circle = 0;
    }
    if (m_VAO_Cylinder) {
        glDeleteVertexArrays(1, &m_VAO_Cylinder);
        m_VAO_Cylinder = 0;
    }
    if (m_VBO_Cylinder) {
        glDeleteBuffers(1, &m_VBO_Cylinder);
        m_VBO_Cylinder = 0;
    }

    if (m_VAO_Hemisphere) {
        glDeleteVertexArrays(1, &m_VAO_Hemisphere);
        m_VAO_Hemisphere = 0;
    }
    if (m_VBO_Hemisphere) {
        glDeleteBuffers(1, &m_VBO_Hemisphere);
        m_VBO_Hemisphere = 0;
    }
    if (m_VAO_Line) {
        glDeleteVertexArrays(1, &m_VAO_Line);
        m_VAO_Line = 0;
    }
    if (m_VBO_Line) {
        glDeleteBuffers(1, &m_VBO_Line);
        m_VBO_Line = 0;
    }
    if (m_ShaderProgram_Plane) {
        glDeleteProgram(m_ShaderProgram_Plane);
        m_ShaderProgram_Plane = 0;
    }
    if (m_ShaderProgram_Circle) {
        glDeleteProgram(m_ShaderProgram_Circle);
        m_ShaderProgram_Circle = 0;
    }
    if (m_FBO_Picking) {
        glDeleteFramebuffers(1, &m_FBO_Picking);
        m_FBO_Picking = 0;
    }
    if (m_TextureID_Picking) {
        glDeleteTextures(1, &m_TextureID_Picking);
        m_TextureID_Picking = 0;
    }
    if (m_RBO_PickingDepth) {
        glDeleteRenderbuffers(1, &m_RBO_PickingDepth);
        m_RBO_PickingDepth = 0;
    }
    if (m_VAO_Arrow) {
        glDeleteVertexArrays(1, &m_VAO_Arrow);
        m_VAO_Arrow = 0;
    }
    if (m_VBO_Arrow) {
        glDeleteBuffers(1, &m_VBO_Arrow);
        m_VBO_Arrow = 0;
    }
    if (m_ShaderProgram_Picking) {
        glDeleteProgram(m_ShaderProgram_Picking);
        m_ShaderProgram_Picking = 0;
    }
    if (m_FBO_PickingDilated) {
        glDeleteFramebuffers(1, &m_FBO_PickingDilated);
        m_FBO_PickingDilated = 0;
    }
    if (m_TextureID_PickingDilated) {
        glDeleteTextures(1, &m_TextureID_PickingDilated);
        m_TextureID_PickingDilated = 0;
    }
    if (m_ShaderProgram_Dilation) {
        glDeleteProgram(m_ShaderProgram_Dilation);
        m_ShaderProgram_Dilation = 0;
    }
    if (m_VAO_FullscreenQuad) {
        glDeleteVertexArrays(1, &m_VAO_FullscreenQuad);
        m_VAO_FullscreenQuad = 0;
    }
    if (m_VBO_FullscreenQuad) {
        glDeleteBuffers(1, &m_VBO_FullscreenQuad);
        m_VBO_FullscreenQuad = 0;
    }
    if (m_VAO_Line) {
        glDeleteVertexArrays(1, &m_VAO_Line);
        m_VAO_Line = 0;
    }
    if (m_VBO_Line) {
        glDeleteBuffers(1, &m_VBO_Line);
        m_VBO_Line = 0;
    }
    // Note: do not delete m_TextureID here -- textures are managed by Application's cache.
    // ResetToInitial() will set m_TextureID to 0 so LoadTextures() can re-acquire or reload it.
    m_b_GameActive = false;

    m_b_ConsoleThreadRunning.store(false);
    if (m_t_ConsoleThread.joinable()) {
        auto start = std::chrono::steady_clock::now();
        while (m_t_ConsoleThread.joinable()) {
            if (std::chrono::steady_clock::now() - start > std::chrono::seconds(1)) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (m_t_ConsoleThread.joinable()) {
            m_t_ConsoleThread.detach();
        }
    }

    // Ensure game data is reset when exiting so re-entering GameState starts fresh
    ResetToInitial();
}

void GameState::ResetToInitial() {
    std::lock_guard<std::mutex> lockPlayers(m_mtx_Players);
    std::lock_guard<std::mutex> lockState(m_mtx_GameState);

    m_vec_Players.clear();
    m_vec_MovedThisRound.clear();
    m_i_PlayersRemainingThisRound.store(0);
    m_i_Round.store(1);
    m_b_GameActive = false;
    m_b_TexturesLoaded = false;

    // Reset HUD
    std::vector<ScotlandYard::UI::TicketSlot> emptySlots(ScotlandYard::UI::k_TicketSlotCount);
    ScotlandYard::UI::SetTicketStates(emptySlots);
    ScotlandYard::UI::SetRound(m_i_Round.load());
}

void GameState::OnPause() {
    m_b_GameActive = false;
}

void GameState::OnResume() {
    m_b_GameActive = true;
}

void GameState::Update(float f_DeltaTime) {
    if (!m_b_GameActive) return;

    if (m_b_Camera3D) {
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

        m_vec3_CameraFront.x = 0.0f;
        m_vec3_CameraFront.y = sin(m_f_CameraAngle);
        m_vec3_CameraFront.z = -cos(m_f_CameraAngle);
        m_vec3_CameraFront = glm::normalize(m_vec3_CameraFront);

        if (fabs(m_f_CameraAngleVelocity) > 0.0001f) {
            glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
            float f_ForwardAcceleration = -m_f_CameraAngleVelocity * k_CameraScrollToForwardRatio;
            m_vec3_CameraVelocity += vec3_Forward * f_ForwardAcceleration;

            float f_Speed = glm::length(m_vec3_CameraVelocity);
            if (f_Speed > k_MaxCameraSpeed) {
                m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
            }
        }

        const Uint8* p_KeyState = SDL_GetKeyboardState(nullptr);

        if (p_KeyState[SDL_SCANCODE_W]) {
            AccelerateCameraForward(f_DeltaTime);
        }
        if (p_KeyState[SDL_SCANCODE_S]) {
            AccelerateCameraBackward(f_DeltaTime);
        }
        if (p_KeyState[SDL_SCANCODE_A]) {
            AccelerateCameraLeft(f_DeltaTime);
        }
        if (p_KeyState[SDL_SCANCODE_D]) {
            AccelerateCameraRight(f_DeltaTime);
        }
    }

    UpdateCameraPhysics(f_DeltaTime);

    // Update AI players and process their moves
    UpdateAIPlayers(nullptr, f_DeltaTime);
    ProcessAIPendingMoves();
}

void GameState::RenderMrXToken(const glm::vec2& vec2_Position, const glm::mat4& mat4_Projection, const glm::mat4& mat4_View, GLint i_MvpLoc, GLint i_ColorLoc) {
    glm::mat4 mat4_Model = glm::mat4(1.0f);
    mat4_Model = glm::translate(mat4_Model, glm::vec3(vec2_Position.x, 0.05f * m_f_GlobalScale, vec2_Position.y));
    mat4_Model = glm::scale(mat4_Model, glm::vec3(2.0f));
    mat4_Model = mat4_Model * m_mat4_GlobalScaleMatrix;

    glm::vec3 vec3_Color = glm::vec3(0.0f, 0.0f, 0.0f);
    glUniform3fv(i_ColorLoc, 1, glm::value_ptr(vec3_Color));

    // Body (cylinder)
    glm::mat4 mat4_CylinderModel = glm::scale(mat4_Model, glm::vec3(1.0f));
    glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_CylinderModel;
    glUniformMatrix4fv(i_MvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    glBindVertexArray(m_VAO_Cylinder);
    glDrawArrays(GL_TRIANGLES, 0, m_i_CylinderVertexCount);
    glBindVertexArray(0);

    // Top (hemisphere)
    glm::mat4 mat4_HemiModel = glm::translate(mat4_Model, glm::vec3(0.0f, 0.1f, 0.0f));
    mat4_MVP = mat4_Projection * mat4_View * mat4_HemiModel;
    glUniformMatrix4fv(i_MvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    glBindVertexArray(m_VAO_Hemisphere);
    glDrawArrays(GL_TRIANGLES, 0, m_i_HemisphereVertexCount);
    glBindVertexArray(0);
}

void GameState::Render(Core::Application* p_App) {
    
    LoadTextures(p_App);
    HandleResize(p_App);

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 mat4_View, mat4_Projection;
    if (m_b_Camera3D) {
        m_vec3_CameraPosition = m_vec3_Saved3DCameraPosition;
        glm::vec3 vec3_Target = m_vec3_CameraPosition + m_vec3_CameraFront;
        mat4_View = glm::lookAt(m_vec3_CameraPosition, vec3_Target, m_vec3_CameraUp);
        mat4_Projection = glm::perspective(glm::radians(45.0f), float(m_i_Width)/m_i_Height, 0.1f,100.0f);
    } else {
        m_vec3_CameraVelocity = glm::vec3(0.0f);
        glm::vec3 vec3_CameraPos = glm::vec3(11.0f * m_f_GlobalScale, 5.0f * m_f_GlobalScale, 8.0f * m_f_GlobalScale);
        glm::vec3 vec3_CameraTarget = glm::vec3(11.0f * m_f_GlobalScale, 0.0f, 8.0f * m_f_GlobalScale);
        glm::vec3 vec3_CameraUp = glm::vec3(0, 0, -1);
        mat4_View = glm::lookAt(vec3_CameraPos, vec3_CameraTarget, vec3_CameraUp);
        float f_Aspect = float(m_i_Width)/m_i_Height;
        float f_HalfHeight = 10.0f * m_f_GlobalScale;
        float f_HalfWidth = f_HalfHeight * f_Aspect;
        mat4_Projection = glm::ortho(-f_HalfWidth, f_HalfWidth, -f_HalfHeight, f_HalfHeight, 0.1f, 10.0f);
    }

    RenderBoard(p_App, mat4_View, mat4_Projection);
    RenderEdges(mat4_View, mat4_Projection);
    RenderStations(mat4_View, mat4_Projection);
    RenderPlayers(mat4_View, mat4_Projection);
    RenderArrows(mat4_View, mat4_Projection);
    RenderHUD(p_App);
    RenderDebugOverlay(p_App);
    RenderPicking(p_App, mat4_View, mat4_Projection);
    RenderEndGameModal(p_App);

    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());

    if (m_b_RequestMenuChange.load() && p_App) {
        if (auto mgr = p_App->GetStateManager()) {
            std::cout << "[GameState] Requesting state change to menu...\n";
            mgr->ChangeState("menu");
        }
        m_b_RequestMenuChange.store(false);
    }
}

void GameState::HandleResize(Core::Application* p_App) {
    int i_NewWidth = p_App->GetWidth();
    int i_NewHeight = p_App->GetHeight();

    if (i_NewWidth != m_i_Width || i_NewHeight != m_i_Height) {
        m_i_Width = i_NewWidth;
        m_i_Height = i_NewHeight;

        glBindTexture(GL_TEXTURE_2D, m_TextureID_Picking);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_i_Width, m_i_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

        glBindRenderbuffer(GL_RENDERBUFFER, m_RBO_PickingDepth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, m_i_Width, m_i_Height);

        glBindTexture(GL_TEXTURE_2D, m_TextureID_PickingDilated);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, m_i_Width, m_i_Height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

        glBindTexture(GL_TEXTURE_2D, 0);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
    }
}

void GameState::RenderBoard(Core::Application* p_App, const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
    glUseProgram(m_ShaderProgram_Plane);

    glm::mat4 mat4_Model = glm::mat4(1.0f);
    mat4_Model = m_mat4_GlobalScaleMatrix * mat4_Model;
    mat4_Model = glm::rotate(mat4_Model, glm::radians(m_f_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
    GLuint mvpLocPlane = glGetUniformLocation(m_ShaderProgram_Plane, "MVP");
    glUniformMatrix4fv(mvpLocPlane, 1, GL_FALSE, glm::value_ptr(mat4_MVP));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    GLuint texLoc = glGetUniformLocation(m_ShaderProgram_Plane, "ourTexture");
    glUniform1i(texLoc, 0);

    glBindVertexArray(m_VAO_Plane);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

void GameState::RenderStations(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
    glUseProgram(m_ShaderProgram_Circle);

    GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Circle, "MVP");
    GLuint colorLoc = glGetUniformLocation(m_ShaderProgram_Circle, "circleColor");

    float f_BaseScale = 7.0f;
    float f_RingStep = 1.0f;
    float f_YStep = 0.02f;
    float f_TopCircleScale = 6.4f;

    std::vector<std::string> vec_TransportOrder = { "metro", "bus", "taxi", "water" };

    for (const auto& station : m_vec_CircleStations) {
        glm::mat4 mat4_ModelBase = glm::translate(glm::mat4(1.0f),
                                             glm::vec3(station.position.x, 0.0f, station.position.y));

        std::vector<std::string> vec_TypesPresent;
        for (const auto& s_Type : vec_TransportOrder) {
            if (std::find(station.transportTypes.begin(), station.transportTypes.end(), s_Type) != station.transportTypes.end()) {
                vec_TypesPresent.push_back(s_Type);
            }
        }

        int i_Count = static_cast<int>(vec_TypesPresent.size());
        float f_YStart = 0.001f;

        for (int i = 0; i < i_Count; ++i) {
            const std::string& s_TransportType = vec_TypesPresent[i];
            glm::vec3 vec3_Color;
            if (s_TransportType == "metro") vec3_Color = {1.0f, 0.0f, 0.0f};
            else if (s_TransportType == "bus") vec3_Color = {0.0f, 1.0f, 0.0f};
            else if (s_TransportType == "taxi") vec3_Color = {1.0f, 1.0f, 0.0f};
            else vec3_Color = {0.0f, 0.4f, 1.0f};

            float f_Scale = f_BaseScale + (i_Count - i) * f_RingStep;
            float f_YOffset = f_YStart + i * f_YStep;
            glm::mat4 mat4_Model = glm::translate(mat4_ModelBase, glm::vec3(0.0f, f_YOffset * m_f_GlobalScale, 0.0f));
            mat4_Model = glm::scale(mat4_Model, glm::vec3(f_Scale, 0.2f, f_Scale)) * m_mat4_GlobalScaleMatrix;

            glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
            glUniform3fv(colorLoc, 1, glm::value_ptr(vec3_Color));

            glBindVertexArray(m_VAO_Circle);
            glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
            glBindVertexArray(0);
        }

        float f_YTop = f_YStart + i_Count * f_YStep;
        glm::mat4 mat4_Model = glm::translate(mat4_ModelBase, glm::vec3(0.0f, f_YTop * m_f_GlobalScale, 0.0f));
        mat4_Model = glm::scale(mat4_Model, glm::vec3(f_TopCircleScale, 0.2f, f_TopCircleScale)) * m_mat4_GlobalScaleMatrix;

        glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
        glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(1.0f)));

        glBindVertexArray(m_VAO_Circle);
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
        glBindVertexArray(0);
    }
}

void GameState::RenderPlayers(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
    glUseProgram(m_ShaderProgram_Circle);
    GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Circle, "MVP");
    GLuint colorLoc = glGetUniformLocation(m_ShaderProgram_Circle, "circleColor");

    for (const auto& player : m_vec_Players) {
        if (player.GetType() != Core::PlayerType::Detective) continue;

        int i_NodeId = player.GetOccupiedNode();
        auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                               [i_NodeId](const StationCircle& sc){ return sc.stationID == i_NodeId; });
        if (it == m_vec_CircleStations.end()) continue;

        glm::vec2 vec2_Position = it->position;
        glm::mat4 mat4_Model = glm::translate(glm::mat4(1.0f), glm::vec3(vec2_Position.x, 0.05f * m_f_GlobalScale, vec2_Position.y));
        mat4_Model = glm::scale(mat4_Model, glm::vec3(2.0f));
        mat4_Model = mat4_Model * m_mat4_GlobalScaleMatrix;

        glm::vec3 vec3_Color = glm::vec3(0.0f, 0.0f, 1.0f);
        glUniform3fv(colorLoc, 1, glm::value_ptr(vec3_Color));

        glBindVertexArray(m_VAO_Cylinder);
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View * mat4_Model));
        glDrawArrays(GL_TRIANGLES, 0, m_i_CylinderVertexCount);
        glBindVertexArray(0);

        glm::mat4 mat4_HemiModel = glm::translate(mat4_Model, glm::vec3(0.0f, 0.1f, 0.0f));
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View * mat4_HemiModel));

        glBindVertexArray(m_VAO_Hemisphere);
        glDrawArrays(GL_TRIANGLES, 0, m_i_HemisphereVertexCount);
        glBindVertexArray(0);
    }

    // Render Mister X if appropriate (debug or misterX is active)
    int i_CurrentRoundForRender = m_i_Round.load();
    for (const auto& player : m_vec_Players) {
        if (player.GetType() != Core::PlayerType::MisterX) continue;

        bool b_ShouldRenderMrX = false;
        if (m_b_DebuggingMode && m_b_ShowMrXInDebug) b_ShouldRenderMrX = true;
        if (player.IsActive()) b_ShouldRenderMrX = true;
        if (!b_ShouldRenderMrX) continue;

        int i_NodeId = player.GetOccupiedNode();
        auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                               [i_NodeId](const StationCircle& sc){ return sc.stationID == i_NodeId; });
        if (it == m_vec_CircleStations.end()) continue;

        RenderMrXToken(it->position, mat4_Projection, mat4_View, mvpLoc, colorLoc);
    }
}

void GameState::RenderArrows(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
    glUseProgram(m_ShaderProgram_Circle);
    GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Circle, "MVP");
    GLuint colorLoc = glGetUniformLocation(m_ShaderProgram_Circle, "circleColor");

    for (const auto& arrow : m_vec_CurrentArrows) {
        glm::vec3 vec3_Color;
        switch (arrow.i_TransportType) {
            case Core::k_TransportTypeTaxi: vec3_Color = glm::vec3(1.0f, 1.0f, 0.0f); break;
            case Core::k_TransportTypeBus: vec3_Color = glm::vec3(0.0f, 1.0f, 0.0f); break;
            case Core::k_TransportTypeMetro: vec3_Color = glm::vec3(1.0f, 0.0f, 0.0f); break;
            case Core::k_TransportTypeWater: vec3_Color = glm::vec3(0.0f, 0.4f, 1.0f); break;
            default: vec3_Color = glm::vec3(1.0f); break;
        }

        glm::mat4 mat4_BaseModel = glm::translate(glm::mat4(1.0f), glm::vec3(arrow.vec2_Position.x, 0.1f * m_f_GlobalScale, arrow.vec2_Position.y));
        mat4_BaseModel = glm::rotate(mat4_BaseModel, arrow.f_Rotation, glm::vec3(0.0f, 1.0f, 0.0f));

        // Draw black outline slightly larger
        glm::mat4 mat4_OutlineModel = mat4_BaseModel * glm::scale(m_mat4_GlobalScaleMatrix, glm::vec3(1.5f));
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View * mat4_OutlineModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(0.0f, 0.0f, 0.0f)));
        glBindVertexArray(m_VAO_Arrow);
        glDrawArrays(GL_TRIANGLES, 0, m_i_ArrowVertexCount);

        // Draw colored arrow on top
        glm::mat4 mat4_ColoredModel = mat4_BaseModel * m_mat4_GlobalScaleMatrix;
        mat4_ColoredModel = glm::translate(mat4_ColoredModel, glm::vec3(0.0f, 0.01f, 0.0f));
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_Projection * mat4_View * mat4_ColoredModel));
        glUniform3fv(colorLoc, 1, glm::value_ptr(vec3_Color));
        glDrawArrays(GL_TRIANGLES, 0, m_i_ArrowVertexCount);

        glBindVertexArray(0);
    }
}

void GameState::RenderEdges(const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
    // Visualize edge geometry as thick 3D lines with transport colors
    GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean b_DepthMaskWas = GL_TRUE;
    glGetBooleanv(GL_DEPTH_WRITEMASK, &b_DepthMaskWas);
    if (b_DepthWas) glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glUseProgram(m_ShaderProgram_Circle);
    GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Circle, "MVP");
    GLuint colorLoc = glGetUniformLocation(m_ShaderProgram_Circle, "circleColor");

    // Build lookup: stationID -> position (already scaled by m_f_GlobalScale)
    std::unordered_map<int, glm::vec2> map_NodePos;
    map_NodePos.reserve(m_vec_CircleStations.size());
    for (const auto& sc : m_vec_CircleStations) map_NodePos[sc.stationID] = sc.position;

    auto colorForType = [](int t) -> glm::vec3 {
        if (t == Core::k_TransportTypeTaxi)  return { Core::k_EdgeColorTaxi[0],  Core::k_EdgeColorTaxi[1],  Core::k_EdgeColorTaxi[2] };
        if (t == Core::k_TransportTypeBus)   return { Core::k_EdgeColorBus[0],   Core::k_EdgeColorBus[1],   Core::k_EdgeColorBus[2] };
        if (t == Core::k_TransportTypeMetro) return { Core::k_EdgeColorMetro[0], Core::k_EdgeColorMetro[1], Core::k_EdgeColorMetro[2] };
        if (t == Core::k_TransportTypeWater) return { Core::k_EdgeColorWater[0], Core::k_EdgeColorWater[1], Core::k_EdgeColorWater[2] };
        return {1.0f, 1.0f, 1.0f};
    };

    auto thicknessForType = [](int t) -> float {
        if (t == Core::k_TransportTypeTaxi)  return Core::k_EdgeThicknessTaxi;
        if (t == Core::k_TransportTypeBus)   return Core::k_EdgeThicknessBus;
        if (t == Core::k_TransportTypeMetro) return Core::k_EdgeThicknessMetro;
        if (t == Core::k_TransportTypeWater) return Core::k_EdgeThicknessWater;
        return Core::k_EdgeThicknessTaxi;
    };

    auto zOffsetForType = [&](int t) -> float {
        // lower values render closer to the board; we lift a bit per type to enforce draw order
        if (t == Core::k_TransportTypeMetro) return 0.006f * m_f_GlobalScale; // lowest
        if (t == Core::k_TransportTypeBus)   return 0.010f * m_f_GlobalScale; // middle
        if (t == Core::k_TransportTypeWater) return 0.012f * m_f_GlobalScale; // between bus and taxi
        if (t == Core::k_TransportTypeTaxi)  return 0.014f * m_f_GlobalScale; // top
        return 0.012f * m_f_GlobalScale;
    };

    struct DrawEdge { int i_Type; std::vector<glm::vec2> vec_Points; };
    std::vector<DrawEdge> vec_EdgesMetro, vec_EdgesBus, vec_EdgesWater, vec_EdgesTaxi;

    for (int i = 1; i <= Core::k_MaxNodes; ++i) {
        Node* n = m_graph.GetNode(i);
        if (!n) continue;
        int sc = n->GetSlotCount();
        for (int s = 0; s < sc; ++s) {
            Edge* e = n->getEdge(s);
            if (!e) continue;
            if (e->getGeometryType() != Edge::GeometryType::Polyline) continue;
            if (e->endpoints[0] != n) continue; // ensure draw once

            int srcId = e->endpoints[0] ? e->endpoints[0]->id : -1;
            int dstId = e->endpoints[1] ? e->endpoints[1]->id : -1;
            const auto& ptsNorm = e->getPolylineNormalized();
            if (ptsNorm.size() < 2) continue;

            std::vector<glm::vec2> ptsWorld;
            ptsWorld.reserve(ptsNorm.size());
            for (const auto& p : ptsNorm) {
                float gx = p.x * Core::k_MapGridMaxX;
                float gy = p.y * Core::k_MapGridMaxY;
                ptsWorld.emplace_back(gx * m_f_GlobalScale, gy * m_f_GlobalScale);
            }
            auto itSrc = map_NodePos.find(srcId);
            if (itSrc != map_NodePos.end()) ptsWorld.front() = itSrc->second;
            auto itDst = map_NodePos.find(dstId);
            if (itDst != map_NodePos.end()) ptsWorld.back() = itDst->second;

            DrawEdge s_DrawEdge{e->type, std::move(ptsWorld)};
            if (e->type == Core::k_TransportTypeMetro) vec_EdgesMetro.push_back(std::move(s_DrawEdge));
            else if (e->type == Core::k_TransportTypeBus) vec_EdgesBus.push_back(std::move(s_DrawEdge));
            else if (e->type == Core::k_TransportTypeTaxi) vec_EdgesTaxi.push_back(std::move(s_DrawEdge));
            else if (e->type == Core::k_TransportTypeWater) vec_EdgesWater.push_back(std::move(s_DrawEdge));
        }
    }

    auto fn_DrawPolyline = [&](const DrawEdge& s_Edge, bool b_Dashed) {
        glm::vec3 vec3_Color = colorForType(s_Edge.i_Type);
        float f_BaseThickness = thicknessForType(s_Edge.i_Type);
        float f_YOffset = zOffsetForType(s_Edge.i_Type);

        for (size_t k = 1; k < s_Edge.vec_Points.size(); ++k) {
            glm::vec2 vec2_P0 = s_Edge.vec_Points[k-1];
            glm::vec2 vec2_P1 = s_Edge.vec_Points[k];
            glm::vec2 vec2_D = vec2_P1 - vec2_P0;
            float f_LenScaled = glm::length(vec2_D);
            if (f_LenScaled <= 1e-6f) continue;
            float f_AngleY = atan2(-vec2_D.y, vec2_D.x);
            glm::vec2 vec2_Dir = vec2_D / f_LenScaled;

            auto fn_DrawOne = [&](const glm::vec2& vec2_SegStart, float f_SegLenScaled) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, glm::vec3(vec2_SegStart.x, f_YOffset, vec2_SegStart.y));
                model = glm::rotate(model, f_AngleY, glm::vec3(0,1,0));
                model = glm::scale(model, glm::vec3(f_SegLenScaled / m_f_GlobalScale, 1.0f, f_BaseThickness));
                model = model * m_mat4_GlobalScaleMatrix;
                glm::mat4 mvp = mat4_Projection * mat4_View * model;
                glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(mvp));
                glUniform3fv(colorLoc, 1, glm::value_ptr(vec3_Color));
                glBindVertexArray(m_VAO_Line);
                glDrawArrays(GL_TRIANGLES, 0, m_i_LineVertexCount);
            };

            if (!b_Dashed) {
                fn_DrawOne(vec2_P0, f_LenScaled);
            } else {
                float f_DashLenScaled = Core::k_MetroDashLen;
                float f_GapLenScaled  = Core::k_MetroGapLen;
                if (s_Edge.i_Type == Core::k_TransportTypeWater) {
                    f_DashLenScaled = Core::k_WaterDashLen;
                    f_GapLenScaled  = Core::k_WaterGapLen;
                }
                float f_T = 0.0f;
                while (f_T < f_LenScaled - 1e-6f) {
                    float f_SegLen = std::min(f_DashLenScaled, std::max(0.0f, f_LenScaled - f_T));
                    glm::vec2 vec2_SegStart = vec2_P0 + vec2_Dir * f_T;
                    fn_DrawOne(vec2_SegStart, f_SegLen);
                    f_T += f_DashLenScaled + f_GapLenScaled;
                }
            }
        }
    };

    for (const auto& e : vec_EdgesMetro) fn_DrawPolyline(e, true);
    for (const auto& e : vec_EdgesBus)   fn_DrawPolyline(e, false);
    for (const auto& e : vec_EdgesWater) fn_DrawPolyline(e, true);  // dashed water
    for (const auto& e : vec_EdgesTaxi)  fn_DrawPolyline(e, false);

    glBindVertexArray(0);

    // Restore depth state
    if (b_DepthWas) glEnable(GL_DEPTH_TEST);
    glDepthMask(b_DepthMaskWas);
}

void GameState::RenderHUD(Core::Application* p_App) {
    std::vector<std::string> labels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };

    int black = -1, dbl = -1;
    for (const auto& pl : m_vec_Players) {
        if (pl.GetType() == Core::PlayerType::MisterX) {
            black = pl.GetBlackTickets();
            dbl = pl.GetDoubleMoveTickets();
            break;
        }
    }
    std::vector<int> counts = { -1, black, dbl, -1, -1, -1 };

    ScotlandYard::UI::SetTopBar(labels, {}, counts);
    ScotlandYard::UI::SetRound(m_i_Round.load());
    ScotlandYard::UI::RenderHUD(p_App);
}

void GameState::RenderDebugOverlay(Core::Application* p_App) {
    if (!m_b_DebuggingMode.load()) return;

    GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
    if (b_DepthWas) glDisable(GL_DEPTH_TEST);
    if (!b_BlendWas) glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    UI::Color white{1.0f,1.0f,1.0f,1.0f};
    UI::DrawTextCenteredPx("DEBUG MODE - Press P: Color Picking View", 10, m_i_Height-60, m_i_Width-10, m_i_Height-40, white, p_App, 0.0f);
    UI::DrawTextCenteredPx("Press M: Toggle Mr X Visibility", 10, m_i_Height-40, m_i_Width-10, m_i_Height-20, white, p_App, 0.0f);

    if (!b_BlendWas) glDisable(GL_BLEND);
    if (b_DepthWas) glEnable(GL_DEPTH_TEST);
}

void GameState::RenderPicking(Core::Application* p_App, const glm::mat4& mat4_View, const glm::mat4& mat4_Projection) {
    if (!m_b_ShowPickingBuffer.load()) return;

    m_map_PickingIDToClickable.clear();
    m_ui_NextPickingID = 0;

    RenderPickingPass(mat4_Projection, mat4_View);
    ApplyDilationPass();

    int i_WindowWidth = p_App->GetWidth();
    int i_WindowHeight = p_App->GetHeight();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_FBO_PickingDilated);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0,0,i_WindowWidth,i_WindowHeight,0,0,i_WindowWidth,i_WindowHeight,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void GameState::RenderEndGameModal(Core::Application* p_App) {
    if (!m_b_ShowEndGameModal.load()) return;

    GLboolean b_DepthWas = glIsEnabled(GL_DEPTH_TEST);
    GLboolean b_BlendWas = glIsEnabled(GL_BLEND);
    if (b_DepthWas) glDisable(GL_DEPTH_TEST);
    if (!b_BlendWas) glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    int i_W = p_App->GetWidth();
    int i_H = p_App->GetHeight();
    float f_ModalWpx = std::min(UI::HUDStyle::k_ModalMaxWidthPx, float(i_W) * UI::HUDStyle::k_ModalWidthRatio);
    float f_ModalHpx = std::min(UI::HUDStyle::k_ModalMaxHeightPx, float(i_H) * UI::HUDStyle::k_ModalHeightRatio);
    float f_Left = (i_W - f_ModalWpx) * 0.5f;
    float f_Bottom = (i_H - f_ModalHpx) * 0.5f;
    float f_Right = f_Left + f_ModalWpx;
    float f_Top = f_Bottom + f_ModalHpx;

    ScotlandYard::UI::DrawRoundedRectScreen(f_Left,f_Bottom,f_Right,f_Top,{0.08f,0.08f,0.12f,0.95f}, UI::HUDStyle::k_ModalCornerRadiusPx, p_App);

    std::string s_Title = "";
    std::string s_Message = "GAME OVER";
    if (m_EndGameWinner == Winner::Detectives) s_Title = "Congratulations! Detectives win";
    else if (m_EndGameWinner == Winner::MisterX) s_Title = "Congratulations! Mister X wins";
    else s_Title = "Game ended";

    ScotlandYard::UI::Color white{1.0f,1.0f,1.0f,1.0f};
    float f_TitleH = f_ModalHpx * 0.14f;
    float f_MsgH = f_ModalHpx * 0.40f;
    float f_TitleBottom = f_Bottom + f_ModalHpx*0.74f;
    float f_TitleTop = f_TitleBottom + f_TitleH;
    float f_MsgBottom = f_Bottom + f_ModalHpx*0.25f;
    float f_MsgTop = f_MsgBottom + f_MsgH;

    ScotlandYard::UI::DrawTextCenteredPx(s_Title,f_Left+20,f_TitleBottom,f_Right-20,f_TitleTop,white,p_App,0.0f);
    ScotlandYard::UI::DrawTextCenteredPx(s_Message,f_Left+20,f_MsgBottom,f_Right-20,f_MsgTop,white,p_App,-6.0f);

    // Draw MENU button
    float f_BtnW=260,f_BtnH=54,f_BtnX0=f_Left+(f_ModalWpx-f_BtnW)*0.5f,f_BtnY0=f_Bottom+18.0f;
    float f_BtnX1=f_BtnX0+f_BtnW,f_BtnY1=f_BtnY0+f_BtnH;

    if (m_b_EndModalBtnHover.load()) {
        ScotlandYard::UI::DrawRoundedRectScreen(f_BtnX0-6,f_BtnY0-6,f_BtnX1+6,f_BtnY1+6,{1,1,1,0.08f},14,p_App);
    }
    ScotlandYard::UI::DrawRoundedRectScreen(f_BtnX0,f_BtnY0,f_BtnX1,f_BtnY1,{0.0f,0.5f,0.9f,1.0f},10,p_App);
    ScotlandYard::UI::DrawTextCenteredPx("MENU",f_BtnX0,f_BtnY0,f_BtnX1,f_BtnY1,white,p_App,-12.0f);

    m_i_EndModalBtnX0=int(f_BtnX0); m_i_EndModalBtnY0=int(f_BtnY0);
    m_i_EndModalBtnX1=int(f_BtnX1); m_i_EndModalBtnY1=int(f_BtnY1);

    if (!b_BlendWas) glDisable(GL_BLEND);
    if (b_DepthWas) glEnable(GL_DEPTH_TEST);
}

void GameState::CheckEndOfGame(Winner winner) {
    if (winner == Winner::None) {
        if (m_i_Round.load() >= Core::k_MaxRounds) {
            winner = Winner::MisterX;
        } else {
            return; 
        }
    }

    m_b_GameActive = false;
    m_EndGameWinner = winner;
    m_b_ShowEndGameModal.store(true);

    if (winner == Winner::Detectives) {
        std::cout << "[Game] Detectives win -- MisterX captured!\n";
    } else if (winner == Winner::MisterX) {
        std::cout << "[Game] Mr X wins -- reached max rounds (" << m_i_Round.load() << ")\n";
    }
}

bool GameState::CheckCapture() const {
    int mrXNode = -1;
    for (const auto& p : m_vec_Players) {
        if (p.GetType() == Core::PlayerType::MisterX) {
            mrXNode = p.GetOccupiedNode();
            break;
        }
    }
    if (mrXNode < 0) return false;

    for (const auto& p : m_vec_Players) {
        if (p.GetType() == Core::PlayerType::Detective && p.GetOccupiedNode() == mrXNode) {
            return true;
        }
    }
    return false;
}

void GameState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                // Can set exit flag
                break;
            case SDLK_p:
                if (m_b_DebuggingMode.load()) {
                    m_b_ShowPickingBuffer.store(!m_b_ShowPickingBuffer.load());
                    std::cout << "[GameState] Picking buffer debug: " << (m_b_ShowPickingBuffer.load() ? "ON" : "OFF") << "\n";
                }
                break;
            case SDLK_m:
                if (m_b_DebuggingMode.load()) {
                    m_b_ShowMrXInDebug.store(!m_b_ShowMrXInDebug.load());
                    std::cout << "[GameState] Mr X visibility: " << (m_b_ShowMrXInDebug.load() ? "ON" : "OFF") << "\n";
                }
                break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int i_X = event.button.x;
        int i_Y = event.button.y;
        if (m_b_ShowEndGameModal.load()) {
            int i_H = p_App ? p_App->GetHeight() : 0;
            int i_PxY = i_Y;
            int i_FlippedY = i_H - i_PxY;

            if (i_X >= m_i_EndModalBtnX0 && i_X <= m_i_EndModalBtnX1 &&
                i_FlippedY >= m_i_EndModalBtnY0 && i_FlippedY <= m_i_EndModalBtnY1) {
                m_b_RequestMenuChange.store(true);
                m_b_ShowEndGameModal.store(false);
                m_b_EndModalBtnHover.store(false);
                return;
            }
            return;
        }

        UI::HandleMouseClick(i_X, i_Y);

        if (event.button.button == SDL_BUTTON_LEFT) {
            HandleColorPicking(i_X, i_Y);
        }
    }

    if (event.type == SDL_MOUSEWHEEL && m_b_Camera3D) {
        float f_ScrollInput = event.wheel.y * k_CameraScrollAcceleration;
        m_f_CameraAngleVelocity -= f_ScrollInput;
    }

    if (event.type == SDL_MOUSEMOTION) {
        if (m_b_ShowEndGameModal.load()) {
            int i_X = event.motion.x;
            int i_Y = event.motion.y;
            int i_H = p_App ? p_App->GetHeight() : 0;
            int i_FlippedY = i_H - i_Y;
            bool b_Hover = (i_X >= m_i_EndModalBtnX0 && i_X <= m_i_EndModalBtnX1 &&
                            i_FlippedY >= m_i_EndModalBtnY0 && i_FlippedY <= m_i_EndModalBtnY1);
            m_b_EndModalBtnHover.store(b_Hover);
            return;
        }
        UI::HandleMouseMotion(event.motion.x, event.motion.y);
    }

}

std::vector<float> GameState::generateCircleVertices(float f_Radius, int i_Segments) {
    std::vector<float> vec_Vertices;

    vec_Vertices.push_back(0.0f);
    vec_Vertices.push_back(0.01f);
    vec_Vertices.push_back(0.0f);

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

std::vector<float> GameState::generateCylinderVertices(float radius, float height, int segments)
{
    std::vector<float> verts;
    const float k_TwoPi = 2.0f * glm::pi<float>();

    for (int i = 0; i < segments; ++i)
    {
        float theta1 = (float)i / segments * k_TwoPi;
        float theta2 = (float)(i+1) / segments * k_TwoPi;

        float x1 = radius * cos(theta1);
        float z1 = radius * sin(theta1);
        float x2 = radius * cos(theta2);
        float z2 = radius * sin(theta2);

        // boczna ściana
        verts.push_back(x1); verts.push_back(0.0f); verts.push_back(z1);
        verts.push_back(x2); verts.push_back(0.0f); verts.push_back(z2);
        verts.push_back(x2); verts.push_back(height); verts.push_back(z2);

        verts.push_back(x1); verts.push_back(0.0f); verts.push_back(z1);
        verts.push_back(x2); verts.push_back(height); verts.push_back(z2);
        verts.push_back(x1); verts.push_back(height); verts.push_back(z1);

        // dolna podstawa
        verts.push_back(0.0f); verts.push_back(0.0f); verts.push_back(0.0f);
        verts.push_back(x2); verts.push_back(0.0f); verts.push_back(z2);
        verts.push_back(x1); verts.push_back(0.0f); verts.push_back(z1);

        // górna podstawa
        verts.push_back(0.0f); verts.push_back(height); verts.push_back(0.0f);
        verts.push_back(x1); verts.push_back(height); verts.push_back(z1);
        verts.push_back(x2); verts.push_back(height); verts.push_back(z2);
    }

    return verts;
}

std::vector<float> GameState::generateHemisphereVertices(float radius, int segments)
{
    std::vector<float> verts;
    const float k_Pi = glm::pi<float>();
    const float k_TwoPi = 2.0f * k_Pi;

    for(int i = 0; i < segments / 2; ++i)
    {
        float theta1 = k_Pi * i / segments;       // dolny pierścień
        float theta2 = k_Pi * (i + 1) / segments; // górny pierścień

        for(int j = 0; j < segments; ++j)
        {
            float phi1 = k_TwoPi * j / segments;
            float phi2 = k_TwoPi * (j + 1) / segments;

            // cztery wierzchołki kwadratu -> dwa trójkąty
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

            // trójkąt 1
            verts.push_back(x1); verts.push_back(y1); verts.push_back(z1);
            verts.push_back(x2); verts.push_back(y2); verts.push_back(z2);
            verts.push_back(x3); verts.push_back(y3); verts.push_back(z3);

            // trójkąt 2
            verts.push_back(x1); verts.push_back(y1); verts.push_back(z1);
            verts.push_back(x3); verts.push_back(y3); verts.push_back(z3);
            verts.push_back(x4); verts.push_back(y4); verts.push_back(z4);
        }
    }

    return verts;
}

void GameState::AccelerateCameraForward(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    // parallel to ground
    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    m_vec3_CameraVelocity += vec3_Forward * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void GameState::AccelerateCameraBackward(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    m_vec3_CameraVelocity += -vec3_Forward * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void GameState::AccelerateCameraLeft(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    glm::vec3 vec3_Left = glm::normalize(glm::cross(glm::vec3(0.0f, 1.0f, 0.0f), vec3_Forward));
    m_vec3_CameraVelocity += vec3_Left * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void GameState::AccelerateCameraRight(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    glm::vec3 vec3_Forward = glm::normalize(glm::vec3(m_vec3_CameraFront.x, 0.0f, m_vec3_CameraFront.z));
    glm::vec3 vec3_Right = glm::normalize(glm::cross(vec3_Forward, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_vec3_CameraVelocity += vec3_Right * k_CameraAcceleration * f_DeltaTime;

    float f_Speed = glm::length(m_vec3_CameraVelocity);
    if (f_Speed > k_MaxCameraSpeed) {
        m_vec3_CameraVelocity = glm::normalize(m_vec3_CameraVelocity) * k_MaxCameraSpeed;
    }
}

void GameState::UpdateCameraPhysics(float f_DeltaTime) {
    if (!m_b_Camera3D) return;

    //friction
    m_vec3_CameraVelocity *= k_CameraFriction;
    if (glm::length(m_vec3_CameraVelocity) < 0.01f) {
        m_vec3_CameraVelocity = glm::vec3(0.0f);
    }

    // update position
    float f_OriginalY = m_vec3_Saved3DCameraPosition.y;
    m_vec3_Saved3DCameraPosition += m_vec3_CameraVelocity * f_DeltaTime;
    m_vec3_Saved3DCameraPosition.y = f_OriginalY;
}

void GameState::UpdateArrowsForSelectedPlayer() {
    m_vec_CurrentArrows.clear();

    if (m_i_SelectedPlayerIndex < 0 || m_i_SelectedPlayerIndex >= static_cast<int>(m_vec_Players.size())) {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mtx_Players);
    const auto& player = m_vec_Players[m_i_SelectedPlayerIndex];
    int i_CurrentNode = player.GetOccupiedNode();

    auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                          [i_CurrentNode](const StationCircle& sc){ return sc.stationID == i_CurrentNode; });
    if (it == m_vec_CircleStations.end()) return;

    glm::vec2 vec2_CurrentPos = it->position;

    auto connections = m_graph.GetConnections(i_CurrentNode);

    for (const auto& conn : connections) {
        auto destIt = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                                  [&conn](const StationCircle& sc){ return sc.stationID == conn.i_NodeId; });
        if (destIt == m_vec_CircleStations.end()) continue;

        glm::vec2 vec2_DestPos = destIt->position;

        glm::vec2 vec2_Direction = glm::normalize(vec2_DestPos - vec2_CurrentPos);

        float f_OrbitalRadius = UI::k_TaxiWaterOrbitalRadius;
        f_OrbitalRadius = f_OrbitalRadius * m_f_GlobalScale;
        if (conn.i_TransportType == Core::k_TransportTypeBus) {
            f_OrbitalRadius = UI::k_BusOrbitalRadius;
            f_OrbitalRadius = f_OrbitalRadius * m_f_GlobalScale;
        } else if (conn.i_TransportType == Core::k_TransportTypeMetro) {
            f_OrbitalRadius = UI::k_MetroOrbitalRadius;
            f_OrbitalRadius = f_OrbitalRadius * m_f_GlobalScale;
        }

        glm::vec2 vec2_ArrowPos = vec2_CurrentPos + vec2_Direction * f_OrbitalRadius;

        float f_Rotation = atan2(-vec2_Direction.y, vec2_Direction.x);
        DirectionArrow arrow;
        
        arrow.vec2_Position = vec2_ArrowPos;
        arrow.f_Rotation = f_Rotation;
        arrow.i_DestinationNode = conn.i_NodeId;
        arrow.i_TransportType = conn.i_TransportType;

        m_vec_CurrentArrows.push_back(arrow);
    }
}

glm::vec3 GameState::IDToColor(uint32_t ui_ID) const {
    float f_R = ((ui_ID & 0x000000FF) >> 0) / 255.0f;
    float f_G = ((ui_ID & 0x0000FF00) >> 8) / 255.0f;
    float f_B = ((ui_ID & 0x00FF0000) >> 16) / 255.0f;
    return glm::vec3(f_R, f_G, f_B);
}

uint32_t GameState::ColorToID(unsigned char r, unsigned char g, unsigned char b) const {
    return (uint32_t(r) << 0) | (uint32_t(g) << 8) | (uint32_t(b) << 16);
}

uint32_t GameState::RegisterClickable(ClickableType e_Type, int i_Index, int i_Data) {
    uint32_t ui_ID = ++m_ui_NextPickingID;
    ClickableID clickable;
    clickable.e_Type = e_Type;
    clickable.i_Index = i_Index;
    clickable.i_Data = i_Data;
    m_map_PickingIDToClickable[ui_ID] = clickable;
    return ui_ID;
}

std::vector<float> GameState::generateArrowVertices() {
    std::vector<float> verts = {
        UI::k_ArrowLength, 0.0f, 0.0f,
        0.0f, 0.0f, -UI::k_ArrowWidth,
        0.0f, 0.0f, UI::k_ArrowWidth
    };

    return verts;
}

void GameState::HandlePlayerClick(int i_PlayerIndex) {
    if (i_PlayerIndex < 0 || i_PlayerIndex >= static_cast<int>(m_vec_Players.size())) {
        return;
    }

    bool b_CanSelect = false;
    {
        std::lock_guard<std::mutex> lock(m_mtx_Players);
        const auto& player = m_vec_Players[i_PlayerIndex];

        bool b_IsMrX = (player.GetType() == Core::PlayerType::MisterX);
        bool b_MrXActive = false;

        for (const auto& p : m_vec_Players) {
            if (p.GetType() == Core::PlayerType::MisterX && p.IsActive()) {
                b_MrXActive = true;
                break;
            }
        }

        if (b_MrXActive && !b_IsMrX) {
            std::cout << "[GameState] Mr X must move first this round.\n";
            return;
        }

        if (!b_MrXActive && b_IsMrX) {
            std::cout << "[GameState] Mr X has already moved this round.\n";
            return;
        }

        b_CanSelect = true;
    }

    {
        std::lock_guard<std::mutex> lockState(m_mtx_GameState);
        if (i_PlayerIndex < static_cast<int>(m_vec_MovedThisRound.size()) && m_vec_MovedThisRound[i_PlayerIndex]) {
            std::cout << "[GameState] Player has already moved this round.\n";
            return;
        }
    }

    if (b_CanSelect) {
        m_i_SelectedPlayerIndex = i_PlayerIndex;
        UpdateArrowsForSelectedPlayer();
        std::cout << "[GameState] Selected player " << i_PlayerIndex << "\n";
    }
}

void GameState::HandleArrowClick(int i_PlayerIndex, int i_DestinationNode) {
    if (i_PlayerIndex != m_i_SelectedPlayerIndex) {
        return;
    }

    auto arrowIt = std::find_if(m_vec_CurrentArrows.begin(), m_vec_CurrentArrows.end(),
                                [i_DestinationNode](const DirectionArrow& a) {
                                    return a.i_DestinationNode == i_DestinationNode;
                                });
    if (arrowIt == m_vec_CurrentArrows.end()) {
        return;
    }

    int i_TransportType = arrowIt->i_TransportType;
    bool b_MoveSuccessful = false;

    {
        std::lock_guard<std::mutex> lock(m_mtx_Players);
        auto& player = m_vec_Players[i_PlayerIndex];

        bool b_TicketAvailable = true;
        if (i_TransportType == Core::k_TransportTypeTaxi) {
            b_TicketAvailable = player.SpendTaxiTicket();
        } else if (i_TransportType == Core::k_TransportTypeBus) {
            b_TicketAvailable = player.SpendBusTicket();
        } else if (i_TransportType == Core::k_TransportTypeMetro) {
            b_TicketAvailable = player.SpendMetroTicket();
        } else if (i_TransportType == Core::k_TransportTypeWater) {
            b_TicketAvailable = player.SpendWaterTicket();
        }

        if (!b_TicketAvailable) {
            std::cout << "[GameState] No tickets available for this transport type.\n";
            return;
        }

        player.MoveTo(i_DestinationNode);
        b_MoveSuccessful = true;
    }

    if (b_MoveSuccessful) {
        std::cout << "[GameState] Player " << i_PlayerIndex << " moved to node " << i_DestinationNode << "\n";

        m_i_SelectedPlayerIndex = -1;
        m_vec_CurrentArrows.clear();

        bool b_Captured = false;
        {
            std::lock_guard<std::mutex> lock(m_mtx_Players);
            b_Captured = CheckCapture();
        }
        if (b_Captured) {
            CheckEndOfGame(Winner::Detectives);
            return;
        }

        {
            std::lock_guard<std::mutex> lock(m_mtx_Players);
            auto& ref_Player = m_vec_Players[i_PlayerIndex];
            if (ref_Player.GetType() == Core::PlayerType::MisterX) {
                UI::TicketMark mark = UI::TicketMark::None;
                if (i_TransportType == Core::k_TransportTypeTaxi) mark = UI::TicketMark::Taxi;
                else if (i_TransportType == Core::k_TransportTypeBus) mark = UI::TicketMark::Bus;
                else if (i_TransportType == Core::k_TransportTypeMetro) mark = UI::TicketMark::Metro;
                else if (i_TransportType == Core::k_TransportTypeWater) mark = UI::TicketMark::Water;
                UI::SetSlotMark(m_i_Round.load(), mark, true);
                ref_Player.SetActive(false);

                // After Mr. X moves, activate all detectives
                for (auto& p : m_vec_Players) {
                    if (p.GetType() == Core::PlayerType::Detective) {
                        p.SetActive(true);
                    }
                }
            } else {
                // Detective moved - deactivate them
                ref_Player.SetActive(false);
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mtx_GameState);
            if (!m_vec_MovedThisRound[i_PlayerIndex]) {
                m_vec_MovedThisRound[i_PlayerIndex] = true;
                int i_Remaining = m_i_PlayersRemainingThisRound.load();
                if (i_Remaining > 0) {
                    m_i_PlayersRemainingThisRound.store(i_Remaining - 1);
                }
            }

            if (m_i_PlayersRemainingThisRound.load() == 0) {
                int i_CurrentRound = m_i_Round.load();
                if (i_CurrentRound >= Core::k_MaxRounds) {
                    CheckEndOfGame();
                } else {
                    m_i_Round.store(i_CurrentRound + 1);
                    std::fill(m_vec_MovedThisRound.begin(), m_vec_MovedThisRound.end(), false);
                    m_i_PlayersRemainingThisRound.store(static_cast<int>(m_vec_Players.size()));
                    UI::SetRound(m_i_Round.load());

                    {
                        std::lock_guard<std::mutex> lockPlayers(m_mtx_Players);
                        for (auto& p : m_vec_Players) {
                            if (p.GetType() == Core::PlayerType::MisterX) p.SetActive(true);
                            else p.SetActive(false);
                        }
                    }
                }
            }
        }
    }
}

void GameState::HandleColorPicking(int i_MouseX, int i_MouseY) {
    m_map_PickingIDToClickable.clear();
    m_ui_NextPickingID = 0;

    glm::mat4 view, projection;
    if (m_b_Camera3D) {
        glm::vec3 vec3_CameraTarget = m_vec3_CameraPosition + m_vec3_CameraFront;
        view = glm::lookAt(m_vec3_CameraPosition, vec3_CameraTarget, m_vec3_CameraUp);
        projection = glm::perspective(glm::radians(45.0f), (float)m_i_Width / (float)m_i_Height, 0.1f, 100.0f);
    } else {
        view = glm::lookAt(glm::vec3(0.0f, 10.0f, 0.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
        float f_Aspect = (float)m_i_Width / (float)m_i_Height;
        float f_HalfHeight = 1.1f;
        float f_HalfWidth = f_HalfHeight * f_Aspect;
        projection = glm::ortho(-f_HalfWidth, f_HalfWidth, -f_HalfHeight, f_HalfHeight, 0.1f, 20.0f);
    }

    RenderPickingPass(projection, view);
    ApplyDilationPass();

    int i_PickY = m_i_Height - i_MouseY;
    unsigned char pixel[3];
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_PickingDilated);
    glReadPixels(i_MouseX, i_PickY, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, pixel);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    uint32_t ui_ClickedID = ColorToID(pixel[0], pixel[1], pixel[2]);

    if (ui_ClickedID == 0) {
        m_i_SelectedPlayerIndex = -1;
        m_vec_CurrentArrows.clear();
        return;
    }

    auto it = m_map_PickingIDToClickable.find(ui_ClickedID);
    if (it == m_map_PickingIDToClickable.end()) {
        return;
    }

    const ClickableID& clickable = it->second;

    if (clickable.e_Type == ClickableType::Detective || clickable.e_Type == ClickableType::MisterX) {
        HandlePlayerClick(clickable.i_Index);
    } else if (clickable.e_Type == ClickableType::Arrow) {
        HandleArrowClick(clickable.i_Index, clickable.i_Data);
    }
}

void GameState::ApplyDilationPass() {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_PickingDilated);
    glViewport(0, 0, m_i_Width, m_i_Height);

    glDisable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(m_ShaderProgram_Dilation);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID_Picking);

    GLint idBufferLoc = glGetUniformLocation(m_ShaderProgram_Dilation, "idBuffer");
    GLint texelSizeLoc = glGetUniformLocation(m_ShaderProgram_Dilation, "texelSize");

    glUniform1i(idBufferLoc, 0);
    glUniform2f(texelSizeLoc, 1.0f / m_i_Width, 1.0f / m_i_Height);

    glBindVertexArray(m_VAO_FullscreenQuad);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GameState::RenderPickingPass(const glm::mat4& mat4_Projection, const glm::mat4& mat4_View) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_FBO_Picking);
    glViewport(0, 0, m_i_Width, m_i_Height);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_ShaderProgram_Picking);
    GLint i_MvpLoc = glGetUniformLocation(m_ShaderProgram_Picking, "MVP");
    GLint i_ColorLoc = glGetUniformLocation(m_ShaderProgram_Picking, "pickingColor");

    {
        std::lock_guard<std::mutex> lock(m_mtx_Players);
        for (size_t i = 0; i < m_vec_Players.size(); ++i) {
            const auto& player = m_vec_Players[i];
            int i_NodeId = player.GetOccupiedNode();

            auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                                  [i_NodeId](const StationCircle& sc){ return sc.stationID == i_NodeId; });
            if (it == m_vec_CircleStations.end()) continue;

            ClickableType e_Type = (player.GetType() == Core::PlayerType::MisterX)
                                   ? ClickableType::MisterX : ClickableType::Detective;
            uint32_t ui_ID = RegisterClickable(e_Type, static_cast<int>(i), 0);
            glm::vec3 vec3_PickingColor = IDToColor(ui_ID);

            glm::mat4 mat4_Model = glm::mat4(1.0f);
            mat4_Model = glm::translate(mat4_Model, glm::vec3(it->position.x, 0.05f * m_f_GlobalScale, it->position.y));
            mat4_Model = glm::scale(mat4_Model, glm::vec3(2.0f));
            mat4_Model = mat4_Model * m_mat4_GlobalScaleMatrix;

            glUniform3fv(i_ColorLoc, 1, glm::value_ptr(vec3_PickingColor));

            glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
            glUniformMatrix4fv(i_MvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
            glBindVertexArray(m_VAO_Cylinder);
            glDrawArrays(GL_TRIANGLES, 0, m_i_CylinderVertexCount);

            glm::mat4 mat4_HemiModel = glm::translate(mat4_Model, glm::vec3(0.0f, 0.1f, 0.0f));
            mat4_MVP = mat4_Projection * mat4_View * mat4_HemiModel;
            glUniformMatrix4fv(i_MvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
            glBindVertexArray(m_VAO_Hemisphere);
            glDrawArrays(GL_TRIANGLES, 0, m_i_HemisphereVertexCount);
        }
    }

    for (const auto& arrow : m_vec_CurrentArrows) {
        uint32_t ui_ID = RegisterClickable(ClickableType::Arrow, m_i_SelectedPlayerIndex, arrow.i_DestinationNode);
        glm::vec3 vec3_PickingColor = IDToColor(ui_ID);

        glm::mat4 mat4_Model = glm::mat4(1.0f);
        mat4_Model = glm::translate(mat4_Model, glm::vec3(arrow.vec2_Position.x, 0.1f * m_f_GlobalScale, arrow.vec2_Position.y));
        mat4_Model = glm::rotate(mat4_Model, arrow.f_Rotation, glm::vec3(0.0f, 1.0f, 0.0f));
        mat4_Model = mat4_Model * m_mat4_GlobalScaleMatrix;

        glm::mat4 mat4_MVP = mat4_Projection * mat4_View * mat4_Model;
        glUniformMatrix4fv(i_MvpLoc, 1, GL_FALSE, glm::value_ptr(mat4_MVP));
        glUniform3fv(i_ColorLoc, 1, glm::value_ptr(vec3_PickingColor));

        glBindVertexArray(m_VAO_Arrow);
        glDrawArrays(GL_TRIANGLES, 0, m_i_ArrowVertexCount);
    }

    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Player Controller
void GameState::InitializePlayerControllers() {
    using namespace ScotlandYard::Core;
    const auto& S = Settings();

    m_vec_PlayerControllers.clear();
    m_vec_PlayerControllers.reserve(m_vec_Players.size());

    auto makeAI = [&](AIAlgorithm algo) {
        auto u = std::make_unique<Core::AIPlayerController>(S.f_AITurnDelay);
        u->SetAlgorithm(algo);
        return u;
        };

    for (size_t i = 0; i < m_vec_Players.size(); ++i) {
        const bool isMrX = (m_vec_Players[i].GetType() == Core::PlayerType::MisterX);
        switch (S.e_Mode) {
        case GameMode::PvP: {
            m_vec_PlayerControllers.push_back(std::make_unique<Core::HumanPlayerController>());
            break;
        }
        case GameMode::PvBot: {
            if (S.e_PvBotHuman == HumanSide::MrX) {
                if (isMrX) m_vec_PlayerControllers.push_back(std::make_unique<Core::HumanPlayerController>());
                else       m_vec_PlayerControllers.push_back(makeAI(S.e_AIDetectives));
            }
            else { // Human = Detectives
                if (isMrX) m_vec_PlayerControllers.push_back(makeAI(S.e_AIMisterX));
                else       m_vec_PlayerControllers.push_back(std::make_unique<Core::HumanPlayerController>());
            }
            break;
        }

        case GameMode::BotvBot: {
            auto algo = isMrX ? S.e_AIMisterX : S.e_AIDetectives;
            m_vec_PlayerControllers.push_back(makeAI(algo));
            break;
        }
        }
    }
}

void GameState::UpdateAIPlayers(Core::Application* p_App, float f_DeltaTime) {
    for (auto& p_Controller : m_vec_PlayerControllers) {
        if (p_Controller && p_Controller->IsAIControlled()) {
            p_Controller->Update(f_DeltaTime);
        }
    }

    // check if move
    std::lock_guard<std::mutex> lock(m_mtx_Players);

    for (size_t i = 0; i < m_vec_Players.size(); ++i) {
        if (i >= m_vec_PlayerControllers.size()) continue;

        auto& p_Controller = m_vec_PlayerControllers[i];
        if (!p_Controller || !p_Controller->IsAIControlled()) continue;

        const auto& player = m_vec_Players[i];

        auto* p_AIController = dynamic_cast<Core::AIPlayerController*>(p_Controller.get());
        if (player.IsActive() &&
            p_AIController &&
            !p_AIController->IsMoveRequested() &&
            i < m_vec_MovedThisRound.size() &&
            !m_vec_MovedThisRound[i]) {

            // Get possible move list
            std::vector<Core::PossibleMove> vec_PossibleMoves = GetPossibleMovesForPlayer(i);

            if (!vec_PossibleMoves.empty()) {
                Core::GameStateData gameState;
                gameState.i_CurrentPlayerIndex = static_cast<int>(i);
                gameState.i_CurrentRound = m_i_Round.load();
                gameState.b_IsRevealRound = Core::IsRevealRound(gameState.i_CurrentRound);
                gameState.b_IsNextRevealRound = Core::IsRevealRound(gameState.i_CurrentRound + 1);
                gameState.p_Graph = &m_graph;

                int i_MrXIndex = -1;
                int i_MrXPosition = -1;
                for (size_t j = 0; j < m_vec_Players.size(); ++j) {
                    const auto& p = m_vec_Players[j];
                    Core::PlayerInfo info;

                    info.i_Position = p.GetOccupiedNode();
                    info.b_IsMisterX = (p.GetType() == Core::PlayerType::MisterX);
                    info.i_TaxiTickets = p.GetTaxiTickets();
                    info.i_BusTickets = p.GetBusTickets();
                    info.i_MetroTickets = p.GetMetroTickets();
                    info.i_BlackTickets = p.GetBlackTickets();
                    info.i_DoubleMoveTickets = p.GetDoubleMoveTickets();

                    if (info.b_IsMisterX) {
                        i_MrXIndex = static_cast<int>(j);
                        i_MrXPosition = info.i_Position;
                        info.b_IsVisible = gameState.b_IsRevealRound || p.IsVisible();
                    } else {
                        info.b_IsVisible = true;
                    }

                    gameState.vec_AllPlayers.push_back(info);
                }

                // Mr X last known position
                if (i_MrXIndex != -1) {
                    if (gameState.b_IsRevealRound) {
                        gameState.i_MrXLastKnownPosition = i_MrXPosition;
                        gameState.i_MrXLastKnownRound = gameState.i_CurrentRound;
                    } else {
                        gameState.i_MrXLastKnownPosition = -1;
                        gameState.i_MrXLastKnownRound = -1;
                        for (int r = 0; r < Core::k_RevealRoundsCount; ++r) {
                            if (Core::k_RevealRounds[r] < gameState.i_CurrentRound) {
                                gameState.i_MrXLastKnownRound = Core::k_RevealRounds[r];
                                gameState.i_MrXLastKnownPosition = -1;
                            }
                        }
                    }
                }
                p_Controller->RequestMove(&player, vec_PossibleMoves, gameState, p_App);
            }
        }
    }
}

void GameState::ProcessAIPendingMoves() {
    std::unique_lock<std::mutex> lock(m_mtx_Players);

    for (size_t i = 0; i < m_vec_PlayerControllers.size(); ++i) {
        auto& p_Controller = m_vec_PlayerControllers[i];
        if (!p_Controller || !p_Controller->IsAIControlled()) continue;

        if (p_Controller->HasPendingMove()) {
            Core::MoveDecision decision = p_Controller->GetMove();

            if (decision.b_HasDecision) {
                // Select the AI player (required for HandleArrowClick), this function needs to be generalized
                m_i_SelectedPlayerIndex = static_cast<int>(i);

                lock.unlock();
                UpdateArrowsForSelectedPlayer();
                HandleArrowClick(static_cast<int>(i), decision.i_DestinationNode);

                lock.lock();
            }
        }
    }
}

std::vector<Core::PossibleMove> GameState::GetPossibleMovesForPlayer(int i_PlayerIndex) {
    std::vector<Core::PossibleMove> vec_Result;
    if (i_PlayerIndex < 0 || i_PlayerIndex >= static_cast<int>(m_vec_Players.size())) {
        return vec_Result;
    }

    const auto& player = m_vec_Players[i_PlayerIndex];
    int i_CurrentNode = player.GetOccupiedNode();

    auto connections = m_graph.GetConnections(i_CurrentNode);
    for (const auto& conn : connections) {
        Core::PossibleMove move;
        move.i_DestinationNode = conn.i_NodeId;
        move.i_TransportType = conn.i_TransportType;
        vec_Result.push_back(move);
    }

    return vec_Result;
}

} // namespace States
} // namespace ScotlandYard