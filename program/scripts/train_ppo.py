"""
PPO Training Automation Script for Cyber-Yard
==============================================
Robust training orchestration with:
- Curriculum learning against heuristic opponents
- Automatic crash recovery and error handling
- Progress logging and metrics tracking
- Background process management

Usage:
    python train_ppo.py                    # Full training with curriculum
    python train_ppo.py --quick-test       # Quick test (10 games)
    python train_ppo.py --opponent greedy  # Train vs specific opponent
    python train_ppo.py --self-play        # PPO vs PPO training

Requirements:
- torch, numpy, pyzmq (for AI servers)
- Built ScotlandYardPlusPlus.exe in program/build/bin/Release/
"""

import subprocess
import time
import os
import sys
import signal
import json
import random
import logging
import argparse
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Optional, Tuple
from dataclasses import dataclass, field
from threading import Thread, Event, Lock
from concurrent.futures import ThreadPoolExecutor
import shutil

# =============================================================================
# CONFIGURATION
# =============================================================================

def find_python_executable() -> str:
    """Find the Python executable, preferring project venv"""
    script_dir = Path(__file__).parent.absolute()
    
    # Look for venv in project root (Cyber-Yard/.venv)
    project_root = script_dir.parent.parent  # scripts -> program -> Cyber-Yard
    venv_candidates = [
        project_root / ".venv" / "Scripts" / "python.exe",  # Windows
        project_root / ".venv" / "bin" / "python",          # Linux/Mac
        script_dir.parent / ".venv" / "Scripts" / "python.exe",  # program/.venv
        script_dir.parent / ".venv" / "bin" / "python",
    ]
    
    for venv_python in venv_candidates:
        if venv_python.exists():
            return str(venv_python)
    
    # Fallback to current interpreter
    return sys.executable


@dataclass
class TrainingConfig:
    """Training configuration with sensible defaults"""
    
    # Paths (relative to script directory)
    script_dir: Path = field(default_factory=lambda: Path(__file__).parent.absolute())
    
    # Python executable (auto-detected venv or current)
    python_exe: str = field(default_factory=find_python_executable)
    
    @property
    def exe_path(self) -> Path:
        return self.script_dir.parent / "build" / "bin" / "Release" / "ScotlandYardPlusPlus.exe"
    
    @property
    def mrx_ai_script(self) -> Path:
        return self.script_dir / "MRXPPO.py"
    
    @property
    def detective_ai_script(self) -> Path:
        return self.script_dir / "DetectiveAI.py"
    
    @property
    def data_dir(self) -> Path:
        """Directory for all training data (logs, metrics, summaries)"""
        data_path = self.script_dir / "training_data"
        data_path.mkdir(exist_ok=True)
        return data_path
    
    @property
    def log_dir(self) -> Path:
        log_path = self.data_dir / "logs"
        log_path.mkdir(exist_ok=True)
        return log_path
    
    @property
    def metrics_file(self) -> Path:
        return self.data_dir / "training_metrics.json"
    
    # Training parameters
    games_per_opponent: int = 500
    games_per_batch: int = 50  # Games before switching to check progress
    
    # Error handling
    max_consecutive_failures: int = 5
    retry_delay_seconds: int = 10
    process_startup_delay: float = 3.0
    
    # Curriculum: opponents from easy to hard
    mrx_curriculum: List[str] = field(default_factory=lambda: [
        "random",      # Easy: random moves
        "greedy",      # Easy: nearest path
        "montecarlo",  # Medium: simulation-based
        "minimax",     # Medium: game tree search
        "frontsearch", # Hard: encirclement
    ])
    
    detective_curriculum: List[str] = field(default_factory=lambda: [
        "random",      # Easy: random moves
        "distmax",     # Easy: maximize distance
        "decoy",       # Medium: deceptive movement
        "montecarlo",  # Medium: simulation-based
        "dfs",         # Hard: depth-first search
    ])
    
    @property
    def summary_file(self) -> Path:
        """CSV file for high-level training summaries"""
        return self.data_dir / "training_summary.csv"


# =============================================================================
# LOGGING SETUP
# =============================================================================

