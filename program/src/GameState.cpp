#include "GameState.h"
#include "Application.h"
#include <GL/glew.h>

#include <random>
#include <algorithm>
#include "../../Graphs/graph_manage.h"

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include "HUDOverlay.h"

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
{
}

GameState::~GameState() {}

void GameState::OnEnter() {
    m_b_GameActive = true;

    // ==============================
    // Dane wierzchołków planszy (pozycja, kolor, UV)
    // ==============================
    float size = 1.0f;
    float planeVertices[] = {
        -size, 0.0f, -size,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         size, 0.0f, -size,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         size, 0.0f,  size,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,

        -size, 0.0f, -size,   0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         size, 0.0f,  size,   0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
        -size, 0.0f,  size,   0.0f, 1.0f, 0.0f,   0.0f, 1.0f
    };

    // ==============================
    // Pozycje kółek z pliku CSV
    // ==============================
    m_vec_CircleStations = LoadCirclePositionsFromCSV("../../Graphs/nodes_with_station.csv");

    if (m_vec_CircleStations.empty()) {
        std::cerr << "[GameState] Warning: No positions loaded from CSV, using defaults.\n";

        m_vec_CircleStations = {
            { glm::vec2(-0.9f, -0.9f), {"taxi"} },
            { glm::vec2(-0.5f, -0.9f), {"bus"} },
            { glm::vec2( 0.0f, -0.9f), {"metro"} },
            { glm::vec2( 0.5f, -0.9f), {"bus", "metro"} },
            { glm::vec2( 0.9f, -0.9f), {"taxi", "bus", "metro"} }
        };
    }

    // ==============================
    // VAO/VBO planszy
    // ==============================
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

    // ==============================
    // VAO/VBO kółek
    // ==============================
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

   // ==============================
    // VAO/VBO cylindra i półkuli pionka
    // ==============================
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

    // ==============================
    // Shadery planszy
    // ==============================
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

    // ==============================
    // Shadery kółek
    // ==============================
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

    UI::SetCameraToggleCallback([this]() {
        m_b_Camera3D = !m_b_Camera3D;
        });

    glEnable(GL_DEPTH_TEST);

    // ==============================
    // Initialize players from graph data (random distinct nodes)
    // ==============================
    m_vec_Players.clear();

    const std::string s_PosFile = "../../Graphs/nodes_with_station.csv";
    const std::string s_ConFile = "../../Graphs/polaczenia.csv";
    GraphManager gm(200);
    gm.LoadData(s_PosFile, s_ConFile, false); // suppress CSV parsing prints

    int i_NodeCount = gm.GetNodeCount();
    if (i_NodeCount <= 0) {
        // fallback to simple hardcoded values if graph failed to load
        m_vec_Players.emplace_back(Core::PlayerType::MisterX, 10);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 1);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 2);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 3);
        m_vec_Players.emplace_back(Core::PlayerType::Detective, 4);
    } else {
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

        const int k_DetectiveCount = 4;
        for (int i = 0; i < k_DetectiveCount; ++i) {
            int i_DNode = pick_unique(vec_Used);
            m_vec_Players.emplace_back(Core::PlayerType::Detective, i_DNode);
        }
    }

    // Print player positions to console
    for (const auto& player : m_vec_Players) {
        printf("Player: %s\n", player.ToString().c_str());
    }

    // --- Console interaction in background thread: allow moving a player to a connected node ---
    const std::string s_PosFileRel = "../../Graphs/nodes_with_station.csv";
    const std::string s_ConFileRel = "../../Graphs/polaczenia.csv";
    m_graph.LoadData(s_PosFileRel, s_ConFileRel, false);

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
            try { idx = std::stoi(sel); } catch(...) { std::cout << "[Console] Invalid selection\n"; continue; }

            int curNode = -1;
            {
                std::lock_guard<std::mutex> lock(m_mtx_Players);
                if (idx < 0 || idx >= static_cast<int>(m_vec_Players.size())) { std::cout << "[Console] Index out of range\n"; continue; }
                curNode = m_vec_Players[idx].GetOccupiedNode();
            }

            std::cout << "[Console] Player " << idx << " is at node " << curNode << "\n";
            auto conns = m_graph.GetConnections(curNode);
            if (conns.empty()) { std::cout << "[Console] No connections from this node.\n"; continue; }

            std::cout << "[Console] Available moves:\n";
            for (size_t i = 0; i < conns.size(); ++i) {
                std::string tname = (conns[i].i_TransportType == 1 ? "taxi" : (conns[i].i_TransportType==2?"bus":(conns[i].i_TransportType==3?"metro":"water")));
                std::cout << i << ": to node " << conns[i].i_NodeId << " via " << tname << "\n";
            }

            std::cout << "[Console] Choose move index: ";
            std::string msel;
            if (!std::getline(std::cin, msel)) { m_b_ConsoleThreadRunning.store(false); break; }
            if (msel.empty()) { std::cout << "[Console] Empty selection\n"; continue; }
            int midx = -1; try { midx = std::stoi(msel); } catch(...) { std::cout << "[Console] Invalid move index\n"; continue; }
            if (midx < 0 || midx >= static_cast<int>(conns.size())) { std::cout << "[Console] Move index out of range\n"; continue; }

            int dest = conns[midx].i_NodeId;
            int transport = conns[midx].i_TransportType;
            bool moved = false;
            {
                std::lock_guard<std::mutex> lock(m_mtx_Players);
                auto& player = m_vec_Players[idx];
                bool ok = true;
                if (transport == 1) { // taxi
                    ok = player.SpendTaxiTicket();
                } else if (transport == 2) { // bus
                    ok = player.SpendBusTicket();
                } else if (transport == 3) { // metro
                    ok = player.SpendMetroTicket();
                } else if (transport == 4) { // water
                    ok = player.SpendWaterTicket();
                }

                if (!ok) {
                    std::cout << "[Console] Player " << idx << " does not have required ticket for this transport.\n";
                } else {
                    player.MoveTo(dest);
                    moved = true;
                }
            }
            if (moved) std::cout << "[Console] Moved player " << idx << " to node " << dest << "\n";
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
    if (m_ShaderProgram_Plane) {
        glDeleteProgram(m_ShaderProgram_Plane);
        m_ShaderProgram_Plane = 0;
    }
    if (m_ShaderProgram_Circle) {
        glDeleteProgram(m_ShaderProgram_Circle);
        m_ShaderProgram_Circle = 0;
    }
    if (m_TextureID) {
        glDeleteTextures(1, &m_TextureID);
        m_TextureID = 0;
    }
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
}

