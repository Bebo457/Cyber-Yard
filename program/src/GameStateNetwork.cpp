#include "GameState.h"
#include "NetworkManager.h"
#include "Application.h"
#include "StateManager.h"
#include "GameConstants.h"
#include "GameSettings.h"
#include "PlayerController.h"
#include "HUDOverlay.h"

#include <iostream>

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
    , m_p_App(nullptr)
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
    , m_i_SelectedDestinationNode(-1)
    , m_ui_NextPickingID(0)
{
    // any ctor-time initialization that needs to run can stay here
}

GameState::~GameState() {
    std::cout << "[GameState] Destructor called\n";

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
        std::cout << "[GameState] Server started on port " << port << "\n";
    } else {
        printf ("[GameState] Failed to start server!\n");
    }
}

// Uruchamianie klienta
void GameState::StartNetworkClient(const std::string& host, uint16_t port) {
    if (!m_pNetworkManager)
        m_pNetworkManager = std::make_unique<Net::NetworkManager>();

    if (m_pNetworkManager->StartClient(host, port)) {
        std::cout << "[GameState] Connected to server " << host << ":" << port << "\n";
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

// Polling wiadomości+
void GameState::PollNetworkMessages() {
    if (!m_pNetworkManager) return;

    ScotlandYard::Net::NetworkMessage msg;
    while (m_pNetworkManager->PollMessage(msg)) {
        std::cout << "[GameState] Received network message from " << msg.sender << ": " << msg.content << "\n";
        // Parse message
        if (m_pNetworkManager->IsServer()) {
            if (msg.content.rfind("PLAYER_LAYOUT|", 0) == 0) {
                std::string msg2 = "PLAYER_LAYOUT|" + SerializePlayerStates(m_vec_Players);
                BroadcastMessage(msg2);
            }
            else{
                size_t pos = 0;
                size_t next = 0;
                int fields[6]; // six pieces of info

                for (int i = 0; i < 6; ++i) {
                    next = msg.content.find('_', pos);
                    if (next == std::string::npos) next = msg.content.size();
                    fields[i] = std::stoi(msg.content.substr(pos, next - pos));
                    pos = next + 1;
                }

                int playerIndex = fields[0];
                int fromNode = fields[1];
                int toNode = fields[2];
                int transportType = fields[3];
                bool usedBlackTicket = (fields[4] != 0);
                bool doubleMovePending = (fields[5] != 0);

                int i_PlayerIndex = playerIndex;
                int i_TransportType = transportType;
                bool b_MrXUsedBlack = usedBlackTicket;
                bool b_MrXSecondMoveWasPending = doubleMovePending;
                
                // Apply move
                m_vec_Players[playerIndex].MoveTo(toNode);

                m_i_SelectedPlayerIndex = -1;
                UI::ShowDetectivePopup(false);
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
                        using UI::TicketMark;
                        TicketMark mark = TicketMark::None;
                        if (b_MrXUsedBlack) mark = TicketMark::Black;
                        else if (i_TransportType == Core::k_TransportTypeTaxi) mark = TicketMark::Taxi;
                        else if (i_TransportType == Core::k_TransportTypeBus) mark = TicketMark::Bus;
                        else if (i_TransportType == Core::k_TransportTypeMetro) mark = TicketMark::Metro;

                        int i_TurnIdx = m_i_MrXTurn.load() + 1;
                        UI::SetSlotMark(i_TurnIdx, mark, true);
                        m_i_MrXTurn.store(i_TurnIdx);

                        bool b_UIDouble = UI::IsMrXDoubleSelected();
                        if (!b_MrXSecondMoveWasPending && !m_b_MrXSecondMovePending.load() && b_UIDouble && ref_Player.GetDoubleMoveTickets() > 0) {
                            if (ref_Player.SpendDoubleMoveTicket()) {
                                m_b_MrXSecondMovePending.store(true);
                                ref_Player.SetActive(true);
                            }
                        }

                        if (b_MrXSecondMoveWasPending) {
                            m_b_MrXSecondMovePending.store(false);
                            ref_Player.SetActive(false);
                        } else if (!m_b_MrXSecondMovePending.load()) {
                            ref_Player.SetActive(false);
                        }

                        // If Mr X turn ended now, activate detectives
                        if (!ref_Player.IsActive()) {
                            for (auto& p : m_vec_Players) {
                                if (p.GetType() == Core::PlayerType::Detective) p.SetActive(true);
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
                        // If Mr X has a second move pending, don't mark as moved yet
                        bool b_IsMrX = false;
                        {
                            std::lock_guard<std::mutex> lockPlayers(m_mtx_Players);
                            b_IsMrX = (m_vec_Players[i_PlayerIndex].GetType() == Core::PlayerType::MisterX);
                        }
                        if (b_IsMrX && m_b_MrXSecondMovePending.load()) {
                            // allow second move within the same round
                        } else {
                            m_vec_MovedThisRound[i_PlayerIndex] = true;
                            int i_Remaining = m_i_PlayersRemainingThisRound.load();
                            if (i_Remaining > 0) {
                                m_i_PlayersRemainingThisRound.store(i_Remaining - 1);
                            }
                        }
                    }
                }

                AdvanceRoundIfComplete();

                // Clear UI selections after applying the move
                UI::ClearMrXSelections();

                BroadcastMessage(msg.content);
            }
            // Rebroadcast authoritative update
            // BroadcastMessage(msg.content);
        }
        else {
            if (msg.content.rfind("PLAYER_LAYOUT|", 0) == 0) {
                std::string data = msg.content.substr(14); // remove prefix
                DeserializePlayerStates(data, m_vec_Players);
            }
           else{
            
            }
        }
    }
}

// Wysyłanie wiadomości do wszystkich klientów (serwer)
void GameState::BroadcastMessage(const std::string& msg) {
    if (!m_pNetworkManager) return;

    if (m_pNetworkManager->IsServer()) {
        m_pNetworkManager->SendToAll(msg);
    }
    else if (m_pNetworkManager->IsClient()) {
        m_pNetworkManager->SendToServer(msg);
    }
}

void GameState::DeserializePlayerStates(const std::string& data, std::vector<Core::Player>& players) {

    size_t start = 0;
    while (start < data.size()) {
        size_t end = data.find(';', start);
        if (end == std::string::npos) end = data.size();

        const std::string entry = data.substr(start, end - start);
        int index, node, taxi, bus, metro, black, dbl;

        if (sscanf(entry.c_str(), "%d:%d:%d:%d:%d:%d:%d", &index, &node, &taxi, &bus, &metro, &black, &dbl) == 7) {
            players[index].SetOccupiedNode(node);
        }

        start = end + 1;
    }
}

} // namespace States
} // namespace ScotlandYard