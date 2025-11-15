#include "Logger.h"
#include <iomanip>
#include <vector>

GameLogger::GameLogger(const std::string& filename) {
    gameStartTime = std::chrono::system_clock::now();
    logFile.open(filename, std::ios::app);

    if (logFile.is_open()) {
        logFile << "timestamp,eventType,playerId,fromNode,toNode,details\n";
        logFile.flush();
    }
}

GameLogger::~GameLogger() {
    if (logFile.is_open()) {
        logFile.close();
    }
}

std::string GameLogger::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - gameStartTime
    );
    return std::to_string(duration.count());
}

void GameLogger::logPlayerMove(int playerId, int fromNode, int toNode, int i_ticketType) {
    if (logFile.is_open()) {
        logFile << getCurrentTimestamp() << ",MOVE," 
                << playerId << "," << fromNode << "," << toNode << ",\n";
        logFile.flush();
    }
}

void GameLogger::logGameEnd(const std::string& winner) {
    if (logFile.is_open()) {
        logFile << getCurrentTimestamp() << ",GAME_END,,-,-," 
                << winner << "\n";
        logFile.flush();
        logFile.close();
    }
}
void GameLogger::logPlayerTicketsNotUsed(int i_playerId, const std::vector<int>& vec_ticketType){
    if (logFile.is_open()) {
        std::ostringstream details;
        details << "TicketsNotUsed: Taxi=" << vec_ticketType[0] 
                << ",Bus=" << vec_ticketType[1] 
                << ",Metro=" << vec_ticketType[2] 
                << ",Water=" << vec_ticketType[3];
        logFile << getCurrentTimestamp() << ",TICKETS_NOT_USED," 
                << i_playerId << ",-,-," << details.str() << "\n";
        logFile.flush();
    }
} // taxi - 0, bus - 1, metro - 2, water - 3

bool GameLogger::isOpen() const {
    return logFile.is_open();
}