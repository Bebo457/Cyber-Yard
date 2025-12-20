"""
Training Metrics Logger
========================
Shared module for logging training metrics to JSON file.
Used by MRXPPO.py and DetectiveAI.py
"""

import json
import os
from datetime import datetime
from threading import Lock

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
METRICS_FILE = os.path.join(SCRIPT_DIR, "training_metrics.json")

_lock = Lock()

def log_game(winner: str, mrx_reward: float = 0, det_reward: float = 0, rounds: int = 0):
    """Log a completed game to the metrics file"""
    with _lock:
        try:
            # Load existing data
            if os.path.exists(METRICS_FILE):
                with open(METRICS_FILE, 'r') as f:
                    data = json.load(f)
            else:
                data = {"games": [], "start_time": datetime.now().isoformat()}
            
            # Add new game
            data["games"].append({
                "timestamp": datetime.now().isoformat(),
                "winner": winner,
                "mrx_reward": mrx_reward,
                "det_reward": det_reward,
                "rounds": rounds
            })
            
            # Save
            with open(METRICS_FILE, 'w') as f:
                json.dump(data, f, indent=2)
                
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
