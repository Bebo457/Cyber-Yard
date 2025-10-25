import pygame
import sys
import numpy as np
import random
from heapq import heappush, heappop
pygame.init()
WIDTH, HEIGHT = 1200, 800
screen = pygame.display.set_mode((WIDTH, HEIGHT))
pygame.display.set_caption("Scotland Yard")

FONT = pygame.font.SysFont("arial", 24)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
GRAY = (200, 200, 200)
BLUE = (50, 100, 200)
YELLOW = (255, 255, 0)
RED = (200, 0, 0)

KOLORY_GRACZY = {
    'mr_x': (0, 0, 0),
    'police': (0, 0, 255)
}

kolory_polaczen = {
    'underground': ((255, 0, 0), 10),
    'bus': ((0, 200, 0), 8),
    'taxi': ((255, 165, 0), 4),
    'water': ((0, 0, 255), 4)
}

# Tury, w których Mr. X jest widoczny (Scotland Yard rules)
REVEAL_TURNS = [3, 8, 13, 18, 24]


# ======= funkcje pomocnicze =======
def znajdz_zakres(punkty):
    xs = [d['x'] for d in punkty.values()]
    ys = [d['y'] for d in punkty.values()]
    return min(xs), max(xs), min(ys), max(ys)


def skaluj(x, y, min_x, max_x, min_y, max_y, width, height, margin=50):
    scale_x = (width - 2 * margin) / (max_x - min_x)
    scale_y = (height - 2 * margin) / (max_y - min_y)
    scale = min(scale_x, scale_y)
    sx = margin + (x - min_x) * scale
    sy = margin + (y - min_y) * scale
    return int(sx), int(sy)


def losuj_pola_startowe(karty, liczba_policjantow):
    karty = karty.copy()
    np.random.shuffle(karty)
    mr_x = karty.pop()
    police = karty[:liczba_policjantow]
    return mr_x, police


def wczytaj_punkty(filename):
    punkty = {}
    with open(filename, 'r') as f:
        for line in f:
            parts = line.strip().split()
            nr = int(parts[0])
            x = int(parts[1])
            y = int(parts[2])
            typy = parts[3].split(',') if len(parts) > 3 else []
            punkty[nr] = {'x': x, 'y': y, 'typy': typy}
    return punkty


def wczytaj_polaczenia(filename):
    polaczenia = []
    with open(filename, 'r') as f:
        for line in f:
            parts = line.strip().split()
            a = int(parts[0])
            b = int(parts[1])
            typ = parts[2]
            polaczenia.append((a, b, typ))
    return polaczenia


# ======= klasy =======
class Pawn:
    def __init__(self, name, position, color, is_mr_x=False):
        self.name = name
        self.position = position
        self.color = color
        self.is_mr_x = is_mr_x
        self.moved_this_turn = False

        if is_mr_x:
            self.tickets = {'taxi': float('inf'), 'bus': float('inf'), 'underground': float('inf'), 'water': 5}
        else:
            self.tickets = {'taxi': 11, 'bus': 8, 'underground': 4, 'water': 0}

    def move_to(self, new_position, transport=None):
        self.position = new_position
        self.moved_this_turn = True
        if transport:
            if self.tickets[transport] != float('inf'):
                self.tickets[transport] -= 1


class HumanPlayer:
    def __init__(self, role):
        self.role = role

    def get_move(self, game_state):
        return None


