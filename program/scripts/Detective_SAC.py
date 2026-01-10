"""
Soft Actor-Critic agent for Detectives (Police) in Scotland Yard++.
Listening on port 5556 (Detective AI), mirroring MRX_SAC structure with
reward sign flipped: Detectives gain when catching Mr X.
"""
import os
import json
from collections import deque
from typing import Dict, Any, List, Optional, Tuple

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Categorical
import zmq

from metrics_logger import log_game


class ObservationEncoder:
    def __init__(self, max_nodes: int = 200, max_players: int = 6, max_moves: int = 10):
        self.max_nodes = max_nodes
        self.max_players = max_players
        self.max_moves = max_moves

    def encode(self, game_state: Dict[str, Any], players: List[Dict[str, Any]], possible_moves: Optional[List[Dict[str, Any]]] = None, graph: Optional[Dict[str, Any]] = None):
        obs: List[float] = []
        obs.append(float(game_state.get("current_round", 0)))
        obs.append(float(game_state.get("current_player_index", 0)))
        obs.append(1.0 if game_state.get("is_reveal_round", False) else 0.0)
        obs.append(float(game_state.get("mr_x_last_known_position", -1)))
        obs.append(float(game_state.get("mr_x_last_known_round", -1)))

        for i in range(self.max_players):
            if i < len(players):
                p = players[i]
                pos = int(p.get("position", -1))
                obs.extend([
                    1.0 if p.get("is_mister_x", False) else 0.0,
                    1.0 if p.get("is_visible", False) else 0.0,
                ])
                tickets = p.get("tickets", {})
                obs.extend([float(tickets.get(k, 0)) for k in ["taxi", "bus", "metro", "black"]])
                obs.append(float(pos) / max(1.0, float(self.max_nodes)))
            else:
                obs.extend([0.0] * (2 + 4 + 1))

        # Add information about available moves
        num_moves = len(possible_moves) if possible_moves else 0
        obs.append(float(num_moves) / float(self.max_moves))  # Normalized number of moves

        # Add features for each possible move
        for i in range(self.max_moves):
            if possible_moves and i < len(possible_moves):
                move = possible_moves[i]
                obs.append(float(move.get("dest", -1)) / max(1.0, float(self.max_nodes)))
                transport_map = {"taxi": 0, "bus": 1, "metro": 2, "black": 3, "ferry": 4}
                transport = move.get("transport", "taxi")
                obs.append(float(transport_map.get(transport, 0)))
            else:
                obs.extend([0.0, 0.0])

        return np.array(obs, dtype=np.float32)


class ReplayBuffer:
    def __init__(self, capacity: int = 100_000):
        self.capacity = capacity
        self.buffer: deque = deque(maxlen=capacity)

    def push(self, transition: Tuple[np.ndarray, int, float, np.ndarray, bool]):
        self.buffer.append(transition)

    def sample(self, batch_size: int):
        indices = np.random.choice(len(self.buffer), batch_size, replace=False)
        batch = [self.buffer[i] for i in indices]
        states, actions, rewards, next_states, dones = zip(*batch)
        return (
            np.stack(states),
            np.array(actions),
            np.array(rewards, dtype=np.float32),
            np.stack(next_states),
            np.array(dones, dtype=np.float32),
        )

    def __len__(self):
        return len(self.buffer)


class PolicyNetwork(nn.Module):
    def __init__(self, state_dim: int, action_dim: int):
        super().__init__()
        self.action_dim = action_dim
        self.net = nn.Sequential(
            nn.Linear(state_dim, 512), nn.ReLU(),
            nn.Linear(512, 512), nn.ReLU(),
            nn.Linear(512, 256), nn.ReLU(),
            nn.Linear(256, action_dim),
        )

    def forward(self, x: torch.Tensor, num_valid_actions: Optional[torch.Tensor] = None):
        logits = self.net(x)

        # Mask invalid actions if num_valid_actions is provided
        if num_valid_actions is not None:
            batch_size = logits.shape[0]
            for i in range(batch_size):
                n_valid = int(num_valid_actions[i].item())
                if n_valid > 0 and n_valid < self.action_dim:
                    logits[i, n_valid:] = float('-inf')

        log_probs = torch.log_softmax(logits, dim=-1)
        probs = torch.exp(log_probs)
        return probs, log_probs


