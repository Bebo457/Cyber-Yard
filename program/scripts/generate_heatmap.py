#!/usr/bin/env python3
"""
Heatmap generator for Scotland Yard game results.
Reads CSV logs from logs folder and generates a heatmap showing detective win percentages.
"""

import os
import re
import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from collections import defaultdict


# Algorithm name mapping
ALGORITHM_MAPPING = {
    'Random': 'Random',
    'Distmax': 'Distance Maximalization',
    'Decoy': 'Decoy Movement',
    'Montecarlo': 'Monte Carlo',
    'Dfs': 'DFS',
    'Greedy': 'Greedy',
    'Minimax': 'MiniMax',
    'Frontsearch': 'FrontSearch',
    'Sac': 'SAC',
    'Ppo': 'PPO',
    'Mappo': 'MAPPO',
}

# Fixed algorithm lists for MrX and Detectives
MRX_ALGORITHMS = [
    'Random',
    'Distance Maximalization',
    'Decoy Movement',
    'Monte Carlo',
    'DFS',
    'PPO',
    'MAPPO',
    'SAC'
]

DETECTIVE_ALGORITHMS = [
    'Random',
    'Monte Carlo',
    'MiniMax',
    'Greedy',
    'FrontSearch',
    'PPO',
    'MAPPO',
    'SAC'
]

# Skip Minimax and DFS algorithms (set to True if limited time)
SKIP_MINIMAX_DFS = True


def extract_algorithm_names(filename):
    """
    Extract MrX and Detective algorithm names from filename.
    Format: MrXAlgoVsDetectiveAlgo.csv
    Example: RandomVsGreedy.csv -> ('Random', 'Greedy')
    """
    # Remove extension
    name = filename.replace('.csv', '')
    
    # Split by 'Vs' (case-insensitive)
    parts = re.split(r'(?<!^)(?=[A-Z])', name)
    
    # Find the 'Vs' split point
    mrx_parts = []
    detective_parts = []
    found_vs = False
    
    for i, part in enumerate(parts):
        if part.lower() == 'vs':
            found_vs = True
            continue
        
        if not found_vs:
            mrx_parts.append(part)
        else:
            detective_parts.append(part)
    
    mrx_algo = ''.join(mrx_parts) if mrx_parts else None
    detective_algo = ''.join(detective_parts) if detective_parts else None
    
    return mrx_algo, detective_algo


def map_algorithm_name(algo_name):
    """Map algorithm name to display name."""
    if algo_name in ALGORITHM_MAPPING:
        return ALGORITHM_MAPPING[algo_name]
    return algo_name


def count_detective_wins(csv_file):
    """
    Count total games and detective wins from a CSV log file.
    Returns: (total_games, detective_wins, win_percentage)
    """
    try:
        df = pd.read_csv(csv_file)
        
        # Filter for GAME_END events
        game_ends = df[df['eventType'] == 'GAME_END']
        
        total_games = len(game_ends)
        
        if total_games == 0:
            return 0, 0, 0.0
        
        # Count wins where details column contains 'Detectives'
        detective_wins = len(game_ends[game_ends['details'].str.contains('Detectives', case=False, na=False)])
        
        win_percentage = (detective_wins / total_games * 100) if total_games > 0 else 0.0
        
        return total_games, detective_wins, win_percentage
    
    except Exception as e:
        print(f"Error processing {csv_file}: {e}")
        return 0, 0, 0.0


def generate_heatmap(logs_folder, output_file='detective_win_heatmap.png'):
    """
    Generate heatmap from all CSV files in logs folder.
    
    Args:
        logs_folder: Path to folder containing CSV log files
        output_file: Output filename for the heatmap image
    """
    logs_path = Path(logs_folder)
    
    if not logs_path.exists():
        print(f"Error: Logs folder not found at {logs_folder}")
        return False
    
    # Collect data: {(mrx_algo, detective_algo): win_percentage}
    results = defaultdict(list)
    
    csv_files = list(logs_path.glob('*.csv'))
    
    if not csv_files:
        print(f"No CSV files found in {logs_folder}")
        return False
    
    print(f"Processing {len(csv_files)} CSV files...")
    
    for csv_file in csv_files:
        mrx_algo, detective_algo = extract_algorithm_names(csv_file.name)
        
        if mrx_algo is None or detective_algo is None:
            print(f"Warning: Could not parse filename {csv_file.name}")
            continue
        
        # Skip Minimax and DFS if requested
        if SKIP_MINIMAX_DFS:
            if detective_algo == 'Minimax' or mrx_algo == 'Dfs':
                print(f"Skipping {csv_file.name} (Minimax/DFS)")
                continue
        
        mrx_algo_mapped = map_algorithm_name(mrx_algo)
        detective_algo_mapped = map_algorithm_name(detective_algo)
        
        total, wins, percentage = count_detective_wins(csv_file)
        
        print(f"{csv_file.name}: {round(wins)}/{round(total)} wins ({round(percentage)}%)")
        
        results[(mrx_algo_mapped, detective_algo_mapped)].append(round(percentage))
    
    if not results:
        print("No valid results to plot")
        return False
    
    # Average percentages if multiple runs for same pair
    results_avg = {k: np.mean(v) for k, v in results.items()}
    
    # Use fixed algorithm lists
    mrx_algos = MRX_ALGORITHMS.copy()
    detective_algos = DETECTIVE_ALGORITHMS.copy()
    
    # Remove Minimax and DFS if skipped
    if SKIP_MINIMAX_DFS:
        mrx_algos = [a for a in mrx_algos if a != 'DFS']
        detective_algos = [a for a in detective_algos if a != 'MiniMax']
    
    # Create matrix
    matrix = np.zeros((len(detective_algos), len(mrx_algos)))
    
    for i, det_algo in enumerate(detective_algos):
        for j, mrx_algo in enumerate(mrx_algos):
            key = (mrx_algo, det_algo)
            value = results_avg.get(key, np.nan)
            if not np.isnan(value):
                matrix[i, j] = round(value)
            else:
                matrix[i, j] = np.nan
    
    # Create heatmap
    plt.figure(figsize=(12, 8))
    ax = sns.heatmap(
        matrix,
        annot=True,
        fmt='.1f',
        cmap='RdYlGn',
        vmin=0,
        vmax=100,
        cbar_kws={'label': 'Detective Win Percentage (%)'},
        xticklabels=mrx_algos,
        yticklabels=detective_algos,
        linewidths=0.5,
        linecolor='gray',
        mask=np.isnan(matrix)
    )
    ax.set_yticklabels(ax.get_yticklabels(), rotation=0)
    
    plt.title('Detective Win Percentage by Algorithm Matchup\n(% wins per 100 games)', fontsize=16, fontweight='bold')
    plt.xlabel('MrX Algorithm', fontsize=12, fontweight='bold')
    plt.ylabel('Detective Algorithm', fontsize=12, fontweight='bold')
    plt.tight_layout()
    
    # Save figure
    output_path = Path(logs_folder).parent / output_file
    plt.savefig(output_path, dpi=300, bbox_inches='tight')
    print(f"\nHeatmap saved to: {output_path}")
    
    plt.close()
    return True


if __name__ == '__main__':
    # Default path to logs folder
    script_dir = Path(__file__).parent / 'logs'
    
    print(f"Looking for CSV files in: {script_dir}")
    generate_heatmap(str(script_dir), 'detective_win_heatmap.png')