void GameState::Render(Core::Application* p_App) {
    LoadTextures(p_App);

    m_i_Width = p_App->GetWidth();
    m_i_Height = p_App->GetHeight();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(m_ShaderProgram_Plane);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(m_f_Rotation), glm::vec3(0.0f, 1.0f, 0.0f));

    glm::mat4 view, projection;

    if (m_b_Camera3D) {
        m_vec3_CameraPosition = m_vec3_Saved3DCameraPosition;
        glm::vec3 vec3_CameraTarget = m_vec3_CameraPosition + m_vec3_CameraFront;
        view = glm::lookAt(m_vec3_CameraPosition, vec3_CameraTarget, m_vec3_CameraUp);
        projection = glm::perspective(glm::radians(45.0f), (float)m_i_Width / (float)m_i_Height, 0.1f, 100.0f);
    }
    else {
        m_vec3_CameraVelocity = glm::vec3(0.0f, 0.0f, 0.0f);

        view = glm::lookAt(
            glm::vec3(0.0f, 10.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, -1.0f)
        );

        float f_Aspect = (float)m_i_Width / (float)m_i_Height;
        float f_HalfHeight = 1.1f;
        float f_HalfWidth = f_HalfHeight * f_Aspect;

        projection = glm::ortho(
            -f_HalfWidth, f_HalfWidth,
            -f_HalfHeight, f_HalfHeight,
            0.1f, 20.0f
        );
    }

    glm::mat4 MVP = projection * view * model;

    GLuint mvpLocPlane = glGetUniformLocation(m_ShaderProgram_Plane, "MVP");
    glUniformMatrix4fv(mvpLocPlane, 1, GL_FALSE, glm::value_ptr(MVP));

    // Ustawienie tekstury
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_TextureID);
    GLuint texLoc = glGetUniformLocation(m_ShaderProgram_Plane, "ourTexture");
    glUniform1i(texLoc, 0);

    // Rysowanie planszy
    glBindVertexArray(m_VAO_Plane);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    // Rysowanie wielokolorowych kółek
    glUseProgram(m_ShaderProgram_Circle);
    GLuint mvpLoc = glGetUniformLocation(m_ShaderProgram_Circle, "MVP");
    GLuint colorLoc = glGetUniformLocation(m_ShaderProgram_Circle, "circleColor");

    float baseScale = 0.5f;      // White circle base scale
    float ringStep = 0.1f;       // Step increase for each transport type
    float yStep = 0.005f;         // Vertical offset step to prevent z-fighting

    for (const auto& station : m_vec_CircleStations)
    {
        glm::mat4 modelBase = glm::mat4(1.0f);
        modelBase = glm::translate(modelBase, glm::vec3(station.position.x, 0.0f, station.position.y));

        // Twarda kolejność typów transportu od dołu do góry
        std::vector<std::string> order = { "metro", "bus", "taxi", "water" };

        // Filtrujemy tylko typy, które są w stacji
        std::vector<std::string> typesPresent;
        for (const auto& t : order)
            if (std::find(station.transportTypes.begin(), station.transportTypes.end(), t) != station.transportTypes.end())
                typesPresent.push_back(t);

        int count = static_cast<int>(typesPresent.size());
        for (int i = 0; i < count; ++i)
        {
            const std::string& type = typesPresent[i];
            glm::vec3 color;

            if (type == "metro") color = glm::vec3(1.0f, 0.0f, 0.0f);      // czerwony
            else if (type == "bus") color = glm::vec3(0.0f, 1.0f, 0.0f);   // zielony
            else if (type == "taxi") color = glm::vec3(1.0f, 1.0f, 0.0f);  // żółty
            else color = glm::vec3(0.0f, 0.4f, 1.0f);                      // woda/łódź

            // Odwrócone skalowanie: największe na dole
            float scale = baseScale + (count - i) * ringStep;
            float yOffset = 0.01f + i * yStep;

            glm::mat4 model = glm::translate(modelBase, glm::vec3(0.0f, yOffset, 0.0f));
            model = glm::scale(model, glm::vec3(scale));
            glm::mat4 MVP = projection * view * model;

            glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));
            glUniform3fv(colorLoc, 1, glm::value_ptr(color));

            glBindVertexArray(m_VAO_Circle);
            glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
            glBindVertexArray(0);
        }

        glm::mat4 model = glm::translate(modelBase, glm::vec3(0.0f, 0.01f + count * yStep, 0.0f));
        model = glm::scale(model, glm::vec3(baseScale));
        glm::mat4 MVP = projection * view * model;

        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));
        glUniform3fv(colorLoc, 1, glm::value_ptr(glm::vec3(1.0f, 1.0f, 1.0f)));

        glBindVertexArray(m_VAO_Circle);
        glDrawArrays(GL_TRIANGLE_FAN, 0, m_i_CircleVertexCount);
        glBindVertexArray(0);
    }

    for (const auto& player : m_vec_Players)
    {
        if (player.GetType() != Core::PlayerType::Detective) continue;

        int nodeId = player.GetOccupiedNode();
        auto it = std::find_if(m_vec_CircleStations.begin(), m_vec_CircleStations.end(),
                            [nodeId](const StationCircle& sc){ return sc.stationID == nodeId; });
        if (it == m_vec_CircleStations.end()) continue;

        glm::vec2 pos = it->position;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(pos.x, 0.01f, pos.y));
        model = glm::scale(model, glm::vec3(0.4f));
        
        // Kolor detektywa (np. niebieski)
        glm::vec3 color = glm::vec3(0.0f, 0.0f, 1.0f);
        glUniform3fv(colorLoc, 1, glm::value_ptr(color));
        
        // Body
        glm::mat4 cylModel = glm::scale(model,glm::vec3(1.0f));
        glm::mat4 MVP = projection * view * cylModel;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

        glBindVertexArray(m_VAO_Cylinder);
        glDrawArrays(GL_TRIANGLES, 0, m_i_CylinderVertexCount);
        glBindVertexArray(0);

        // Top
        glm::mat4 hemiModel = glm::translate(model, glm::vec3(0.0f, 0.1f, 0.0f)); // przesunięcie na górę cylindra
        MVP = projection * view * hemiModel;
        glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, glm::value_ptr(MVP));

        glBindVertexArray(m_VAO_Hemisphere);
        glDrawArrays(GL_TRIANGLES, 0, m_i_HemisphereVertexCount);
        glBindVertexArray(0);
    }

    
    std::vector<std::string> labels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };

    // counters for Black and 2x tickets for Mr X
    int black = -1, dbl = -1;
    for (const auto& pl : m_vec_Players) {
        if (pl.GetType() == ScotlandYard::Core::PlayerType::MisterX) {
            black = pl.GetBlackTickets(); // 5 for Mr X
            dbl = pl.GetDoubleMoveTickets(); // 2 for Mr X
            break;
        }
    }
    std::vector<int> counts = { -1, black, dbl, -1, -1, -1 };

    // to HUD
    ScotlandYard::UI::SetTopBar(labels, {}, counts);
    ScotlandYard::UI::SetRound(1);
    ScotlandYard::UI::RenderHUD(p_App);


    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