def setup_logging(config: TrainingConfig) -> logging.Logger:
    """Configure logging to file and console"""
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = config.log_dir / f"training_{timestamp}.log"
    
    # Create formatter
    formatter = logging.Formatter(
        '[%(asctime)s] %(levelname)-8s %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    
    # File handler
    file_handler = logging.FileHandler(log_file, encoding='utf-8')
    file_handler.setLevel(logging.DEBUG)
    file_handler.setFormatter(formatter)
    
    # Console handler  
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.INFO)
    console_handler.setFormatter(formatter)
    
    # Configure root logger
    logger = logging.getLogger("PPOTrainer")
    logger.setLevel(logging.DEBUG)
    logger.addHandler(file_handler)
    logger.addHandler(console_handler)
    
    logger.info(f"Logging to: {log_file}")
    return logger


# =============================================================================
# PROCESS MANAGER
# =============================================================================

class ProcessManager:
    """Manages subprocess lifecycle with error handling"""
    
    def __init__(self, logger: logging.Logger):
        self.logger = logger
        self.processes: Dict[str, subprocess.Popen] = {}
        self._stop_event = Event()
        
    def start_process(self, name: str, cmd: List[str], cwd: Optional[Path] = None) -> bool:
        """Start a named subprocess"""
        try:
            self.logger.debug(f"Starting {name}: {' '.join(cmd)}")
            
            # Use CREATE_NEW_PROCESS_GROUP on Windows for proper signal handling
            kwargs = {
                'stdout': subprocess.PIPE,
                'stderr': subprocess.STDOUT,
                'cwd': str(cwd) if cwd else None,
                'text': True,
                'bufsize': 1,
            }
            
            if sys.platform == 'win32':
                kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
            
            proc = subprocess.Popen(cmd, **kwargs)
            self.processes[name] = proc
            
            # Start output reader thread
            Thread(target=self._read_output, args=(name, proc), daemon=True).start()
            
            self.logger.info(f"Started {name} (PID: {proc.pid})")
            return True
            
        except Exception as e:
            self.logger.error(f"Failed to start {name}: {e}")
            return False
    
    def _read_output(self, name: str, proc: subprocess.Popen):
        """Read and log subprocess output"""
        try:
            for line in proc.stdout:
                if self._stop_event.is_set():
                    break
                line = line.strip()
                if line:
                    # Only log important lines to avoid spam
                    if any(kw in line.lower() for kw in ['error', 'exception', 'warning', 'game over', 'winner', 'training', 'saved', 'loaded']):
                        self.logger.debug(f"[{name}] {line}")
        except Exception:
            pass
    
    def stop_process(self, name: str, timeout: float = 5.0) -> bool:
        """Stop a named subprocess gracefully"""
        if name not in self.processes:
            return True
            
        proc = self.processes[name]
        if proc.poll() is not None:
            del self.processes[name]
            return True
            
        try:
            self.logger.debug(f"Stopping {name} (PID: {proc.pid})")
            
            if sys.platform == 'win32':
                proc.terminate()
            else:
                proc.send_signal(signal.SIGTERM)
            
            try:
                proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                self.logger.warning(f"Force killing {name}")
                proc.kill()
                proc.wait()
            
            del self.processes[name]
            self.logger.info(f"Stopped {name}")
            return True
            
        except Exception as e:
            self.logger.error(f"Error stopping {name}: {e}")
            return False
    
    def is_running(self, name: str) -> bool:
        """Check if a process is still running"""
        if name not in self.processes:
            return False
        return self.processes[name].poll() is None
    
    def stop_all(self):
        """Stop all managed processes"""
        self._stop_event.set()
        for name in list(self.processes.keys()):
            self.stop_process(name)


# =============================================================================
# TRAINING SESSION
# =============================================================================

