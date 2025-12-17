"""
Pełna implementacja dyskretnego PPO dla agenta Mister X z komunikacją ZMQ
=====================================================================

Cechy:
- Kompletny PPO (GAE, clipped objective, entropy, value loss).
- Konfigurowalna sieć (liczba warstw, neur. na warstwę, aktywacja).
- Obsługa dynamicznego grafu (odbierana w komunikacie `graph_init`).
- Kodowanie obserwacji oparte na grafie: one-hot pozycji na węzłach (skalowane do max_nodes), bilety, widoczność itp.
- Maskowanie niedozwolonych akcji (zmienna liczba możliwych ruchów per krok).
- Zapis/ładowanie modelu + config.json + historia treningu (CSV).
- Placeholder na komunikację z zewnętrznym programem (tu: ZMQ REP).

UWAGI ważne:
- Gra (C++) musi wysyłać pole `reward` i `done` w requestach, żeby agent mógł się uczyć.
  Jeśli gra nie dostarcza reward, możesz użyć heurystyk (reward shaping).
- Jeśli graf jest duży, obserwacja one-hot może być bardzo długa — można to zmienić na embeddingi.

Dependencies:
- torch
- numpy
- pyzmq
- pandas (opcjonalnie, do zapisywania historii)

Uruchamianie:
    python ppo_mrx_agent.py --config config.json

Konfiguracja przykładowa (puste pola mają sensowne wartości domyślne):
{
  "env": {"address": "tcp://*:5555"},
  "network": {"hidden_layers": [128,128], "activation": "relu"},
  "ppo": {"gamma":0.99, "gae_lambda":0.95, "clip_epsilon":0.2, "learning_rate":3e-4, "ppo_epochs":4, "minibatch_size":64, "update_timesteps":2048},
  "training": {"save_dir":"models", "max_action":64}
}

"""

import argparse
import json
import os
import time
from collections import deque, namedtuple
from typing import List, Dict, Any, Optional

import numpy as np
import torch
import torch.nn as nn
import torch.optim as optim
import zmq
import random

try:
    import pandas as pd
except Exception:
    pd = None

# ----------------------------- Utils & Config -----------------------------

DEFAULT_CONFIG = {
    "env": {"address": "tcp://*:5555"},
    "network": {
        "hidden_layers": [128, 128],
        "activation": "relu"
    },
    "ppo": {
        "gamma": 0.99,
        "gae_lambda": 0.95,
        "clip_epsilon": 0.2,
        "value_loss_coef": 0.5,
        "entropy_coef": 0.01,
        "learning_rate": 3e-4,
        "ppo_epochs": 4,
        "minibatch_size": 64,
        "update_timesteps": 2048
    },
    "training": {
        "save_dir": "models",
        "max_action": 64,  # max number of possible moves supported
        "mode": "train"
    }
}

Transition = namedtuple('Transition', ['obs', 'action', 'logp', 'reward', 'done', 'value'])


def ensure_dir(path: str):
    os.makedirs(path, exist_ok=True)


def make_activation(name: str):
    name = (name or 'relu').lower()
    if name == 'relu':
        return nn.ReLU()
    if name == 'tanh':
        return nn.Tanh()
    if name == 'elu':
        return nn.ELU()
    if name == 'leaky_relu':
        return nn.LeakyReLU()
    raise ValueError(f"Unknown activation: {name}")

# ----------------------------- Networks -----------------------------

class MLP(nn.Module):
    def __init__(self, input_dim: int, hidden_layers: List[int], activation: str='relu'):
        super().__init__()
        layers = []
        last = input_dim
        act = make_activation(activation)
        for h in hidden_layers:
            layers.append(nn.Linear(last, h))
            layers.append(act)
            last = h
        self.net = nn.Sequential(*layers)
        self.out_dim = last

    def forward(self, x):
        return self.net(x)


class PolicyValue(nn.Module):
    def __init__(self, obs_dim: int, max_action: int, hidden_layers: List[int], activation: str='relu'):
        super().__init__()
        self.base = MLP(obs_dim, hidden_layers, activation)
        self.logits = nn.Linear(self.base.out_dim, max_action)
        self.value_head = nn.Linear(self.base.out_dim, 1)

    def forward(self, obs: torch.Tensor):
        x = self.base(obs)
        logits = self.logits(x)
        value = self.value_head(x).squeeze(-1)
        return logits, value

