#include "PythonBridge.h"

PythonBridge* PythonBridge::s_instance = nullptr;

PythonBridge::PythonBridge(const std::string& addr)
    : ctx_(1), socket_(ctx_, zmq::socket_type::req) {
    socket_.connect(addr);
    s_instance = this; // register global instance
    worker_ = std::thread(&PythonBridge::workerThread, this);
}

PythonBridge::~PythonBridge() {
    {
        std::lock_guard lock(mtx_);
        stop_ = true;
        cv_.notify_all();
    }
    if (worker_.joinable()) worker_.join();
    s_instance = nullptr; // unregister
    try { socket_.close(); } catch(...) {}
    try { ctx_.close(); } catch(...) {}
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