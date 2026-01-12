#pragma once
#include <string>
#include <sys/types.h>
#include "GameSettings.h"

namespace ScotlandYard {
namespace Core {

/**
 * Manages Python AI server processes.
 * Launches appropriate scripts based on selected AIAlgorithm.
 */
class PythonProcessLauncher {
public:
    /// Launch a Python script by name, returns process ID (or -1 on failure)
    static pid_t LaunchScript(const std::string& scriptPath);
    
    /// Terminate a previously launched script
    static void TerminateScript(pid_t pid);
    
    /// Wait for script to be ready (probe ZeroMQ port)
    static bool WaitForServer(const std::string& addr, int timeoutMs = 5000);
    
    /// Get script filename for a given AI algorithm
    /// Returns empty string if algorithm doesn't need Python
    static std::string GetScriptForAlgorithm(AIAlgorithm algo);
    
    /// Get ZeroMQ port for a given AI algorithm
    static int GetPortForAlgorithm(AIAlgorithm algo);
};

} // namespace Core
} // namespace ScotlandYard
