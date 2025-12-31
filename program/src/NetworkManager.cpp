#include "NetworkManager.h"
#include <iostream>

using namespace ScotlandYard::Net;

NetworkManager::NetworkManager() {
    if (enet_initialize() != 0)
        std::cerr << "[Network] ENet initialization failed!\n";
}

NetworkManager::~NetworkManager() {
    Stop();
    enet_deinitialize();
}


bool NetworkManager::StartServer(uint16_t port) {
    if (m_Role != NetRole::None) return false;

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = port;

    m_Host = enet_host_create(&address, 32, 2, 0, 0);
    if (!m_Host) {
        std::cerr << "[Network] Could not create server host.\n";
        return false;
    }

    m_Role = NetRole::Server;
    m_Running = true;
    m_Thread = std::thread(&NetworkManager::NetworkLoop, this);
    std::cout << "[Network] Server started on port " << port << "\n";
    return true;
}

bool NetworkManager::StartClient(const std::string& host, uint16_t port) {
    if (m_Role != NetRole::None) return false;

    m_Host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!m_Host) {
        std::cerr << "[Network] Could not create client host.\n";
        return false;
    }

    ENetAddress address;
    enet_address_set_host(&address, host.c_str());
    address.port = port;

    m_Peer = enet_host_connect(m_Host, &address, 2, 0);
    if (!m_Peer) {
        std::cerr << "[Network] Could not connect to server.\n";
        return false;
    }

    ENetEvent event;
    if (enet_host_service(m_Host, &event, 5000) > 0 && event.type == ENET_EVENT_TYPE_CONNECT) {
        std::cout << "[Network] Connected to server\n";
    } else {
        std::cerr << "[Network] Connection to server failed (timeout)\n";
        return false;
    }

    m_Role = NetRole::Client;
    m_Running = true;
    m_Thread = std::thread(&NetworkManager::NetworkLoop, this);
    std::cout << "[Network] Connecting to " << host << ":" << port << "\n";
    return true;
}

void NetworkManager::Stop() {
    if (!m_Running) return;
    m_Running = false;

    if (m_Thread.joinable())
        m_Thread.join();

    if (m_Host) {
        enet_host_destroy(m_Host);
        m_Host = nullptr;
    }

    m_Role = NetRole::None;
}

void NetworkManager::SendToAll(const std::string& msg) {
    if (m_Role != NetRole::Server || !m_Host) return;

    ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_host_broadcast(m_Host, 0, packet);
    enet_host_flush(m_Host);
    printf ("[NetworkManager] Broadcasting to all message: %s\n", msg.c_str());
}

void NetworkManager::SendToServer(const std::string& msg) {
    if (m_Role != NetRole::Client || !m_Peer) return;

    ENetPacket* packet = enet_packet_create(msg.c_str(), msg.size() + 1, ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(m_Peer, 0, packet);
    enet_host_flush(m_Host);
    printf ("[NetworkManager] Broadcasting to server message: %s\n", msg.c_str());
}

bool NetworkManager::PollMessage(NetworkMessage& outMsg) {
    std::lock_guard<std::mutex> lock(m_MutexQueue);
    if (m_Incoming.empty()) return false;
    outMsg = m_Incoming.front();
    m_Incoming.pop();
    return true;
}

void NetworkManager::NetworkLoop() {
    ENetEvent event;
    while (m_Running) {
        while (enet_host_service(m_Host, &event, 50) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT:
                    std::cout << "[Network] Peer connected.\n";
                    break;

                case ENET_EVENT_TYPE_RECEIVE: {
                    std::string msg(reinterpret_cast<char*>(event.packet->data));
                    {
                        std::lock_guard<std::mutex> lock(m_MutexQueue);
                        m_Incoming.push({ "peer", msg });
                    }
                    enet_packet_destroy(event.packet);
                    break;
                }

                case ENET_EVENT_TYPE_DISCONNECT:
                    std::cout << "[Network] Peer disconnected.\n";
                    break;

                default:
                    break;
            }
        }
    }
}

