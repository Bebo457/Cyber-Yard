import pygame
import sys
import numpy as np

pygame.init()
WIDTH, HEIGHT = 1200, 800
screen = pygame.display.set_mode((WIDTH, HEIGHT))

pygame.display.set_caption("Scotland Yard")

FONT = pygame.font.SysFont("arial", 36)
WHITE = (255, 255, 255)
BLACK = (0, 0, 0)
GRAY = (200, 200, 200)
BLUE = (50, 100, 200)

#wartości punktów startowych pobrane od chata
PUNKTY_STARTOWE = [13, 14, 15, 18, 19, 21, 23, 26, 29, 34,
                   35, 42, 44, 45, 46, 51, 53, 61, 62, 67,
                   68, 69, 74, 75, 78, 79, 82, 86, 94]

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

def kolor_stacji(typy):
    if len(typy) == 1 and typy[0] == 'taxi':
        return (128, 128, 128)
    elif 'water' in typy:
        return (0, 0, 255)
    elif 'underground' in typy:
        return (255, 0, 0)
    elif 'bus' in typy:
        return (0, 200, 0)
    else:
        return (128, 0, 128)

def znajdz_zakres(punkty):
    xs = [d['x'] for d in punkty.values()]
    ys = [d['y'] for d in punkty.values()]
    return min(xs), max(xs), min(ys), max(ys)

def skaluj(x, y, min_x, max_x, min_y, max_y, width, height, margin=50):
    # przeskaluj współrzędne do rozmiaru okna z marginesem
    scale_x = (width - 2 * margin) / (max_x - min_x)
    scale_y = (height - 2 * margin) / (max_y - min_y)
    sx = margin + (x - min_x) * scale_x
    sy = margin + (y - min_y) * scale_y
    return int(sx), int(sy)

def losuj_pola_startowe(karty, liczba_policjantow):
    karty = karty.copy()
    np.random.shuffle(karty)
    mr_x = karty.pop()
    police = karty[:liczba_policjantow]
    return mr_x, police

class Game:
    def __init__(self, mr_x_player, police_players):
        self.mr_x = mr_x_player
        self.police = police_players
        self.turn = 0
        self.running = True
        self.punkty = wczytaj_punkty('punkty.txt')
        self.polaczenia = wczytaj_polaczenia('polaczenia.txt')
        self.min_x, self.max_x, self.min_y, self.max_y = znajdz_zakres(self.punkty)
        mr_x_start, police_starts = losuj_pola_startowe(list(self.punkty.keys()), len(police_players))
        self.pozycje = {
            'mr_x': mr_x_start,
            'police': police_starts
        }

    def update(self):
        # Tu będzie logika aktualizacji stanu gry (np. ruchy graczy)
        pass

    def draw_map(self, screen):
        # Rysuj połączenia
        for typ in ['water', 'underground', 'bus', 'taxi']:
            kolor, grubosc = kolory_polaczen[typ]
            for a, b, t in self.polaczenia:
                if t == typ:
                    x1, y1 = skaluj(self.punkty[a]['x'], self.punkty[a]['y'],
                                    self.min_x, self.max_x, self.min_y, self.max_y,
                                    WIDTH, HEIGHT)
                    x2, y2 = skaluj(self.punkty[b]['x'], self.punkty[b]['y'],
                                    self.min_x, self.max_x, self.min_y, self.max_y,
                                    WIDTH, HEIGHT)
                    pygame.draw.line(screen, kolor, (x1, y1), (x2, y2), grubosc)

        # Rysuj stacje
        for nr, dane in self.punkty.items():
            x, y = skaluj(dane['x'], dane['y'],
                          self.min_x, self.max_x, self.min_y, self.max_y,
                          WIDTH, HEIGHT)

            typy = dane['typy']
            scale_factor = min(WIDTH, HEIGHT) / 1200

            promienie = {
                'underground': int(30 * scale_factor),
                'bus': int(20 * scale_factor),
                'taxi': int(10 * scale_factor),
                'water': int(10 * scale_factor)
            }

            for typ in ['underground', 'bus', 'taxi', 'water']:
                if typ in typy:
                    kolor, _ = kolory_polaczen[typ]
                    r = promienie[typ]
                    pygame.draw.circle(screen, kolor, (x, y), r)

            max_r = max([promienie[t] for t in typy if t in promienie], default=10)
            pygame.draw.circle(screen, (0, 0, 0), (x, y), max_r, 2)

        # Rysuj Mr. X i policje
        x, y = skaluj(self.punkty[self.pozycje['mr_x']]['x'], self.punkty[self.pozycje['mr_x']]['y'],
                      self.min_x, self.max_x, self.min_y, self.max_y, WIDTH, HEIGHT)
        x_surf = FONT.render("X", True, KOLORY_GRACZY['mr_x'])
        x_rect = x_surf.get_rect(center=(x, y))
        screen.blit(x_surf, x_rect)

        for i, pos in enumerate(self.pozycje['police']):
            kolor = KOLORY_GRACZY['police']
            x, y = skaluj(self.punkty[pos]['x'], self.punkty[pos]['y'],
                          self.min_x, self.max_x, self.min_y, self.max_y, WIDTH, HEIGHT)
            x_surf = FONT.render("X", True, kolor)
            x_rect = x_surf.get_rect(center=(x, y))
            screen.blit(x_surf, x_rect)

    def draw(self, screen):
        screen.fill((255, 255, 255))
        self.draw_map(screen)
        pygame.display.flip()

    def run(self):
        while self.running:
            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False

            self.update()
            self.draw(screen)

class HumanPlayer:
    def __init__(self, role):
        self.role = role

    def get_move(self, game_state):
        # odczyt ruchu z interfejsu (np. kliknięcie)
        pass

class MrXAI:
    def __init__(self, role):
        self.role = role

    def get_move(self, game_state):
        # logika AI Mr. X
        pass

class PoliceAI:
    def __init__(self, role):
        self.role = role

    def get_move(self, game_state):
        # logika AI policji
        pass

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

# Funkcje trybów
def start_as_police():
    mr_x = MrXAI('mr_x')
    police = [HumanPlayer('police') for _ in range(5)]
    game = Game(mr_x, police)
    game.run()

def start_as_mr_x():
    mr_x = HumanPlayer('mr_x')
    police = [PoliceAI('police') for _ in range(5)]
    game = Game(mr_x, police)
    game.run()

def start_ai_vs_ai():
    mr_x = MrXAI('mr_x')
    police = [PoliceAI('police') for _ in range(5)]
    game = Game(mr_x, police)
    game.run()

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

buttons = [
    Button("Gracz jako policja", 250, 150, 300, 60, start_as_police),
    Button("Gracz jako Mr. X", 250, 250, 300, 60, start_as_mr_x),
    Button("AI vs AI", 250, 350, 300, 60, start_ai_vs_ai),
]

# Pętla menu
def main_menu():
    while True:
        screen.fill(WHITE)
        title = FONT.render("Wybierz tryb gry", True, BLUE)
        screen.blit(title, (WIDTH // 2 - title.get_width() // 2, 50))

        for button in buttons:
            button.draw(screen)

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                pygame.quit()
                sys.exit()
            for button in buttons:
                button.handle_event(event)

        pygame.display.flip()

main_menu()
