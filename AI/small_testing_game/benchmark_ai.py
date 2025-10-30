"""Automated benchmarking suite for Scotland Yard AI strategies.

This module imports the playable game logic from ``mini_gra`` and provides utilities
for running large batches of AI-vs-AI matches without rendering the pygame window.
It measures win rates for both sides, aggregates supporting statistics, and writes
out a heatmap-style matrix showing how often each detective algorithm
defeats a given Mr. X strategy.
"""

from __future__ import annotations

import argparse
import json
import os
import random
import time
from collections import defaultdict
from concurrent.futures import FIRST_COMPLETED, ProcessPoolExecutor, wait
from dataclasses import dataclass
from itertools import product
from typing import Any, DefaultDict, Dict, Iterable, List, Tuple

import numpy as np

# Force pygame to use a headless video driver before mini_gra initialises it.
os.environ.setdefault("SDL_VIDEODRIVER", "dummy")

import matplotlib

matplotlib.use("Agg")  # Non-interactive backend suitable for CI and servers.
import matplotlib.pyplot as plt

from mini_gra import Game, MrXAI, PoliceAI

# Default algorithm pools. These can be overridden from the CLI if needed.
DEFAULT_MRX_ALGOS = ["random", "decoy", "dfs", "monte_carlo"]
DEFAULT_POLICE_ALGOS = ["random", "astar_greedy", "monte_carlo", "minimax", "front_encirclement"]
DEFAULT_GAMES_PER_PAIR = 5
DEFAULT_MAX_TURNS = 22
DEFAULT_WORKERS = max(os.cpu_count() or 1, 1)


@dataclass
class MatchResult:
    mr_x_algo: str
    police_algo: str
    winner: str
    win_reason: str
    turns: int
    seed: int


def simulate_game(
    mr_x_algo: str,
    police_algo: str,
    *,
    seed: int,
    max_turns: int,
    step_guard_multiplier: int = 4,
) -> MatchResult:
    """Run a single headless match and capture the outcome."""

    random.seed(seed)
    np.random.seed(seed)

    mr_x_player = MrXAI("mr_x", algorithm=mr_x_algo)
    police_players = [PoliceAI("police", algorithm=police_algo) for _ in range(5)]
    game = Game(
        mr_x_player,
        police_players,
        mr_x_algorithm=mr_x_algo,
        police_algorithm=police_algo,
        quiet=True,
    )
    game.max_turns = max_turns

    max_steps = max_turns * step_guard_multiplier * 2 + 10
    steps = 0
    while game.running and steps < max_steps:
        game.execute_ai_turn()
        steps += 1

    if game.running:
        # Fail-safe: treat unresolved matches as unknown outcomes.
        game.running = False
        if game.winner is None:
            game.winner = "unknown"
            game.win_reason = "step_guard_exceeded"

    return MatchResult(
        mr_x_algo=mr_x_algo,
        police_algo=police_algo,
        winner=game.winner or "unknown",
        win_reason=game.win_reason or "unknown",
        turns=game.turn_number,
        seed=seed,
    )


def _simulate_game_task(payload: Tuple[str, str, int, int, int]) -> MatchResult:
    mr_x_algo, police_algo, seed, max_turns, step_guard_multiplier = payload
    return simulate_game(
        mr_x_algo,
        police_algo,
        seed=seed,
        max_turns=max_turns,
        step_guard_multiplier=step_guard_multiplier,
    )


