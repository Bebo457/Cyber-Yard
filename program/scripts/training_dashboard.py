"""
Training Dashboard for Scotland Yard AI
========================================
Enhanced live visualization of training progress using matplotlib animation.

Features:
- Win rate over time (MrX vs Detectives) with rolling average
- Average reward per episode tracking
- Episode length (rounds per game)
- Training phase indicator
- Model checkpoint status
- Time-based statistics

Run alongside training:
    python training_dashboard.py

Or auto-launch with training:
    python train_ppo.py --dashboard

Dependencies:
- matplotlib
- numpy
"""

import matplotlib.pyplot as plt
import matplotlib.animation as animation
from matplotlib.patches import Rectangle
from collections import deque
import os
import json
import time
from datetime import datetime, timedelta
from pathlib import Path

# Configuration
SCRIPT_DIR = Path(__file__).parent.absolute()
METRICS_FILE = SCRIPT_DIR / "training_metrics.json"
MAX_POINTS = 500  # Maximum points to display
WINDOW_SIZE = 50  # Rolling average window
UPDATE_INTERVAL_MS = 2000  # Update every 2 seconds


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
        
        # Recent games for detailed view
        self.recent_games = deque(maxlen=20)
        
        # Last file modification time
        self.last_mtime = 0
        self.last_file_size = 0
        
        # Session timing
        self.session_start = datetime.now()
        
        # Setup figure
        self.setup_plot()
        
    def setup_plot(self):
        """Setup matplotlib figure with enhanced subplots"""
        plt.style.use('dark_background')
        
        # Create figure with custom layout
        self.fig = plt.figure(figsize=(16, 10))
        self.fig.suptitle('Cyber-Yard PPO Training Dashboard', fontsize=18, fontweight='bold', color='#00ff88')
        
        # Create grid spec for custom layout
        gs = self.fig.add_gridspec(3, 3, hspace=0.35, wspace=0.3, 
                                   left=0.06, right=0.98, top=0.92, bottom=0.08)
        
        # Subplot 1: Win Rate Over Time (large, top-left)
        self.ax_winrate = self.fig.add_subplot(gs[0, :2])
        self.ax_winrate.set_title('Win Rate (Rolling Average)', fontsize=12, fontweight='bold')
        self.ax_winrate.set_xlabel('Games')
        self.ax_winrate.set_ylabel('Win Rate %')
        self.ax_winrate.set_ylim(0, 100)
        self.ax_winrate.grid(True, alpha=0.3, linestyle='--')
        self.ax_winrate.axhline(y=50, color='white', linestyle=':', alpha=0.5, label='50% baseline')
        self.line_mrx_wr, = self.ax_winrate.plot([], [], 'r-', label='Mr X PPO', linewidth=2.5, alpha=0.9)
        self.line_det_wr, = self.ax_winrate.plot([], [], 'cyan', label='Detectives PPO', linewidth=2.5, alpha=0.9)
        self.ax_winrate.legend(loc='upper right', framealpha=0.8)
        self.ax_winrate.set_facecolor('#1a1a2e')
        
        # Subplot 2: Stats Box (top-right)
        self.ax_stats_box = self.fig.add_subplot(gs[0, 2])
        self.ax_stats_box.set_title('Session Statistics', fontsize=12, fontweight='bold')
        self.ax_stats_box.axis('off')
        self.stats_text_obj = self.ax_stats_box.text(
            0.5, 0.5, 'Waiting for data...', 
            transform=self.ax_stats_box.transAxes,
            fontsize=11, family='monospace',
            verticalalignment='center', horizontalalignment='center',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='#1f2937', edgecolor='#374151', alpha=0.9)
        )
        self.ax_stats_box.set_facecolor('#1a1a2e')
        
        # Subplot 3: Cumulative Wins (middle-left)
        self.ax_cumulative = self.fig.add_subplot(gs[1, 0])
        self.ax_cumulative.set_title('Cumulative Wins', fontsize=11, fontweight='bold')
        self.ax_cumulative.set_xlabel('Games')
        self.ax_cumulative.set_ylabel('Total Wins')
        self.ax_cumulative.grid(True, alpha=0.3, linestyle='--')
        self.line_mrx_cum, = self.ax_cumulative.plot([], [], 'r-', label='Mr X', linewidth=2)
        self.line_det_cum, = self.ax_cumulative.plot([], [], 'cyan', label='Detectives', linewidth=2)
        self.ax_cumulative.legend(loc='upper left', fontsize=9)
        self.ax_cumulative.set_facecolor('#1a1a2e')
        
        # Subplot 4: Average Reward (middle-center)
        self.ax_reward = self.fig.add_subplot(gs[1, 1])
        self.ax_reward.set_title('Episode Reward (Rolling Avg)', fontsize=11, fontweight='bold')
        self.ax_reward.set_xlabel('Games')
        self.ax_reward.set_ylabel('Reward')
        self.ax_reward.grid(True, alpha=0.3, linestyle='--')
        self.line_mrx_rew, = self.ax_reward.plot([], [], 'r-', label='Mr X', linewidth=2)
        self.line_det_rew, = self.ax_reward.plot([], [], 'cyan', label='Detectives', linewidth=2)
        self.ax_reward.axhline(y=0, color='white', linestyle=':', alpha=0.5)
        self.ax_reward.legend(loc='upper right', fontsize=9)
        self.ax_reward.set_facecolor('#1a1a2e')
        
        # Subplot 5: Episode Length (middle-right)
        self.ax_length = self.fig.add_subplot(gs[1, 2])
        self.ax_length.set_title('Game Duration (Rounds)', fontsize=11, fontweight='bold')
        self.ax_length.set_xlabel('Games')
        self.ax_length.set_ylabel('Rounds')
        self.ax_length.grid(True, alpha=0.3, linestyle='--')
        self.line_length, = self.ax_length.plot([], [], '#00ff88', label='Rounds/Game', linewidth=2)
        self.line_length_avg, = self.ax_length.plot([], [], 'yellow', label='Moving Avg', linewidth=1.5, linestyle='--')
        self.ax_length.legend(loc='upper right', fontsize=9)
        self.ax_length.set_facecolor('#1a1a2e')
        
        # Subplot 6: Recent Games Bar Chart (bottom-left, spans 2 columns)
        self.ax_recent = self.fig.add_subplot(gs[2, :2])
        self.ax_recent.set_title('Recent 20 Games', fontsize=11, fontweight='bold')
        self.ax_recent.set_xlabel('Game #')
        self.ax_recent.set_ylabel('Rounds')
        self.ax_recent.set_facecolor('#1a1a2e')
        
        # Subplot 7: Win Rate Pie Chart (bottom-right)
        self.ax_pie = self.fig.add_subplot(gs[2, 2])
        self.ax_pie.set_title('Overall Win Distribution', fontsize=11, fontweight='bold')
        self.ax_pie.set_facecolor('#1a1a2e')
        
    def rolling_average(self, data, window=WINDOW_SIZE):
        """Calculate rolling average"""
        if len(data) == 0:
            return []
        if len(data) < window:
            window = max(1, len(data))
        result = []
        for i in range(len(data)):
            start = max(0, i - window + 1)
            result.append(sum(list(data)[start:i+1]) / (i - start + 1))
        return result
    
    def load_metrics(self):
        """Load metrics from JSON file efficiently"""
        if not METRICS_FILE.exists():
            return False
            
        try:
            # Check both mtime and size for faster detection
            stat = METRICS_FILE.stat()
            mtime = stat.st_mtime
            size = stat.st_size
            
            if mtime <= self.last_mtime and size == self.last_file_size:
                return False  # No updates
                
            with open(METRICS_FILE, 'r', encoding='utf-8') as f:
                data = json.load(f)
            
            self.last_mtime = mtime
            self.last_file_size = size
            
            # Process new games
            games = data.get('games', [])
            if len(games) > self.games_played:
                for game in games[self.games_played:]:
                    winner = game.get('winner', 'unknown')
                    mrx_reward = game.get('mrx_reward', 0)
                    det_reward = game.get('det_reward', 0)
                    rounds = game.get('rounds', 0)
                    timestamp = game.get('timestamp', '')
                    
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
                    self.timestamps.append(timestamp)
                    
                    # Store recent game info
                    self.recent_games.append({
                        'winner': winner,
                        'rounds': rounds,
                        'mrx_reward': mrx_reward,
                        'det_reward': det_reward
                    })
                    
                self.games_played = len(games)
                return True
                
        except json.JSONDecodeError as e:
            print(f"JSON decode error (file may be updating): {e}")
        except Exception as e:
            print(f"Error loading metrics: {e}")
            
        return False
    
    def format_duration(self, td: timedelta) -> str:
        """Format timedelta to readable string"""
        total_seconds = int(td.total_seconds())
        hours, remainder = divmod(total_seconds, 3600)
        minutes, seconds = divmod(remainder, 60)
        if hours > 0:
            return f"{hours}h {minutes}m {seconds}s"
        elif minutes > 0:
            return f"{minutes}m {seconds}s"
        else:
            return f"{seconds}s"
    
    def update_plot(self, frame):
        """Update plot data (called by animation)"""
        self.load_metrics()
        
        if self.games_played == 0:
            return []
        
        x = list(range(1, self.games_played + 1))
        x_display = x[-MAX_POINTS:] if len(x) > MAX_POINTS else x
        
        # Win rate (rolling average)
        mrx_wr = [w * 100 for w in self.rolling_average(self.mrx_wins)]
        det_wr = [w * 100 for w in self.rolling_average(self.det_wins)]
        self.line_mrx_wr.set_data(x_display, mrx_wr[-len(x_display):])
        self.line_det_wr.set_data(x_display, det_wr[-len(x_display):])
        self.ax_winrate.set_xlim(max(1, self.games_played - MAX_POINTS), self.games_played + 5)
        
        # Cumulative wins
        mrx_cum = []
        det_cum = []
        mrx_total, det_total = 0, 0
        for m, d in zip(self.mrx_wins, self.det_wins):
            mrx_total += m
            det_total += d
            mrx_cum.append(mrx_total)
            det_cum.append(det_total)
        self.line_mrx_cum.set_data(x_display, mrx_cum[-len(x_display):])
        self.line_det_cum.set_data(x_display, det_cum[-len(x_display):])
        self.ax_cumulative.set_xlim(max(1, self.games_played - MAX_POINTS), self.games_played + 5)
        max_cum = max(mrx_cum[-1] if mrx_cum else 1, det_cum[-1] if det_cum else 1)
        self.ax_cumulative.set_ylim(0, max_cum * 1.1)
        
        # Rewards (rolling average)
        mrx_rew_avg = self.rolling_average(self.mrx_rewards)
        det_rew_avg = self.rolling_average(self.det_rewards)
        self.line_mrx_rew.set_data(x_display, mrx_rew_avg[-len(x_display):])
        self.line_det_rew.set_data(x_display, det_rew_avg[-len(x_display):])
        self.ax_reward.set_xlim(max(1, self.games_played - MAX_POINTS), self.games_played + 5)
        all_rewards = list(mrx_rew_avg) + list(det_rew_avg)
        if all_rewards:
            rmin, rmax = min(all_rewards), max(all_rewards)
            margin = max(10, (rmax - rmin) * 0.1)
            self.ax_reward.set_ylim(rmin - margin, rmax + margin)
        
        # Episode length with raw + moving average
        ep_lengths_list = list(self.episode_lengths)
        ep_len_avg = self.rolling_average(self.episode_lengths)
        self.line_length.set_data(x_display, ep_lengths_list[-len(x_display):])
        self.line_length_avg.set_data(x_display, ep_len_avg[-len(x_display):])
        self.ax_length.set_xlim(max(1, self.games_played - MAX_POINTS), self.games_played + 5)
        if ep_lengths_list:
            self.ax_length.set_ylim(0, max(ep_lengths_list) * 1.2)
        
        # Recent games bar chart
        self.ax_recent.clear()
        self.ax_recent.set_title('Recent 20 Games', fontsize=11, fontweight='bold')
        self.ax_recent.set_xlabel('Game #')
        self.ax_recent.set_ylabel('Rounds')
        self.ax_recent.set_facecolor('#1a1a2e')
        
        if self.recent_games:
            recent_list = list(self.recent_games)
            x_recent = list(range(1, len(recent_list) + 1))
            rounds = [g['rounds'] for g in recent_list]
            colors = ['red' if g['winner'] == 'MrX' else 'cyan' for g in recent_list]
            bars = self.ax_recent.bar(x_recent, rounds, color=colors, alpha=0.8, edgecolor='white', linewidth=0.5)
            self.ax_recent.set_xlim(0.5, len(recent_list) + 0.5)
            
            # Add legend
            from matplotlib.patches import Patch
            legend_elements = [
                Patch(facecolor='red', edgecolor='white', label='MrX Win'),
                Patch(facecolor='cyan', edgecolor='white', label='Detective Win')
            ]
            self.ax_recent.legend(handles=legend_elements, loc='upper right', fontsize=9)
        
        # Win rate pie chart
        self.ax_pie.clear()
        self.ax_pie.set_title('Overall Win Distribution', fontsize=11, fontweight='bold')
        self.ax_pie.set_facecolor('#1a1a2e')
        
        if self.total_mrx_wins + self.total_det_wins > 0:
            sizes = [self.total_mrx_wins, self.total_det_wins]
            labels = [f'MrX\n{self.total_mrx_wins}', f'Detectives\n{self.total_det_wins}']
            colors_pie = ['#ff4444', '#00cccc']
            explode = (0.02, 0.02)
            wedges, texts, autotexts = self.ax_pie.pie(
                sizes, labels=labels, colors=colors_pie, explode=explode,
                autopct='%1.1f%%', startangle=90, textprops={'fontsize': 10, 'color': 'white'}
            )
            for autotext in autotexts:
                autotext.set_color('white')
                autotext.set_fontweight('bold')
        
        # Stats text box
        total = self.total_mrx_wins + self.total_det_wins
        mrx_pct = (self.total_mrx_wins / total * 100) if total > 0 else 0
        det_pct = (self.total_det_wins / total * 100) if total > 0 else 0
        avg_len = sum(self.episode_lengths) / len(self.episode_lengths) if self.episode_lengths else 0
        
        # Calculate rates
        duration = datetime.now() - self.session_start
        secs_per_game = duration.total_seconds() / self.games_played if self.games_played > 0 else 0
        
        # Recent trend (last 50 games)
        recent_mrx_wr = 0
        if len(self.mrx_wins) >= 10:
            recent_mrx_wr = sum(list(self.mrx_wins)[-50:]) / min(50, len(self.mrx_wins)) * 100
        
        # Get current matchup from latest log
        matchup = self._get_current_matchup()
        
        stats_text = (
            f"  Matchup: {matchup}\n"
            f"{'─'*28}\n"
            f"  Games Played: {self.games_played}\n"
            f"  MrX Wins: {self.total_mrx_wins} ({mrx_pct:.1f}%)\n"
            f"  Det Wins: {self.total_det_wins} ({det_pct:.1f}%)\n"
            f"{'─'*28}\n"
            f"  Avg Rounds: {avg_len:.1f}\n"
            f"  Secs/Game: {secs_per_game:.2f}\n"
            f"  Recent MrX WR: {recent_mrx_wr:.1f}%\n"
            f"{'─'*28}\n"
            f"  Duration: {self.format_duration(duration)}\n"
        )
        self.stats_text_obj.set_text(stats_text)
        
        return []
    
    def _get_current_matchup(self) -> str:
        """Try to read current matchup from training logs"""
        try:
            log_dir = SCRIPT_DIR / "training_logs"
            if log_dir.exists():
                logs = sorted(log_dir.glob("training_*.log"), key=lambda x: x.stat().st_mtime, reverse=True)
                if logs:
                    log_file = logs[0]
                    file_size = log_file.stat().st_size
                    
                    # Read last 20KB of file
                    read_size = min(20000, file_size)
                    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
                        if file_size > read_size:
                            f.seek(file_size - read_size)
                        content = f.read()
                    
                    # Find all matchups in content
                    matchups = []
                    for line in content.split('\n'):
                        if 'Running' in line and 'MrX=' in line:
                            # Extract matchup info
                            idx = line.find('MrX=')
                            if idx != -1:
                                rest = line[idx:].strip()
                                # Parse "MrX=ppo vs Det=random"
                                parts = rest.split()
                                if len(parts) >= 3:
                                    matchups.append(f"{parts[0]} {parts[1]} {parts[2]}")
                    
                    if matchups:
                        return matchups[-1]  # Return most recent
                        
        except Exception as e:
            print(f"[Dashboard] Matchup read error: {e}")
        return "..."
    
    def run(self):
        """Start the dashboard"""
        print(f"{'='*52}")
        print(f"  Cyber-Yard Training Dashboard Started")
        print(f"{'='*52}")
        print(f"  Watching: {str(METRICS_FILE)[:38]}")
        print(f"  Press Ctrl+C in terminal to stop")
        print(f"{'='*52}")
        print("\nWaiting for training data...")
        
        ani = animation.FuncAnimation(
            self.fig, self.update_plot, 
            interval=UPDATE_INTERVAL_MS, 
            blit=False, 
            cache_frame_data=False
        )
        plt.show()


if __name__ == "__main__":
    dashboard = TrainingDashboard()
    dashboard.run()
