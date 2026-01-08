import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Categorical
import zmq
import json
import numpy as np
import os
from typing import Dict, List, Any, Optional
from metrics_logger import log_game
from copy import deepcopy

# WAŻNE: Ten kod jest uproszczonym MAPPO ze względu na to że decyzja jest podejmowana tylko przez Mr X,
# jest to zrobione po to aby kod szybciej działał i się uczył.

# =================================================================
# 1. KODER OBSERWACJI
# =================================================================
class ObservationEncoder:
    def __init__(self, max_nodes: int = 200, max_players: int = 6):
        self.max_nodes = max_nodes
        self.max_players = max_players

    def encode(self, game_state, players):
        obs = []
        obs += [
            float(game_state.get('current_round', 0)),
            float(game_state.get('current_player_index', 0)),
            1.0 if game_state.get('is_reveal_round', False) else 0.0,
            float(game_state.get('mr_x_last_known_position', -1)),
            float(game_state.get('mr_x_last_known_round', -1)),
        ]

        for i in range(self.max_players):
            if i < len(players):
                p = players[i]
                obs += [
                    1.0 if p.get('is_mister_x', False) else 0.0,
                    1.0 if p.get('is_visible', False) else 0.0,
                ]
                t = p.get('tickets', {})
                obs += [float(t.get(k, 0)) for k in ['taxi','bus','metro','black']]
                pos = p.get('position', -1)
                obs.append(float(pos) / self.max_nodes)
            else:
                obs += [0.0] * (2 + 4 + 1)

        return np.array(obs, dtype=np.float32)

# =================================================================
# 2. MODELE MAPPO
# =================================================================
class Actor(nn.Module):
    def __init__(self, obs_dim, act_dim):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(obs_dim, 256), nn.ReLU(),
            nn.Linear(256, 256), nn.ReLU(),
            nn.Linear(256, act_dim)
        )

    def forward(self, x):
        return torch.softmax(self.net(x), dim=-1)


class CentralizedCritic(nn.Module):
    def __init__(self, joint_obs_dim):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(joint_obs_dim, 256), nn.ReLU(),
            nn.Linear(256, 256), nn.ReLU(),
            nn.Linear(256, 1)
        )

    def forward(self, x):
        return self.net(x)


