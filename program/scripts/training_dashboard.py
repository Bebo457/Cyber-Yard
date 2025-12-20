"""
Training Dashboard for Scotland Yard AI
========================================
Live visualization of training progress using matplotlib animation.

Reads metrics from AI scripts and displays:
- Win rate over time (MrX vs Detectives)
- Average reward per episode
- Episode length (rounds per game)
- Rolling averages

Run this alongside training:
    python training_dashboard.py

Dependencies:
- matplotlib
- numpy
"""

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque
import os
import json
import time
from datetime import datetime

# Configuration
METRICS_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "training_metrics.json")
MAX_POINTS = 200  # Maximum points to display
WINDOW_SIZE = 20  # Rolling average window

class TrainingDashboard:
    def __init__(self):
        # Data storage
        self.games_played = 0
        self.mrx_wins = deque(maxlen=MAX_POINTS)
        self.det_wins = deque(maxlen=MAX_POINTS)
        self.mrx_rewards = deque(maxlen=MAX_POINTS)
        self.det_rewards = deque(maxlen=MAX_POINTS)
        self.episode_lengths = deque(maxlen=MAX_POINTS)
        self.timestamps = deque(maxlen=MAX_POINTS)
        
        # Win rate tracking
        self.total_mrx_wins = 0
        self.total_det_wins = 0
        
        # Last file modification time
        self.last_mtime = 0
        
        # Setup figure
        self.setup_plot()
        
    def setup_plot(self):
        """Setup matplotlib figure with subplots"""
        plt.style.use('dark_background')
        self.fig, self.axes = plt.subplots(2, 2, figsize=(14, 9))
        self.fig.suptitle('Scotland Yard AI Training Dashboard', fontsize=16, fontweight='bold')
        
        # Subplot 1: Win Rate Over Time
        self.ax_winrate = self.axes[0, 0]
        self.ax_winrate.set_title('Win Rate (Rolling Average)')
        self.ax_winrate.set_xlabel('Games')
        self.ax_winrate.set_ylabel('Win Rate %')
        self.ax_winrate.set_ylim(0, 100)
        self.ax_winrate.grid(True, alpha=0.3)
        self.line_mrx_wr, = self.ax_winrate.plot([], [], 'r-o', label='Mr X', linewidth=2, markersize=4)
        self.line_det_wr, = self.ax_winrate.plot([], [], 'b-o', label='Detectives', linewidth=2, markersize=4)
        self.ax_winrate.legend(loc='upper right')
        
        # Subplot 2: Cumulative Wins
        self.ax_cumulative = self.axes[0, 1]
        self.ax_cumulative.set_title('Cumulative Wins')
        self.ax_cumulative.set_xlabel('Games')
        self.ax_cumulative.set_ylabel('Total Wins')
        self.ax_cumulative.grid(True, alpha=0.3)
        self.line_mrx_cum, = self.ax_cumulative.plot([], [], 'r-o', label='Mr X', linewidth=2, markersize=4)
        self.line_det_cum, = self.ax_cumulative.plot([], [], 'b-o', label='Detectives', linewidth=2, markersize=4)
        self.ax_cumulative.legend(loc='upper left')
        
        # Subplot 3: Average Reward
        self.ax_reward = self.axes[1, 0]
        self.ax_reward.set_title('Episode Reward (Rolling Average)')
        self.ax_reward.set_xlabel('Games')
        self.ax_reward.set_ylabel('Reward')
        self.ax_reward.grid(True, alpha=0.3)
        self.line_mrx_rew, = self.ax_reward.plot([], [], 'r-o', label='Mr X', linewidth=2, markersize=4)
        self.line_det_rew, = self.ax_reward.plot([], [], 'b-o', label='Detectives', linewidth=2, markersize=4)
        self.ax_reward.axhline(y=0, color='white', linestyle='--', alpha=0.5)
        self.ax_reward.legend(loc='upper right')
        
        # Subplot 4: Episode Length & Stats
        self.ax_stats = self.axes[1, 1]
        self.ax_stats.set_title('Episode Length & Statistics')
        self.ax_stats.set_xlabel('Games')
        self.ax_stats.set_ylabel('Rounds')
        self.ax_stats.grid(True, alpha=0.3)
        self.line_length, = self.ax_stats.plot([], [], 'g-o', label='Rounds per Game', linewidth=2, markersize=4)
        self.ax_stats.legend(loc='upper right')
        
        # Stats text
        self.stats_text = self.fig.text(0.02, 0.02, '', fontsize=10, family='monospace',
                                         verticalalignment='bottom')
        
        plt.tight_layout(rect=[0, 0.05, 1, 0.95])
        
    def rolling_average(self, data, window=WINDOW_SIZE):
        """Calculate rolling average"""
        if len(data) < window:
            window = max(1, len(data))
        result = []
        for i in range(len(data)):
            start = max(0, i - window + 1)
            result.append(sum(list(data)[start:i+1]) / (i - start + 1))
        return result
    
    def load_metrics(self):
        """Load metrics from JSON file"""
        if not os.path.exists(METRICS_FILE):
            return False
            
        try:
            mtime = os.path.getmtime(METRICS_FILE)
            if mtime <= self.last_mtime:
                return False  # No updates
                
            with open(METRICS_FILE, 'r') as f:
                data = json.load(f)
            
            self.last_mtime = mtime
            
            # Process new games
            games = data.get('games', [])
            if len(games) > self.games_played:
                for game in games[self.games_played:]:
                    winner = game.get('winner', 'unknown')
                    mrx_reward = game.get('mrx_reward', 0)
                    det_reward = game.get('det_reward', 0)
                    rounds = game.get('rounds', 0)
                    
                    if winner == 'MrX':
                        self.total_mrx_wins += 1
                        self.mrx_wins.append(1)
                        self.det_wins.append(0)
                    else:
                        self.total_det_wins += 1
                        self.mrx_wins.append(0)
                        self.det_wins.append(1)
                    
                    self.mrx_rewards.append(mrx_reward)
                    self.det_rewards.append(det_reward)
                    self.episode_lengths.append(rounds)
                    self.timestamps.append(datetime.now())
                    
                self.games_played = len(games)
                return True
                
        except Exception as e:
            print(f"Error loading metrics: {e}")
            
        return False
    
    def update_plot(self, frame):
        """Update plot data (called by animation)"""
        self.load_metrics()
        
        if self.games_played == 0:
            return self.line_mrx_wr, self.line_det_wr
        
        x = list(range(1, self.games_played + 1))
        
        # Win rate (rolling average)
        mrx_wr = [w * 100 for w in self.rolling_average(self.mrx_wins)]
        det_wr = [w * 100 for w in self.rolling_average(self.det_wins)]
        self.line_mrx_wr.set_data(x, mrx_wr)
        self.line_det_wr.set_data(x, det_wr)
        self.ax_winrate.set_xlim(0, max(10, self.games_played + 1))
        
        # Cumulative wins
        mrx_cum = []
        det_cum = []
        mrx_total, det_total = 0, 0
        for m, d in zip(self.mrx_wins, self.det_wins):
            mrx_total += m
            det_total += d
            mrx_cum.append(mrx_total)
            det_cum.append(det_total)
        self.line_mrx_cum.set_data(x, mrx_cum)
        self.line_det_cum.set_data(x, det_cum)
        self.ax_cumulative.set_xlim(0, max(10, self.games_played + 1))
        self.ax_cumulative.set_ylim(0, max(10, max(mrx_cum[-1] if mrx_cum else 1, det_cum[-1] if det_cum else 1) + 5))
        
        # Rewards (rolling average)
        mrx_rew_avg = self.rolling_average(self.mrx_rewards)
        det_rew_avg = self.rolling_average(self.det_rewards)
        self.line_mrx_rew.set_data(x, mrx_rew_avg)
        self.line_det_rew.set_data(x, det_rew_avg)
        self.ax_reward.set_xlim(0, max(10, self.games_played + 1))
        all_rewards = list(mrx_rew_avg) + list(det_rew_avg)
        if all_rewards:
            self.ax_reward.set_ylim(min(all_rewards) - 10, max(all_rewards) + 10)
        
        # Episode length
        ep_len_avg = self.rolling_average(self.episode_lengths)
        self.line_length.set_data(x, ep_len_avg)
        self.ax_stats.set_xlim(0, max(10, self.games_played + 1))
        if ep_len_avg:
            self.ax_stats.set_ylim(0, max(ep_len_avg) + 5)
        
        # Stats text
        total = self.total_mrx_wins + self.total_det_wins
        mrx_pct = (self.total_mrx_wins / total * 100) if total > 0 else 0
        det_pct = (self.total_det_wins / total * 100) if total > 0 else 0
        avg_len = sum(self.episode_lengths) / len(self.episode_lengths) if self.episode_lengths else 0
        
        stats = f"Games: {self.games_played} | Mr X: {self.total_mrx_wins} ({mrx_pct:.1f}%) | Detectives: {self.total_det_wins} ({det_pct:.1f}%) | Avg Rounds: {avg_len:.1f}"
        self.stats_text.set_text(stats)
        
        return self.line_mrx_wr, self.line_det_wr, self.line_mrx_cum, self.line_det_cum, self.line_mrx_rew, self.line_det_rew, self.line_length
    
    def run(self):
        """Start the dashboard"""
        print(f"Training Dashboard started. Watching: {METRICS_FILE}")
        print("Waiting for training data...")
        
        ani = animation.FuncAnimation(self.fig, self.update_plot, interval=1000, blit=False, cache_frame_data=False)
        plt.show()


if __name__ == "__main__":
    dashboard = TrainingDashboard()
    dashboard.run()