class TrainingSession:
    """Manages a complete training session"""
    
    def __init__(self, config: TrainingConfig, logger: logging.Logger):
        self.config = config
        self.logger = logger
        self.pm = ProcessManager(logger)
        self.stats = TrainingStats()
        self._interrupted = False
        
        # Register signal handlers
        signal.signal(signal.SIGINT, self._handle_interrupt)
        if hasattr(signal, 'SIGTERM'):
            signal.signal(signal.SIGTERM, self._handle_interrupt)
    
    def _handle_interrupt(self, signum, frame):
        """Handle Ctrl+C gracefully"""
        self.logger.warning("Interrupt received, shutting down...")
        self._interrupted = True
    
    def validate_environment(self) -> bool:
        """Check that all required files exist"""
        checks = [
            (self.config.exe_path, "Game executable"),
            (self.config.mrx_ai_script, "MrX PPO script"),
            (self.config.detective_ai_script, "Detective PPO script"),
        ]
        
        all_ok = True
        for path, name in checks:
            if not path.exists():
                self.logger.error(f"{name} not found: {path}")
                all_ok = False
            else:
                self.logger.debug(f"{name} OK: {path}")
        
        # Check Python
        try:
            result = subprocess.run([self.config.python_exe, "--version"], capture_output=True, text=True)
            self.logger.info(f"Python (venv): {result.stdout.strip()}")
            self.logger.info(f"Python path: {self.config.python_exe}")
        except Exception as e:
            self.logger.error(f"Python check failed: {e}")
            all_ok = False
        
        return all_ok
    
    def log_opponent_summary(self, role: str, opponent: str, games: int, wins: int, 
                             duration_secs: float, avg_reward: float = 0.0):
        """Log summary after completing training against an opponent to CSV"""
        import csv
        
        summary_file = self.config.summary_file
        file_exists = summary_file.exists()
        
        win_rate = (wins / games * 100) if games > 0 else 0.0
        secs_per_game = duration_secs / games if games > 0 else 0.0
        
        row = {
            'timestamp': datetime.now().isoformat(),
            'role': role,
            'opponent': opponent,
            'games': games,
            'wins': wins,
            'losses': games - wins,
            'win_rate': f"{win_rate:.1f}",
            'avg_reward': f"{avg_reward:.2f}",
            'secs_per_game': f"{secs_per_game:.2f}",
            'duration_mins': f"{duration_secs/60:.1f}",
        }
        
        try:
            with open(summary_file, 'a', newline='', encoding='utf-8') as f:
                writer = csv.DictWriter(f, fieldnames=row.keys())
                if not file_exists:
                    writer.writeheader()
                writer.writerow(row)
            
            self.logger.info(f"[Summary] {role} vs {opponent}: {wins}/{games} wins ({win_rate:.1f}%) in {duration_secs/60:.1f}min")
        except Exception as e:
            self.logger.error(f"Error writing summary: {e}")
    
    def start_ai_servers(self, train_mrx: bool = True, train_detective: bool = True) -> bool:
        """Start Python AI servers using project venv"""
        success = True
        python_exe = self.config.python_exe
        
        if train_mrx:
            success &= self.pm.start_process(
                "mrx_ai",
                [python_exe, str(self.config.mrx_ai_script)],
                cwd=self.config.script_dir
            )
        
        if train_detective:
            success &= self.pm.start_process(
                "detective_ai", 
                [python_exe, str(self.config.detective_ai_script)],
                cwd=self.config.script_dir
            )
        
        if success:
            self.logger.info(f"Waiting {self.config.process_startup_delay}s for AI servers to initialize...")
            time.sleep(self.config.process_startup_delay)
        
        return success
    
    def run_games(self, ai_mrx: str, ai_detectives: str, num_games: int) -> Tuple[bool, int]:
        """Run a batch of games and return (success, games_completed)"""
        cmd = [
            str(self.config.exe_path),
            "--server", "--port", "12345",
            "--training",
            "--mode", "botvbot",
            "--ai-mrx", ai_mrx,
            "--ai-detectives", ai_detectives,
            "--games", str(num_games),
        ]
        
        self.logger.info(f"Running {num_games} games: MrX={ai_mrx} vs Det={ai_detectives}")
        
        games_before = self.stats.get_total_games()
        
        if not self.pm.start_process("game", cmd, cwd=self.config.exe_path.parent):
            return False, 0
        
        # Wait for game to complete
        while self.pm.is_running("game"):
            if self._interrupted:
                self.pm.stop_process("game")
                return False, 0
            time.sleep(1.0)
        
        games_after = self._count_games_from_metrics()
        games_completed = games_after - games_before
        
        self.logger.info(f"Batch complete: {games_completed} games played")
        return True, games_completed
    
    def _count_games_from_metrics(self) -> int:
        """Count total games from metrics file"""
        try:
            if self.config.metrics_file.exists():
                with open(self.config.metrics_file, 'r') as f:
                    data = json.load(f)
                    return len(data.get('games', []))
        except Exception:
            pass
        return 0
    
    def train_role(self, role: str, opponents: List[str], games_per_opponent: int) -> bool:
        """Train a specific role (mrx or detective) against opponents"""
        self.logger.info(f"\n{'='*60}")
        self.logger.info(f"TRAINING: {role.upper()} vs {len(opponents)} opponents")
        self.logger.info(f"{'='*60}")
        
        for opponent in opponents:
            if self._interrupted:
                break
                
            self.logger.info(f"\n--- Training {role} vs {opponent} ({games_per_opponent} games) ---")
            
            games_remaining = games_per_opponent
            consecutive_failures = 0
            
            while games_remaining > 0 and not self._interrupted:
                batch_size = min(self.config.games_per_batch, games_remaining)
                
                # Set up AI configuration
                if role == "mrx":
                    ai_mrx = "ppo"
                    ai_det = opponent
                else:
                    ai_mrx = opponent
                    ai_det = "ppo"
                
                success, games_played = self.run_games(ai_mrx, ai_det, batch_size)
                
                if success and games_played > 0:
                    games_remaining -= games_played
                    consecutive_failures = 0
                    self.stats.update(role, opponent, games_played)
                    self._log_progress()
                else:
                    consecutive_failures += 1
                    self.logger.warning(f"Batch failed (attempt {consecutive_failures}/{self.config.max_consecutive_failures})")
                    
                    if consecutive_failures >= self.config.max_consecutive_failures:
                        self.logger.error(f"Too many failures, skipping {opponent}")
                        break
                    
                    # Restart AI servers after failure
                    self.logger.info("Restarting AI servers...")
                    self.pm.stop_all()
                    time.sleep(self.config.retry_delay_seconds)
                    self.start_ai_servers(train_mrx=(role=="mrx"), train_detective=(role=="detective"))
            
            self.logger.info(f"Completed training vs {opponent}")
        
        return not self._interrupted
    
    def _log_progress(self):
        """Log current training progress"""
        stats = self.stats.get_summary()
        self.logger.info(
            f"Progress: {stats['total_games']} games | "
            f"MrX wins: {stats['mrx_wins']} ({stats['mrx_winrate']:.1f}%) | "
            f"Det wins: {stats['det_wins']} ({stats['det_winrate']:.1f}%)"
        )
    
    def run_curriculum(self, train_mrx: bool = True, train_detective: bool = True):
        """Run full curriculum training"""
        self.logger.info("\n" + "="*60)
        self.logger.info("STARTING CURRICULUM TRAINING")
        self.logger.info(f"Train MrX: {train_mrx} | Train Detective: {train_detective}")
        self.logger.info(f"Games per opponent: {self.config.games_per_opponent}")
        self.logger.info("="*60 + "\n")
        
        # Validate environment
        if not self.validate_environment():
            self.logger.error("Environment validation failed!")
            return
        
        try:
            # Phase 1: Train MrX against detective heuristics
            if train_mrx and not self._interrupted:
                self.logger.info("\n" + "="*60)
                self.logger.info("PHASE 1: Training MrX PPO")
                self.logger.info("="*60)
                
                if not self.start_ai_servers(train_mrx=True, train_detective=False):
                    self.logger.error("Failed to start MrX AI server")
                    return
                
                self.train_role("mrx", self.config.mrx_curriculum, self.config.games_per_opponent)
                self.pm.stop_all()
            
            # Phase 2: Train Detective against MrX heuristics
            if train_detective and not self._interrupted:
                self.logger.info("\n" + "="*60)
                self.logger.info("PHASE 2: Training Detective PPO")
                self.logger.info("="*60)
                
                if not self.start_ai_servers(train_mrx=False, train_detective=True):
                    self.logger.error("Failed to start Detective AI server")
                    return
                
                self.train_role("detective", self.config.detective_curriculum, self.config.games_per_opponent)
                self.pm.stop_all()
            
            # Final summary
            self._log_final_summary()
            
        except Exception as e:
            self.logger.exception(f"Training error: {e}")
        finally:
            self.pm.stop_all()
            self.logger.info("Training session ended")
    
    def run_interleaved(self, games_per_switch: int = 50):
        """Run interleaved training - alternating between MrX and Detective"""
        self.logger.info("\n" + "="*60)
        self.logger.info("INTERLEAVED TRAINING: MrX and Detective alternating")
        self.logger.info(f"Games per switch: {games_per_switch}")
        self.logger.info(f"Games per opponent: {self.config.games_per_opponent}")
        self.logger.info("="*60 + "\n")
        
        if not self.validate_environment():
            self.logger.error("Environment validation failed!")
            return
        
        try:
            # Build training queue - interleave opponents
            mrx_queue = [(opp, self.config.games_per_opponent) for opp in self.config.mrx_curriculum]
            det_queue = [(opp, self.config.games_per_opponent) for opp in self.config.detective_curriculum]
            
            mrx_current = None
            det_current = None
            mrx_remaining = 0
            det_remaining = 0
            
            training_mrx = True  # Start with MrX
            
            while (mrx_queue or mrx_remaining > 0 or det_queue or det_remaining > 0) and not self._interrupted:
                
                if training_mrx:
                    # Get next MrX opponent if needed
                    if mrx_remaining <= 0 and mrx_queue:
                        mrx_current, mrx_remaining = mrx_queue.pop(0)
                        self.logger.info(f"\n--- MrX PPO vs {mrx_current} ({mrx_remaining} games total) ---")
                    
                    if mrx_remaining > 0:
                        # Start MrX AI server
                        if not self.pm.is_running("mrx_ai"):
                            self.pm.stop_all()
                            self.pm.start_process(
                                "mrx_ai",
                                [self.config.python_exe, str(self.config.mrx_ai_script)],
                                cwd=self.config.script_dir
                            )
                            time.sleep(self.config.process_startup_delay)
                        
                        batch = min(games_per_switch, mrx_remaining)
                        success, played = self.run_games("ppo", mrx_current, batch)
                        if success:
                            mrx_remaining -= played
                            self._log_progress()
                        
                    # Switch to Detective
                    if det_queue or det_remaining > 0:
                        training_mrx = False
                    elif mrx_remaining <= 0:
                        break
                        
                else:
                    # Get next Detective opponent if needed
                    if det_remaining <= 0 and det_queue:
                        det_current, det_remaining = det_queue.pop(0)
                        self.logger.info(f"\n--- Detective PPO vs {det_current} ({det_remaining} games total) ---")
                    
                    if det_remaining > 0:
                        # Start Detective AI server
                        if not self.pm.is_running("detective_ai"):
                            self.pm.stop_all()
                            self.pm.start_process(
                                "detective_ai",
                                [self.config.python_exe, str(self.config.detective_ai_script)],
                                cwd=self.config.script_dir
                            )
                            time.sleep(self.config.process_startup_delay)
                        
                        batch = min(games_per_switch, det_remaining)
                        success, played = self.run_games(det_current, "ppo", batch)
                        if success:
                            det_remaining -= played
                            self._log_progress()
                    
                    # Switch to MrX
                    if mrx_queue or mrx_remaining > 0:
                        training_mrx = True
                    elif det_remaining <= 0:
                        break
            
            self._log_final_summary()
            
        except Exception as e:
            self.logger.exception(f"Interleaved training error: {e}")
        finally:
            self.pm.stop_all()
            self.logger.info("Training session ended")
    
    def run_parallel(self):
        """Run truly parallel training - MrX and Detective train simultaneously"""
        self.logger.info("\n" + "="*60)
        self.logger.info("PARALLEL TRAINING: MrX and Detective running simultaneously")
        self.logger.info(f"Games per opponent: {self.config.games_per_opponent}")
        self.logger.info("="*60 + "\n")
        
        if not self.validate_environment():
            self.logger.error("Environment validation failed!")
            return
        
        
        # Store ProcessManagers for cleanup
        mrx_pm = None
        det_pm = None
        
        def mrx_wrapper():
            nonlocal mrx_pm
            mrx_pm = ProcessManager(self.logger)
            train_mrx_worker_impl(mrx_pm)
        
        def det_wrapper():
            nonlocal det_pm
            det_pm = ProcessManager(self.logger)
            train_detective_worker_impl(det_pm)
        
        def train_mrx_worker_impl(pm):
            """Worker thread for MrX training"""
            try:
                # Start MrX AI server
                pm.start_process(
                    "mrx_ai",
                    [self.config.python_exe, str(self.config.mrx_ai_script)],
                    cwd=self.config.script_dir
                )
                time.sleep(self.config.process_startup_delay)
                
                for opponent in self.config.mrx_curriculum:
                    if self._interrupted:
                        break
                    self.logger.info(f"\n[MrX] Training vs {opponent} ({self.config.games_per_opponent} games)")
                    
                    opponent_start = time.time()
                    games_played = 0
                    games_remaining = self.config.games_per_opponent
                    
                    while games_remaining > 0 and not self._interrupted:
                        batch = min(self.config.games_per_batch, games_remaining)
                        
                        cmd = [
                            str(self.config.exe_path),
                            "--server", "--port", "12345",
                            "--training", "--mode", "botvbot",
                            "--ai-mrx", "ppo", "--ai-detectives", opponent,
                            "--games", str(batch),
                        ]
                        
                        if pm.start_process("mrx_game", cmd, cwd=self.config.exe_path.parent):
                            while pm.is_running("mrx_game") and not self._interrupted:
                                time.sleep(1.0)
                            games_played += batch
                            games_remaining -= batch
                    
                    # Log summary for this opponent - read wins from metrics file
                    if games_played > 0:
                        try:
                            import json
                            metrics_file = self.config.script_dir / "training_metrics_mrx.json"
                            if metrics_file.exists():
                                with open(metrics_file, 'r') as f:
                                    data = json.load(f)
                                # Count recent wins (last games_played games)
                                recent = data.get('games', [])[-games_played:]
                                wins = sum(1 for g in recent if g.get('winner') == 'MrX')
                                duration = time.time() - opponent_start
                                self.log_opponent_summary("MrX", opponent, games_played, wins, duration)
                        except Exception as e:
                            self.logger.warning(f"Could not log MrX summary: {e}")
            finally:
                pm.stop_all()
                self.logger.info("[MrX] Training thread finished")
        
        def train_detective_worker_impl(pm):
            """Worker thread for Detective training"""
            try:
                # Start Detective AI server
                pm.start_process(
                    "detective_ai",
                    [self.config.python_exe, str(self.config.detective_ai_script)],
                    cwd=self.config.script_dir
                )
                time.sleep(self.config.process_startup_delay + 1)  # Slight offset
                
                for opponent in self.config.detective_curriculum:
                    if self._interrupted:
                        break
                    self.logger.info(f"\n[Detective] Training vs {opponent} ({self.config.games_per_opponent} games)")
                    
                    opponent_start = time.time()
                    games_played = 0
                    games_remaining = self.config.games_per_opponent
                    
                    while games_remaining > 0 and not self._interrupted:
                        batch = min(self.config.games_per_batch, games_remaining)
                        
                        cmd = [
                            str(self.config.exe_path),
                            "--server", "--port", "12346",
                            "--training", "--mode", "botvbot",
                            "--ai-mrx", opponent, "--ai-detectives", "ppo",
                            "--games", str(batch),
                        ]
                        
                        if pm.start_process("det_game", cmd, cwd=self.config.exe_path.parent):
                            while pm.is_running("det_game") and not self._interrupted:
                                time.sleep(1.0)
                            games_played += batch
                            games_remaining -= batch
                    
                    # Log summary for this opponent
                    if games_played > 0:
                        try:
                            import json
                            metrics_file = self.config.script_dir / "training_metrics_det.json"
                            if metrics_file.exists():
                                with open(metrics_file, 'r') as f:
                                    data = json.load(f)
                                recent = data.get('games', [])[-games_played:]
                                wins = sum(1 for g in recent if g.get('winner') == 'Detectives')
                                duration = time.time() - opponent_start
                                self.log_opponent_summary("Detective", opponent, games_played, wins, duration)
                        except Exception as e:
                            self.logger.warning(f"Could not log Detective summary: {e}")
            finally:
                pm.stop_all()
                self.logger.info("[Detective] Training thread finished")
        
        try:
            with ThreadPoolExecutor(max_workers=2) as executor:
                mrx_future = executor.submit(mrx_wrapper)
                det_future = executor.submit(det_wrapper)
                
                # Wait for both to complete, but check for interruption
                while not mrx_future.done() or not det_future.done():
                    if self._interrupted:
                        break
                    time.sleep(0.5)
                
                if not self._interrupted:
                    mrx_future.result()
                    det_future.result()
            
            self._log_final_summary()
            
        except KeyboardInterrupt:
            self.logger.info("\nCaught Ctrl+C - stopping all training...")
            self._interrupted = True
        except Exception as e:
            self.logger.exception(f"Parallel training error: {e}")
        finally:
            # Make sure all processes are killed
            if mrx_pm:
                mrx_pm.stop_all()
            if det_pm:
                det_pm.stop_all()
            self.logger.info("Parallel training session ended")
    
    def run_self_play(self, num_games: int = 1000):
        """Run PPO vs PPO self-play training"""
        self.logger.info("\n" + "="*60)
        self.logger.info("SELF-PLAY TRAINING: PPO MrX vs PPO Detective")
        self.logger.info("="*60)
        
        if not self.validate_environment():
            return
        
        try:
            if not self.start_ai_servers(train_mrx=True, train_detective=True):
                self.logger.error("Failed to start AI servers")
                return
            
            games_remaining = num_games
            while games_remaining > 0 and not self._interrupted:
                batch_size = min(self.config.games_per_batch, games_remaining)
                success, games_played = self.run_games("ppo", "ppo", batch_size)
                
                if success:
                    games_remaining -= games_played
                    self._log_progress()
                else:
                    self.logger.warning("Self-play batch failed, restarting...")
                    self.pm.stop_all()
                    time.sleep(self.config.retry_delay_seconds)
                    self.start_ai_servers()
            
        finally:
            self.pm.stop_all()
    
    def _log_final_summary(self):
        """Log final training summary"""
        self.logger.info("\n" + "="*60)
        self.logger.info("TRAINING COMPLETE - FINAL SUMMARY")
        self.logger.info("="*60)
        
        stats = self.stats.get_summary()
        self.logger.info(f"Total games played: {stats['total_games']}")
        self.logger.info(f"MrX wins: {stats['mrx_wins']} ({stats['mrx_winrate']:.1f}%)")
        self.logger.info(f"Detective wins: {stats['det_wins']} ({stats['det_winrate']:.1f}%)")
        self.logger.info(f"\nModel checkpoints saved in: {self.config.script_dir}")
        self.logger.info(f"  - ppo_mrx.pth")
        self.logger.info(f"  - ppo_detective.pth")
        self.logger.info(f"\nTraining logs: {self.config.log_dir}")
        self.logger.info("="*60)


