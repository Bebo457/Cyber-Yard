"""
Training Metrics Logger
========================
Shared module for logging training metrics to JSON file.
Used by MRXPPO.py and DetectiveAI.py

Fixed: Uses proper file locking and atomic writes to prevent corruption
when multiple processes write simultaneously.
"""

import json
import os
from datetime import datetime
from threading import Lock
import io

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
METRICS_FILE = os.path.join(SCRIPT_DIR, "training_metrics.json")

_lock = Lock()

def log_game(winner: str, mrx_reward: float = 0, det_reward: float = 0, rounds: int = 0):
    """Log a completed game to the metrics file with proper file handling"""
    with _lock:
        try:
            # Load existing data with proper file handling
            data = {"games": [], "start_time": datetime.now().isoformat()}
            
            if os.path.exists(METRICS_FILE):
                try:
                    with open(METRICS_FILE, 'r', encoding='utf-8') as f:
                        content = f.read().strip()
                        if content:
                            data = json.loads(content)
                except json.JSONDecodeError as e:
                    # File is corrupted - backup and start fresh
                    backup_path = METRICS_FILE + f".corrupted_{int(datetime.now().timestamp())}"
                    try:
                        os.rename(METRICS_FILE, backup_path)
                        print(f"[Metrics] Corrupted file backed up to {backup_path}")
                    except:
                        pass
                    data = {"games": [], "start_time": datetime.now().isoformat()}
            
            # Add new game
            data["games"].append({
                "timestamp": datetime.now().isoformat(),
                "winner": winner,
                "mrx_reward": mrx_reward,
                "det_reward": det_reward,
                "rounds": rounds
            })
            
            # Serialize to string first
            json_str = json.dumps(data, indent=2)
            
            # Write atomically: temp file -> rename
            temp_path = METRICS_FILE + ".tmp"
            with open(temp_path, 'w', encoding='utf-8') as f:
                f.write(json_str)
                f.flush()
                os.fsync(f.fileno())
            
            # Atomic replace
            if os.path.exists(METRICS_FILE):
                os.remove(METRICS_FILE)
            os.rename(temp_path, METRICS_FILE)
                
            print(f"[Metrics] Logged game #{len(data['games'])}: {winner} wins")
            
        except Exception as e:
            print(f"[Metrics] Error logging game: {e}")

def clear_metrics():
    """Clear all metrics (start fresh)"""
    with _lock:
        try:
            if os.path.exists(METRICS_FILE):
                os.remove(METRICS_FILE)
                print("[Metrics] Cleared all metrics")
        except Exception as e:
            print(f"[Metrics] Error clearing: {e}")
