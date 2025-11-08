#pragma once
#include <enet/enet.h>
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

namespace ScotlandYard {
namespace Net {

enum class NetRole { None, Server, Client };

struct NetworkMessage {
    std::string sender;
    std::string content;
};

class NetworkManager {
public:
    NetworkManager();
    ~NetworkManager();

    bool StartServer(uint16_t port);
    bool StartClient(const std::string& host, uint16_t port);
    void Stop();

    void SendToAll(const std::string& message);
    void SendToServer(const std::string& message);
    bool PollMessage(NetworkMessage& outMsg);

    NetRole GetRole() const { return m_Role; } // <-- tu było błędnie

private:
    void NetworkLoop();

    ENetHost* m_Host = nullptr;
    ENetPeer* m_Peer = nullptr;
    NetRole m_Role = NetRole::None;

    std::thread m_Thread;
    std::atomic_bool m_Running{false};

    std::mutex m_MutexQueue;
    std::queue<NetworkMessage> m_Incoming;
};

} // namespace Net
} // namespace ScotlandYard
