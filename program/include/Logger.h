#pragma once
#include <string>
#include <fstream>
#include <chrono>
#include <sstream>
#include <vector>
class GameLogger {
private:
    std::ofstream logFile;
    std::chrono::system_clock::time_point gameStartTime;

    std::string getCurrentTimestamp();

public:
    GameLogger(const std::string& s_filename = "game_log.csv");
    ~GameLogger();

    void logPlayerMove(int i_playerId, int i_fromNode, int i_toNode, int i_ticketType);
    void logGameEnd(const std::string& winner);
    void logPlayerTicketsNotUsed(int i_playerId, const std::vector<int>& vec_ticketType); // taxi - 0, bus, metro, water - 3

    bool isOpen() const;
};