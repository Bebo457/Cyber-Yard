#include "PythonProcessLauncher.h"
#include "PythonBridge.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#endif

namespace ScotlandYard {
namespace Core {

std::string PythonProcessLauncher::GetScriptForAlgorithm(AIAlgorithm algo) {
    switch (algo) {
        // MrX ML algorithms
        case AIAlgorithm::PPOMrX:         return "MrX_PPO.py";
        case AIAlgorithm::MAPPOMrX:       return "MRX_MAPPO.py";
        case AIAlgorithm::DiscreteSACMrX: return "MRX_SAC.py";
        case AIAlgorithm::NeuralNet:      return "MrX_PPO.py";  // Legacy
        
        // Detective ML algorithms
        case AIAlgorithm::PPOPolice:         return "Detective_PPO.py";
        case AIAlgorithm::MAPPOPolice:       return "Detective_MAPPO.py";
        case AIAlgorithm::DiscreteSACPolice: return "Detective_SAC.py";
        case AIAlgorithm::NeuralNetPolice:   return "Detective_PPO.py";  // Legacy
        
        default:
            return "";  // No Python script needed
    }
}

int PythonProcessLauncher::GetPortForAlgorithm(AIAlgorithm algo) {
    switch (algo) {
        // MrX uses port 5555
        case AIAlgorithm::PPOMrX:
        case AIAlgorithm::MAPPOMrX:
        case AIAlgorithm::DiscreteSACMrX:
        case AIAlgorithm::NeuralNet:
            return 5555;
        
        // Detectives use port 5556
        case AIAlgorithm::PPOPolice:
        case AIAlgorithm::MAPPOPolice:
        case AIAlgorithm::DiscreteSACPolice:
        case AIAlgorithm::NeuralNetPolice:
            return 5556;
        
        default:
            return 0;
    }
}

pid_t PythonProcessLauncher::LaunchScript(const std::string& scriptPath) {
    if (scriptPath.empty()) {
        return -1;
    }
    
    std::cout << "[PythonLauncher] Starting: " << scriptPath << std::endl;
    
#ifdef _WIN32
    // Windows implementation
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    std::string cmd = "python " + scriptPath;
    
    if (CreateProcessA(nullptr, const_cast<char*>(cmd.c_str()),
                       nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return static_cast<pid_t>(pi.dwProcessId);
    }
    std::cerr << "[PythonLauncher] Failed to start process\n";
    return -1;
#else
    // Linux/POSIX implementation
    pid_t pid = fork();
    
    if (pid == 0) {
        // Child process - execute Python script
        // Redirect output to /dev/null to avoid cluttering terminal
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        
        execlp("python3", "python3", scriptPath.c_str(), nullptr);
        
        // If execlp returns, it failed
        std::cerr << "[PythonLauncher] execlp failed\n";
        _exit(1);
    }
    else if (pid > 0) {
        // Parent process
        std::cout << "[PythonLauncher] Started process PID: " << pid << std::endl;
        return pid;
    }
    else {
        std::cerr << "[PythonLauncher] fork() failed\n";
        return -1;
    }
#endif
}

void PythonProcessLauncher::TerminateScript(pid_t pid) {
    if (pid <= 0) {
        return;
    }
    
    std::cout << "[PythonLauncher] Terminating PID: " << pid << std::endl;
    
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (hProcess) {
        TerminateProcess(hProcess, 0);
        CloseHandle(hProcess);
    }
#else
    // Send SIGTERM first (graceful)
    kill(pid, SIGTERM);
    
    // Wait briefly for graceful shutdown
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // Check if still running and force kill if needed
    int status;
    pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == 0) {
        // Still running, force kill
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
#endif
}

bool PythonProcessLauncher::WaitForServer(const std::string& addr, int timeoutMs) {
    const int checkIntervalMs = 200;
    int elapsed = 0;
    
    while (elapsed < timeoutMs) {
        if (PythonBridge::ProbeServer(addr)) {
            std::cout << "[PythonLauncher] Server ready at " << addr << std::endl;
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(checkIntervalMs));
        elapsed += checkIntervalMs;
    }
    
    std::cerr << "[PythonLauncher] Timeout waiting for server at " << addr << std::endl;
    return false;
}

} // namespace Core
} // namespace ScotlandYard