class QNetwork(nn.Module):
    def __init__(self, state_dim: int, action_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(state_dim, 512), nn.ReLU(),
            nn.Linear(512, 512), nn.ReLU(),
            nn.Linear(512, 256), nn.ReLU(),
            nn.Linear(256, action_dim),
        )

    def forward(self, x: torch.Tensor):
        return self.net(x)


class DiscreteSACAgent:
    def __init__(self, state_dim: int, action_dim: int, model_path: Optional[str] = None):
        self.state_dim = state_dim
        self.action_dim = action_dim
        self.device = torch.device("cpu")

        if model_path is None:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            model_path = os.path.join(script_dir, "sac_detective.pth")
        self.model_path = model_path

        self.policy = PolicyNetwork(state_dim, action_dim).to(self.device)
        self.q1 = QNetwork(state_dim, action_dim).to(self.device)
        self.q2 = QNetwork(state_dim, action_dim).to(self.device)
        self.target_q1 = QNetwork(state_dim, action_dim).to(self.device)
        self.target_q2 = QNetwork(state_dim, action_dim).to(self.device)
        self.target_q1.load_state_dict(self.q1.state_dict())
        self.target_q2.load_state_dict(self.q2.state_dict())

        self.policy_opt = optim.Adam(self.policy.parameters(), lr=3e-4)
        self.q_opt = optim.Adam(list(self.q1.parameters()) + list(self.q2.parameters()), lr=3e-4)

        # Alpha for entropy regularization (higher = more exploration)
        self.alpha = 0.5  # Increased from 0.2 for better exploration

        self.gamma = 0.99
        self.tau = 0.01
        self.batch_size = 32
        self.update_after = 100  # Don't update until we have some experience
        self.update_every = 1
        self.epsilon = 1.0  # Epsilon-greedy for initial exploration
        self.epsilon_decay = 0.995
        self.epsilon_min = 0.1
        self.steps = 0
        self.episode_reward = 0.0  # Track episode reward for logging

        self.buffer = ReplayBuffer()

        if os.path.exists(self.model_path):
            checkpoint = torch.load(self.model_path, map_location=self.device)
            self.policy.load_state_dict(checkpoint["policy"])
            self.q1.load_state_dict(checkpoint["q1"])
            self.q2.load_state_dict(checkpoint["q2"])
            self.target_q1.load_state_dict(checkpoint["target_q1"])
            self.target_q2.load_state_dict(checkpoint["target_q2"])
            self.policy_opt.load_state_dict(checkpoint["policy_opt"])
            self.q_opt.load_state_dict(checkpoint["q_opt"])
            print(f"[Detective SAC] Loaded checkpoint: {self.model_path}")

    def select_action(self, state: np.ndarray, num_valid_actions: int) -> int:
        # Use epsilon-greedy for exploration at the beginning
        if np.random.random() < self.epsilon:
            return np.random.randint(0, min(num_valid_actions, self.action_dim))

        with torch.no_grad():
            s = torch.FloatTensor(state).to(self.device)
            valid_actions = torch.tensor([float(num_valid_actions)]).to(self.device)
            probs, _ = self.policy(s.unsqueeze(0), valid_actions)
            dist = Categorical(probs.squeeze(0))
            return int(dist.sample().item())

    def store(self, s: np.ndarray, a: int, r: float, s_next: np.ndarray, done: bool):
        self.buffer.push((s, a, r, s_next, done))
        self.steps += 1
        if len(self.buffer) >= self.update_after and self.steps % self.update_every == 0:
            self.update()
        # Decay epsilon after each step
        self.epsilon = max(self.epsilon_min, self.epsilon * self.epsilon_decay)

    def update(self):
        if len(self.buffer) < self.batch_size:
            return

        states, actions, rewards, next_states, dones = self.buffer.sample(self.batch_size)
        s = torch.FloatTensor(states).to(self.device)
        a = torch.LongTensor(actions).to(self.device)
        r = torch.FloatTensor(rewards).to(self.device)
        s_next = torch.FloatTensor(next_states).to(self.device)
        d = torch.FloatTensor(dones).to(self.device)

        with torch.no_grad():
            next_probs, next_log_probs = self.policy(s_next)
            next_q1 = self.target_q1(s_next)
            next_q2 = self.target_q2(s_next)
            next_q = torch.min(next_q1, next_q2)
            next_v = (next_probs * (next_q - self.alpha * next_log_probs)).sum(dim=1)
            target_q = r + self.gamma * (1 - d) * next_v

        current_q1 = self.q1(s).gather(1, a.view(-1, 1)).squeeze()
        current_q2 = self.q2(s).gather(1, a.view(-1, 1)).squeeze()
        q_loss = nn.MSELoss()(current_q1, target_q) + nn.MSELoss()(current_q2, target_q)

        self.q_opt.zero_grad()
        q_loss.backward()
        self.q_opt.step()

        probs, log_probs = self.policy(s)
        q_pi = torch.min(self.q1(s), self.q2(s))
        policy_loss = (probs * (self.alpha * log_probs - q_pi)).sum(dim=1).mean()

        self.policy_opt.zero_grad()
        policy_loss.backward()
        self.policy_opt.step()

        for target_param, param in zip(self.target_q1.parameters(), self.q1.parameters()):
            target_param.data.copy_(self.tau * param.data + (1 - self.tau) * target_param.data)
        for target_param, param in zip(self.target_q2.parameters(), self.q2.parameters()):
            target_param.data.copy_(self.tau * param.data + (1 - self.tau) * target_param.data)

        torch.save({
            "policy": self.policy.state_dict(),
            "q1": self.q1.state_dict(),
            "q2": self.q2.state_dict(),
            "target_q1": self.target_q1.state_dict(),
            "target_q2": self.target_q2.state_dict(),
            "policy_opt": self.policy_opt.state_dict(),
            "q_opt": self.q_opt.state_dict(),
        }, self.model_path)