class MrXAI:
    def __init__(self, role, algorithm="random"):
        self.role = role
        self.algorithm = algorithm  # "random", "decoy", "dfs", "monte_carlo"

    def get_move(self, game_state):
        if self.algorithm == "random":
            return self._random_move(game_state)
        elif self.algorithm == "decoy":
            return self._decoy_movement(game_state)
        elif self.algorithm == "dfs":
            return self._dfs_move(game_state)
        elif self.algorithm == "monte_carlo":
            return self._monte_carlo_move(game_state)
        else:
            return self._random_move(game_state)

    def _random_move(self, game_state):
        """Losowe ruchy - domyślny algorytm"""
        options = game_state.get_available_moves(game_state.mr_x)
        return random.choice(options) if options else None

    def _decoy_movement(self, game_state):
        import random
        from collections import deque, defaultdict

        posX = game_state.mr_x.position
        tickets = game_state.mr_x.tickets  # bilety Mr. X
        police_list = game_state.police
        graph_edges = game_state.polaczenia
        options = game_state.get_available_moves(game_state.mr_x)
        if not options:
            return None

        turn = getattr(game_state, 'turn', 0)
        next_reveal_turn = getattr(game_state, 'next_reveal_turn', float('inf'))
        prev_pos = getattr(game_state.mr_x, 'prev_pos', None)
        last_revealed = getattr(game_state.mr_x, 'last_revealed_position', None)

        graph = defaultdict(list)
        for a, b, typ in graph_edges:
            graph[a].append((b, typ))
            graph[b].append((a, typ))

        def bfs_distances(start):
            dist = {start: 0}
            q = deque([start])
            while q:
                u = q.popleft()
                for v, _ in graph[u]:
                    if v not in dist:
                        dist[v] = dist[u] + 1
                        q.append(v)
            return dist

        police_dists = {p.position: bfs_distances(p.position) for p in police_list}

        # Fallback w przypadku zagrożenia lub ujawnienia
        if any(police_dists[p.position].get(posX, float('inf')) <= 2 for p in police_list) \
                or next_reveal_turn - turn <= 2:
            # tylko ruchy z dostępnymi biletami
            valid_options = [(dest, typ) for dest, typ in options if tickets.get(typ, 0) > 0]
            if not valid_options:
                return None
            return max(valid_options,
                       key=lambda it: min(police_dists[p.position].get(it[0], float('inf')) for p in police_list))

        prediction_depth = 1

        def predict_police_positions(turns_ahead=prediction_depth):
            predicted = {0: set(p.position for p in police_list)}
            for t in range(1, turns_ahead + 1):
                nextset = set()
                for police in police_list:
                    frontier = {police.position}
                    for _ in range(t):
                        new_front = set()
                        for pos in frontier:
                            new_front.update([v for v, _ in graph[pos]])
                        if new_front:
                            frontier = new_front
                    nextset.update(frontier)
                predicted[t] = nextset
            return predicted

        predicted = predict_police_positions()

        def safety_risk_for(node):
            min_dist = min(police_dists[p.position].get(node, float('inf')) for p in police_list)
            return 1.0 / (min_dist + 1e-6) + 0.8 / max(1, len(graph[node]))

        def deception_simple(dest):
            score = 0.0
            if prev_pos and prev_pos != posX and prev_pos != dest:
                d_prev = bfs_distances(prev_pos)
                if d_prev.get(dest, float('inf')) > d_prev.get(posX, float('inf')):
                    score += 0.6
            if last_revealed:
                d_last = bfs_distances(last_revealed).get(dest, float('inf'))
                if d_last != float('inf'):
                    score += 0.5 / (d_last + 1.0)
            if 1 in predicted and predicted[1]:
                min_pred = min(bfs_distances(p).get(dest, float('inf')) for p in predicted[1])
                score += 0.4 / (min_pred + 1.0)
            return score

        def rollout_score_for_candidate(start_pos, num_rollouts=6, depth=3):
            total = 0.0
            for _ in range(num_rollouts):
                mr_pos = start_pos
                police_pos = [p.position for p in police_list]
                caught = False
                cum_min_dist = 0.0
                for _ in range(depth):
                    mr_dist_map = bfs_distances(mr_pos)
                    new_police_pos = []
                    for pp in police_pos:
                        if pp == mr_pos:
                            caught = True
                            break
                        best_nb, best_d = pp, mr_dist_map.get(pp, float('inf'))
                        for nb, _ in graph[pp]:
                            dnb = mr_dist_map.get(nb, float('inf'))
                            if dnb < best_d:
                                best_d, best_nb = dnb, nb
                        new_police_pos.append(best_nb)
                    if caught or any(p == mr_pos for p in new_police_pos):
                        cum_min_dist += 0
                        break
                    police_pos = new_police_pos
                    nbrs = [v for v, _ in graph[mr_pos]]
                    if not nbrs:
                        break
                    mr_pos = random.choice(nbrs)
                    min_d = min(bfs_distances(pp).get(mr_pos, float('inf')) for pp in police_pos)
                    cum_min_dist += min_d if min_d != float('inf') else max(len(graph), 1)
                total += -100.0 if caught else cum_min_dist / max(1, depth)
            return total / max(1, num_rollouts)

        # Kandydaci – tylko ruchy możliwe z dostępnymi biletami
        candidates, risks, deceptions = [], [], []
        for dest, transport in options:
            if tickets.get(transport, 0) <= 0:
                continue
            candidates.append((dest, transport))
            risks.append(safety_risk_for(dest))
            deceptions.append(deception_simple(dest))

        candidate_risk_max = 1.8
        filtered = [(c, r, d) for c, r, d in zip(candidates, risks, deceptions) if r <= candidate_risk_max]
        if not filtered:
            return max(candidates,
                       key=lambda it: min(police_dists[p.position].get(it[0], float('inf')) for p in police_list),
                       default=None)

        filtered.sort(key=lambda x: x[1])
        sim_targets = filtered[:5]

        utilities, sim_moves = [], []
        for (dest, transport), risk, deception in sim_targets:
            rscore = rollout_score_for_candidate(dest)
            rnorm = -1.0 if rscore < -50 else rscore / (1.0 + rscore)
            utilities.append(deception - risk + 1.5 * rnorm)
            sim_moves.append((dest, transport))

        if all(u <= 0 for u in utilities):
            dest, transport = max(filtered, key=lambda x: -x[1])[0]
            return (dest, transport)

        min_u = min(utilities)
        weights = [u - min_u + 1e-6 for u in utilities]
        total = sum(weights)
        pick = random.random() * total
        cum = 0.0
        for mv, w in zip(sim_moves, weights):
            cum += w
            if pick <= cum:
                return mv

        return sim_moves[-1]

    def _dfs_move(self, game_state):
        TARGET_LENGTH = 6
        MAX_DEPTH = 10
        MAX_ATTEMPTS_PER_NODE = 2
        PREDICTION_DEPTH = 3  # Ile ruchów policji do przodu przewidujemy

        # === HELPER FUNCTIONS ===

        def bfs_distances(start, graph):
            """Zwraca słownik odległości od start do wszystkich węzłów"""
            distances = {start: 0}
            queue = [start]
            idx = 0
            while idx < len(queue):
                current = queue[idx]
                idx += 1
                for neighbor, _ in get_neighbors(current, graph):
                    if neighbor not in distances:
                        distances[neighbor] = distances[current] + 1
                        queue.append(neighbor)
            return distances

        def get_neighbors(node, graph):
            """Zwraca listę (sąsiad, typ_transportu)"""
            neighbors = []
            for a, b, typ in graph:
                if a == node:
                    neighbors.append((b, typ))
                elif b == node:
                    neighbors.append((a, typ))
            return neighbors

        def predict_police_positions(police_list, turns_ahead, graph):
            """
            Przewiduje możliwe pozycje policji po 'turns_ahead' turach.
            Zwraca słownik: {turn: set(możliwe_pozycje_wszystkich_policjantów)}
            """
            predicted_positions = {0: set(p.position for p in police_list)}

            for turn in range(1, turns_ahead + 1):
                current_positions = predicted_positions[turn - 1]
                next_positions = set()

                for police in police_list:
                    # Dla każdego policjanta, znajdź wszystkie możliwe pozycje po 'turn' ruchach
                    reachable = {police.position}

                    for _ in range(turn):
                        new_reachable = set()
                        for pos in reachable:
                            # Symuluj możliwe ruchy policjanta
                            for neighbor, transport in get_neighbors(pos, graph):
                                # Sprawdź czy policjant ma bilet
                                if police.tickets.get(transport, 0) > 0:
                                    new_reachable.add(neighbor)
                        reachable = new_reachable if new_reachable else reachable

                    next_positions.update(reachable)

                predicted_positions[turn] = next_positions

            return predicted_positions

        def is_position_safe_at_turn(position, turn, predicted_police_positions, mr_x_position, graph):
            """
            Sprawdza czy pozycja jest bezpieczna w danej turze.
            Pozycja jest bezpieczna jeśli Mr. X jest bliżej niż przewidywane pozycje policji.
            """
            if turn not in predicted_police_positions:
                return True

            mr_x_distances = bfs_distances(mr_x_position, graph)
            mr_x_dist_to_pos = mr_x_distances.get(position, float('inf'))

            for police_pos in predicted_police_positions[turn]:
                police_distances = bfs_distances(police_pos, graph)
                police_dist = police_distances.get(position, float('inf'))

                # Jeśli policja może być tak blisko lub bliżej, pozycja nie jest bezpieczna
                if police_dist <= mr_x_dist_to_pos:
                    return False

            return True

        def calculate_path_safety_score(path, predicted_police_positions, mr_x_start, graph):
            """
            Ocenia bezpieczeństwo całej ścieżki biorąc pod uwagę przewidywane pozycje policji.
            """
            score = 0
            current_turn = 0

            for node, transport in path:
                current_turn += 1

                # Sprawdź bezpieczeństwo w tej turze
                if is_position_safe_at_turn(node, current_turn, predicted_police_positions, mr_x_start, graph):
                    score += 100
                else:
                    score -= 200  # Duża kara za niebezpieczną pozycję

                # Dodatkowy bonus za dystans od przewidywanych pozycji policji
                if current_turn in predicted_police_positions:
                    min_dist_to_police = float('inf')
                    for police_pos in predicted_police_positions[current_turn]:
                        dist = bfs_distances(police_pos, graph).get(node, float('inf'))
                        min_dist_to_police = min(min_dist_to_police, dist)

                    score += min_dist_to_police * 15

            return score

        def count_available_moves_from(node, tickets, graph):
            """Liczy ile ruchów jest dostępnych z danego węzła"""
            count = 0
            for neighbor, transport in get_neighbors(node, graph):
                if tickets.get(transport, 0) > 0:
                    count += 1
            return count

        def evaluate_endpoint(node, predicted_police_positions, graph, tickets):
            """Ocenia końcowy węzeł ścieżki"""
            score = 0

            # 1. Liczba dostępnych ruchów z tego węzła
            moves_count = count_available_moves_from(node, tickets, graph)
            score += moves_count * 20

            # 2. Różnorodność transportu
            transport_types = set()
            for neighbor, transport in get_neighbors(node, graph):
                if tickets.get(transport, 0) > 0:
                    transport_types.add(transport)
            score += len(transport_types) * 10

            # 3. Dystans od przewidywanych pozycji policji w ostatniej turze
            max_turn = max(predicted_police_positions.keys())
            if max_turn in predicted_police_positions:
                min_dist = float('inf')
                for police_pos in predicted_police_positions[max_turn]:
                    dist = bfs_distances(police_pos, graph).get(node, float('inf'))
                    min_dist = min(min_dist, dist)
                score += min_dist * 25

            return score

        # === GŁÓWNA FUNKCJA DFS ===

        def dfs(current, path, visited, tickets, attempts, depth, predicted_police_pos):
            nonlocal best_path, best_score

            if attempts.get(current, 0) >= MAX_ATTEMPTS_PER_NODE:
                return

            attempts[current] = attempts.get(current, 0) + 1

            # Oceń obecną ścieżkę
            path_safety = calculate_path_safety_score(path, predicted_police_pos, mr_x_pos, graph)
            endpoint_quality = evaluate_endpoint(current, predicted_police_pos, graph, tickets)
            current_score = path_safety + endpoint_quality + len(path) * 15

            if current_score > best_score:
                best_score = current_score
                best_path = path[:]

            # Zatrzymaj się jeśli osiągnięto cel
            if len(path) >= TARGET_LENGTH or depth >= MAX_DEPTH:
                return

            # Pobierz i oceń sąsiadów
            neighbors = get_neighbors(current, graph)

            def neighbor_score(neighbor_info):
                neighbor, transport = neighbor_info
                if neighbor in visited or tickets.get(transport, 0) <= 0:
                    return -float('inf')

                # Sprawdź bezpieczeństwo w następnej turze
                next_turn = len(path) + 1
                if not is_position_safe_at_turn(neighbor, next_turn, predicted_police_pos, mr_x_pos, graph):
                    return -float('inf')

                # Oceń potencjał tego ruchu
                score = 0
                if next_turn in predicted_police_pos:
                    min_dist = float('inf')
                    for police_pos in predicted_police_pos[next_turn]:
                        dist = bfs_distances(police_pos, graph).get(neighbor, float('inf'))
                        min_dist = min(min_dist, dist)
                    score += min_dist * 20

                return score

            neighbors_sorted = sorted(neighbors, key=neighbor_score, reverse=True)

            # Eksploruj najlepsze kierunki
            for neighbor, transport in neighbors_sorted[:8]:  # Top 8 kierunków
                if neighbor in visited or tickets.get(transport, 0) <= 0:
                    continue

                next_turn = len(path) + 1
                if not is_position_safe_at_turn(neighbor, next_turn, predicted_police_pos, mr_x_pos, graph):
                    continue

                # Dodaj do ścieżki
                visited.add(neighbor)
                path.append((neighbor, transport))
                new_tickets = tickets.copy()
                if new_tickets[transport] != float('inf'):
                    new_tickets[transport] -= 1

                dfs(neighbor, path, visited, new_tickets, attempts.copy(), depth + 1, predicted_police_pos)

                visited.remove(neighbor)
                path.pop()

        # === Główna logika ===

        options = game_state.get_available_moves(game_state.mr_x)
        if not options:
            return None

        police_list = game_state.police
        mr_x_pos = game_state.mr_x.position
        graph = game_state.polaczenia

        predicted_police_positions = predict_police_positions(police_list, PREDICTION_DEPTH, graph)
        game_state.predicted_police_positions = predicted_police_positions

        best_path = []
        best_score = -float('inf')
        initial_moves = []

        for dest, transport in options:
            if is_position_safe_at_turn(dest, 1, predicted_police_positions, mr_x_pos, graph):
                score = 0
                if 1 in predicted_police_positions:
                    min_dist = float('inf')
                    for police_pos in predicted_police_positions[1]:
                        dist = bfs_distances(police_pos, graph).get(dest, float('inf'))
                        min_dist = min(min_dist, dist)
                    score = min_dist
                initial_moves.append((dest, transport, score))

        if not initial_moves:
            for dest, transport in options:
                min_dist = float('inf')
                for police in police_list:
                    dist = bfs_distances(police.position, graph).get(dest, float('inf'))
                    min_dist = min(min_dist, dist)
                initial_moves.append((dest, transport, min_dist))

        initial_moves.sort(key=lambda x: x[2], reverse=True)

        for dest, transport, _ in initial_moves[:5]:
            visited = {mr_x_pos, dest}
            path = [(dest, transport)]
            tickets = game_state.mr_x.tickets.copy()
            if tickets[transport] != float('inf'):
                tickets[transport] -= 1
            attempts = {}
            dfs(dest, path, visited, tickets, attempts, 1, predicted_police_positions)

        if best_path:
            return best_path[0]

        if initial_moves:
            return (initial_moves[0][0], initial_moves[0][1])

        return random.choice(options) if options else None