# =================================================================
# 3. MAPPO AGENT
# =================================================================
class MAPPOAgent:
    def __init__(self, obs_dim, act_dim, n_agents, model_path=None):
        self.n_agents = n_agents
        self.obs_dim = obs_dim
        self.act_dim = act_dim

        if model_path is None:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            model_path = os.path.join(script_dir, "mappo_mrx.pth")
        self.model_path = model_path

        self.actors = nn.ModuleList([
            Actor(obs_dim, act_dim) for _ in range(n_agents)
        ])

        self.actors_old = deepcopy(self.actors)
        for a in self.actors_old:
            a.eval()

        self.critic = CentralizedCritic(obs_dim * n_agents)

        self.optimizer = optim.Adam(
            list(self.actors.parameters()) + list(self.critic.parameters()),
            lr=3e-4
        )

        if os.path.exists(self.model_path):
            print(f"[AI] Wczytano MAPPO z: {self.model_path}")
            checkpoint = torch.load(self.model_path)
            for i, actor in enumerate(self.actors):
                actor.load_state_dict(checkpoint["actors"][i])
            self.critic.load_state_dict(checkpoint["critic"])
            self.optimizer.load_state_dict(checkpoint["optimizer"])

        self.buffer = []
        self.rollout_len = 128
        self.step_counter = 0
        self.gamma = 0.99
        self.lam = 0.95

        self.new_game = True
        self.episode_reward = 0.0
        self.round_counter = 0

    def select_action(self, obs, agent_id, action_mask):
        with torch.no_grad():
            obs_t = torch.FloatTensor(obs)
            logits = self.actors_old[agent_id].net(obs_t)

            mask = torch.tensor(action_mask, dtype=torch.float32)
            if mask.sum() <= 0:
                mask = torch.ones_like(mask)
            masked_logits = logits.masked_fill(mask < 0.5, -1e9)

            probs = torch.softmax(masked_logits, dim=-1)
            dist = Categorical(probs)
            action = dist.sample()
            return action.item(), dist.log_prob(action)

    def store(self, joint_obs, actions, logps, reward, done, action_mask):
        with torch.no_grad():
            value = self.critic(torch.FloatTensor(joint_obs)).item()
        self.buffer.append(
            (joint_obs, actions, logps, reward, done, value, action_mask)
        )
        self.step_counter += 1

    def compute_gae(self, rewards, values, dones):
        advantages = []
        gae = 0.0
        values = values + [0.0]  # V(s_{T+1}) = 0

        for t in reversed(range(len(rewards))):
            delta = rewards[t] + self.gamma * values[t+1] * (1 - dones[t]) - values[t]
            gae = delta + self.gamma * self.lam * (1 - dones[t]) * gae
            advantages.insert(0, gae)

        return torch.tensor(advantages, dtype=torch.float32)


    def update(self):
        if not self.buffer:
            return

        joint_obs, actions, logps, rewards, dones, values, masks = zip(*self.buffer)


        joint_obs = torch.from_numpy(np.stack(joint_obs)).float()
        rewards = list(rewards)
        dones = list(dones)
        values = list(values)

        advantages = self.compute_gae(rewards, values, dones)
        returns = advantages + torch.tensor(values, dtype=torch.float32)

        if advantages.numel() > 1:
            adv_mean = advantages.mean()
            adv_std = advantages.std(unbiased=False)
            if adv_std > 1e-8:
                advantages = (advantages - adv_mean) / (adv_std + 1e-8)
            else:
                advantages = advantages - adv_mean
        else:
            advantages = advantages - advantages.mean()

        loss = 0
        mask_tensor = torch.tensor(np.array(masks, dtype=np.float32))
        zero_rows = (mask_tensor.sum(dim=1, keepdim=True) <= 0)
        if zero_rows.any():
            mask_tensor = mask_tensor.masked_fill(zero_rows, 1.0)
        mask_tensor = mask_tensor.to(joint_obs.device)
        for i in range(self.n_agents):
            obs_i = joint_obs[:, i * self.obs_dim:(i + 1) * self.obs_dim]
            logits = self.actors[i].net(obs_i)

            masked_logits = logits.masked_fill(mask_tensor < 0.5, -1e9)

            dist = Categorical(logits=masked_logits)
            new_logp = dist.log_prob(torch.tensor([a[i] for a in actions]))

            ratio = torch.exp(new_logp - torch.stack([lp[i] for lp in logps]))
            surr = torch.min(
                ratio * advantages,
                torch.clamp(ratio, 0.8, 1.2) * advantages
            )
            entropy = dist.entropy().mean()
            loss -= surr.mean()
            loss -= 0.01 * entropy

        values_pred = self.critic(joint_obs).view(-1)
        loss += 0.5 * nn.MSELoss()(values_pred, returns.detach())

        self.optimizer.zero_grad()
        loss.backward()
        self.optimizer.step()
        self.buffer.clear()
        
        # Save checkpoint with retry logic
        self._save_checkpoint()
        
        for i in range(self.n_agents):
            self.actors_old[i].load_state_dict(self.actors[i].state_dict())
            self.actors_old[i].eval()

        print(f"[AI] Zapisano MAPPO do {self.model_path} | Rounds: {self.round_counter}")
    
    def _save_checkpoint(self):
        """
        Save model checkpoint with proper file handling.
        Uses BytesIO buffer to serialize first, then writes atomically.
        This ensures no file handle leaks.
        """
        import io
        
        checkpoint = {
            "actors": [a.state_dict() for a in self.actors],
            "critic": self.critic.state_dict(),
            "optimizer": self.optimizer.state_dict()
        }
        
        # Serialize to memory buffer first (no file handle involved yet)
        buffer = io.BytesIO()
        torch.save(checkpoint, buffer)
        data = buffer.getvalue()
        buffer.close()
        
        # Write to temp file with explicit close
        temp_path = self.model_path + ".tmp"
        try:
            with open(temp_path, 'wb') as f:
                f.write(data)
                f.flush()
                os.fsync(f.fileno())  # Force write to disk
            # f is now guaranteed closed
            
            # Atomic replace (on Windows, need to remove first)
            if os.path.exists(self.model_path):
                os.remove(self.model_path)
            os.rename(temp_path, self.model_path)
            
        except Exception as e:
            print(f"[ERROR] Save failed: {e}")
            # Cleanup temp file
            if os.path.exists(temp_path):
                try:
                    os.remove(temp_path)
                except:
                    pass

