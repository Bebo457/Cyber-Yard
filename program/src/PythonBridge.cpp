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
    : addr_(addr), ctx_(1), socket_(ctx_, zmq::socket_type::req) {
    // Set timeouts to prevent infinite hangs (ms)
    // 2 seconds is enough for Python to respond even during initialization
    int timeout = 2000;
    socket_.set(zmq::sockopt::rcvtimeo, timeout);
    socket_.set(zmq::sockopt::sndtimeo, timeout);
    socket_.set(zmq::sockopt::linger, 0);
    
    socket_.connect(addr);
    s_instance = this; // register global instance
    worker_ = std::thread(&PythonBridge::workerThread, this);
}

void PythonBridge::resetSocket() {
    // Reset socket after timeout to recover from REQ/REP desync
    try {
        socket_.close();
        socket_ = zmq::socket_t(ctx_, zmq::socket_type::req);
        int timeout = 2000;
        socket_.set(zmq::sockopt::rcvtimeo, timeout);
        socket_.set(zmq::sockopt::sndtimeo, timeout);
        socket_.set(zmq::sockopt::linger, 0);
        socket_.connect(addr_);
        std::cout << "[PythonBridge] Socket reset successful\n";
    } catch (const std::exception& e) {
        std::cerr << "[PythonBridge] Socket reset failed: " << e.what() << "\n";
    }
}

PythonBridge::~PythonBridge() {
    // Signal worker to stop (no mutex - just set flag and notify)
    stop_ = true;
    cv_.notify_all();

    // Give worker time to exit (it should exit after recv timeout or cv_ wake)
    if (worker_.joinable()) {
        // Use a detach + short sleep approach to avoid join deadlock
        // Worker will exit on its own when stop_ is true
        try {
            worker_.detach();
        } catch (...) {}
    }
    
    // Brief wait for detached thread to notice stop_
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Close socket and context
    try { 
        socket_.set(zmq::sockopt::linger, 0);
        socket_.close(); 
    } catch (...) {}
    
    try { 
        ctx_.close(); 
    } catch (...) {}

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

        // Check stop_ before doing any socket operations
        if (stop_) {
            // Set exception to unblock any waiters
            try {
                job.prom.set_exception(std::make_exception_ptr(std::runtime_error("Bridge shutting down")));
            } catch (...) {}
            continue;
        }

        try {
            std::string out = job.req.dump();
            zmq::message_t msg(out.begin(), out.end());
            auto send_result = socket_.send(msg, zmq::send_flags::none);
            
            // Check if we should stop after send
            if (stop_ || !send_result) {
                job.prom.set_exception(std::make_exception_ptr(std::runtime_error("Bridge shutting down")));
                continue;
            }

            zmq::message_t reply;
            auto recv_result = socket_.recv(reply, zmq::recv_flags::none);
            
            // Check if recv failed (timeout or shutdown)
            if (!recv_result || stop_) {
                job.prom.set_exception(std::make_exception_ptr(std::runtime_error("Recv timeout or shutdown")));
                // Reset socket to recover from REQ/REP desync
                if (!stop_) {
                    resetSocket();
                }
                continue;
            }
            
            std::string sreply(static_cast<char*>(reply.data()), reply.size());
            job.prom.set_value(nlohmann::json::parse(sreply));
        } catch (...) {
            try {
                job.prom.set_exception(std::current_exception());
            } catch (...) {}
        }
    }
}