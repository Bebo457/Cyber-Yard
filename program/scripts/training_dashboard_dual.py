"""
Dual Training Dashboard for Parallel RL Training
==================================================
Shows TWO separate windows:
- Window 1: MrX training progress
- Window 2: Detective training progress

Used with --parallel mode in train.py
Supports: PPO, MAPPO, SAC algorithms via --algorithm flag
"""

import argparse
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Patch
from pathlib import Path
import json
from datetime import datetime
from collections import deque
from threading import Thread
import sys

SCRIPT_DIR = Path(__file__).parent
BASE_DATA_DIR = SCRIPT_DIR / "training_data"

# Will be set by command line argument
ALGORITHM = "PPO"
TARGET_GAMES = 500  # Default target games per opponent
DATA_DIR = None
METRICS_FILE_MRX = None
METRICS_FILE_DET = None
LOG_DIR = None

def set_algorithm(algo: str, games: int = 500):
    """Set paths based on algorithm and target games"""
    global ALGORITHM, TARGET_GAMES, DATA_DIR, METRICS_FILE_MRX, METRICS_FILE_DET, LOG_DIR
    ALGORITHM = algo.upper()
    try:
        if games > 0:
            TARGET_GAMES = games
    except (ValueError, TypeError):
        pass
        
    DATA_DIR = BASE_DATA_DIR / ALGORITHM
    METRICS_FILE_MRX = DATA_DIR / "training_metrics_mrx.json"
    METRICS_FILE_DET = DATA_DIR / "training_metrics_det.json"
    LOG_DIR = DATA_DIR / "logs"

# Default to PPO
set_algorithm("ppo", 500)

MAX_POINTS = 1000


