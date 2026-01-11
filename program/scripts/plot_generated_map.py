import csv
import matplotlib.pyplot as plt
import os
import numpy as np

def read_nodes(filename):
    nodes = {}
    with open(filename, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            idx = int(row['id'])
            x = float(row['pos_x'])
            y = float(row['pos_y'])
            nodes[idx] = (x, y, row.get('station_type', ''))
    return nodes

def read_edges(filename):
    edges = []
    with open(filename, newline='', encoding='utf-8') as f:
        reader = csv.DictReader(f)
        for row in reader:
            src = int(row['source'])
            dst = int(row['dest'])
            typ = row['type']
            points = []
            if row['points']:
                for pt in row['points'].split('|'):
                    x, y = map(float, pt.split(';'))
                    points.append((x, y))
            edges.append((src, dst, typ, points))
    return edges

def plot_map(nodes, edges):
    color_map = {'taxi': 'yellow', 'bus': 'green', 'metro': 'red', 'water': 'blue'}
    size_map = {'taxi': 30, 'bus': 60, 'metro': 100}
    zorder_map = {'taxi': 1, 'bus': 2, 'metro': 3}
    plt.figure(figsize=(10, 10))
    ax = plt.gca()

    # Przygotuj strukturę do szybkiego wyszukiwania sąsiadów
    neighbors = {idx: set() for idx in nodes}
    edge_lookup = {}
    for src, dst, typ, points in edges:
        neighbors[src].add(dst)
        neighbors[dst].add(src)
        edge_lookup[(src, dst)] = (typ, points)
        edge_lookup[(dst, src)] = (typ, list(reversed(points)))

    # Rysuj krawędzie i zapamiętaj linie
    edge_lines = {}
    for src, dst, typ, points in edges:
        color = color_map.get(typ, 'black')
        lw = 2
        if typ == 'metro':
            lw = 4
        elif typ == 'bus':
            lw = 3
        path = [(nodes[src][0], -nodes[src][1])] + [(x, -y) for (x, y) in points] + [(nodes[dst][0], -nodes[dst][1])]
        xs, ys = zip(*path)
        line, = ax.plot(xs, ys, color=color, linewidth=lw, alpha=0.8, zorder=zorder_map.get(typ, 1))
        edge_lines[(src, dst)] = line
        edge_lines[(dst, src)] = line

    # Rysuj węzły i zapamiętaj scattery
    node_scatters = {}
    for idx, (x, y, stype) in nodes.items():
        my = -y
        if 'metro' in stype:
            sc = ax.scatter(x, my, c=color_map['metro'], s=size_map['metro'], marker='o', zorder=3, edgecolors='black', linewidths=0.7, picker=True)
        elif 'bus' in stype:
            sc = ax.scatter(x, my, c=color_map['bus'], s=size_map['bus'], marker='s', zorder=2, edgecolors='black', linewidths=0.7, picker=True)
        elif 'taxi' in stype:
            sc = ax.scatter(x, my, c=color_map['taxi'], s=size_map['taxi'], marker='.', zorder=1, edgecolors='black', linewidths=0.5, picker=True)
        else:
            sc = ax.scatter(x, my, c='gray', s=10, marker='.', zorder=0, picker=True)
        node_scatters[idx] = sc

    plt.axis('equal')
    plt.title('Generated Map (hover to highlight)')
    plt.xlabel('X')
    plt.ylabel('Y')
    plt.tight_layout()

    # Funkcja podświetlania
    def highlight(event):
        if not hasattr(event, 'ind') or event.artist not in node_scatters.values():
            return
        # Znajdź indeks klikniętego punktu
        for idx, sc in node_scatters.items():
            if event.artist == sc:
                break
        else:
            return
        # Podświetl sąsiadów i krawędzie
        highlight_color = 'blue'
        for nidx, nsc in node_scatters.items():
            nsc.set_facecolor(nsc.get_facecolor())  # reset
        for line in edge_lines.values():
            line.set_color(line.get_color())  # reset
        # Podświetl wybrany punkt
        node_scatters[idx].set_facecolor(highlight_color)
        # Podświetl sąsiadów i krawędzie
        for neighbor in neighbors[idx]:
            node_scatters[neighbor].set_facecolor(highlight_color)
            if (idx, neighbor) in edge_lines:
                edge_lines[(idx, neighbor)].set_color(highlight_color)
        plt.draw()

    def reset(event):
        # Przywróć kolory po opuszczeniu figury
        for idx, (x, y, stype) in nodes.items():
            my = -y
            if 'metro' in stype:
                node_scatters[idx].set_facecolor(color_map['metro'])
            elif 'bus' in stype:
                node_scatters[idx].set_facecolor(color_map['bus'])
            elif 'taxi' in stype:
                node_scatters[idx].set_facecolor(color_map['taxi'])
            else:
                node_scatters[idx].set_facecolor('gray')
        for src, dst, typ, points in edges:
            color = color_map.get(typ, 'black')
            edge_lines[(src, dst)].set_color(color)
        plt.draw()

    plt.gcf().canvas.mpl_connect('pick_event', highlight)
    plt.gcf().canvas.mpl_connect('figure_leave_event', reset)
    plt.show()

def main():
    nodes_file = 'nodes_with_station.csv'
    edges_file = 'edges_geometry.csv'
    # game_connections_file = 'game_connections.csv'  # jeśli potrzebujesz
    if not os.path.exists(nodes_file) or not os.path.exists(edges_file):
        print('Brak wymaganych plików CSV w folderze!')
        return
    nodes = read_nodes(nodes_file)
    edges = read_edges(edges_file)
    plot_map(nodes, edges)

if __name__ == '__main__':
    main()
