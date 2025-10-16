import pygame
import sys
import numpy as np
import random

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
    
    def _dfs_move(self, game_state):
        """Algorytm DFS (Depth-First Search) dla Mr. X
        TODO: Implementacja algorytmu DFS do unikania policji
        """
        # Placeholder dla implementacji DFS Mr. X
        options = game_state.get_available_moves(game_state.mr_x)
        return random.choice(options) if options else None
    
    def _monte_carlo_move(self, game_state):
        """Algorytm Monte Carlo dla Mr. X
        TODO: Implementacja algorytmu Monte Carlo
        """
        # Placeholder dla implementacji Monte Carlo Mr. X
        options = game_state.get_available_moves(game_state.mr_x)
        return random.choice(options) if options else None

class PoliceAI:
    def __init__(self, role, algorithm="random"):
        self.role = role
        self.algorithm = algorithm  # "random", "astar_greedy", "monte_carlo"
    
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
    
    def _astar_greedy_move(self, game_state, pawn):
        """Algorytm A* Greedy dla Policji
        TODO: Implementacja algorytmu A* do poszukiwania Mr. X
        """
        # Placeholder dla implementacji A* Greedy Policji
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

        self.ai_mode = all(isinstance(p, (MrXAI, PoliceAI)) for p in [mr_x_player, *police_players])
        self.mixed_mode = not self.ai_mode and any(isinstance(p, (MrXAI, PoliceAI)) for p in [mr_x_player, *police_players])

        self.reset_pawn_moves()
        self.update_highlighted_nodes()
        self.selected_for_tickets = None

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

    # --- rysowanie ---
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
        x, y = skaluj(self.punkty[self.mr_x.position]['x'], self.punkty[self.mr_x.position]['y'], self.min_x, self.max_x, self.min_y, self.max_y, WIDTH-200, HEIGHT)
        pygame.draw.circle(screen, self.mr_x.color, (x, y), 15)
        if self.selected_pawn == self.mr_x:
            pygame.draw.circle(screen, YELLOW, (x, y), 20, 3)
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
        for i, move in enumerate(self.mr_x_moves[-22:]):
            text = FONT.render(f"{i+1}. {move}", True, BLACK)
            screen.blit(text, (panel_x+10, panel_y+40 + i*25))
        if self.selected_for_tickets:
            y_offset = panel_y + 500
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

    def draw(self, screen):
        screen.fill(WHITE)
        self.draw_map(screen)
        self.draw_mr_x_moves_panel(screen)
        self.draw_algorithm_info(screen)
        info = FONT.render(f"Tura: {self.turn_number}", True, BLUE)
        screen.blit(info, (20, 5))
        if (self.turn_phase == "mr_x" and isinstance(self.mr_x_player, MrXAI)) or \
           (self.turn_phase == "police" and any(isinstance(p, PoliceAI) for p in self.police_players)):
            text = FONT.render("ENTER aby AI wykonało ruch", True, RED)
            screen.blit(text, (20, 30))
        
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