class PoliceAI:
    def __init__(self, role, algorithm="random"):
        self.role = role
        self.algorithm = algorithm  # "random", "astar_greedy", "monte_carlo"
        self.alpha = 0.5  # Parametr ważenia odległości w mapie probabilistycznej

    def get_move(self, game_state, pawn):
        if self.algorithm == "random":
            return self._random_move(game_state, pawn)
        elif self.algorithm == "astar_greedy":
            return self._astar_greedy_move(game_state, pawn)
        elif self.algorithm == "monte_carlo":
            return self._monte_carlo_move(game_state, pawn)
        else:
            return self._random_move(game_state, pawn)

    def _random_move(self, game_state, pawn):
        """Losowe ruchy - domyślny algorytm"""
        options = game_state.get_available_moves(pawn)
        return random.choice(options) if options else None

    def _heuristic_distance(self, pos1, pos2, game_state):
        """Oblicza odległość euklidesową między dwoma węzłami na podstawie współrzędnych"""
        if pos1 not in game_state.punkty or pos2 not in game_state.punkty:
            return float('inf')
        x1, y1 = game_state.punkty[pos1]['x'], game_state.punkty[pos1]['y']
        x2, y2 = game_state.punkty[pos2]['x'], game_state.punkty[pos2]['y']
        return ((x1 - x2) ** 2 + (y1 - y2) ** 2) ** 0.5

    def _generate_reachable_nodes(self, start_pos, turns_since_reveal, game_state, mr_x_tickets):
        """Generuje wszystkie węzły osiągalne przez Mr. X z ostatniej znanej pozycji
        w przeciągu turnsSinceReveal tur"""
        reachable = {start_pos}

        for _ in range(turns_since_reveal):
            next_reachable = set()
            for node in reachable:
                for neighbor, _ in game_state.get_available_moves(
                        type('Pawn', (), {'position': node, 'tickets': mr_x_tickets})()
                ):
                    next_reachable.add(neighbor)
            reachable = next_reachable if next_reachable else reachable

        return reachable

    def _filter_reachable_by_moves(self, start_pos, game_state, moves_sequence):
        """Filtruje osiągalne węzły na podstawie sekwencji środków transportu.
        np. jeśli moves_sequence = ['underground', 'underground', 'underground'],
        to węzeł musi być osiągalny poprzez dokładnie 3 przejazdy metrem z rzędu
        lub zawrócić (2 w jedną stronę, 1 w drugą)"""

        if not moves_sequence:
            return {start_pos}

        # Ustawienie biletów Mr. X (nieskończone)
        mr_x_tickets = {'taxi': float('inf'), 'bus': float('inf'), 'underground': float('inf'), 'water': 5}

        # Używamy BFS aby znaleźć wszystkie węzły osiągalne dokładnie tymi ruchami
        from collections import deque
        queue = deque([(start_pos, 0)])  # (node, move_index)
        visited = {(start_pos, 0)}
        reachable = set()

        while queue:
            node, move_idx = queue.popleft()

            if move_idx == len(moves_sequence):
                reachable.add(node)
                continue

            required_transport = moves_sequence[move_idx]

            # Pobierz dostępne ruchy z tego węzła
            available_moves = game_state.get_available_moves(
                type('Pawn', (), {'position': node, 'tickets': mr_x_tickets})()
            )

            # Filtruj tylko ruchy tym środkiem transportu
            for neighbor, transport in available_moves:
                if transport == required_transport:
                    state = (neighbor, move_idx + 1)
                    if state not in visited:
                        visited.add(state)
                        queue.append(state)

        return reachable if reachable else {start_pos}

    def get_suspected_positions(self, game_state):
        """Zwraca zbiór podejrzanych pozycji Mr. X na podstawie ostatnich ujawnień i ruchów"""

        if game_state.is_mr_x_revealed():
            # Jeśli Mr. X jest widoczny, wiemy dokładnie gdzie jest
            return {game_state.mr_x.position}, [game_state.mr_x.position]

        if game_state.last_known_pos is None:
            # Brak ujawniania - możliwe wszędzie
            return set(game_state.punkty.keys()), []

        # Pobierz ruchy od ostatniego ujawnienia
        moves_since_reveal = game_state.mr_x_moves[game_state.last_reveal_turn - 1:]

        # Filtruj węzły na podstawie sekwencji ruchów
        reachable = self._filter_reachable_by_moves(
            game_state.last_known_pos,
            game_state,
            moves_since_reveal
        )

        return reachable, moves_since_reveal

    def _compute_probability_map(self, game_state, pawn):
        """Oblicza mapę probabilistyczną możliwych pozycji Mr. X
        na podstawie historii ujawnień i dostępnych biletów"""

        # Jeśli Mr. X jest ujawniony teraz, wiemy dokładnie gdzie jest
        if game_state.is_mr_x_revealed():
            probability_map = {game_state.mr_x.position: 1.0}
            return probability_map

        # Jeśli nie ma ujawnienia i brak danych o ruchach, wszyscy mogą być wszędzie
        if game_state.last_known_pos is None:
            probability_map = {}
            for node in game_state.punkty.keys():
                probability_map[node] = 1.0 / len(game_state.punkty)
            return probability_map

        # Mamy ostatnią znaną pozycję - generujemy osiągalne węzły od tego momentu
        last_known_pos = game_state.last_known_pos
        turns_since_reveal = game_state.turn_number - game_state.last_reveal_turn

        reachable = self._generate_reachable_nodes(
            last_known_pos,
            turns_since_reveal,
            game_state,
            game_state.mr_x.tickets
        )

        # Inicjalizujemy mapę probabilistyczną
        probability_map = {}

        # Przydzielamy równomierną bazową probabilistykę osiągalnym węzłom
        for node in reachable:
            probability_map[node] = 1.0 / len(reachable) if reachable else 0

        # Dopasowujemy probabilistykę na podstawie odległości od policji
        for node in probability_map:
            min_distance = float('inf')
            for police_pawn in game_state.police:
                dist = self._heuristic_distance(police_pawn.position, node, game_state)
                min_distance = min(min_distance, dist)

            # Węzły dalej od policji mają wyższą probabilistykę
            probability_map[node] *= (1.0 + self.alpha * min_distance)

        # Normalizujemy probabilistyki
        total_prob = sum(probability_map.values())
        if total_prob > 0:
            for node in probability_map:
                probability_map[node] /= total_prob

        return probability_map

    def _astar_search(self, game_state, start, goal, pawn_tickets, max_iterations=100):
        """Implementacja algorytmu A* do znalezienia najkrótszej ścieżki
        uwzględniającej dostępne bilety"""
        # f_score = g_score + h_score (koszt + heurystyka)
        open_set = []
        heappush(open_set, (0, start))

        came_from = {}
        g_score = {start: 0}
        f_score = {start: self._heuristic_distance(start, goal, game_state)}

        closed_set = set()
        iterations = 0

        while open_set and iterations < max_iterations:
            iterations += 1
            _, current = heappop(open_set)

            if current == goal:
                # Rekonstruujemy ścieżkę
                path = [current]
                while current in came_from:
                    current = came_from[current]
                    path.append(current)
                return list(reversed(path))

            closed_set.add(current)

            # Eksplorujemy sąsiadów
            moves = game_state.get_available_moves(
                type('Pawn', (), {'position': current, 'tickets': pawn_tickets})()
            )

            for neighbor, transport_type in moves:
                if neighbor in closed_set:
                    continue

                # Koszt przejścia do sąsiada (1 ruch = 1 bilet)
                tentative_g_score = g_score[current] + 1

                if neighbor not in g_score or tentative_g_score < g_score[neighbor]:
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g_score
                    f = tentative_g_score + self._heuristic_distance(neighbor, goal, game_state)
                    f_score[neighbor] = f
                    heappush(open_set, (f, neighbor))

        return None  # Brak ścieżki

    def _astar_greedy_move(self, game_state, pawn):
        """Algorytm A* Greedy dla Policji

        Kroki:
        1. Oblicza mapę probabilistyczną możliwych pozycji Mr. X
        2. Wybiera cel o maksymalnej probabilistyce (tie-breaker: najbliższy węzeł)
        3. Używa A* do znalezienia najkrótszej ścieżki do celu
        4. Wykonuje pierwszy krok ścieżki
        """
        # Generujemy mapę probabilistyczną
        probability_map = self._compute_probability_map(game_state, pawn)

        if not probability_map:
            # Fallback do losowego ruchu
            options = game_state.get_available_moves(pawn)
            return random.choice(options) if options else None

        # Znajdujemy maksymalną probabilistykę
        max_prob = max(probability_map.values()) if probability_map else 0

        # Zbieramy wszystkie węzły o maksymalnej probabilistyce
        candidates = [node for node, prob in probability_map.items() if abs(prob - max_prob) < 1e-9]

        if not candidates:
            options = game_state.get_available_moves(pawn)
            return random.choice(options) if options else None

        # Tie-breaker: wybieramy węzeł najbliższy policjantowi
        target = min(candidates, key=lambda node: self._heuristic_distance(pawn.position, node, game_state))

        # Używamy A* do znalezienia ścieżki do celu
        path = self._astar_search(game_state, pawn.position, target, pawn.tickets)

        if path and len(path) > 1:
            # Bierzemy pierwszy krok ścieżki
            next_node = path[1]

            # Znajdujemy transport type do następnego węzła
            available_moves = game_state.get_available_moves(pawn)
            for neighbor, transport_type in available_moves:
                if neighbor == next_node:
                    return (neighbor, transport_type)

        # Fallback: jeśli A* nie znalazł ścieżki, wybieramy losowy ruch
        options = game_state.get_available_moves(pawn)
        return random.choice(options) if options else None

    def choose_move(self, game_state, pawn):
        """
        Główna funkcja decyzyjna AI dla policji.
        """
        # Jeśli Mr X jeszcze nie został ujawniony -> tryb poszukiwania
        if not getattr(game_state.mr_x, 'last_known_position', None) and game_state.round <= 3:
            return self._search_mode_move(game_state, pawn)

        # W przeciwnym razie -> tryb pościgu (Monte Carlo)
        return self._monte_carlo_move(game_state, pawn)

    def _search_mode_move(self, game_state, pawn):
        """
        Strategia dla pierwszych tur (przed ujawnieniem Mr X):
        Policjant stara się dojść w ciągu 3 ruchów do pola, które ma
        największą liczbę połączeń komunikacyjnych (czyli jest strategicznie centralne).
        """
        from collections import deque

        start = pawn.position
        best_node = start
        best_degree = len(game_state.graph.get(start, []))

        # BFS do głębokości 3
        queue = deque([(start, 0)])
        visited = {start}

        while queue:
            node, depth = queue.popleft()
            if depth >= 3:
                continue

            for (neighbor, transport) in game_state.graph.get(node, []):
                if neighbor not in visited:
                    visited.add(neighbor)
                    queue.append((neighbor, depth + 1))
                    degree = len(game_state.graph.get(neighbor, []))
                    if degree > best_degree:
                        best_degree = degree
                        best_node = neighbor

        # Ruch w kierunku best_node — wybieramy ruch z aktualnych możliwych,
        # który minimalizuje heurystyczną odległość do best_node
        options = game_state.get_available_moves(pawn)
        if not options:
            return None

        best_move = min(options, key=lambda m: self._heuristic_distance(m[0], best_node, game_state))
        return best_move

    def _monte_carlo_move(self, game_state, pawn):
        """
        Monte Carlo symulacja ruchu policjanta z uwzględnieniem zużycia biletów.
        Dla każdego możliwego ruchu wykonuje wiele losowych symulacji,
        gdzie policjanci i Mr. X poruszają się w ramach swoich dostępnych biletów.
        """
        import random
        from copy import deepcopy

        options = game_state.get_available_moves(pawn)
        if not options:
            return None

        SIMULATIONS_PER_OPTION = 100  # liczba symulacji na ruch
        SIMULATION_DEPTH = 3  # ile tur symulować
        CAPTURE_DISCOUNT = 0.9

        # mapa prawdopodobieństw pozycji Mr. X
        prob_map = self._compute_probability_map(game_state, pawn)
        if not prob_map:
            return random.choice(options)

        nodes, probs = zip(*[(n, p) for n, p in prob_map.items()])
        total = sum(probs)
        probs = [p / total for p in probs] if total > 0 else [1 / len(nodes)] * len(nodes)

        def sample_mr_x_pos():
            return random.choices(nodes, weights=probs, k=1)[0]

        def avail_moves_from(pos, tickets):
            return game_state.get_available_moves(type('Pawn', (), {'position': pos, 'tickets': tickets})())

        def simulate_once(start_move):
            # inicjalizacja kopii stanu
            police_positions = [p.position for p in game_state.police]
            police_tickets = [deepcopy(p.tickets) for p in game_state.police]
            pawn_index = next((i for i, p in enumerate(game_state.police) if p is pawn), 0)

            # ruch startowy policjanta
            dest_node, transport = start_move
            if police_tickets[pawn_index].get(transport, 0) <= 0:
                return 0.0  # nie ma biletu — ruch nielegalny
            police_positions[pawn_index] = dest_node
            police_tickets[pawn_index][transport] -= 1

            mr_x_pos = sample_mr_x_pos()
            if mr_x_pos in police_positions:
                return 1.0

            value, discount = 0.0, 1.0

            for _ in range(SIMULATION_DEPTH):
                # --- Mr. X ---
                mr_x_tickets = getattr(game_state.mr_x, 'tickets',
                                       {'taxi': float('inf'), 'bus': float('inf'), 'underground': float('inf'),
                                        'water': 5})
                mr_options = avail_moves_from(mr_x_pos, mr_x_tickets)
                if mr_options:
                    mr_x_pos = random.choice(mr_options)[0]

                # --- Policjanci ---
                for i in range(len(police_positions)):
                    pos = police_positions[i]
                    tickets = police_tickets[i]
                    moves = avail_moves_from(pos, tickets)

                    # tylko ruchy, na które mamy bilet
                    moves = [(d, t) for (d, t) in moves if tickets.get(t, 0) > 0]
                    if not moves:
                        continue  # brak ruchów

                    # 30% szansy na ruch w stronę Mr. X
                    if random.random() < 0.9:
                        best_move = min(moves, key=lambda m: self._heuristic_distance(m[0], mr_x_pos, game_state))
                    else:
                        best_move = random.choice(moves)

                    dest, transport = best_move
                    police_positions[i] = dest
                    tickets[transport] -= 1  # zużycie biletu

                # sprawdzenie złapania
                if mr_x_pos in police_positions:
                    value += discount * 1.0
                    break
                discount *= CAPTURE_DISCOUNT

            return value

        # Monte Carlo ocena każdego możliwego ruchu
        move_scores = {}
        for move in options:
            total = 0.0
            for _ in range(SIMULATIONS_PER_OPTION):
                total += simulate_once(move)
            move_scores[move] = total / SIMULATIONS_PER_OPTION

        return max(move_scores.items(), key=lambda kv: kv[1])[0]

    # ======= Game =======