def run_benchmark(
    mr_x_algos: Iterable[str],
    police_algos: Iterable[str],
    *,
    games_per_pair: int,
    max_turns: int,
    base_seed: int,
    workers: int,
    step_guard_multiplier: int = 4,
) -> Tuple[
    List[MatchResult],
    DefaultDict[Tuple[str, str], Dict[str, Any]],
    np.ndarray,
    np.ndarray,
    np.ndarray,
    DefaultDict[str, Dict[str, Any]],
    DefaultDict[str, Dict[str, Any]],
]:
    """Execute the full benchmark grid and compute intermediate aggregates."""

    mr_x_algos = list(mr_x_algos)
    police_algos = list(police_algos)

    results: List[MatchResult] = []
    pair_stats: DefaultDict[Tuple[str, str], Dict[str, Any]] = defaultdict(
        lambda: {
            "games": 0,
            "police_wins": 0,
            "mr_x_wins": 0,
            "unknown": 0,
            "turn_sum": 0.0,
            "reasons": defaultdict(int),
        }
    )

    mr_x_overall: DefaultDict[str, Dict[str, Any]] = defaultdict(
        lambda: {"games": 0, "wins": 0, "timeouts": 0, "unknown": 0, "turn_sum": 0.0}
    )
    police_overall: DefaultDict[str, Dict[str, Any]] = defaultdict(
        lambda: {"games": 0, "wins": 0, "captures": 0, "unknown": 0, "turn_sum": 0.0}
    )

    mrx_index = {algo: idx for idx, algo in enumerate(mr_x_algos)}
    police_index = {algo: idx for idx, algo in enumerate(police_algos)}
    police_wins_matrix = np.zeros((len(mr_x_algos), len(police_algos)), dtype=float)
    games_matrix = np.zeros_like(police_wins_matrix)
    turn_sum_matrix = np.zeros_like(police_wins_matrix)

    tasks: List[Tuple[str, str, int, int, int]] = []
    for pair_idx, (mr_x_algo, police_algo) in enumerate(product(mr_x_algos, police_algos)):
        for game_idx in range(games_per_pair):
            seed = base_seed + pair_idx * 1000 + game_idx
            tasks.append((mr_x_algo, police_algo, seed, max_turns, step_guard_multiplier))

    total_games = len(tasks)
    if total_games == 0:
        print("[Benchmark] No games scheduled; check selected algorithms or games per pair.", flush=True)
        return (
            results,
            pair_stats,
            police_wins_matrix,
            games_matrix,
            turn_sum_matrix,
            police_overall,
            mr_x_overall,
        )

    print(
        f"[Benchmark] Dispatching {total_games} games across {len(mr_x_algos)}x{len(police_algos)} matchups.",
        flush=True,
    )

    completed_games = 0
    progress_interval = max(5.0, min(30.0, max_turns / 2.0 if max_turns > 0 else 5.0))
    last_progress_notice = time.time()
    pair_start_times: Dict[Tuple[str, str], float] = {}

    def emit_progress(now: float, *, force: bool = False) -> None:
        nonlocal last_progress_notice
        if total_games == 0:
            return
        if not force and (now - last_progress_notice) < progress_interval:
            return
        percent = (completed_games / total_games) * 100.0 if total_games else 0.0
        active_pairs = [
            f"{mr}->{pol} {stats['games']}/{games_per_pair}"
            for (mr, pol), stats in pair_stats.items()
            if stats["games"] < games_per_pair
        ]
        if len(active_pairs) > 3:
            active_summary = ", ".join(active_pairs[:3]) + f", +{len(active_pairs) - 3} more"
        elif active_pairs:
            active_summary = ", ".join(active_pairs)
        else:
            active_summary = "none"
        print(
            f"[Progress] {completed_games}/{total_games} games done ({percent:.1f}%). Active pairs: {active_summary}",
            flush=True,
        )
        last_progress_notice = now

    def process_match(match: MatchResult, now: float) -> None:
        nonlocal completed_games
        results.append(match)
        completed_games += 1

        key = (match.mr_x_algo, match.police_algo)
        if key not in pair_start_times:
            pair_start_times[key] = now
            print(
                f"[Pair start] {match.mr_x_algo} vs {match.police_algo} kicked off ({games_per_pair} games).",
                flush=True,
            )

        stats = pair_stats[key]
        stats["games"] += 1
        stats["turn_sum"] += match.turns
        stats["reasons"][match.win_reason] += 1

        mr_total = mr_x_overall[match.mr_x_algo]
        mr_total["games"] += 1
        mr_total["turn_sum"] += match.turns

        police_total = police_overall[match.police_algo]
        police_total["games"] += 1
        police_total["turn_sum"] += match.turns

        i = mrx_index[match.mr_x_algo]
        j = police_index[match.police_algo]
        games_matrix[i, j] += 1
        turn_sum_matrix[i, j] += match.turns

        if match.winner == "police":
            stats["police_wins"] += 1
            police_total["wins"] += 1
            if match.win_reason == "capture":
                police_total["captures"] += 1
            police_wins_matrix[i, j] += 1
        elif match.winner == "mr_x":
            stats["mr_x_wins"] += 1
            mr_total["wins"] += 1
            if match.win_reason == "timeout":
                mr_total["timeouts"] += 1
        else:
            stats["unknown"] += 1
            mr_total["unknown"] += 1
            police_total["unknown"] += 1

        avg_turns_so_far = stats["turn_sum"] / stats["games"] if stats["games"] else 0.0
        print(
            f"[Result] {match.mr_x_algo} vs {match.police_algo} "
            f"game {stats['games']}/{games_per_pair}: winner={match.winner} "
            f"({match.win_reason}), turns={match.turns}, seed={match.seed}, avg turns so far={avg_turns_so_far:.2f}",
            flush=True,
        )

        if stats["games"] == games_per_pair:
            pair_elapsed = now - pair_start_times.pop(key, now)
            police_win_rate = stats["police_wins"] / games_per_pair if games_per_pair else 0.0
            print(
                f"[Pair complete] {match.mr_x_algo} vs {match.police_algo}: "
                f"{games_per_pair} games in {pair_elapsed:.2f}s; police win rate={police_win_rate:.1%}, "
                f"mr_x wins={stats['mr_x_wins']}, police wins={stats['police_wins']}, unknown={stats['unknown']}, "
                f"avg turns={stats['turn_sum'] / games_per_pair if games_per_pair else 0.0:.2f}",
                flush=True,
            )

        emit_progress(now)

    executor = None
    try:
        if workers <= 1 or total_games <= 1:
            print("[Benchmark] Running sequentially (workers=1).", flush=True)
            for payload in tasks:
                match = _simulate_game_task(payload)
                process_match(match, time.time())
        else:
            max_workers = min(workers, len(tasks))
            print(f"[Benchmark] Using up to {max_workers} worker processes.", flush=True)
            executor = ProcessPoolExecutor(max_workers=max_workers)
            future_to_payload = {executor.submit(_simulate_game_task, payload): payload for payload in tasks}
            pending = set(future_to_payload.keys())
            while pending:
                done, pending = wait(pending, timeout=progress_interval, return_when=FIRST_COMPLETED)
                if not done:
                    emit_progress(time.time(), force=True)
                    continue
                for future in done:
                    payload = future_to_payload.pop(future)
                    try:
                        match = future.result()
                    except Exception as exc:  # noqa: PERF203 - provide helpful context before propagating
                        mr_x_algo, police_algo, seed, *_ = payload
                        print(
                            f"[Error] Simulation failed for {mr_x_algo} vs {police_algo} (seed {seed}): {exc}",
                            flush=True,
                        )
                        raise
                    process_match(match, time.time())
    finally:
        emit_progress(time.time(), force=True)
        if executor is not None:
            executor.shutdown(wait=True)

    return (
        results,
        pair_stats,
        police_wins_matrix,
        games_matrix,
        turn_sum_matrix,
        police_overall,
        mr_x_overall,
    )


