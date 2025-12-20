#pragma once
#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <future>
#include <nlohmann/json.hpp>
#include <zmq.hpp>

class PythonBridge {
public:
    static bool ProbeServer(const std::string& addr);

    PythonBridge(const std::string& addr = "tcp://localhost:5555");
    ~PythonBridge();

    // wysyła żądanie json, zwraca future z odpowiedzią json
    std::future<nlohmann::json> sendRequestAsync(const nlohmann::json& req);

    // accessor do globalnej instancji (zwraca nullptr jeśli nie zainicjowano)
    static PythonBridge* Instance();

private:
    void workerThread();
    void resetSocket();

    std::string addr_;
    zmq::context_t ctx_;
    zmq::socket_t socket_;
    std::thread worker_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_{false};

    struct Job {
        nlohmann::json req;
        std::promise<nlohmann::json> prom;
    };
    std::queue<Job> queue_;

    // singleton-like pointer (nie zarządza cyklem życia poza konstruktorem/destruktorem)
    static PythonBridge* s_instance;
};