# =================================================================
# 4. PĘTLA ZMQ
# =================================================================
encoder = ObservationEncoder()
agent = None

ctx = zmq.Context()
sock = ctx.socket(zmq.REP)
sock.bind("tcp://*:5555")

print("AI MAPPO Server listening on 5555")

try:
    while True:
        raw_msg = sock.recv().decode("utf-8").strip()
        print(f"\n[RECEIVE] RAW MSG: {raw_msg}")

        try:
            req = json.loads(raw_msg)
        except json.JSONDecodeError:
            print("[ERROR] Invalid JSON payload from engine")
            sock.send_json({"status": "error", "message": "Invalid JSON"})
            continue

        if req.get("type") == "ping":
            print("[INFO] Handshake: ping -> pong")
            sock.send_json({"type": "pong"})
            continue

        if "winner" in req or "[GameOver]" in raw_msg:
            winner = req.get("winner", "")
            final_reward = 100.0 if winner == "MrX" else -100.0
            current_round = req.get("game_state", {}).get("current_round", 0)
            print(f"[EVENT] Game Over! Winner: {winner if winner else 'N/A'}")

            total_episode_reward = final_reward
            final_rounds = 0
            if agent:
                total_episode_reward += agent.episode_reward
                final_rounds = agent.round_counter

            if agent and agent.buffer:
                last_joint_obs, last_actions, last_logps, _, _, last_value, last_mask = agent.buffer[-1]

                agent.buffer[-1] = (
                    last_joint_obs,
                    last_actions,
                    last_logps,
                    final_reward,
                    True,
                    last_value,
                    last_mask
                )

                agent.update()
                agent.new_game = True
                agent.step_counter = 0
                print(f"[DATA] Final reward applied: {final_reward}")

            if agent:
                agent.episode_reward = 0.0
                agent.round_counter = 0

            log_game(winner=winner, mrx_reward=total_episode_reward, rounds=final_rounds)
            sock.send_json({"status": "ok"})
            continue

        obs = encoder.encode(req["game_state"], req["players"])

        if agent is None:
            n_agents = len(req["players"])
            agent = MAPPOAgent(
                obs_dim=len(obs),
                act_dim=50,
                n_agents=n_agents
            )

        moves = req.get("possible_moves", [])
        if not moves:
            print("[WARN] Engine przesłał pustą listę ruchów - brak decyzji")
            sock.send_json({"status": "ok", "selected_index": None})
            continue

        action_mask = [1.0 if i < len(moves) else 0.0 for i in range(agent.act_dim)]

        joint_obs = np.concatenate([obs for _ in range(agent.n_agents)])
        actions, logps = [], []

        for i in range(agent.n_agents):
            a, lp = agent.select_action(obs, i, action_mask)
            actions.append(a)
            logps.append(lp)

        idx = actions[0]
        if idx >= len(moves):
            print(f"[WARN] idx {idx} poza zakresem -> używam ostatniego ruchu")
            idx = len(moves) - 1
        if idx < 0:
            idx = 0
        sel = moves[idx]

        reward = req.get("game_state", {}).get("reward", 0.0)

        if agent.new_game:
            reward = 0.0
            agent.new_game = False

        agent.episode_reward += reward
        
        # Increment round counter
        current_round = req.get("game_state", {}).get("current_round", 0)
        agent.round_counter = current_round

        done = req.get("game_over", False)

        agent.store(joint_obs, actions, logps, reward, done, action_mask)
        print(f"[DATA] Stored step | Round: {agent.round_counter} | reward={reward:.3f} done={done}")

        if done or agent.step_counter >= agent.rollout_len:
            print(f"[AI] UPDATE | steps={agent.step_counter} rounds={agent.round_counter} done={done}")
            agent.update()
            agent.step_counter = 0

        sel = moves[idx] if moves else {}
        print(f"[DECISION] move #{idx} out of {len(moves)} options")
        sock.send_json({
            "status": "ok",
            "selected_index": idx,
            "destination": sel.get("dest"),
            "transport": sel.get("transport", 0)
        })

except KeyboardInterrupt:
    print("Server stopped")
finally:
    sock.close()
    ctx.term()