class Game:
    def __init__(self, mr_x_player, police_players, mr_x_algorithm="random", police_algorithm="random"):
        self.mr_x_player = mr_x_player
        self.police_players = police_players
        self.mr_x_algorithm = mr_x_algorithm
        self.police_algorithm = police_algorithm
        self.running = True

        self.punkty = wczytaj_punkty('punkty.txt')
        self.polaczenia = wczytaj_polaczenia('polaczenia.txt')
        self.min_x, self.max_x, self.min_y, self.max_y = znajdz_zakres(self.punkty)

        mr_x_start, police_starts = losuj_pola_startowe(list(self.punkty.keys()), len(police_players))
        self.mr_x = Pawn("Mr. X", mr_x_start, KOLORY_GRACZY['mr_x'], True)
        self.police = [Pawn(f"Police {i + 1}", pos, KOLORY_GRACZY['police']) for i, pos in enumerate(police_starts)]

        self.turn_phase = "mr_x"
        self.selected_pawn = None
        self.highlighted_nodes = set()
        self.turn_number = 1
        self.max_turns = 22
        self.mr_x_moves = []

        # Śledzenie pozycji Mr. X dla przewidywania
        self.last_known_pos = None  # Ostatnia znana pozycja (None jeśli nie ujawniony)
        self.last_reveal_turn = 0  # Tura ostatniego ujawnienia (0 = brak)
        self.mr_x_position_history = {}  # Historia pozycji: tura -> pozycja (tylko ujawnienia)
        self.suspected_positions = set()  # Potencjalne pozycje - puste dopóki brak danych

        self.ai_mode = all(isinstance(p, (MrXAI, PoliceAI)) for p in [mr_x_player, *police_players])
        self.mixed_mode = not self.ai_mode and any(
            isinstance(p, (MrXAI, PoliceAI)) for p in [mr_x_player, *police_players])

        self.reset_pawn_moves()
        self.update_highlighted_nodes()
        self.selected_for_tickets = None

    # --- metody sprawdzające stan gry ---
    def is_mr_x_revealed(self):
        """Sprawdza czy Mr. X jest ujawniony w bieżącej turze"""
        return self.turn_number in REVEAL_TURNS

    # --- logika biletów ---
    def get_available_moves(self, pawn):
        moves = []
        for a, b, typ in self.polaczenia:
            if a == pawn.position:
                dest = b
            elif b == pawn.position:
                dest = a
            else:
                continue
            if pawn.tickets[typ] > 0:
                moves.append((dest, typ))
        return moves

    def reset_pawn_moves(self):
        self.mr_x.moved_this_turn = False
        for p in self.police:
            p.moved_this_turn = False

    def are_connected(self, a, b):
        return any((x == a and y == b) or (x == b and y == a) for x, y, t in self.polaczenia)

    def get_neighbors(self, node):
        return [b for a, b, t in self.polaczenia if a == node] + [a for a, b, t in self.polaczenia if b == node]

    def update_highlighted_nodes(self):
        self.highlighted_nodes.clear()
        if self.turn_phase == "mr_x" and isinstance(self.mr_x_player, HumanPlayer):
            if not self.mr_x.moved_this_turn:
                self.highlighted_nodes = set(dest for dest, typ in self.get_available_moves(self.mr_x))
        elif self.turn_phase == "police":
            for p, pl in zip(self.police, self.police_players):
                if isinstance(pl, HumanPlayer) and not p.moved_this_turn:
                    self.highlighted_nodes.update(dest for dest, typ in self.get_available_moves(p))

    def move_mr_x(self, move):
        dest, typ = move
        self.mr_x_moves.append(typ)
        self.mr_x.move_to(dest, typ)

        # Jeśli Mr. X jest ujawniony, zaktualizuj ostatnią znaną pozycję
        if self.is_mr_x_revealed():
            self.last_known_pos = dest
            self.last_reveal_turn = self.turn_number
            self.mr_x_position_history[self.turn_number] = dest
            self.suspected_positions = {dest}
        else:
            # Jeśli Mr. X nie jest ujawniony, aktualizuj podejrzane pozycje na podstawie ruchów
            if self.last_known_pos is not None:
                # Mamy ostatnią znaną pozycję - filtruj na podstawie sekwencji ruchów
                moves_since_reveal = self.mr_x_moves[self.last_reveal_turn - 1:]

                # Oblicz możliwe węzły przy danej sekwencji transportów
                from collections import deque
                mr_x_tickets = {'taxi': float('inf'), 'bus': float('inf'), 'underground': float('inf'), 'water': 5}

                queue = deque([(self.last_known_pos, 0)])
                visited = {(self.last_known_pos, 0)}
                reachable = set()

                while queue:
                    node, move_idx = queue.popleft()

                    if move_idx == len(moves_since_reveal):
                        reachable.add(node)
                        continue

                    required_transport = moves_since_reveal[move_idx]

                    available_moves = self.get_available_moves(
                        type('Pawn', (), {'position': node, 'tickets': mr_x_tickets})()
                    )

                    for neighbor, transport in available_moves:
                        if transport == required_transport:
                            state = (neighbor, move_idx + 1)
                            if state not in visited:
                                visited.add(state)
                                queue.append(state)

                self.suspected_positions = reachable if reachable else {self.last_known_pos}

    def handle_click(self, pos):
        for nr, dane in self.punkty.items():
            x, y = skaluj(dane['x'], dane['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH - 200, HEIGHT)
            if (pos[0] - x) ** 2 + (pos[1] - y) ** 2 < 15 ** 2:
                self.handle_node_click(nr)
                self.check_pawn_selection_for_tickets(nr)
                break

    def check_pawn_selection_for_tickets(self, node):
        for pawn in [self.mr_x] + self.police:
            if pawn.position == node:
                self.selected_for_tickets = pawn
                return
        self.selected_for_tickets = None

    def handle_node_click(self, node):
        if self.selected_pawn is None:
            if self.turn_phase == "mr_x" and isinstance(self.mr_x_player, HumanPlayer):
                if node == self.mr_x.position and not self.mr_x.moved_this_turn:
                    self.selected_pawn = self.mr_x
                    self.highlighted_nodes = set(dest for dest, typ in self.get_available_moves(self.mr_x))
            elif self.turn_phase == "police":
                for p, player in zip(self.police, self.police_players):
                    if isinstance(player, HumanPlayer) and node == p.position and not p.moved_this_turn:
                        self.selected_pawn = p
                        self.highlighted_nodes = set(dest for dest, typ in self.get_available_moves(p))
                        break
        else:
            available = self.get_available_moves(self.selected_pawn)
            for dest, typ in available:
                if dest == node:
                    self.selected_pawn.move_to(dest, typ)
                    if self.selected_pawn.is_mr_x:
                        self.mr_x_moves.append(typ)
                    self.selected_pawn = None
                    self.highlighted_nodes.clear()
                    self.check_game_end()
                    if self.turn_phase == "mr_x":
                        self.turn_phase = "police"
                    else:
                        all_humans_moved = all(
                            p.moved_this_turn for p, player in zip(self.police, self.police_players)
                            if isinstance(player, HumanPlayer)
                        )
                        if all_humans_moved:
                            self.turn_phase = "mr_x"
                            self.reset_pawn_moves()
                            self.turn_number += 1
                            if self.turn_number > self.max_turns:
                                print("KONIEC GRY – Mr. X nie został złapany!")
                                self.running = False
                    self.update_highlighted_nodes()
                    break
            else:
                self.selected_pawn = None
                self.highlighted_nodes.clear()
                self.update_highlighted_nodes()

    def execute_ai_turn(self):
        if self.turn_phase == "mr_x" and isinstance(self.mr_x_player, MrXAI):
            move = self.mr_x_player.get_move(self)
            if move:
                self.move_mr_x(move)
            self.check_game_end()
            self.turn_phase = "police"
            self.update_highlighted_nodes()
            return

        if self.turn_phase == "police":
            for pawn, ai in zip(self.police, self.police_players):
                if isinstance(ai, PoliceAI) and not pawn.moved_this_turn:
                    move = ai.get_move(self, pawn)
                    if move:
                        pawn.move_to(move[0], move[1])
            self.check_game_end()
            self.turn_phase = "mr_x"
            self.reset_pawn_moves()
            self.turn_number += 1
            if self.turn_number > self.max_turns:
                print("KONIEC GRY – Mr. X nie został złapany!")
                self.running = False
            self.update_highlighted_nodes()

    def check_game_end(self):
        for p in self.police:
            if p.position == self.mr_x.position:
                print("KONIEC GRY – Mr. X złapany!")
                self.running = False

    def draw_map(self, screen):
        for typ in ['water', 'underground', 'bus', 'taxi']:
            kolor, grubosc = kolory_polaczen[typ]
            for a, b, t in self.polaczenia:
                if t == typ:
                    x1, y1 = skaluj(self.punkty[a]['x'], self.punkty[a]['y'], self.min_x, self.max_x, self.min_y,
                                    self.max_y, WIDTH - 200, HEIGHT)
                    x2, y2 = skaluj(self.punkty[b]['x'], self.punkty[b]['y'], self.min_x, self.max_x, self.min_y,
                                    self.max_y, WIDTH - 200, HEIGHT)
                    pygame.draw.line(screen, kolor, (x1, y1), (x2, y2), grubosc)
        for nr, dane in self.punkty.items():
            x, y = skaluj(dane['x'], dane['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH - 200, HEIGHT)
            pygame.draw.circle(screen, YELLOW if nr in self.highlighted_nodes else BLACK, (x, y),
                               8 if nr in self.highlighted_nodes else 5)

        # Rysuj potencjalne pozycje Mr. X (na podstawie sekwencji ruchów) - fioletowe kółka
        if not self.is_mr_x_revealed() and self.last_known_pos is not None and any(
                isinstance(p, PoliceAI) for p in self.police_players):
            for suspected_node in self.suspected_positions:
                if suspected_node != self.last_known_pos:  # Nie rysuj ostatniej znanej pozycji (to będzie główne kółko)
                    x, y = skaluj(self.punkty[suspected_node]['x'], self.punkty[suspected_node]['y'], self.min_x,
                                  self.max_x, self.min_y, self.max_y, WIDTH - 200, HEIGHT)
                    pygame.draw.circle(screen, (200, 100, 200), (x, y), 7)  # Fioletowe kółka

        # Rysuj ostatnią znaną pozycję - większe fioletowe kółko
        if not self.is_mr_x_revealed() and self.last_known_pos is not None and self.last_known_pos in self.punkty and any(
                isinstance(p, PoliceAI) for p in self.police_players):
            x, y = skaluj(self.punkty[self.last_known_pos]['x'], self.punkty[self.last_known_pos]['y'], self.min_x,
                          self.max_x, self.min_y, self.max_y, WIDTH - 200, HEIGHT)
            pygame.draw.circle(screen, (200, 100, 200), (x, y), 10)  # Większe fioletowe kółko
            pygame.draw.circle(screen, (200, 100, 200), (x, y), 10, 2)  # Obramowanie

        # Rysuj Mr. X zawsze dla grającego, lub jeśli jest ujawniony dla policji
        should_show_mr_x = isinstance(self.mr_x_player, HumanPlayer) or self.is_mr_x_revealed()
        if should_show_mr_x:
            x, y = skaluj(self.punkty[self.mr_x.position]['x'], self.punkty[self.mr_x.position]['y'], self.min_x,
                          self.max_x, self.min_y, self.max_y, WIDTH - 200, HEIGHT)
            pygame.draw.circle(screen, self.mr_x.color, (x, y), 15)
            if self.selected_pawn == self.mr_x:
                pygame.draw.circle(screen, YELLOW, (x, y), 20, 3)

        if hasattr(self, 'predicted_police_positions') and isinstance(self.mr_x_player, MrXAI):
            for turn, positions in self.predicted_police_positions.items():
                if turn > 0 and turn <= 3:  # Pokazuj tylko 3 tury do przodu
                    # Kolor zależy od tury (im dalej, tym jaśniejszy)
                    alpha = 255 - (turn * 60)
                    color = (255, 165, 0, alpha)  # Pomarańczowy z alpha

                    for pos in positions:
                        if pos in self.punkty:
                            x, y = skaluj(self.punkty[pos]['x'], self.punkty[pos]['y'],
                                          self.min_x, self.max_x, self.min_y, self.max_y, WIDTH - 200, HEIGHT)

                            # Rysuj półprzezroczyste kółko
                            radius = 6 - turn  # Mniejsze dla dalszych tur
                            s = pygame.Surface((radius * 4, radius * 4), pygame.SRCALPHA)
                            pygame.draw.circle(s, color, (radius * 2, radius * 2), radius)
                            screen.blit(s, (x - radius * 2, y - radius * 2))

        for p, player_obj in zip(self.police, self.police_players):
            x, y = skaluj(self.punkty[p.position]['x'], self.punkty[p.position]['y'], self.min_x, self.max_x,
                          self.min_y, self.max_y, WIDTH - 200, HEIGHT)
            pygame.draw.circle(screen, p.color, (x, y), 12)
            if isinstance(player_obj, HumanPlayer) and self.turn_phase == "police" and not p.moved_this_turn:
                pygame.draw.circle(screen, YELLOW, (x, y), 18, 3)
            if self.selected_pawn == p:
                pygame.draw.circle(screen, YELLOW, (x, y), 20, 3)

    def draw_mr_x_moves_panel(self, screen):
        panel_x = WIDTH - 200
        panel_y = 50
        panel_width = 180
        panel_height = HEIGHT - 100
        pygame.draw.rect(screen, GRAY, (panel_x, panel_y, panel_width, panel_height))
        pygame.draw.rect(screen, BLUE, (panel_x, panel_y, panel_width, panel_height), 3)
        title = FONT.render("Mr. X transport", True, BLUE)
        screen.blit(title, (panel_x + 10, panel_y + 10))
        for i, move in enumerate(self.mr_x_moves[-10:]):
            text = FONT.render(f"{i + 1}. {move}", True, BLACK)
            screen.blit(text, (panel_x + 10, panel_y + 40 + i * 25))

        # Informacja o ujawnieniach - tylko dla gracza (Mr. X)
        if isinstance(self.mr_x_player, HumanPlayer):
            reveal_info_y = panel_y + 280
            small_font = pygame.font.SysFont("arial", 14)
            reveal_title = small_font.render("Ujawnienia:", True, RED)
            screen.blit(reveal_title, (panel_x + 10, reveal_info_y))

            for i, turn in enumerate(REVEAL_TURNS):
                if turn in self.mr_x_position_history:
                    pos = self.mr_x_position_history[turn]
                    reveal_text = small_font.render(f"Tura {turn}: pozycja {pos}", True, BLACK)
                else:
                    reveal_text = small_font.render(f"Tura {turn}: ???", True, GRAY)
                screen.blit(reveal_text, (panel_x + 10, reveal_info_y + 20 + i * 20))

        if self.selected_for_tickets:
            y_offset = panel_y + 480
            title2 = FONT.render(f"Bilety {self.selected_for_tickets.name}:", True, RED)
            screen.blit(title2, (panel_x + 10, y_offset))
            for j, (typ, ile) in enumerate(self.selected_for_tickets.tickets.items()):
                text = FONT.render(f"{typ}: {ile if ile != float('inf') else '∞'}", True, BLACK)
                screen.blit(text, (panel_x + 10, y_offset + 25 * (j + 1)))

    def get_algorithm_display_name(self, algorithm):
        """Konwertuje nazwę algorytmu na przyjazną dla użytkownika"""
        names = {
            'human': 'Gracz',
            'random': 'Random',
            'decoy': 'Decoy Movement',
            'dfs': 'DFS',
            'monte_carlo': 'Monte Carlo',
            'astar_greedy': 'A* Greedy',
        }
        return names.get(algorithm, algorithm)

    def draw_algorithm_info(self, screen):
        """Wyświetla informacje o algorytmach używanych przez graczy"""
        small_font = pygame.font.SysFont("arial", 18)
        info_y = 5

        mr_x_name = self.get_algorithm_display_name(self.mr_x_algorithm)
        police_name = self.get_algorithm_display_name(self.police_algorithm)

        mr_x_text = small_font.render(f"Mr. X: {mr_x_name}", True, BLACK)
        police_text = small_font.render(f"Policja: {police_name}", True, BLACK)

        screen.blit(mr_x_text, (WIDTH - 190, info_y))
        screen.blit(police_text, (WIDTH - 190, info_y + 25))

    def draw_legend(self, screen):
        small_font = pygame.font.SysFont("arial", 14)
        legend_y = HEIGHT - 150
        legend_x = 20

        pygame.draw.line(screen, GRAY, (legend_x, legend_y), (legend_x + 200, legend_y), 1)

        y_offset = legend_y + 10

        # Przewidywane pozycje policji (pomarańczowe)
        if hasattr(self, 'predicted_police_positions') and isinstance(self.mr_x_player, MrXAI):
            pygame.draw.circle(screen, (255, 165, 0), (legend_x + 10, y_offset), 5)
            text = small_font.render("Przewidywane pozycje policji", True, BLACK)
            screen.blit(text, (legend_x + 25, y_offset - 7))
            y_offset += 25

        # Ostatnia znana pozycja Mr. X
        if self.last_known_pos is not None:
            pygame.draw.circle(screen, (200, 100, 200), (legend_x + 10, y_offset), 8)
            pygame.draw.circle(screen, (200, 100, 200), (legend_x + 10, y_offset), 8, 2)
            text = small_font.render("Ostatnia znana pozycja Mr. X", True, BLACK)
            screen.blit(text, (legend_x + 25, y_offset - 7))
            y_offset += 25

            # Możliwe pozycje Mr. X
            pygame.draw.circle(screen, (200, 100, 200), (legend_x + 10, y_offset), 6)
            text = small_font.render("Możliwe pozycje Mr. X", True, BLACK)
            screen.blit(text, (legend_x + 25, y_offset - 7))

    def draw(self, screen):
        screen.fill(WHITE)
        self.draw_map(screen)
        self.draw_mr_x_moves_panel(screen)
        self.draw_algorithm_info(screen)
        self.draw_legend(screen)
        info = FONT.render(f"Tura: {self.turn_number}", True, BLUE)
        screen.blit(info, (20, 5))

        # Informacja o ujawnieniu Mr. X
        reveal_status = "UJAWNIONY!" if self.is_mr_x_revealed() else "UKRYTY"
        reveal_text = FONT.render(f"Mr. X: {reveal_status}", True, RED if self.is_mr_x_revealed() else BLUE)
        screen.blit(reveal_text, (20, 35))

        if (self.turn_phase == "mr_x" and isinstance(self.mr_x_player, MrXAI)) or \
                (self.turn_phase == "police" and any(isinstance(p, PoliceAI) for p in self.police_players)):
            text = FONT.render("ENTER aby AI wykonało ruch", True, RED)
            screen.blit(text, (20, 60))

        small_font = pygame.font.SysFont("arial", 16)
        esc_text = small_font.render("ESC aby powrócić do menu", True, GRAY)
        screen.blit(esc_text, (20, HEIGHT - 30))

        pygame.display.flip()

    def run(self):
        self.update_highlighted_nodes()
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                    self.running = False
                    return "menu"
                elif event.type == pygame.MOUSEBUTTONDOWN:
                    if self.turn_phase == "mr_x" and isinstance(self.mr_x_player, HumanPlayer):
                        self.handle_click(event.pos)
                    elif self.turn_phase == "police" and any(isinstance(p, HumanPlayer) for p in self.police_players):
                        self.handle_click(event.pos)
                elif event.type == pygame.KEYDOWN and event.key == pygame.K_RETURN:
                    if (self.turn_phase == "mr_x" and isinstance(self.mr_x_player, MrXAI)) or \
                            (self.turn_phase == "police" and any(isinstance(p, PoliceAI) for p in self.police_players)):
                        self.execute_ai_turn()
            self.draw(screen)


# ======= menu =======
class Button:
    def __init__(self, text, x, y, width, height, callback):
        self.rect = pygame.Rect(x, y, width, height)
        self.text = text
        self.callback = callback

    def draw(self, surface):
        pygame.draw.rect(surface, GRAY, self.rect)
        pygame.draw.rect(surface, BLUE, self.rect, 3)
        text_surf = FONT.render(self.text, True, BLUE)
        text_rect = text_surf.get_rect(center=self.rect.center)
        surface.blit(text_surf, text_rect)

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN and self.rect.collidepoint(event.pos):
            self.callback()


class RadioButton:
    def __init__(self, text, x, y, group_name, value):
        self.text = text
        self.rect = pygame.Rect(x, y, 20, 20)
        self.group_name = group_name
        self.value = value
        self.selected = False

    def draw(self, surface):
        pygame.draw.circle(surface, BLUE, self.rect.center, 10, 2)
        if self.selected:
            pygame.draw.circle(surface, BLUE, self.rect.center, 6)

        text_surf = FONT.render(self.text, True, BLACK)
        surface.blit(text_surf, (self.rect.x + 30, self.rect.y - 5))

    def handle_event(self, event, radio_groups):
        if event.type == pygame.MOUSEBUTTONDOWN:
            if self.rect.collidepoint(event.pos):
                for button in radio_groups.get(self.group_name, []):
                    button.selected = False
                self.selected = True


def start_as_police(algorithm):
    while True:
        game = Game(MrXAI('mr_x', algorithm), [HumanPlayer('police') for _ in range(5)], mr_x_algorithm=algorithm,
                    police_algorithm="human")
        result = game.run()
        if result == "menu":
            return
        break


def start_as_mr_x(algorithm):
    while True:
        game = Game(HumanPlayer('mr_x'), [PoliceAI('police', algorithm) for _ in range(5)], mr_x_algorithm="human",
                    police_algorithm=algorithm)
        result = game.run()
        if result == "menu":
            return
        break


def start_ai_vs_ai(mr_x_algo, police_algo):
    while True:
        game = Game(MrXAI('mr_x', mr_x_algo), [PoliceAI('police', police_algo) for _ in range(5)],
                    mr_x_algorithm=mr_x_algo, police_algorithm=police_algo)
        result = game.run()
        if result == "menu":
            return
        break


buttons = [
    Button("Gracz jako policja", 250, 150, 300, 60, lambda: None),  # Callback zostanie ustawiony w menu
    Button("Gracz jako Mr. X", 250, 280, 300, 60, lambda: None),
    Button("AI vs AI", 250, 410, 300, 60, lambda: None),
]

mr_x_algorithms = [
    RadioButton("Decoy Movement", 600, 155, "mr_x_algo", "decoy"),
    RadioButton("DFS", 600, 190, "mr_x_algo", "dfs"),
    RadioButton("Monte Carlo", 600, 225, "mr_x_algo", "monte_carlo"),
]

police_algorithms = [
    RadioButton("A* Greedy", 600, 300, "police_algo", "astar_greedy"),
    RadioButton("Monte Carlo", 600, 335, "police_algo", "monte_carlo"),
]

radio_groups = {
    "mr_x_algo": mr_x_algorithms,
    "police_algo": police_algorithms,
}


def get_selected_algorithm(group_name):
    for button in radio_groups.get(group_name, []):
        if button.selected:
            return button.value
    return "random"


def main_menu():
    buttons[0].callback = lambda: start_as_police(get_selected_algorithm("mr_x_algo"))
    buttons[1].callback = lambda: start_as_mr_x(get_selected_algorithm("police_algo"))
    buttons[2].callback = lambda: start_ai_vs_ai(get_selected_algorithm("mr_x_algo"),
                                                 get_selected_algorithm("police_algo"))

    small_font = pygame.font.SysFont("arial", 18)

    while True:
        screen.fill(WHITE)
        title = FONT.render("Wybierz tryb gry", True, BLUE)
        screen.blit(title, (WIDTH // 2 - title.get_width() // 2, 30))
        for button in buttons:
            button.draw(screen)

        mr_x_title = small_font.render("Algorytm Mr. X:", True, BLUE)
        screen.blit(mr_x_title, (600, 125))

        police_title = small_font.render("Algorytm Policji:", True, BLUE)
        screen.blit(police_title, (600, 280))

        for radio in mr_x_algorithms:
            radio.draw(screen)

        for radio in police_algorithms:
            radio.draw(screen)

        esc_info_font = pygame.font.SysFont("arial", 16)
        esc_info = esc_info_font.render("ESC aby wyjść", True, GRAY)
        screen.blit(esc_info, (WIDTH // 2 - esc_info.get_width() // 2, HEIGHT - 50))

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                pygame.quit()
                sys.exit()
            for button in buttons:
                button.handle_event(event)
            for radio in mr_x_algorithms:
                radio.handle_event(event, radio_groups)
            for radio in police_algorithms:
                radio.handle_event(event, radio_groups)

        pygame.display.flip()


main_menu()