def run_server():
    encoder = ObservationEncoder(max_moves=10)
    agent: Optional[DiscreteSACAgent] = None
    last_step: Optional[Tuple[np.ndarray, int, int]] = None  # (state, action, num_valid_actions)

    ctx = zmq.Context()
    sock = ctx.socket(zmq.REP)
    sock.bind("tcp://*:5556")
    print("Detective SAC Server listening on 5556")

    try:
        while True:
            raw = sock.recv().decode("utf-8").strip()
            #print(f"[RECEIVE] {raw}")

            try:
                req = json.loads(raw)
            except json.JSONDecodeError:
                sock.send_json({"status": "error", "message": "Invalid JSON"})
                continue

            if req.get("type") == "ping":
                sock.send_json({"type": "pong", "status": "ready"})
                continue

            if "[GameOver]" in raw or "winner" in req or "winner" in raw:
                winner = req.get("winner", "MrX" if "MrX" in raw else "Detectives")
                current_round = req.get("game_state", {}).get("current_round", 0)
                final_reward = 100.0 if winner == "Detectives" else -100.0
                if agent and last_step:
                    s_prev, a_prev, n_valid_prev = last_step
                    agent.store(s_prev, a_prev, final_reward, s_prev, True)
                    agent.update()

                # Log with accumulated episode reward (matching MAPPO behavior)
                total_reward = final_reward + agent.episode_reward if agent else final_reward
                log_game(winner=winner, det_reward=total_reward, rounds=current_round)
                if agent:
                    agent.episode_reward = 0.0
                last_step = None
                sock.send_json({"status": "ok"})
                continue

            obs = encoder.encode(req.get("game_state", {}), req.get("players", []), req.get("possible_moves", []))

            if agent is None:
                agent = DiscreteSACAgent(obs.size, action_dim=10)  # Max 10 possible moves
                print(f"[INIT] Detective SAC agent ready. Obs dim={obs.size}")

            step_reward = req.get("game_state", {}).get("reward", 0.0)
            # Normalize rewards to avoid large spikes
            step_reward = step_reward / 10.0 if step_reward != 0 else 0.0

            moves = req.get("possible_moves", [])
            num_valid_actions = len(moves)

            if last_step:
                s_prev, a_prev, n_valid_prev = last_step
                agent.store(s_prev, a_prev, step_reward, obs, False)

            # Accumulate episode reward for logging (matching MAPPO)
            agent.episode_reward += step_reward

            action = agent.select_action(obs, num_valid_actions)
            last_step = (obs, action, num_valid_actions)

            # Safely select move
            idx = action if action < len(moves) else (np.random.randint(0, len(moves)) if moves else 0)
            if moves:
                sel = moves[idx]
                resp = {
                    "status": "ok",
                    "selected_index": idx,
                    "destination": sel.get("dest"),
                    "transport": sel.get("transport", 0),
                }
            else:
                resp = {"status": "ok", "selected_index": None}

            sock.send_json(resp)

    except KeyboardInterrupt:
        print("[STOP] Detective SAC server stopped by user.")
    finally:
        sock.close()
        ctx.term()


if __name__ == "__main__":
    run_server()
