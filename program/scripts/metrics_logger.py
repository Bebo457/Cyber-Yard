"""
Training Metrics Logger
========================
Shared module for logging training metrics to JSON files.
Used by MRXPPO.py, DetectiveAI.py, and other AI scripts.

Now supports separate files for MrX and Detective training,
with algorithm-specific subdirectories (PPO, MAPPO, SAC).
"""

import json
import os
from datetime import datetime
from threading import Lock

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
BASE_DATA_DIR = os.path.join(SCRIPT_DIR, "training_data")
os.makedirs(BASE_DATA_DIR, exist_ok=True)

# Current algorithm (can be set by AI scripts or train_ppo.py)
_current_algorithm = "ppo"


def set_algorithm(algorithm: str):
    """Set the current algorithm for metrics logging (ppo, mappo, sac)"""
    global _current_algorithm
    _current_algorithm = algorithm.lower()


def get_data_dir(algorithm: str = None) -> str:
    """Get algorithm-specific data directory"""
    algo = algorithm or _current_algorithm
    data_dir = os.path.join(BASE_DATA_DIR, algo.upper())
    os.makedirs(data_dir, exist_ok=True)
    return data_dir


def get_metrics_file(algorithm: str = None) -> str:
    return os.path.join(get_data_dir(algorithm), "training_metrics.json")


def get_metrics_file_mrx(algorithm: str = None) -> str:
    return os.path.join(get_data_dir(algorithm), "training_metrics_mrx.json")


def get_metrics_file_det(algorithm: str = None) -> str:
    return os.path.join(get_data_dir(algorithm), "training_metrics_det.json")


# Legacy compatibility - default paths (used when algorithm not specified)
METRICS_FILE = os.path.join(BASE_DATA_DIR, "PPO", "training_metrics.json")
METRICS_FILE_MRX = os.path.join(BASE_DATA_DIR, "PPO", "training_metrics_mrx.json")
METRICS_FILE_DET = os.path.join(BASE_DATA_DIR, "PPO", "training_metrics_det.json")

# Opponent tracking files (written by train_ppo.py, read by AI scripts)
OPPONENT_FILE_MRX = os.path.join(BASE_DATA_DIR, "current_opponent_mrx.txt")
OPPONENT_FILE_DET = os.path.join(BASE_DATA_DIR, "current_opponent_det.txt")

_lock_main = Lock()
_lock_mrx = Lock()
_lock_det = Lock()


def _write_to_file(filepath: str, game_data: dict, lock: Lock):
    """Write game data to a specific metrics file"""
    with lock:
        try:
            data = {"games": [], "start_time": datetime.now().isoformat()}
            
            if os.path.exists(filepath):
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read().strip()
                        if content:
                            data = json.loads(content)
                except json.JSONDecodeError:
                    backup_path = filepath + f".corrupted_{int(datetime.now().timestamp())}"
                    try:
                        os.rename(filepath, backup_path)
                    except:
                        pass
                    data = {"games": [], "start_time": datetime.now().isoformat()}
            
            data["games"].append(game_data)
            
            json_str = json.dumps(data, indent=2)
            temp_path = filepath + ".tmp"
            with open(temp_path, 'w', encoding='utf-8') as f:
                f.write(json_str)
                f.flush()
                os.fsync(f.fileno())
            
            if os.path.exists(filepath):
                os.remove(filepath)
            os.rename(temp_path, filepath)
            
        except Exception as e:
            print(f"[Metrics] Error writing to {filepath}: {e}")


def log_game(winner: str, mrx_reward: float = 0, det_reward: float = 0, 
             rounds: int = 0, training_role: str = "unknown", opponent: str = ""):
    """Log a completed game to metrics files
    
    Args:
        winner: "MrX" or "Detectives"
        mrx_reward: Reward for MrX
        det_reward: Reward for Detective
        rounds: Number of rounds
        training_role: "mrx" or "detective" - which agent is being trained
        opponent: Name of the opponent algorithm
    """
    # Auto-detect training role from rewards if not specified
    if training_role == "unknown":
        if abs(mrx_reward) > 0.01:
            training_role = "mrx"
        elif abs(det_reward) > 0.01:
            training_role = "detective"
    
    game_data = {
        "timestamp": datetime.now().isoformat(),
        "winner": winner,
        "mrx_reward": mrx_reward,
        "det_reward": det_reward,
        "rounds": rounds,
        "training_role": training_role,
        "opponent": opponent,
        "algorithm": _current_algorithm
    }
    
    # Write to role-specific file in algorithm-specific directory
    if training_role == "mrx":
        filepath = get_metrics_file_mrx()
        _write_to_file(filepath, game_data, _lock_mrx)
        print(f"[MrX {_current_algorithm.upper()}] Game #{_count_games(filepath)}: {winner} wins")
    elif training_role == "detective":
        filepath = get_metrics_file_det()
        _write_to_file(filepath, game_data, _lock_det)
        print(f"[Det {_current_algorithm.upper()}] Game #{_count_games(filepath)}: {winner} wins")
    else:
        # Only write to main file when training_role is unknown (non-parallel mode)
        filepath = get_metrics_file()
        _write_to_file(filepath, game_data, _lock_main)
        print(f"[Metrics {_current_algorithm.upper()}] Game logged: {winner} wins")


def _count_games(filepath: str) -> int:
    """Count games in a metrics file"""
    try:
        if os.path.exists(filepath):
            with open(filepath, 'r', encoding='utf-8') as f:
                data = json.load(f)
                return len(data.get("games", []))
    except:
        pass
    return 0


def clear_metrics():
    """Clear all metrics files"""
    for filepath in [METRICS_FILE, METRICS_FILE_MRX, METRICS_FILE_DET]:
        try:
            if os.path.exists(filepath):
                os.remove(filepath)
        except Exception as e:
            print(f"[Metrics] Error clearing {filepath}: {e}")
    print("[Metrics] Cleared all metrics files")


def set_current_opponent(role: str, opponent: str):
    """Set current opponent for a training role (called by train_ppo.py)"""
    filepath = OPPONENT_FILE_MRX if role == "mrx" else OPPONENT_FILE_DET
    try:
        with open(filepath, 'w') as f:
            f.write(opponent)
    except Exception as e:
        print(f"[Metrics] Error writing opponent file: {e}")


def get_current_opponent(role: str) -> str:
    """Get current opponent for a training role (called by AI scripts)"""
    filepath = OPPONENT_FILE_MRX if role == "mrx" else OPPONENT_FILE_DET
    try:
        if os.path.exists(filepath):
            with open(filepath, 'r') as f:
                return f.read().strip()
    except:
        pass
    return ""