void GameState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_ESCAPE:
                // Can set exit flag
                break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN) {
        int i_X = event.button.x;
        int i_Y = event.button.y;

        UI::HandleMouseClick(i_X, i_Y);
    }

    if (event.type == SDL_MOUSEWHEEL && m_b_Camera3D) {
        float f_ScrollInput = event.wheel.y * k_CameraScrollAcceleration;
        m_f_CameraAngleVelocity -= f_ScrollInput;
    }

}

std::vector<GameState::StationCircle> GameState::LoadCirclePositionsFromCSV(const std::string& filePath)
{
    std::vector<StationCircle> stations;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[GameState] Could not open CSV file: " << filePath << std::endl;
        return stations;
    }

    std::string line;
    bool b_FirstLine = true;
    const float k_MapSize = 13.0f;
    const float k_HalfMap = k_MapSize / 2.0f;

    while (std::getline(file, line)) {
        if (line.empty()) continue;

        if (b_FirstLine) {
            b_FirstLine = false;
            continue;
        }

        std::stringstream ss(line);
        std::string idStr, xStr, yStr, typeStr;

        if (!std::getline(ss, idStr, ',')) continue;
        if (!std::getline(ss, xStr, ',')) continue;
        if (!std::getline(ss, yStr, ',')) continue;
        if (!std::getline(ss, typeStr, ',')) continue;

        try {
            StationCircle circle;

            float x = std::stof(xStr);
            float y = std::stof(yStr);
            circle.position = glm::vec2(x, y);

            // float normX = (x - k_HalfMap) / k_HalfMap;
            // float normY = (y - k_HalfMap) / k_HalfMap;
            // circle.position = glm::vec2(normX, -normY);

            circle.stationID = std::stoi(idStr);

            // rozbij typy transportu po podkreśleniach
            std::stringstream ssType(typeStr);
            std::string transport;
            while (std::getline(ssType, transport, '_')) {
                circle.transportTypes.push_back(transport);
            }

            stations.push_back(circle);
        }
        catch (...) {
            std::cerr << "[GameState] Invalid CSV line: " << line << std::endl;
        }
    }

    file.close();
    std::cout << "[GameState] Loaded " << stations.size() << " station circles from CSV.\n";
    return stations;
}

std::vector<float> GameState::generateCircleVertices(float f_Radius, int i_Segments) {
    std::vector<float> vec_Vertices;

    vec_Vertices.push_back(0.0f);
    vec_Vertices.push_back(0.01f);
    vec_Vertices.push_back(0.0f);

    constexpr float k_Pi = 3.14159265358979323846f;
    for (int i = 0; i <= i_Segments; i++) {
        float f_Theta = 2.0f * k_Pi * i / i_Segments;
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
    constexpr float PI = 3.14159265358979323846f;

    for (int i = 0; i < segments; ++i)
    {
        float theta1 = (float)i / segments * 2.0f * PI;
        float theta2 = (float)(i+1) / segments * 2.0f * PI;

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
    constexpr float PI = 3.14159265359f;

    for(int i = 0; i < segments / 2; ++i)
    {
        float theta1 = PI * i / segments;       // dolny pierścień
        float theta2 = PI * (i + 1) / segments; // górny pierścień

        for(int j = 0; j < segments; ++j)
        {
            float phi1 = 2.0f * PI * j / segments;
            float phi2 = 2.0f * PI * (j + 1) / segments;

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

} // namespace States
} // namespace ScotlandYard