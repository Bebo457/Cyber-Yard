"""
True Multi-Agent MAPPO for Detectives (Police) in Cyber-Yard
=============================================================
FULL MAPPO implementation with CTDE (Centralized Training, Decentralized Execution)

Key differences from Bonk's simplified MRX_MAPPO:
1. Each detective has its OWN Actor network with LOCAL observations
2. Centralized Critic sees JOINT observations from ALL agents
3. Per-agent action selection based on current_player_index
4. Proper multi-agent experience buffer with per-agent data
5. Credit assignment through centralized value function

Architecture:
- N Actors (one per detective) - decentralized execution
- 1 Centralized Critic - centralized training
- Joint observation = concatenation of all agents' observations
- Each agent's Actor only sees its own local observation

Port: 5556 (shared with other detective AI servers)
Reward: Detectives win = +1000, lose = -1000
"""

import torch
import torch.nn as nn
import torch.optim as optim
from torch.distributions import Categorical
import zmq
import json
import numpy as np
import os
from typing import Dict, List, Any, Optional, Tuple
from metrics_logger import log_game
from copy import deepcopy


# =================================================================
# 1. OBSERVATION ENCODER
# =================================================================
class ObservationEncoder:
    """Encodes game state into observation vectors."""
    
    def __init__(self, max_nodes: int = 200, max_players: int = 6):
        self.max_nodes = max_nodes
        self.max_players = max_players
    
    def encode_global(self, game_state: Dict, players: List[Dict]) -> np.ndarray:
        """Encode global game state (shared info for all agents)."""
        obs = [
            float(game_state.get('current_round', 0)) / 24.0,  # Normalize
            1.0 if game_state.get('is_reveal_round', False) else 0.0,
            float(game_state.get('mr_x_last_known_position', -1)) / self.max_nodes,
            float(game_state.get('mr_x_last_known_round', -1)) / 24.0,
        ]
        return np.array(obs, dtype=np.float32)
    
    def encode_agent(self, game_state: Dict, players: List[Dict], agent_idx: int) -> np.ndarray:
        """
        Encode observation from the perspective of a specific agent.
        This is the LOCAL observation that the agent's Actor will see.
        """
        obs = []
        
        # Global state info
        obs.extend(self.encode_global(game_state, players).tolist())
        
        # Current agent's own state (emphasized - this is MY state)
        if agent_idx < len(players):
            p = players[agent_idx]
            obs.extend([
                float(p.get('position', -1)) / self.max_nodes,
                1.0 if p.get('is_mister_x', False) else 0.0,
                1.0 if p.get('is_visible', False) else 0.0,
            ])
            t = p.get('tickets', {})
            obs.extend([
                float(t.get('taxi', 0)) / 10.0,
                float(t.get('bus', 0)) / 8.0,
                float(t.get('metro', 0)) / 4.0,
            ])
        else:
            obs.extend([0.0] * 6)
        
        # Other agents' states (teammates and Mr. X)
        for i in range(self.max_players):
            if i == agent_idx:
                continue  # Skip self, already encoded above
            if i < len(players):
                p = players[i]
                obs.extend([
                    float(p.get('position', -1)) / self.max_nodes if p.get('is_visible', True) else -1.0,
                    1.0 if p.get('is_mister_x', False) else 0.0,
                    1.0 if p.get('is_visible', False) else 0.0,
                ])
                t = p.get('tickets', {})
                obs.extend([
                    float(t.get('taxi', 0)) / 10.0,
                    float(t.get('bus', 0)) / 8.0,
                    float(t.get('metro', 0)) / 4.0,
                ])
            else:
                obs.extend([0.0] * 6)
        
        return np.array(obs, dtype=np.float32)
    
    def encode_joint(self, game_state: Dict, players: List[Dict], n_agents: int) -> np.ndarray:
        """
        Encode JOINT observation for the Centralized Critic.
        This concatenates all agents' individual observations.
        """
        joint = []
        for i in range(n_agents):
            agent_obs = self.encode_agent(game_state, players, i)
            joint.extend(agent_obs.tolist())
        return np.array(joint, dtype=np.float32)


