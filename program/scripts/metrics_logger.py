"""
Training Metrics Logger
========================
Shared module for logging training metrics to JSON files.
Used by MRXPPO.py and DetectiveAI.py

Now supports separate files for MrX and Detective training.
"""

import json
import os
from datetime import datetime
from threading import Lock

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DATA_DIR = os.path.join(SCRIPT_DIR, "training_data")
os.makedirs(DATA_DIR, exist_ok=True)

METRICS_FILE = os.path.join(DATA_DIR, "training_metrics.json")
METRICS_FILE_MRX = os.path.join(DATA_DIR, "training_metrics_mrx.json")
METRICS_FILE_DET = os.path.join(DATA_DIR, "training_metrics_det.json")

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
        training_role: "mrx" or "detective" - which PPO is being trained
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
        "opponent": opponent
    }
    
    # Write to role-specific file only (avoids race condition in parallel mode)
    if training_role == "mrx":
        _write_to_file(METRICS_FILE_MRX, game_data, _lock_mrx)
        print(f"[MrX Metrics] Game #{_count_games(METRICS_FILE_MRX)}: {winner} wins")
    elif training_role == "detective":
        _write_to_file(METRICS_FILE_DET, game_data, _lock_det)
        print(f"[Det Metrics] Game #{_count_games(METRICS_FILE_DET)}: {winner} wins")
    else:
        # Only write to main file when training_role is unknown (non-parallel mode)
        _write_to_file(METRICS_FILE, game_data, _lock_main)
        print(f"[Metrics] Game logged: {winner} wins")


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
