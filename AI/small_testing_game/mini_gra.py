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
        """Algorytm Decoy Movement dla Mr. X
        TODO: Implementacja algorytmu zwodzenia policji
        """
        # Placeholder dla implementacji decoy Mr. X
        options = game_state.get_available_moves(game_state.mr_x)
        return random.choice(options) if options else None
    
    # def _dfs_move(self, game_state):
    #     """Algorytm DFS (Depth-First Search) dla Mr. X
    #     TODO: Implementacja algorytmu DFS do unikania policji
    #     """
    #     # Placeholder dla implementacji DFS Mr. X
    #     options = game_state.get_available_moves(game_state.mr_x)
    #     """
    #     choose where want to be at mrX reviel - how far is it, and how to get there (is it possible, how far police is from there and doesnt anybody go to that place)
    #     prioritize those place starting from one with the highest moves opportunities, distance form police (can police reach that place in that number of moves, 
    #     can they catch us on the way, which tickets to use, try not to use rare tickets unless it is close to the game end or there are no other way to escape). 
    #     other ideas i skipped, but are explained in dfs sheet.
    #     """
    #     available_police_moves = game_state.get_available_moves(game_state.police)
    #     current_round = game_state.round()

    #     #for now copyied that from return
    #     move = random.choice(options) if options else None
    #     return move #random.choice(options) if options else None
    
    # def _dfs_move(self, game_state):
    #     """Algorytm DFS (Depth-First Search) dla Mr. X"""
        
    #     # Parameters
    #     TARGET_LENGTH = 5  # Desired path length
    #     MAX_DEPTH = 8  # Maximum search depth
    #     MAX_ATTEMPTS_PER_NODE = 3  # Max attempts per node to avoid infinite loops
        
    #     # Helper function to calculate shortest distance between two nodes (BFS)
    #     def shortest_distance(start, end, graph):
    #         if start == end:
    #             return 0
    #         visited = {start}
    #         queue = [(start, 0)]
    #         idx = 0
    #         while idx < len(queue):
    #             current, dist = queue[idx]
    #             idx += 1
    #             for neighbor in get_neighbors(current, graph):
    #                 if neighbor in visited:
    #                     continue
    #                 if neighbor == end:
    #                     return dist + 1
    #                 visited.add(neighbor)
    #                 queue.append((neighbor, dist + 1))
    #         return float('inf')
        
    #     # Helper function to get neighbors
    #     def get_neighbors(node, graph):
    #         neighbors = []
    #         for a, b, typ in graph:
    #             if a == node:
    #                 neighbors.append((b, typ))
    #             elif b == node:
    #                 neighbors.append((a, typ))
    #         return neighbors
        
    #     # Helper function to check if move is safe
    #     def is_safe_move(node, mr_x_pos, police_positions, graph):
    #         mr_x_distance = shortest_distance(mr_x_pos, node, graph)
    #         for police_pos in police_positions:
    #             police_distance = shortest_distance(police_pos, node, graph)
    #             if mr_x_distance >= police_distance:
    #                 return False
    #         return True
        
    #     # Recursive DFS function
    #     def dfs(current, path, visited, water_tickets, attempts, current_path_length):
    #         nonlocal best_path, successful_paths
            
    #         # Check attempt limit
    #         if attempts.get(current, 0) >= MAX_ATTEMPTS_PER_NODE:
    #             return
            
    #         attempts[current] = attempts.get(current, 0) + 1
            
    #         # Get neighbors
    #         neighbors = get_neighbors(current, game_state.polaczenia)
            
    #         for neighbor, transport_type in neighbors:
    #             # Skip if already visited
    #             if neighbor in visited:
    #                 continue
                
    #             # Check if Mr. X has tickets for this transport
    #             if game_state.mr_x.tickets.get(transport_type, 0) <= 0:
    #                 continue
                
    #             # Safety check: Mr. X must be closer than all police
    #             police_positions = [p.position for p in game_state.police]
    #             if not is_safe_move(neighbor, game_state.mr_x.position, police_positions, game_state.polaczenia):
    #                 continue
                
    #             # Check if current path to neighbor is optimal
    #             optimal_distance = shortest_distance(game_state.mr_x.position, neighbor, game_state.polaczenia)
    #             if current_path_length + 1 > optimal_distance:
    #                 continue
                
    #             # Handle water (ferry) tickets
    #             water_tickets_used = 0
    #             if transport_type == 'water':
    #                 if water_tickets <= 0:
    #                     continue
    #                 water_tickets_used = 1
                
    #             # Add to path
    #             visited.add(neighbor)
    #             path.append((neighbor, transport_type))
                
    #             # Check if we reached target length
    #             if len(path) >= TARGET_LENGTH:
    #                 successful_paths.append(path[:])
    #                 visited.remove(neighbor)
    #                 path.pop()
    #                 return
                
    #             # Continue DFS if within depth limit
    #             if len(path) < MAX_DEPTH:
    #                 dfs(neighbor, path, visited, water_tickets - water_tickets_used, 
    #                     attempts.copy(), current_path_length + 1)
                
    #             # Update best path if this is longer
    #             if len(path) > len(best_path):
    #                 best_path = path[:]
                
    #             # Backtrack
    #             visited.remove(neighbor)
    #             path.pop()
        
    #     # Main algorithm
    #     options = game_state.get_available_moves(game_state.mr_x)
    #     if not options:
    #         return None
        
    #     # Initialize variables
    #     best_path = []
    #     successful_paths = []
    #     mr_x_pos = game_state.mr_x.position
    #     initial_water_tickets = game_state.mr_x.tickets.get('water', 0)
        
    #     # Try DFS from each available neighbor
    #     initial_neighbors = get_neighbors(mr_x_pos, game_state.polaczenia)
        
    #     for neighbor, transport_type in initial_neighbors:
    #         # Check if move is available (has tickets)
    #         if game_state.mr_x.tickets.get(transport_type, 0) <= 0:
    #             continue
            
    #         # Check if this is a valid first move
    #         if (neighbor, transport_type) not in options:
    #             continue
            
    #         # Initialize for this branch
    #         visited = {mr_x_pos, neighbor}
    #         path = [(neighbor, transport_type)]
    #         attempts = {}
    #         water_used = 1 if transport_type == 'water' else 0
            
    #         # Run DFS
    #         dfs(neighbor, path, visited, initial_water_tickets - water_used, attempts, 1)
            
    #         # If we found a successful path, return it
    #         if successful_paths:
    #             longest = max(successful_paths, key=len)
    #             return longest[0]  # Return first move of longest path
        
    #     # If no successful path found, return first move of best path
    #     if best_path:
    #         return best_path[0]
        
    #     # Fallback: choose safest available move
    #     police_positions = [p.position for p in game_state.police]
    #     safe_moves = []
    #     for dest, transport_type in options:
    #         if is_safe_move(dest, mr_x_pos, police_positions, game_state.polaczenia):
    #             safe_moves.append((dest, transport_type))
        
    #     if safe_moves:
    #         # Prefer moves that maximize distance from police
    #         def min_police_distance(move):
    #             dest = move[0]
    #             return min(shortest_distance(p_pos, dest, game_state.polaczenia) 
    #                     for p_pos in police_positions)
            
    #         return max(safe_moves, key=min_police_distance)
        
    #     # Last resort: random move
    #     return random.choice(options) if options else None

    #     def _monte_carlo_move(self, game_state):
    #         """Algorytm Monte Carlo dla Mr. X
    #         TODO: Implementacja algorytmu Monte Carlo
    #         """
    #         # Placeholder dla implementacji Monte Carlo Mr. X
    #         options = game_state.get_available_moves(game_state.mr_x)
    #         return random.choice(options) if options else None

    def _dfs_move(self, game_state):
        """Ulepszony algorytm DFS z predykcją ruchów policji"""
        
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
                
                # Rekurencja
                dfs(neighbor, path, visited, new_tickets, attempts.copy(), depth + 1, predicted_police_pos)
                
                # Backtrack
                visited.remove(neighbor)
                path.pop()
        
        # === GŁÓWNA LOGIKA ===
        
        options = game_state.get_available_moves(game_state.mr_x)
        if not options:
            return None
        
        police_list = game_state.police
        mr_x_pos = game_state.mr_x.position
        graph = game_state.polaczenia
        
        # KROK 1: Przewiduj pozycje policji
        print(f"\n🔮 Przewidywanie ruchów policji na {PREDICTION_DEPTH} tury do przodu...")
        predicted_police_positions = predict_police_positions(police_list, PREDICTION_DEPTH, graph)
        
        for turn, positions in predicted_police_positions.items():
            print(f"  Tura +{turn}: {len(positions)} możliwych pozycji policji")
        
        # KROK 2: Zapisz przewidywane pozycje do wizualizacji (opcjonalne)
        game_state.predicted_police_positions = predicted_police_positions
        
        # KROK 3: Inicjalizacja najlepszej ścieżki
        best_path = []
        best_score = -float('inf')
        
        # KROK 4: Oceń wszystkie możliwe pierwsze ruchy
        print(f"📊 Ocenianie {len(options)} możliwych ruchów...")
        initial_moves = []
        
        for dest, transport in options:
            # Sprawdź bezpieczeństwo pierwszego ruchu
            if is_position_safe_at_turn(dest, 1, predicted_police_positions, mr_x_pos, graph):
                # Szybka ocena tego ruchu
                score = 0
                if 1 in predicted_police_positions:
                    min_dist = float('inf')
                    for police_pos in predicted_police_positions[1]:
                        dist = bfs_distances(police_pos, graph).get(dest, float('inf'))
                        min_dist = min(min_dist, dist)
                    score = min_dist
                
                initial_moves.append((dest, transport, score))
        
        if not initial_moves:
            print("⚠️  Brak bezpiecznych ruchów! Wybieranie najmniej niebezpiecznego...")
            # Fallback: wybierz ruch który maksymalizuje dystans
            for dest, transport in options:
                min_dist = float('inf')
                for police in police_list:
                    dist = bfs_distances(police.position, graph).get(dest, float('inf'))
                    min_dist = min(min_dist, dist)
                initial_moves.append((dest, transport, min_dist))
        
        initial_moves.sort(key=lambda x: x[2], reverse=True)
        
        # KROK 5: Eksploruj najlepsze początkowe ruchy za pomocą DFS
        print(f"🔍 Eksploracja top {min(5, len(initial_moves))} ruchów...")
        for dest, transport, _ in initial_moves[:5]:
            visited = {mr_x_pos, dest}
            path = [(dest, transport)]
            tickets = game_state.mr_x.tickets.copy()
            if tickets[transport] != float('inf'):
                tickets[transport] -= 1
            attempts = {}
            
            dfs(dest, path, visited, tickets, attempts, 1, predicted_police_positions)
        
        # KROK 6: Zwróć najlepszy ruch
        if best_path:
            print(f"✅ Wybrano ścieżkę długości {len(best_path)} ze score: {best_score:.0f}")
            print(f"   Pierwszy ruch: {best_path[0][0]} ({best_path[0][1]})")
            return best_path[0]
        
        # Fallback
        print("⚠️  Używam fallback - najbezpieczniejszy pojedynczy ruch")
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
    
    def _monte_carlo_move(self, game_state, pawn):
        """Algorytm Monte Carlo dla Policji
        TODO: Implementacja algorytmu Monte Carlo do kooperacji
        """
        # Placeholder dla implementacji Monte Carlo Policji
        options = game_state.get_available_moves(pawn)
        return random.choice(options) if options else None  

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
        self.mixed_mode = not self.ai_mode and any(isinstance(p, (MrXAI, PoliceAI)) for p in [mr_x_player, *police_players])

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
            x, y = skaluj(dane['x'], dane['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
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
                    x1, y1 = skaluj(self.punkty[a]['x'], self.punkty[a]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
                    x2, y2 = skaluj(self.punkty[b]['x'], self.punkty[b]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
                    pygame.draw.line(screen, kolor, (x1, y1), (x2, y2), grubosc)
        for nr, dane in self.punkty.items():
            x, y = skaluj(dane['x'], dane['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
            pygame.draw.circle(screen, YELLOW if nr in self.highlighted_nodes else BLACK, (x, y), 8 if nr in self.highlighted_nodes else 5)
        
        # Rysuj potencjalne pozycje Mr. X (na podstawie sekwencji ruchów) - fioletowe kółka
        if not self.is_mr_x_revealed() and self.last_known_pos is not None and any(isinstance(p, PoliceAI) for p in self.police_players):
            for suspected_node in self.suspected_positions:
                if suspected_node != self.last_known_pos:  # Nie rysuj ostatniej znanej pozycji (to będzie główne kółko)
                    x, y = skaluj(self.punkty[suspected_node]['x'], self.punkty[suspected_node]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
                    pygame.draw.circle(screen, (200, 100, 200), (x, y), 7)  # Fioletowe kółka
        
        # Rysuj ostatnią znaną pozycję - większe fioletowe kółko
        if not self.is_mr_x_revealed() and self.last_known_pos is not None and self.last_known_pos in self.punkty and any(isinstance(p, PoliceAI) for p in self.police_players):
            x, y = skaluj(self.punkty[self.last_known_pos]['x'], self.punkty[self.last_known_pos]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
            pygame.draw.circle(screen, (200, 100, 200), (x, y), 10)  # Większe fioletowe kółko
            pygame.draw.circle(screen, (200, 100, 200), (x, y), 10, 2)  # Obramowanie
        
        # Rysuj Mr. X zawsze dla grającego, lub jeśli jest ujawniony dla policji
        should_show_mr_x = isinstance(self.mr_x_player, HumanPlayer) or self.is_mr_x_revealed()
        if should_show_mr_x:
            x, y = skaluj(self.punkty[self.mr_x.position]['x'], self.punkty[self.mr_x.position]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
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
                                        self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
                            
                            # Rysuj półprzezroczyste kółko
                            radius = 6 - turn  # Mniejsze dla dalszych tur
                            s = pygame.Surface((radius*4, radius*4), pygame.SRCALPHA)
                            pygame.draw.circle(s, color, (radius*2, radius*2), radius)
                            screen.blit(s, (x-radius*2, y-radius*2))
        
        for p, player_obj in zip(self.police, self.police_players):
            x, y = skaluj(self.punkty[p.position]['x'], self.punkty[p.position]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
            pygame.draw.circle(screen, p.color, (x, y), 12)
            if isinstance(player_obj, HumanPlayer) and self.turn_phase == "police" and not p.moved_this_turn:
                pygame.draw.circle(screen, YELLOW, (x, y), 18, 3)
            if self.selected_pawn == p:
                pygame.draw.circle(screen, YELLOW, (x, y), 20, 3)

    def draw_mr_x_moves_panel(self, screen):
        panel_x = WIDTH-200
        panel_y = 50
        panel_width = 180
        panel_height = HEIGHT-100
        pygame.draw.rect(screen, GRAY, (panel_x, panel_y, panel_width, panel_height))
        pygame.draw.rect(screen, BLUE, (panel_x, panel_y, panel_width, panel_height), 3)
        title = FONT.render("Mr. X transport", True, BLUE)
        screen.blit(title, (panel_x+10, panel_y+10))
        for i, move in enumerate(self.mr_x_moves[-10:]):
            text = FONT.render(f"{i+1}. {move}", True, BLACK)
            screen.blit(text, (panel_x+10, panel_y+40 + i*25))
        
        # Informacja o ujawnieniach - tylko dla gracza (Mr. X)
        if isinstance(self.mr_x_player, HumanPlayer):
            reveal_info_y = panel_y + 280
            small_font = pygame.font.SysFont("arial", 14)
            reveal_title = small_font.render("Ujawnienia:", True, RED)
            screen.blit(reveal_title, (panel_x+10, reveal_info_y))
            
            for i, turn in enumerate(REVEAL_TURNS):
                if turn in self.mr_x_position_history:
                    pos = self.mr_x_position_history[turn]
                    reveal_text = small_font.render(f"Tura {turn}: pozycja {pos}", True, BLACK)
                else:
                    reveal_text = small_font.render(f"Tura {turn}: ???", True, GRAY)
                screen.blit(reveal_text, (panel_x+10, reveal_info_y + 20 + i*20))
        
        if self.selected_for_tickets:
            y_offset = panel_y + 480
            title2 = FONT.render(f"Bilety {self.selected_for_tickets.name}:", True, RED)
            screen.blit(title2, (panel_x+10, y_offset))
            for j, (typ, ile) in enumerate(self.selected_for_tickets.tickets.items()):
                text = FONT.render(f"{typ}: {ile if ile!=float('inf') else '∞'}", True, BLACK)
                screen.blit(text, (panel_x+10, y_offset + 25*(j+1)))

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
        
        screen.blit(mr_x_text, (WIDTH-190, info_y))
        screen.blit(police_text, (WIDTH-190, info_y + 25))

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
        game = Game(MrXAI('mr_x', algorithm), [HumanPlayer('police') for _ in range(5)], mr_x_algorithm=algorithm, police_algorithm="human")
        result = game.run()
        if result == "menu":
            return
        break

def start_as_mr_x(algorithm):
    while True:
        game = Game(HumanPlayer('mr_x'), [PoliceAI('police', algorithm) for _ in range(5)], mr_x_algorithm="human", police_algorithm=algorithm)
        result = game.run()
        if result == "menu":
            return
        break

def start_ai_vs_ai(mr_x_algo, police_algo):
    while True:
        game = Game(MrXAI('mr_x', mr_x_algo), [PoliceAI('police', police_algo) for _ in range(5)], mr_x_algorithm=mr_x_algo, police_algorithm=police_algo)
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
    buttons[2].callback = lambda: start_ai_vs_ai(get_selected_algorithm("mr_x_algo"), get_selected_algorithm("police_algo"))
    
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