# =================================================================
# 2. NEURAL NETWORK MODELS
# =================================================================
class Actor(nn.Module):
    """
    Decentralized Actor network - one per agent.
    Takes LOCAL observation, outputs action probabilities.
    """
    def __init__(self, obs_dim: int, act_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(obs_dim, 256), nn.ReLU(),
            nn.Linear(256, 256), nn.ReLU(),
            nn.Linear(256, act_dim)
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)  # Return logits, apply softmax later with masking


class CentralizedCritic(nn.Module):
    """
    Centralized Critic network - shared by all agents.
    Takes JOINT observation (all agents' obs concatenated), outputs value estimate.
    """
    def __init__(self, joint_obs_dim: int):
        super().__init__()
        self.net = nn.Sequential(
            nn.Linear(joint_obs_dim, 512), nn.ReLU(),
            nn.Linear(512, 256), nn.ReLU(),
            nn.Linear(256, 1)
        )

    def forward(self, x: torch.Tensor) -> torch.Tensor:
        return self.net(x)


# =================================================================
# 3. EXPERIENCE BUFFER FOR MULTI-AGENT
# =================================================================
class MultiAgentBuffer:
    """
    Experience buffer that properly stores per-agent transitions.
    Stores experiences for a full round (all detectives moved).
    """
    def __init__(self):
        self.clear()
    
    def clear(self):
        self.rounds = []  # List of round data
        self.current_round = {}  # agent_id -> (obs, action, logp, mask)
    
    def store_agent_step(self, agent_id: int, obs: np.ndarray, action: int, 
                          logp: torch.Tensor, action_mask: List[float]):
        """Store a single agent's step within a round."""
        self.current_round[agent_id] = {
            'obs': obs,
            'action': action,
            'logp': logp,
            'mask': action_mask
        }
    
    def complete_round(self, joint_obs: np.ndarray, reward: float, done: bool, value: float):
        """
        Called when all detectives have moved (end of round).
        Stores the joint experience.
        """
        if self.current_round:
            self.rounds.append({
                'agent_data': dict(self.current_round),
                'joint_obs': joint_obs,
                'reward': reward,
                'done': done,
                'value': value
            })
            self.current_round = {}
    
    def __len__(self):
        return len(self.rounds)


