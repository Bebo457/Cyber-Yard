"""
PPO Agent for Detectives (Police) in Scotland Yard++
=====================================================
Based on MRXPPO.py implementation from colleagues.
Port: 5556 (Detective AI) vs 5555 (Mr X AI)

Detectives win by catching Mr X = positive reward
Detectives lose when Mr X survives = negative reward

Dependencies:
- torch
- numpy
- pyzmq
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Categorical
import zmq
import json
import numpy as np
import os
from typing import Dict, List, Any, Optional
from metrics_logger import log_game, get_current_opponent

# =================================================================
# 1. OBSERVATION ENCODER
# =================================================================
class ObservationEncoder:
    def __init__(self, max_nodes: int = 200, max_players: int = 6):
        self.max_nodes = max_nodes
        self.max_players = max_players

    def encode(self, game_state: Dict[str, Any], players: List[Dict[str, Any]], graph: Optional[Dict[str, Any]] = None):
        obs = []
        obs.append(float(game_state.get('current_round', 0)))
        obs.append(float(game_state.get('current_player_index', 0)))
        obs.append(1.0 if game_state.get('is_reveal_round', False) else 0.0)
        obs.append(float(game_state.get('mr_x_last_known_position', -1)))
        obs.append(float(game_state.get('mr_x_last_known_round', -1)))

        use_one_hot = False 
        node_count = 200

        for i in range(self.max_players):
            if i < len(players):
                p = players[i]
                pos = int(p.get('position', -1))
                obs.extend([
                    1.0 if p.get('is_mister_x', False) else 0.0, 
                    1.0 if p.get('is_visible', False) else 0.0
                ])
                t = p.get('tickets', {})
                obs.extend([float(t.get(k, 0)) for k in ['taxi','bus','metro','black']])
                
                if use_one_hot:
                    oh = [0.0] * node_count
                    if 0 <= pos < node_count: oh[pos] = 1.0
                    obs.extend(oh)
                else:
                    obs.append(float(pos) / max(1.0, float(self.max_nodes)))
            else:
                padding_len = (2 + 4 + node_count) if use_one_hot else (2 + 4 + 1)
                obs.extend([0.0] * padding_len)

        return np.array(obs, dtype=np.float32)

# =================================================================
# 2. MODELS AND AGENT
# =================================================================
class ActorCritic(nn.Module):
    def __init__(self, state_dim, action_dim):
        super(ActorCritic, self).__init__()
        self.common = nn.Sequential(
            nn.Linear(state_dim, 256), nn.ReLU(),
            nn.Linear(256, 256), nn.ReLU()
        )
        self.actor = nn.Linear(256, action_dim)
        self.critic = nn.Linear(256, 1)

    def forward(self, x):
        x = self.common(x)
        return torch.softmax(self.actor(x), dim=-1), self.critic(x)

class PPOAgent:
    def __init__(self, state_dim, action_dim, model_path=None):
        # Use script directory for model file
        if model_path is None:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            model_path = os.path.join(script_dir, "ppo_detective.pth")
        self.model_path = model_path
        self.buffer = {'s': [], 'a': [], 'lp': [], 'v': [], 'r': [], 'd': []}
        self.policy = ActorCritic(state_dim, action_dim)
        self.optimizer = optim.Adam(self.policy.parameters(), lr=3e-4)
        self.policy_old = ActorCritic(state_dim, action_dim)
        
        if os.path.exists(self.model_path):
            print(f"[Detective AI] Model loaded from: {self.model_path}")
            checkpoint = torch.load(self.model_path)
            self.policy.load_state_dict(checkpoint['model_state'])
            self.optimizer.load_state_dict(checkpoint['optimizer_state'])
        
        self.policy_old.load_state_dict(self.policy.state_dict())

    def select_action(self, state):
        with torch.no_grad():
            state_tensor = torch.FloatTensor(state)
            probs, val = self.policy_old(state_tensor)
            dist = Categorical(probs)
            action = dist.sample()
            return action.item(), dist.log_prob(action), val

    def store_transition(self, s, a, lp, v, r, d):
        self.buffer['s'].append(torch.FloatTensor(s))
        self.buffer['a'].append(torch.tensor(a))
        self.buffer['lp'].append(lp)
        self.buffer['v'].append(v)
        self.buffer['r'].append(r)
        self.buffer['d'].append(d)

    def update(self):
        if not self.buffer['r']: return
        print(f"[Detective AI] Training on {len(self.buffer['r'])} steps...")
        
        rewards = []
        discounted_reward = 0
        for r, d in zip(reversed(self.buffer['r']), reversed(self.buffer['d'])):
            if d: discounted_reward = 0
            discounted_reward = r + (0.99 * discounted_reward)
            rewards.insert(0, discounted_reward)
        
        rewards = torch.tensor(rewards, dtype=torch.float32)
        s_obs = torch.stack(self.buffer['s']).detach()
        a_obs = torch.stack(self.buffer['a']).detach()
        lp_obs = torch.stack(self.buffer['lp']).detach()
        v_obs = torch.stack(self.buffer['v']).detach().squeeze()

        for _ in range(10):
            probs, values = self.policy(s_obs)
            dist = Categorical(probs)
            logprobs = dist.log_prob(a_obs)
            ratios = torch.exp(logprobs - lp_obs)
            advantages = (rewards - v_obs).detach()
            surr1 = ratios * advantages
            surr2 = torch.clamp(ratios, 0.8, 1.2) * advantages
            loss = -torch.min(surr1, surr2).mean() + 0.5 * nn.MSELoss()(values.squeeze(), rewards)
            
            self.optimizer.zero_grad()
            loss.backward()
            self.optimizer.step()
        
        self.policy_old.load_state_dict(self.policy.state_dict())
        for k in self.buffer: self.buffer[k] = []
        torch.save({
            'model_state': self.policy.state_dict(), 
            'optimizer_state': self.optimizer.state_dict()
        }, self.model_path)


# =================================================================
# 3. MAIN LOOP (matching MRXPPO.py structure)
# =================================================================
encoder = ObservationEncoder()
agent, last_step = None, None
ctx = zmq.Context()
sock = ctx.socket(zmq.REP)
sock.bind("tcp://*:5556")  # Port 5556 for detectives (5555 is for Mr X)

print("AI PPO Detective Server listening on 5556")

current_round = 0

try:
    while True:
        msg = sock.recv()
        raw_msg = msg.decode("utf-8").strip()

        print(f"\n[RECEIVE] RAW MSG: {raw_msg}")

        try:
            req = json.loads(raw_msg)
            # Update local round tracking
            if "game_state" in req and "current_round" in req["game_state"]:
                current_round = req["game_state"]["current_round"]
        except json.JSONDecodeError:
            print(f"[ERROR] Invalid JSON received!")
            sock.send_json({"status": "error", "message": "Invalid JSON"})
            continue

        # --- HANDSHAKE ---
        if req.get("type") == "ping":
            print("[INFO] Handshake: Ping received -> Sending Pong")
            sock.send_json({"type": "pong", "status": "ready"})
            continue

        # --- GAME OVER ---
        if "[GameOver]" in raw_msg or "winner" in req or "winner" in raw_msg:
            winner = req.get("winner", "MrX" if "MrX" in raw_msg else "Detectives")
            # Use tracked round
            rounds = req.get("game_state", {}).get("current_round", current_round)
            
            print(f"[EVENT] Game Over! Winner: {winner}, Rounds: {rounds}")
            det_reward = 0.0
            if agent and last_step:
                # Detectives win = positive reward, Mr X wins = negative reward
                det_reward = 100.0 if winner == "Detectives" else -100.0
                agent.store_transition(*last_step, det_reward, True)
                agent.update()
            # Log metrics for dashboard (detective perspective)
            opponent = get_current_opponent("detective")
            log_game(winner=winner, det_reward=det_reward, rounds=rounds, training_role="detective", opponent=opponent)
            last_step = None
            current_round = 0  # Reset
            sock.send_json({"status": "ok"})
            continue

        # --- MOVE HANDLING ---
        obs = encoder.encode(req.get('game_state', {}), req.get('players', []))

        if agent is None:
            print(f"[INIT] First move - initializing agent. Obs dim: {obs.size}")
            agent = PPOAgent(obs.size, 50)

        # Get step reward from C++ (distance-based reward for detective)
        step_reward = req.get("game_state", {}).get("reward", 0.0)

        if last_step:
            print(f"[DATA] Storing transition. Reward for previous step: {step_reward}")
            agent.store_transition(*last_step, step_reward, False)

        action, lp, v = agent.select_action(obs)
        last_step = (obs, action, lp, v)

        moves = req.get("possible_moves", [])
        idx = action if action < len(moves) else 0

        print(f"[DECISION] Selected action: {idx} (from {len(moves)} possible)")

        if moves:
            sel = moves[idx]
            resp = {
                "status": "ok",
                "selected_index": idx,
                "destination": sel.get("dest"),
                "transport": sel.get("transport", 0)
            }
        else:
            resp = {"status": "ok", "selected_index": None}

        sock.send_json(resp)

except KeyboardInterrupt:
    print("\n[STOP] Server stopped by user.")
finally:
    sock.close()
    ctx.term()