# =============================================================================
# TRAINING STATS
# =============================================================================

class TrainingStats:
    """Track training statistics"""
    
    def __init__(self):
        self.games_by_opponent: Dict[str, int] = {}
        self.mrx_wins = 0
        self.det_wins = 0
        self.start_time = datetime.now()
    
    def update(self, role: str, opponent: str, games: int):
        """Update stats after games completed"""
        key = f"{role}_vs_{opponent}"
        self.games_by_opponent[key] = self.games_by_opponent.get(key, 0) + games
    
    def get_total_games(self) -> int:
        return sum(self.games_by_opponent.values())
    
    def get_summary(self) -> Dict:
        # Read actual stats from metrics file
        script_dir = Path(__file__).parent
        metrics_file = script_dir / "training_metrics.json"
        
        mrx_wins = 0
        det_wins = 0
        
        try:
            if metrics_file.exists():
                with open(metrics_file, 'r') as f:
                    data = json.load(f)
                    for game in data.get('games', []):
                        if game.get('winner') == 'MrX':
                            mrx_wins += 1
                        else:
                            det_wins += 1
        except Exception:
            pass
        
        total = mrx_wins + det_wins
        return {
            'total_games': total,
            'mrx_wins': mrx_wins,
            'det_wins': det_wins,
            'mrx_winrate': (mrx_wins / total * 100) if total > 0 else 0,
            'det_winrate': (det_wins / total * 100) if total > 0 else 0,
        }