# =================================================================
# 4. MAPPO AGENT
# =================================================================
class MAPPODetectiveAgent:
    """
    True Multi-Agent PPO with Centralized Training, Decentralized Execution.
    
    - Each detective has its own Actor (decentralized execution)
    - All detectives share a Centralized Critic (centralized training)
    - Joint observations are used for value estimation
    - Individual observations are used for action selection
    """
    
    def __init__(self, local_obs_dim: int, act_dim: int, n_detectives: int, model_path: str = None):
        self.n_detectives = n_detectives
        self.local_obs_dim = local_obs_dim
        self.joint_obs_dim = local_obs_dim * n_detectives
        self.act_dim = act_dim

        if model_path is None:
            script_dir = os.path.dirname(os.path.abspath(__file__))
            model_path = os.path.join(script_dir, "mappo_detective.pth")
        self.model_path = model_path

        # Decentralized Actors - one per detective
        self.actors = nn.ModuleList([
            Actor(local_obs_dim, act_dim) for _ in range(n_detectives)
        ])
        
        # Old actors for PPO ratio computation
        self.actors_old = deepcopy(self.actors)
        for a in self.actors_old:
            a.eval()

        # Centralized Critic - shared
        self.critic = CentralizedCritic(self.joint_obs_dim)

        # Single optimizer for all networks
        self.optimizer = optim.Adam(
            list(self.actors.parameters()) + list(self.critic.parameters()),
            lr=3e-4
        )

        # Load checkpoint if exists
        if os.path.exists(self.model_path):
            print(f"[Detective MAPPO] Loading model from: {self.model_path}")
            checkpoint = torch.load(self.model_path, weights_only=False)
            for i, actor in enumerate(self.actors):
                if i < len(checkpoint.get("actors", [])):
                    actor.load_state_dict(checkpoint["actors"][i])
            self.critic.load_state_dict(checkpoint["critic"])
            if "optimizer" in checkpoint:
                self.optimizer.load_state_dict(checkpoint["optimizer"])
            # Sync old actors
            for i in range(self.n_detectives):
                self.actors_old[i].load_state_dict(self.actors[i].state_dict())

        # Experience buffer
        self.buffer = MultiAgentBuffer()
        
        # Hyperparameters
        self.gamma = 0.99
        self.lam = 0.95
        self.clip_eps = 0.2
        self.entropy_coef = 0.01
        self.value_coef = 0.5
        self.max_grad_norm = 0.5
        self.ppo_epochs = 4
        
        # Episode tracking
        self.episode_reward = 0.0
        self.new_game = True

    def select_action(self, local_obs: np.ndarray, detective_idx: int, 
                       action_mask: List[float]) -> Tuple[int, torch.Tensor]:
        """
        Select action for a specific detective using its own Actor.
        This is DECENTRALIZED execution - each agent uses only local observation.
        """
        with torch.no_grad():
            obs_t = torch.FloatTensor(local_obs)
            logits = self.actors_old[detective_idx](obs_t)
            
            # Apply action masking
            mask = torch.tensor(action_mask, dtype=torch.float32)
            if mask.sum() <= 0:
                mask = torch.ones_like(mask)
            masked_logits = logits.masked_fill(mask < 0.5, -1e9)
            
            probs = torch.softmax(masked_logits, dim=-1)
            dist = Categorical(probs)
            action = dist.sample()
            
            return action.item(), dist.log_prob(action)

    def compute_gae(self, rewards: List[float], values: List[float], 
                     dones: List[bool]) -> torch.Tensor:
        """Compute Generalized Advantage Estimation."""
        advantages = []
        gae = 0.0
        values = values + [0.0]  # Bootstrap value

        for t in reversed(range(len(rewards))):
            delta = rewards[t] + self.gamma * values[t+1] * (1 - dones[t]) - values[t]
            gae = delta + self.gamma * self.lam * (1 - dones[t]) * gae
            advantages.insert(0, gae)

        return torch.tensor(advantages, dtype=torch.float32)

    def update(self):
        """
        Update all Actors and the Centralized Critic using collected experiences.
        This is CENTRALIZED training.
        """
        if len(self.buffer) == 0:
            return

        # Extract data from buffer
        rounds = self.buffer.rounds
        
        joint_obs = torch.FloatTensor(np.stack([r['joint_obs'] for r in rounds]))
        rewards = [r['reward'] for r in rounds]
        dones = [r['done'] for r in rounds]
        values = [r['value'] for r in rounds]
        
        # Compute advantages and returns
        advantages = self.compute_gae(rewards, values, dones)
        returns = advantages + torch.tensor(values, dtype=torch.float32)
        
        # Normalize advantages
        if advantages.numel() > 1:
            adv_std = advantages.std(unbiased=False)
            if adv_std > 1e-8:
                advantages = (advantages - advantages.mean()) / (adv_std + 1e-8)
            else:
                advantages = advantages - advantages.mean()

        # PPO update epochs
        for _ in range(self.ppo_epochs):
            total_loss = 0.0
            
            # Update each actor
            for agent_id in range(self.n_detectives):
                agent_obs = []
                agent_actions = []
                agent_old_logps = []
                agent_masks = []
                agent_advs = []
                
                for round_idx, r in enumerate(rounds):
                    if agent_id in r['agent_data']:
                        data = r['agent_data'][agent_id]
                        agent_obs.append(data['obs'])
                        agent_actions.append(data['action'])
                        agent_old_logps.append(data['logp'])
                        agent_masks.append(data['mask'])
                        agent_advs.append(advantages[round_idx])
                
                if not agent_obs:
                    continue
                
                obs_t = torch.FloatTensor(np.stack(agent_obs))
                actions_t = torch.LongTensor(agent_actions)
                old_logps_t = torch.stack(agent_old_logps)
                masks_t = torch.FloatTensor(np.array(agent_masks))
                advs_t = torch.stack(agent_advs) if isinstance(agent_advs[0], torch.Tensor) else torch.FloatTensor(agent_advs)
                
                # Get new log probs
                logits = self.actors[agent_id](obs_t)
                masked_logits = logits.masked_fill(masks_t < 0.5, -1e9)
                dist = Categorical(logits=masked_logits)
                new_logps = dist.log_prob(actions_t)
                
                # PPO clipped objective
                ratio = torch.exp(new_logps - old_logps_t.detach())
                surr1 = ratio * advs_t
                surr2 = torch.clamp(ratio, 1 - self.clip_eps, 1 + self.clip_eps) * advs_t
                actor_loss = -torch.min(surr1, surr2).mean()
                
                # Entropy bonus for exploration
                entropy = dist.entropy().mean()
                actor_loss -= self.entropy_coef * entropy
                
                total_loss += actor_loss
            
            # Critic loss
            values_pred = self.critic(joint_obs).view(-1)
            critic_loss = self.value_coef * nn.MSELoss()(values_pred, returns.detach())
            total_loss += critic_loss
            
            # Optimize
            self.optimizer.zero_grad()
            total_loss.backward()
            nn.utils.clip_grad_norm_(
                list(self.actors.parameters()) + list(self.critic.parameters()),
                self.max_grad_norm
            )
            self.optimizer.step()
        
        # Update old actors
        for i in range(self.n_detectives):
            self.actors_old[i].load_state_dict(self.actors[i].state_dict())
            self.actors_old[i].eval()
        
        # Clear buffer
        self.buffer.clear()
        
        # Save checkpoint
        torch.save({
            "actors": [a.state_dict() for a in self.actors],
            "critic": self.critic.state_dict(),
            "optimizer": self.optimizer.state_dict()
        }, self.model_path)
        
        print(f"[Detective MAPPO] Model saved to {self.model_path}")