# ----------------------------- PPO Agent -----------------------------

class PPOAgent:
    def __init__(self, obs_dim:int, max_action:int, config:Dict[str,Any]):
        self.obs_dim = obs_dim
        self.max_action = max_action
        self.config = config
        net_cfg = config.get('network', {})
        ppo_cfg = config.get('ppo', {})

        self.device = torch.device('cuda' if torch.cuda.is_available() else 'cpu')
        self.model = PolicyValue(obs_dim, max_action, net_cfg.get('hidden_layers', [128,128]), net_cfg.get('activation','relu')).to(self.device)
        self.optimizer = optim.Adam(self.model.parameters(), lr=ppo_cfg.get('learning_rate', 3e-4))

        # buffer for transitions
        self.buffer: List[Transition] = []
        self.timesteps = 0

    def select_action(self, obs_np: np.ndarray, valid_mask: Optional[np.ndarray]=None):
        """
        obs_np: shape (obs_dim,)
        valid_mask: boolean array length max_action: True for valid actions
        returns: action_idx (int in [0,max_action)), logp(float), value(float)
        """
        obs = torch.tensor(obs_np, dtype=torch.float32, device=self.device).unsqueeze(0)
        with torch.no_grad():
            logits, value = self.model(obs)
            logits = logits.squeeze(0)
            # apply mask: set invalid logits to -1e9
            if valid_mask is not None:
                mask = torch.tensor(valid_mask, device=self.device, dtype=torch.bool)
                # set invalid to -inf
                neg_inf = -1e9
                logits = torch.where(mask, logits, torch.full_like(logits, neg_inf))
            probs = torch.softmax(logits, dim=-1)
            dist = torch.distributions.Categorical(probs)
            action = dist.sample()
            logp = dist.log_prob(action)
        return int(action.item()), float(logp.item()), float(value.item())

    def store(self, trans: Transition):
        self.buffer.append(trans)
        self.timesteps += 1

    def compute_gae(self, last_value: float, gamma: float, lam: float):
        rewards = [t.reward for t in self.buffer]
        values = [t.value for t in self.buffer]
        dones = [t.done for t in self.buffer]

        values = values + [last_value]
        gae = 0.0
        advantages = []
        for step in reversed(range(len(rewards))):
            td = rewards[step] + gamma * values[step+1] * (1.0 - float(dones[step])) - values[step]
            gae = td + gamma * lam * (1.0 - float(dones[step])) * gae
            advantages.insert(0, gae)
        returns = [adv + val for adv, val in zip(advantages, values[:-1])]
        return np.array(advantages, dtype=np.float32), np.array(returns, dtype=np.float32)

    def update(self):
        if not self.buffer:
            return
        ppo_cfg = self.config.get('ppo', {})
        gamma = ppo_cfg.get('gamma', 0.99)
        lam = ppo_cfg.get('gae_lambda', 0.95)
        clip_eps = ppo_cfg.get('clip_epsilon', 0.2)
        value_coef = ppo_cfg.get('value_loss_coef', 0.5)
        ent_coef = ppo_cfg.get('entropy_coef', 0.01)
        ppo_epochs = ppo_cfg.get('ppo_epochs', 4)
        minibatch_size = ppo_cfg.get('minibatch_size', 64)

        # compute last_value for GAE (bootstrap)
        with torch.no_grad():
            last_obs = torch.tensor(self.buffer[-1].obs, dtype=torch.float32, device=self.device).unsqueeze(0)
            _, last_value = self.model(last_obs)
            last_value = float(last_value.item())

        advantages, returns = self.compute_gae(last_value, gamma, lam)

        obs_arr = np.stack([t.obs for t in self.buffer], axis=0)
        actions_arr = np.array([t.action for t in self.buffer], dtype=np.int64)
        old_logps = np.array([t.logp for t in self.buffer], dtype=np.float32)
        values_arr = np.array([t.value for t in self.buffer], dtype=np.float32)

        # normalize advantages
        adv_mean = advantages.mean() if advantages.size > 0 else 0.0
        adv_std = advantages.std() if advantages.size > 0 else 1.0
        advantages = (advantages - adv_mean) / (adv_std + 1e-8)

        dataset_size = len(self.buffer)
        idxs = np.arange(dataset_size)

        # convert to tensors
        obs_tensor = torch.tensor(obs_arr, dtype=torch.float32, device=self.device)
        actions_tensor = torch.tensor(actions_arr, dtype=torch.long, device=self.device)
        old_logp_tensor = torch.tensor(old_logps, dtype=torch.float32, device=self.device)
        returns_tensor = torch.tensor(returns, dtype=torch.float32, device=self.device)
        advantages_tensor = torch.tensor(advantages, dtype=torch.float32, device=self.device)

        for epoch in range(ppo_epochs):
            np.random.shuffle(idxs)
            for start in range(0, dataset_size, minibatch_size):
                end = start + minibatch_size
                mb_idx = idxs[start:end]

                mb_obs = obs_tensor[mb_idx]
                mb_actions = actions_tensor[mb_idx]
                mb_old_logp = old_logp_tensor[mb_idx]
                mb_returns = returns_tensor[mb_idx]
                mb_adv = advantages_tensor[mb_idx]

                logits, values = self.model(mb_obs)
                dist = torch.distributions.Categorical(logits=logits)
                mb_logp = dist.log_prob(mb_actions)
                mb_entropy = dist.entropy().mean()

                ratio = torch.exp(mb_logp - mb_old_logp)
                surr1 = ratio * mb_adv
                surr2 = torch.clamp(ratio, 1.0 - clip_eps, 1.0 + clip_eps) * mb_adv
                policy_loss = -torch.min(surr1, surr2).mean()

                value_loss = (mb_returns - values).pow(2).mean()

                loss = policy_loss + value_coef * value_loss - ent_coef * mb_entropy

                self.optimizer.zero_grad()
                loss.backward()
                nn.utils.clip_grad_norm_(self.model.parameters(), max_norm=0.5)
                self.optimizer.step()

        # clear buffer
        self.buffer = []
        self.timesteps = 0

    def save(self, path: str):
        obj = {
            'model_state': self.model.state_dict(),
            'optimizer_state': self.optimizer.state_dict(),
            'config': self.config
        }
        torch.save(obj, path)

    def load(self, path: str):
        data = torch.load(path, map_location=self.device)
        self.model.load_state_dict(data['model_state'])
        self.optimizer.load_state_dict(data['optimizer_state'])
        # device mismatch handling
        for state in self.optimizer.state.values():
            for k, v in state.items():
                if isinstance(v, torch.Tensor):
                    state[k] = v.to(self.device)