# =============================================================================
# MAIN
# =============================================================================

def main():
    parser = argparse.ArgumentParser(description="PPO Training for Cyber-Yard")
    parser.add_argument("--quick-test", action="store_true", help="Quick test with 10 games")
    parser.add_argument("--opponent", type=str, help="Train vs specific opponent only")
    parser.add_argument("--self-play", action="store_true", help="PPO vs PPO self-play")
    parser.add_argument("--games", type=int, default=500, help="Games per opponent (default: 500)")
    parser.add_argument("--mrx-only", action="store_true", help="Train MrX only")
    parser.add_argument("--detective-only", action="store_true", help="Train Detective only")
    parser.add_argument("--dashboard", action="store_true", help="Also launch training dashboard")
    parser.add_argument("--reset-metrics", action="store_true", help="Clear metrics before starting")
    parser.add_argument("--parallel", action="store_true", help="Train both MrX and Detective in parallel (best CPU usage)")
    parser.add_argument("--interleaved", action="store_true", help="Alternate between MrX and Detective training")
    args = parser.parse_args()
    
    # Create config
    config = TrainingConfig()
    
    # Reset metrics if requested - clear all metric files
    if args.reset_metrics:
        metrics_files = [
            config.metrics_file,
            config.script_dir / "training_metrics_mrx.json",
            config.script_dir / "training_metrics_det.json",
        ]
        for mf in metrics_files:
            if mf.exists():
                mf.unlink()
        print(f"Cleared all metrics files")
    
    if args.games:
        config.games_per_opponent = args.games
    
    if args.quick_test:
        config.games_per_opponent = 10
        config.games_per_batch = 5
        config.mrx_curriculum = ["random"]
        config.detective_curriculum = ["random"]
    
    if args.opponent:
        config.mrx_curriculum = [args.opponent]
        config.detective_curriculum = [args.opponent]
    
    # Setup logging
    logger = setup_logging(config)
    
    # Create session
    session = TrainingSession(config, logger)
    
    # Optionally launch dashboard
    dashboard_proc = None
    if args.dashboard:
        try:
            # Use dual dashboard for parallel mode
            if args.parallel:
                dashboard_script = config.script_dir / "training_dashboard_dual.py"
            else:
                dashboard_script = config.script_dir / "training_dashboard.py"
            
            if dashboard_script.exists():
                dashboard_proc = subprocess.Popen(
                    [config.python_exe, str(dashboard_script)],
                    cwd=str(config.script_dir)
                )
                logger.info(f"Training dashboard launched: {dashboard_script.name}")
        except Exception as e:
            logger.warning(f"Could not launch dashboard: {e}")
    
    try:
        # Run appropriate training mode
        if args.self_play:
            session.run_self_play(args.games * 2)
        elif args.parallel:
            session.run_parallel()
        elif args.interleaved:
            session.run_interleaved()
        else:
            train_mrx = not args.detective_only
            train_det = not args.mrx_only
            session.run_curriculum(train_mrx=train_mrx, train_detective=train_det)
    finally:
        if dashboard_proc:
            dashboard_proc.terminate()


if __name__ == "__main__":
    main()