# =================================================================
# 5. ZMQ SERVER LOOP
# =================================================================
def run_server():
    encoder = ObservationEncoder()
    agent: Optional[MAPPODetectiveAgent] = None
    
    # Track round state
    last_round = -1
    detectives_moved_this_round = set()
    cached_joint_obs = None
    n_detectives = 5  # Default, will be updated
    
    ctx = zmq.Context()
    sock = ctx.socket(zmq.REP)
    sock.bind("tcp://*:5556")
    
    print("Detective MAPPO Server (True Multi-Agent) listening on 5556")
    
    try:
        while True:
            raw_msg = sock.recv().decode("utf-8").strip()
            print(f"\n[RECEIVE] {raw_msg[:200]}...")

            try:
                req = json.loads(raw_msg)
            except json.JSONDecodeError:
                print("[ERROR] Invalid JSON")
                sock.send_json({"status": "error", "message": "Invalid JSON"})
                continue

            # Handshake
            if req.get("type") == "ping":
                print("[INFO] Handshake: ping -> pong")
                sock.send_json({"type": "pong"})
                continue

            # Game Over
            if "winner" in req or "[GameOver]" in raw_msg:
                winner = req.get("winner", "")
                final_reward = 1000.0 if winner == "Detectives" else -1000.0
                current_round = req.get("game_state", {}).get("current_round", 0)
                print(f"[EVENT] Game Over! Winner: {winner}")
                
                if agent and len(agent.buffer) > 0:
                    # Complete any pending round
                    if cached_joint_obs is not None:
                        with torch.no_grad():
                            value = agent.critic(torch.FloatTensor(cached_joint_obs)).item()
                        agent.buffer.complete_round(cached_joint_obs, final_reward, True, value)
                    
                    agent.update()
                    print(f"[DATA] Final reward: {final_reward}")
                
                log_game(winner=winner, det_reward=final_reward, rounds=current_round)
                
                # Reset state
                last_round = -1
                detectives_moved_this_round = set()
                cached_joint_obs = None
                if agent:
                    agent.new_game = True
                    agent.episode_reward = 0.0
                    agent.buffer.clear()
                
                sock.send_json({"status": "ok"})
                continue

            # Get game state
            game_state = req.get("game_state", {})
            players = req.get("players", [])
            current_player_idx = game_state.get("current_player_index", 0)
            current_round = game_state.get("current_round", 0)
            
            # Count detectives (non-MrX players)
            detective_indices = [i for i, p in enumerate(players) if not p.get("is_mister_x", False)]
            n_detectives = len(detective_indices)
            
            if n_detectives == 0:
                sock.send_json({"status": "ok", "selected_index": 0})
                continue
            
            # Map current_player_idx to detective index (0-4)
            if current_player_idx in detective_indices:
                detective_idx = detective_indices.index(current_player_idx)
            else:
                # Current player is Mr. X, shouldn't happen for detective AI
                print(f"[WARN] Called for non-detective player {current_player_idx}")
                sock.send_json({"status": "ok", "selected_index": 0})
                continue
            
            # Initialize agent if needed
            if agent is None:
                local_obs = encoder.encode_agent(game_state, players, current_player_idx)
                agent = MAPPODetectiveAgent(
                    local_obs_dim=len(local_obs),
                    act_dim=50,
                    n_detectives=n_detectives
                )
                print(f"[INIT] Agent initialized: {n_detectives} detectives, obs_dim={len(local_obs)}")
            
            # Check for new round
            if current_round != last_round:
                # Complete previous round if we have data
                if last_round >= 0 and cached_joint_obs is not None and len(agent.buffer.current_round) > 0:
                    step_reward = game_state.get("reward", 0.0)
                    if agent.new_game:
                        step_reward = 0.0
                        agent.new_game = False
                    agent.episode_reward += step_reward
                    
                    with torch.no_grad():
                        value = agent.critic(torch.FloatTensor(cached_joint_obs)).item()
                    agent.buffer.complete_round(cached_joint_obs, step_reward, False, value)
                
                last_round = current_round
                detectives_moved_this_round = set()
            
            # Get moves
            moves = req.get("possible_moves", [])
            if not moves:
                print("[WARN] No possible moves")
                sock.send_json({"status": "ok", "selected_index": None})
                continue
            
            # Encode observations
            local_obs = encoder.encode_agent(game_state, players, current_player_idx)
            cached_joint_obs = encoder.encode_joint(game_state, players, n_detectives)
            
            # Create action mask
            action_mask = [1.0 if i < len(moves) else 0.0 for i in range(agent.act_dim)]
            
            # Select action using this detective's Actor
            action, logp = agent.select_action(local_obs, detective_idx, action_mask)
            
            # Bounds check
            if action >= len(moves):
                action = len(moves) - 1
            if action < 0:
                action = 0
            
            # Store this agent's step
            agent.buffer.store_agent_step(detective_idx, local_obs, action, logp, action_mask)
            detectives_moved_this_round.add(detective_idx)
            
            # Check if round is complete (all detectives moved)
            if len(detectives_moved_this_round) >= n_detectives:
                step_reward = game_state.get("reward", 0.0)
                with torch.no_grad():
                    value = agent.critic(torch.FloatTensor(cached_joint_obs)).item()
                agent.buffer.complete_round(cached_joint_obs, step_reward, False, value)
                detectives_moved_this_round = set()
                
                # Periodic update
                if len(agent.buffer) >= 32:
                    print(f"[AI] UPDATE | {len(agent.buffer)} rounds collected")
                    agent.update()
            
            sel = moves[action]
            print(f"[DECISION] Detective {detective_idx} (player {current_player_idx}): move #{action}")
            
            sock.send_json({
                "status": "ok",
                "selected_index": action,
                "destination": sel.get("dest"),
                "transport": sel.get("transport", 0)
            })

    except KeyboardInterrupt:
        print("\n[STOP] Detective MAPPO server stopped")
    finally:
        sock.close()
        ctx.term()


if __name__ == "__main__":
    run_server()