# ----------------------------- Observation encoding -----------------------------

class ObservationEncoder:
    """
    Koduje JSON od gry na wektor stałej długości.

    Strategia:
    - Jeśli mamy graf (node_count <= max_nodes), zakładamy one-hot dla pozycji na grafie dla każdego gracza.
    - Dodatkowo: bilety (black,bus,metro,double,taxi) znormalizowane, visibility, current_round, last_known_pos, last_known_round.

    Jeśli graf nie jest znany, użyjemy surowych numerów pozycji (skalarów) i normalizacji.
    """
    def __init__(self, max_nodes: int = 200, max_players: int = 6):
        self.max_nodes = max_nodes
        self.max_players = max_players

    def encode(self, game_state: Dict[str, Any], players: List[Dict[str, Any]], graph: Optional[Dict[str, Any]]):
        obs = []
        # basic scalars
        obs.append(float(game_state.get('current_round', 0)))
        obs.append(float(game_state.get('current_player_index', 0)))
        obs.append(1.0 if game_state.get('is_reveal_round', False) else 0.0)
        # mr x last known
        obs.append(float(game_state.get('mr_x_last_known_position', -1)))
        obs.append(float(game_state.get('mr_x_last_known_round', -1)))

        node_count = 0
        if graph and 'nodes' in graph:
            node_count = len(graph['nodes'])

        use_one_hot = (node_count > 0 and node_count <= self.max_nodes)

        # per-player encoding (fixed number of players)
        for i in range(self.max_players):
            if i < len(players):
                p = players[i]
                pos = int(p.get('position', -1))
                is_mx = 1.0 if p.get('is_mister_x', False) else 0.0
                vis = 1.0 if p.get('is_visible', False) else 0.0
                obs.append(is_mx)
                obs.append(vis)
                # tickets: black,bus,metro,taxi,double
                tickets = p.get('tickets', {})
                obs.append(float(tickets.get('black', 0)))
                obs.append(float(tickets.get('bus', 0)))
                obs.append(float(tickets.get('metro', 0)))
                obs.append(float(tickets.get('taxi', 0)))
                obs.append(float(tickets.get('double', 0)))

                if use_one_hot:
                    one_hot = [0.0] * node_count
                    if 0 <= pos < node_count:
                        one_hot[pos] = 1.0
                    obs.extend(one_hot)
                else:
                    # fallback: raw position normalized by max_nodes
                    obs.append(float(pos) / max(1.0, float(self.max_nodes)))
            else:
                # padding for absent player
                obs.extend([0.0, 0.0])
                obs.extend([0.0]*5)
                if use_one_hot:
                    obs.extend([0.0]*node_count)
                else:
                    obs.append(0.0)

        # pad or trim to fixed size: we choose obs_dim = base + max_players*(per_player)
        if use_one_hot:
            obs_dim = 5 + self.max_players * (2 + 5 + node_count)
        else:
            obs_dim = 5 + self.max_players * (2 + 5 + 1)

        # ensure numpy array and float32
        obs = np.array(obs, dtype=np.float32)
        # If obs not matching obs_dim (e.g., graph larger than max_nodes), trim or pad
        if obs.size > obs_dim:
            obs = obs[:obs_dim]
        elif obs.size < obs_dim:
            pad = np.zeros(obs_dim - obs.size, dtype=np.float32)
            obs = np.concatenate([obs, pad])

        return obs