def compute_rates(numerator: np.ndarray, denominator: np.ndarray) -> np.ndarray:
    with np.errstate(divide="ignore", invalid="ignore"):
        rates = np.true_divide(numerator, denominator)
        rates[~np.isfinite(rates)] = 0.0
    return rates


def plot_heatmap(matrix: np.ndarray, mr_x_algos: List[str], police_algos: List[str], output_path: str) -> None:
    fig, ax = plt.subplots(figsize=(1.2 * len(police_algos) + 2, 1.2 * len(mr_x_algos) + 1.5))
    im = ax.imshow(matrix, vmin=0.0, vmax=1.0, cmap="Blues")

    ax.set_xticks(range(len(police_algos)))
    ax.set_yticks(range(len(mr_x_algos)))
    ax.set_xticklabels(police_algos, rotation=45, ha="right")
    ax.set_yticklabels(mr_x_algos)
    ax.set_xlabel("Detective algorithm")
    ax.set_ylabel("Mr. X algorithm")
    ax.set_title("Police win rate heatmap")

    for i in range(len(mr_x_algos)):
        for j in range(len(police_algos)):
            value = matrix[i, j]
            label = f"{value * 100:.1f}%"
            ax.text(j, i, label, ha="center", va="center", color="white" if value > 0.6 else "black", fontsize=9)

    cbar = fig.colorbar(im, ax=ax, fraction=0.046, pad=0.04)
    cbar.set_label("Police win rate")
    fig.tight_layout()
    fig.savefig(output_path, dpi=200)
    plt.close(fig)


def build_ranking(overall_stats: Dict[str, Dict[str, Any]], *, side: str) -> List[Dict[str, Any]]:
    ranking: List[Dict[str, Any]] = []
    for algo, stats in overall_stats.items():
        games = stats["games"]
        wins = stats["wins"]
        win_rate = wins / games if games else 0.0
        avg_turns = stats["turn_sum"] / games if games else 0.0
        entry = {
            "algorithm": algo,
            "games": games,
            "wins": wins,
            "win_rate": win_rate,
            "average_turns": avg_turns,
        }
        if side == "police":
            entry["capture_rate"] = (stats["captures"] / games) if games else 0.0
            entry["unknown_rate"] = (stats["unknown"] / games) if games else 0.0
        else:
            entry["timeout_rate"] = (stats["timeouts"] / games) if games else 0.0
            entry["unknown_rate"] = (stats["unknown"] / games) if games else 0.0
        ranking.append(entry)

    ranking.sort(key=lambda item: item["win_rate"], reverse=True)
    return ranking


def serialise_pair_stats(pair_stats: Dict[Tuple[str, str], Dict[str, Any]]) -> List[Dict[str, Any]]:
    serialised: List[Dict[str, Any]] = []
    for (mr_x_algo, police_algo), stats in sorted(pair_stats.items()):
        games = stats["games"] or 1
        serialised.append(
            {
                "mr_x_algo": mr_x_algo,
                "police_algo": police_algo,
                "games": stats["games"],
                "police_wins": stats["police_wins"],
                "mr_x_wins": stats["mr_x_wins"],
                "unknown": stats["unknown"],
                "average_turns": stats["turn_sum"] / games,
                "reasons": dict(stats["reasons"]),
            }
        )
    return serialised