class RoleDashboard:
    """Dashboard for a single training role"""
    
    def __init__(self, role: str, metrics_file: Path, color: str, fig_num: int):
        self.role = role  # "MrX" or "Detective"
        self.metrics_file = metrics_file
        self.color = color
        self.fig_num = fig_num
        
        self.games_played = 0
        self.wins = 0
        self.losses = 0
        self.rewards = deque(maxlen=MAX_POINTS)
        self.win_history = deque(maxlen=MAX_POINTS)
        self.moves_history = deque(maxlen=MAX_POINTS)
        self.session_start = datetime.now()
        self.current_opponent = "..."
        self.current_opponent_games = 0  # Games played against current opponent in this sequence
        
        self.last_file_size = 0
        self.setup_figure()
    
    def setup_figure(self):
        """Create figure with comprehensive layout"""
        plt.style.use('dark_background')
        
        self.fig = plt.figure(num=self.fig_num, figsize=(14, 9))
        title = f"{ALGORITHM} {self.role} Training"
        self.fig.suptitle(title, fontsize=16, fontweight='bold', color=self.color)
        
        # 3 rows, 2 columns. Left col for graphs, Right col for stats
        gs = self.fig.add_gridspec(3, 2, width_ratios=[2.5, 1], hspace=0.35, wspace=0.15,
                                   left=0.05, right=0.98, top=0.92, bottom=0.05)
        
        # 1. Top-Left: Win Rate
        self.ax_winrate = self.fig.add_subplot(gs[0, 0])
        self.ax_winrate.set_title('Win Rate (Rolling 50)', fontweight='bold', fontsize=10)
        self.ax_winrate.set_ylabel('Win %')
        self.ax_winrate.set_ylim(0, 105)
        self.ax_winrate.grid(True, alpha=0.3)
        self.ax_winrate.axhline(y=50, color='gray', linestyle=':', alpha=0.5)
        self.line_wr, = self.ax_winrate.plot([], [], color=self.color, linewidth=2)
        self.ax_winrate.set_facecolor('#1a1a2e')
        self.ax_winrate.tick_params(labelbottom=False)  # Hide x labels for top plots
        
        # 2. Mid-Left: Moves per Game
        self.ax_moves = self.fig.add_subplot(gs[1, 0], sharex=self.ax_winrate)
        self.ax_moves.set_title('Moves per Game (Rolling 50)', fontweight='bold', fontsize=10)
        self.ax_moves.set_ylabel('Moves')
        self.ax_moves.grid(True, alpha=0.3)
        self.line_moves, = self.ax_moves.plot([], [], color='#eba134', linewidth=1.5)
        self.ax_moves.set_facecolor('#1a1a2e')
        self.ax_moves.tick_params(labelbottom=False)
        
        # 3. Bottom-Left: Reward
        self.ax_reward = self.fig.add_subplot(gs[2, 0], sharex=self.ax_winrate)
        self.ax_reward.set_title('Episode Reward', fontweight='bold', fontsize=10)
        self.ax_reward.set_xlabel('Games Played')
        self.ax_reward.set_ylabel('Reward')
        self.ax_reward.grid(True, alpha=0.3)
        self.line_reward, = self.ax_reward.plot([], [], color=self.color, linewidth=1, alpha=0.6)
        self.ax_reward.set_facecolor('#1a1a2e')
        
        # 4. Right Side Top: Stats Box (Spanning 2 rows)
        self.ax_stats = self.fig.add_subplot(gs[0:2, 1])
        self.ax_stats.axis('off')
        self.ax_stats.set_facecolor('#1a1a2e')
        self.stats_text = self.ax_stats.text(
            0.5, 0.5, 'Waiting for data...',
            transform=self.ax_stats.transAxes,
            fontsize=11, family='monospace',
            verticalalignment='center', horizontalalignment='center',
            bbox=dict(boxstyle='round,pad=1', facecolor='#1f2937', edgecolor='#374151')
        )
        
        # 5. Right Side Bottom: Cycle Comparison
        self.ax_compare = self.fig.add_subplot(gs[2, 1])
        self.ax_compare.set_title('Cycle Comparison', fontweight='bold', fontsize=10)
        self.ax_compare.set_facecolor('#1a1a2e')
        self.ax_compare.axis('off')
        self.compare_text = self.ax_compare.text(
            0.5, 0.5, 'No previous data',
            transform=self.ax_compare.transAxes,
            fontsize=11, family='monospace',
            verticalalignment='center', horizontalalignment='center',
            bbox=dict(boxstyle='round,pad=0.8', facecolor='#1f2937', edgecolor='#374151')
        )
    
    def get_cycle_comparison(self, current_opponent: str) -> float:
        """Read training_summary.csv and find the LAST win rate for this role+opponent.
        Returns the previous win rate (from CSV) or None if no history exists."""
        import csv
        summary_file = DATA_DIR / "training_summary.csv"
        
        try:
            if not summary_file.exists():
                return None
            
            # Read all entries for this role + opponent
            entries = []
            with open(summary_file, 'r', encoding='utf-8') as f:
                reader = csv.DictReader(f)
                for row in reader:
                    role = row.get('role', '')
                    opp = row.get('opponent', '').lower()
                    
                    # Match role (MrX or Detective)
                    role_match = (self.role == "MrX" and role == "MrX") or \
                                 (self.role == "Detective" and role == "Detective")
                    
                    if role_match and opp == current_opponent.lower():
                        try:
                            win_rate = float(row.get('win_rate', 0))
                            entries.append(win_rate)
                        except ValueError:
                            pass
            
            # Return the LAST entry as 'previous' (for comparison with current live data)
            if entries:
                return entries[-1]
            return None
                
        except Exception as e:
            return None
    
    def load_data(self):
        """Load metrics from file"""
        try:
            if not self.metrics_file.exists():
                return False
            
            file_size = self.metrics_file.stat().st_size
            if file_size == self.last_file_size:
                return False  # No change
            self.last_file_size = file_size
            
            with open(self.metrics_file, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            games = data.get('games', [])
            if not games:
                return False
            
            self.games_played = len(games)
            self.wins = 0
            self.losses = 0
            self.rewards.clear()
            self.win_history.clear()
            self.moves_history.clear()
            self.current_opponent_games = 0
            
            # Find current opponent from last game or logs
            last_opp_in_file = None
            if games:
                last_opp_in_file = games[-1].get('opponent', '')
            
            # Count backwards to find how many games against CURRENT opponent sequence
            if self.current_opponent == "...":
                if last_opp_in_file:
                    self.current_opponent = last_opp_in_file
            
            calc_opp = self.current_opponent if self.current_opponent != "..." else last_opp_in_file
            
            if calc_opp:
                count = 0
                for g in reversed(games):
                    if g.get('opponent', '').lower() == calc_opp.lower():
                        count += 1
                    else:
                        break
                self.current_opponent_games = count
                if self.current_opponent == "...":
                    self.current_opponent = calc_opp

            for game in games:
                winner = game.get('winner', '')
                rounds = game.get('rounds', 0)
                
                # Determine if this role won
                if self.role == "MrX":
                    reward = game.get('mrx_reward', 0)
                    won = winner == "MrX"
                else:
                    reward = game.get('det_reward', 0)
                    won = winner == "Detectives"
                
                if won:
                    self.wins += 1
                else:
                    self.losses += 1
                
                self.rewards.append(reward)
                self.win_history.append(1 if won else 0)
                self.moves_history.append(rounds)
                
                # Get opponent from game data
                opponent = game.get('opponent', '')
                if opponent:
                    self.current_opponent = opponent
            
            return True
            
        except Exception as e:
            return False
    
    def get_matchup_from_logs(self):
        """Read current matchup from training logs - cache when found"""
        try:
            if LOG_DIR.exists():
                logs = sorted(LOG_DIR.glob("training_*.log"), key=lambda x: x.stat().st_mtime, reverse=True)
                if logs:
                    with open(logs[0], 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    
                    search_tag = f"[{self.role}]" if self.role == "MrX" else "[Detective]"
                    
                    for line in reversed(content.split('\n')):
                        if search_tag in line and 'Training vs' in line:
                            idx = line.find('Training vs ')
                            if idx != -1:
                                rest = line[idx + len('Training vs '):].strip()
                                opponent = rest.split()[0] if rest else None
                                if opponent:
                                    self.current_opponent = opponent.upper()
                                    return self.current_opponent
        except Exception as e:
            pass
        return self.current_opponent if self.current_opponent != "..." else "?"
    
    def rolling_avg(self, data, window=50):
        """Calculate rolling average"""
        result = []
        for i in range(len(data)):
            start = max(0, i - window + 1)
            result.append(sum(list(data)[start:i+1]) / (i - start + 1))
        return result
    
    def update(self, frame):
        """Update animation frame"""
        self.load_data()
        self.get_matchup_from_logs()  # Refresh opponent from logs
        
        if self.games_played == 0:
            return []
        
        x = list(range(1, self.games_played + 1))
        view_start = max(1, self.games_played - MAX_POINTS)
        view_end = self.games_played + 10
        
        # 1. Win rate (Rolling 50)
        win_rates = [w * 100 for w in self.rolling_avg(self.win_history, 50)]
        self.line_wr.set_data(x[-len(win_rates):], win_rates)
        self.ax_winrate.set_xlim(view_start, view_end)
        
        # 2. Moves (Rolling 50)
        moves_smooth = self.rolling_avg(self.moves_history, 50)
        self.line_moves.set_data(x[-len(moves_smooth):], moves_smooth)
        self.ax_moves.set_xlim(view_start, view_end)
        if moves_smooth:
            mmax = max(moves_smooth)
            self.ax_moves.set_ylim(0, mmax * 1.2)
        
        # 3. Rewards
        reward_list = list(self.rewards)
        self.line_reward.set_data(x[-len(reward_list):], reward_list)
        self.ax_reward.set_xlim(view_start, view_end)
        if reward_list:
            rmin, rmax = min(reward_list), max(reward_list)
            margin = max(10, abs(rmax - rmin) * 0.1)
            self.ax_reward.set_ylim(rmin - margin, rmax + margin)
        
        # 4. Stats text
        duration = datetime.now() - self.session_start
        win_pct = (self.wins / self.games_played * 100) if self.games_played > 0 else 0
        secs_per_game = duration.total_seconds() / self.games_played if self.games_played > 0 else 0
        
        avg_moves = 0
        if self.moves_history:
            recent_moves = list(self.moves_history)[-50:]
            avg_moves = sum(recent_moves) / len(recent_moves)
        
        stats = (
            f"vs {self.current_opponent}\n"
            f"{'='*15}\n\n"
            f"Total Games: {self.games_played}\n"
            f"WINS: {self.wins}\n"
            f"LOSSES: {self.losses}\n"
            f"Win Rate: {win_pct:.1f}%\n\n"
            f"Avg Moves (50): {avg_moves:.1f}\n"
            f"Secs/Game: {secs_per_game:.2f}\n\n"
            f"Duration: {int(duration.total_seconds()//60)}m {int(duration.total_seconds()%60)}s"
        )
        self.stats_text.set_text(stats)
        
        # 5. Cycle comparison - compare previous (from CSV) vs current (live)
        prev_wr = self.get_cycle_comparison(self.current_opponent)
        curr_wr = win_pct  # Use live win rate
        
        # Calculate progress
        progress_pct = min(100.0, (self.current_opponent_games / TARGET_GAMES * 100)) if TARGET_GAMES > 0 else 0.0
        progress_str = f"Progress: {progress_pct:.0f}% ({self.current_opponent_games}/{TARGET_GAMES})"
        
        if prev_wr is not None:
            diff = curr_wr - prev_wr
            sign = "+" if diff >= 0 else ""
            
            compare_str = (
                f"vs {self.current_opponent.upper()}\n"
                f"{progress_str}\n\n"
                f"{prev_wr:.1f}% → {curr_wr:.1f}%\n"
                f"({sign}{diff:.1f}%)"
            )
        else:
            compare_str = (
                f"vs {self.current_opponent.upper()}\n"
                f"{progress_str}\n\n"
                f"Current: {curr_wr:.1f}%\n"
                f"(No history)"
            )
        
        self.compare_text.set_text(compare_str)
        
        # Update title
        self.fig.suptitle(f"{ALGORITHM} {self.role} vs {self.current_opponent}", 
                         fontsize=16, fontweight='bold', color=self.color)
        
        return []


def run_dual_dashboard():
    """Run both dashboards side by side"""
    print("="*50)
    print(f"  {ALGORITHM} Dual Training Dashboard (Target: {TARGET_GAMES} games)")
    print("="*50)
    print(f"  Algorithm: {ALGORITHM}")
    print(f"  Data dir: {DATA_DIR}")
    print(f"  MrX metrics: {METRICS_FILE_MRX}")
    print(f"  Detective metrics: {METRICS_FILE_DET}")
    print("="*50)
    
    mrx_dash = RoleDashboard("MrX", METRICS_FILE_MRX, '#ff4444', 1)
    det_dash = RoleDashboard("Detective", METRICS_FILE_DET, '#00cccc', 2)
    
    try:
        mrx_manager = mrx_dash.fig.canvas.manager
        det_manager = det_dash.fig.canvas.manager
        if hasattr(mrx_manager, 'window'):
            mrx_manager.window.wm_geometry("+0+0")
            det_manager.window.wm_geometry("+520+0")
    except:
        pass
    
    ani_mrx = animation.FuncAnimation(mrx_dash.fig, mrx_dash.update, interval=2000, cache_frame_data=False)
    ani_det = animation.FuncAnimation(det_dash.fig, det_dash.update, interval=2000, cache_frame_data=False)
    
    print("\nWaiting for training data...")
    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Dual Training Dashboard")
    parser.add_argument("--algorithm", type=str, default="ppo", choices=["ppo", "mappo", "sac"],
                        help="Training algorithm to monitor (default: ppo)")
    parser.add_argument("--games", type=int, default=500, 
                        help="Target games per opponent for progress tracking (default: 500)")
    args = parser.parse_args()
    
    set_algorithm(args.algorithm, args.games)
    run_dual_dashboard()
