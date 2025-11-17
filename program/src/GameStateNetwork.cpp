#include "GameState.h"
#include "NetworkManager.h"

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
    // any ctor-time initialization that needs to run can stay here
}

GameState::~GameState() {
    // clean up network manager safely if still present
    if (m_pNetworkManager) {
        m_pNetworkManager->Stop();
        m_pNetworkManager.reset();
    }
    // other cleanup if necessary
}

// Uruchamianie serwera
void GameState::StartNetworkServer(uint16_t port) {
    if (!m_pNetworkManager)
        m_pNetworkManager = std::make_unique<Net::NetworkManager>();

    if (m_pNetworkManager->StartServer(port)) {
        printf ("[GameState] Server started on port ", port , "\n");
    } else {
        printf ("[GameState] Failed to start server!\n");
    }
}

// Uruchamianie klienta
void GameState::StartNetworkClient(const std::string& host, uint16_t port) {
    if (!m_pNetworkManager)
        m_pNetworkManager = std::make_unique<Net::NetworkManager>();

    if (m_pNetworkManager->StartClient(host, port)) {
        printf ("[GameState] Connected to server ", host, ":" ,port);
    } else {
        printf ("[GameState] Failed to connect to server!");
    }
}

// Zatrzymanie sieci
void GameState::StopNetwork() {
    if (m_pNetworkManager) {
        m_pNetworkManager->Stop();
        m_pNetworkManager.reset();
    }
}

// Polling wiadomości
void GameState::PollNetworkMessages() {
    if (!m_pNetworkManager) return;

    ScotlandYard::Net::NetworkMessage msg;
    while (m_pNetworkManager->PollMessage(msg)) {
        printf ("Otrzymano od ", msg.sender , ": " , msg.content );
        // Tutaj dodaj logikę gry dla odebranej wiadomości
        size_t pos = msg.content.find('_');
        int i_PlayerIndex = std::stoi(msg.content.substr(0, pos));
        int i_DestinationNode = std::stoi(msg.content.substr(pos + 1));

        auto& player = m_vec_Players[i_PlayerIndex];
        player.MoveTo(i_DestinationNode);
    }
}

// Wysyłanie wiadomości do wszystkich klientów (serwer)
void GameState::BroadcastMessage(const std::string& msg) {
    if (m_pNetworkManager){
        m_pNetworkManager->SendToAll(msg);
        m_pNetworkManager->SendToServer(msg);
    }
}

} // namespace States
} // namespace ScotlandYard