def format_ranking_table(ranking: List[Dict[str, Any]], *, side: str) -> str:
    headers = ["Algorithm", "Games", "Wins", "Win%", "Avg Turns", "Outcome Notes"]
    lines = [" | ".join(headers), " | ".join("-" * len(h) for h in headers)]
    for entry in ranking:
        outcome = (
            f"captures: {entry['capture_rate']:.2f}" if side == "police" else f"timeouts: {entry['timeout_rate']:.2f}"
        )
        outcome += f", unknown: {entry['unknown_rate']:.2f}"
        lines.append(
            " | ".join(
                [
                    entry["algorithm"],
                    f"{entry['games']}",
                    f"{entry['wins']}",
                    f"{entry['win_rate'] * 100:.2f}",
                    f"{entry['average_turns']:.2f}",
                    outcome,
                ]
            )
        )
    return "\n".join(lines)


def main() -> None:
    parser = argparse.ArgumentParser(description="Benchmark Scotland Yard AI strategies")
    parser.add_argument("--games", type=int, default=DEFAULT_GAMES_PER_PAIR, help="Matches per algorithm pairing")
    parser.add_argument("--max-turns", type=int, default=DEFAULT_MAX_TURNS, help="Turn cap per match")
    parser.add_argument("--seed", type=int, default=1337, help="Base RNG seed for reproducibility")
    parser.add_argument(
        "--workers",
        type=int,
        default=DEFAULT_WORKERS,
        help="Number of parallel worker processes to use (defaults to CPU count)",
    )
    parser.add_argument(
        "--mr-x",
        dest="mr_x_algos",
        nargs="*",
        default=DEFAULT_MRX_ALGOS,
        help="Subset of Mr. X algorithms to benchmark (defaults to all)",
    )
    parser.add_argument(
        "--police",
        dest="police_algos",
        nargs="*",
        default=DEFAULT_POLICE_ALGOS,
        help="Subset of police algorithms to benchmark (defaults to all)",
    )
    parser.add_argument(
        "--heatmap",
        default="benchmark_matrix.png",
        help="Destination path for the matrix heatmap",
    )
    parser.add_argument(
        "--summary",
        default="benchmark_results.json",
        help="Destination path for the JSON summary report",
    )
    args = parser.parse_args()

    start_time = time.time()
    print(
        f"Running benchmark: {len(args.mr_x_algos)} Mr. X algos x {len(args.police_algos)} police algos, "
        f"{args.games} games/pair, workers={args.workers}"
    )
    (
        results,
        pair_stats,
        police_wins_matrix,
        games_matrix,
        turn_sum_matrix,
        police_overall,
        mr_x_overall,
    ) = run_benchmark(
        args.mr_x_algos,
        args.police_algos,
        games_per_pair=args.games,
        max_turns=args.max_turns,
        base_seed=args.seed,
        workers=args.workers,
    )
    elapsed = time.time() - start_time

    police_win_rates = compute_rates(police_wins_matrix, games_matrix)
    average_turns = compute_rates(turn_sum_matrix, games_matrix)

    plot_heatmap(police_win_rates, args.mr_x_algos, args.police_algos, args.heatmap)

    police_ranking = build_ranking(police_overall, side="police")
    mr_x_ranking = build_ranking(mr_x_overall, side="mr_x")

    summary = {
        "config": {
            "games_per_pair": args.games,
            "max_turns": args.max_turns,
            "seed": args.seed,
            "workers": args.workers,
            "mr_x_algorithms": list(args.mr_x_algos),
            "police_algorithms": list(args.police_algos),
        },
        "run_stats": {
            "total_games": len(results),
            "elapsed_seconds": elapsed,
        },
        "pairings": serialise_pair_stats(pair_stats),
        "police_ranking": police_ranking,
        "mr_x_ranking": mr_x_ranking,
        "average_turns_matrix": average_turns.tolist(),
        "police_win_rate_matrix": police_win_rates.tolist(),
    }

    with open(args.summary, "w", encoding="utf-8") as fh:
        json.dump(summary, fh, indent=2)

    print(f"Benchmark finished in {elapsed:.2f}s across {len(results)} games.")
    print(f"Heatmap saved to: {args.heatmap}")
    print(f"Summary saved to: {args.summary}")
    print()
    print("Police ranking (win rate):")
    print(format_ranking_table(police_ranking, side="police"))
    print()
    print("Mr. X ranking (win rate):")
    print(format_ranking_table(mr_x_ranking, side="mr_x"))


if __name__ == "__main__":
    main()