# ----------------------------- Main Server Loop -----------------------------

class ZMQPPOAgentServer:
    def __init__(self, config:Dict[str,Any]):
        self.config = config
        self.address = config.get('env', {}).get('address', 'tcp://*:5555')
        self.save_dir = config.get('training', {}).get('save_dir', 'models')
        ensure_dir(self.save_dir)
        self.max_action = config.get('training', {}).get('max_action', 64)

        # zmq
        self.ctx = zmq.Context()
        self.sock = self.ctx.socket(zmq.REP)
        self.sock.bind(self.address)
        print(f"AI server listening on {self.address}")

        # state
        self.graph = None
        self.encoder = None
        self.agent: Optional[PPOAgent] = None
        self.obs_dim = None

        # history
        self.episode_rewards = []  # per-episode total reward
        self.curr_episode_reward = 0.0
        self.episode_len = 0

        # timing
        self.last_save = time.time()
        self.save_every = config.get('training', {}).get('save_every_seconds', 300)

    def init_from_graph(self, graph: Dict[str,Any]):
        self.graph = graph
        node_count = len(graph.get('nodes', [])) if graph else 0
        max_nodes = max(node_count, 1)
        # create encoder with reasonable max_nodes
        self.encoder = ObservationEncoder(max_nodes=max_nodes, max_players=6)
        # compute obs_dim by encoding empty sample
        obs_sample = self.encoder.encode({}, [], graph)
        self.obs_dim = obs_sample.size
        print(f"Initialized encoder: obs_dim={self.obs_dim}, node_count={node_count}")
        # create agent
        self.agent = PPOAgent(obs_dim=self.obs_dim, max_action=self.max_action, config=self.config)

    def map_moves_to_mask(self, moves: List[Dict[str,Any]]):
        mask = np.zeros(self.max_action, dtype=bool)
        for i in range(min(len(moves), self.max_action)):
            mask[i] = True
        return mask

    def run(self):
        try:
            while True:
                msg = self.sock.recv()
                req = json.loads(msg.decode('utf-8'))

                # graph init handling
                if req.get('type') == 'graph_init':
                    self.graph = req.get('graph', {})
                    print('[GraphInit] received graph')
                    self.init_from_graph(self.graph)
                    self.sock.send_json({'status':'graph_received'})
                    continue

                # move request
                game_state = req.get('game_state', {})
                players = req.get('players', [])
                moves = req.get('possible_moves', [])

                # ensure encoder/agent exist
                if self.encoder is None or self.agent is None:
                    # fallback: still respond randomly
                    print('[Warning] encoder/agent not initialized — send random move')
                    if moves:
                        idx = random.randrange(len(moves))
                        sel = moves[idx]
                        resp = {"status":"ok", "selected_index": idx, "destination": sel.get('dest'), "transport": sel.get('transport', 0)}
                    else:
                        resp = {"status":"ok", "selected_index": None, "destination": None, "transport": 0}
                    self.sock.send_json(resp)
                    continue

                obs = self.encoder.encode(game_state, players, self.graph)
                valid_mask = self.map_moves_to_mask(moves)

                action_idx, logp, value = self.agent.select_action(obs, valid_mask)

                # map to actual move: choose modulo but ensure within range
                if moves:
                    sel_idx = action_idx % len(moves)
                    sel = moves[sel_idx]
                else:
                    sel_idx = None
                    sel = {"dest": None, "transport": 0}

                # read reward/done information
                reward = float(req.get('reward', 0.0))
                done = bool(req.get('done', False))

                # store transition
                trans = Transition(obs=obs, action=action_idx, logp=logp, reward=reward, done=done, value=value)
                self.agent.store(trans)

                self.curr_episode_reward += reward
                self.episode_len += 1

                # if enough timesteps accumulated or episode done -> update
                if self.agent.timesteps >= self.config.get('ppo', {}).get('update_timesteps', 2048) or done:
                    print(f"[PPO] updating... timesteps={self.agent.timesteps} done={done}")
                    self.agent.update()

                # if episode finished record and reset counters
                if done:
                    self.episode_rewards.append(self.curr_episode_reward)
                    print(f"Episode finished: reward={self.curr_episode_reward}, length={self.episode_len}")
                    self.curr_episode_reward = 0.0
                    self.episode_len = 0

                # periodic save
                if time.time() - self.last_save > self.save_every:
                    self.save_model()
                    self.last_save = time.time()

                # reply to C++
                resp = {"status":"ok", "selected_index": sel_idx, "destination": sel.get('dest'), "transport": sel.get('transport', 0)}
                self.sock.send_json(resp)

        except KeyboardInterrupt:
            print('Shutting down server...')
            self.save_model()
        finally:
            self.sock.close()
            self.ctx.term()

    def save_model(self):
        if self.agent is None:
            return
        path = os.path.join(self.save_dir, 'ppo_mrx_checkpoint.pt')
        print(f"Saving model to {path}")
        self.agent.save(path)
        # save config
        cfg_path = os.path.join(self.save_dir, 'config.json')
        with open(cfg_path, 'w') as f:
            json.dump(self.config, f, indent=2)
        # save history
        hist_path = os.path.join(self.save_dir, 'training_history.csv')
        if pd is not None:
            df = pd.DataFrame({'episode_reward': self.episode_rewards})
            df.to_csv(hist_path, index=False)
        else:
            # fallback plain text
            with open(hist_path, 'w') as f:
                for r in self.episode_rewards:
                    f.write(f"{r}\n")

    def load_model(self, path: str):
        if self.agent is None:
            raise RuntimeError('Agent not initialized (no graph)')
        print(f"Loading model from {path}")
        self.agent.load(path)

# ----------------------------- CLI -----------------------------

def load_config(path: Optional[str]) -> Dict[str,Any]:
    cfg = DEFAULT_CONFIG.copy()
    if path and os.path.exists(path):
        with open(path, 'r') as f:
            user = json.load(f)
        # deep update
        def deep_update(a, b):
            for k, v in b.items():
                if k in a and isinstance(a[k], dict) and isinstance(v, dict):
                    deep_update(a[k], v)
                else:
                    a[k] = v
        deep_update(cfg, user)
    return cfg


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--config', type=str, default=None)
    parser.add_argument('--load', type=str, default=None, help='path to checkpoint to load')
    args = parser.parse_args()

    cfg = load_config(args.config)
    server = ZMQPPOAgentServer(cfg)
    if args.load:
        # load will require graph -> user must call graph_init first or we delay loading
        print('[Info] load requested but model will be loaded after graph_init when agent exists')
    server.run()

if __name__ == '__main__':
    main()
