#include "PythonBridge.h"
#include <iostream>
PythonBridge* PythonBridge::s_instance = nullptr;

// Statyczna metoda sprawdzająca serwer przed inicjalizacją
bool PythonBridge::ProbeServer(const std::string& addr) {
    try {
        zmq::context_t temp_ctx(1);
        zmq::socket_t temp_sock(temp_ctx, zmq::socket_type::req);
        
        // Ustawiamy krótkie timeouty (ms)
        int timeout = 500; 
        temp_sock.set(zmq::sockopt::rcvtimeo, timeout);
        temp_sock.set(zmq::sockopt::sndtimeo, timeout);
        temp_sock.set(zmq::sockopt::linger, 0);

        temp_sock.connect(addr);

        nlohmann::json ping = {{"type", "ping"}};
        std::string out = ping.dump();
        
        temp_sock.send(zmq::buffer(out), zmq::send_flags::none);

        zmq::message_t reply;
        auto res = temp_sock.recv(reply, zmq::recv_flags::none);
        
        if (res) {
            auto j_reply = nlohmann::json::parse(reply.to_string());
            return j_reply["type"] == "pong";
        }
    } catch (...) {
        return false;
    }
    return false;
}

PythonBridge::PythonBridge(const std::string& addr)
    : ctx_(1), socket_(ctx_, zmq::socket_type::req) {
    // Set timeouts to prevent infinite hangs (ms)
    int timeout = 5000;  // 5 second timeout
    socket_.set(zmq::sockopt::rcvtimeo, timeout);
    socket_.set(zmq::sockopt::sndtimeo, timeout);
    socket_.set(zmq::sockopt::linger, 0);
    
    socket_.connect(addr);
    s_instance = this; // register global instance
    worker_ = std::thread(&PythonBridge::workerThread, this);
}

PythonBridge::~PythonBridge() {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        stop_ = true;
        cv_.notify_all();
    }

    try { socket_.close(); } catch (...) {}   // przerwie blokujące recv
    if (worker_.joinable()) worker_.join();
    try { ctx_.close(); } catch (...) {}

    s_instance = nullptr;
}

PythonBridge* PythonBridge::Instance() {
    return s_instance;
}

std::future<nlohmann::json> PythonBridge::sendRequestAsync(const nlohmann::json& req) {
    Job j;
    j.req = req;
    auto fut = j.prom.get_future();
    {
        std::lock_guard lock(mtx_);
        queue_.push(std::move(j));
    }
    cv_.notify_one();
    return fut;
}

void PythonBridge::workerThread() {
    while (true) {
        Job job;
        {
            std::unique_lock<std::mutex> lock(mtx_);
            cv_.wait(lock, [this]{ return stop_ || !queue_.empty(); });
            if (stop_ && queue_.empty()) return;
            job = std::move(queue_.front());
            queue_.pop();
        }

        try {
            std::string out = job.req.dump();
            zmq::message_t msg(out.begin(), out.end());
            socket_.send(msg, zmq::send_flags::none);

            zmq::message_t reply;
            socket_.recv(reply, zmq::recv_flags::none);
            std::string sreply(static_cast<char*>(reply.data()), reply.size());
            job.prom.set_value(nlohmann::json::parse(sreply));
        } catch (...) {
            job.prom.set_exception(std::current_exception());
        }
    }
}