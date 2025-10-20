#include <SDL2/SDL.h>
#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include "graph_manage.h"
#include <map>

const int WINDOW_WIDTH = 1200;
const int WINDOW_HEIGHT = 900;
const int NODE_RADIUS = 5;
const int PADDING = 40;
const int CLICK_RADIUS = 10;

// Kolory dla typów transportu
SDL_Color getColorForType(int type) {
    switch (type) {
        case 1: return {255, 255, 0, 255};   // taxi - żółty
        case 2: return {0, 128, 0, 255};     // bus - zielony
        case 3: return {0, 0, 255, 255};     // metro - niebieski
        case 4: return {0, 255, 255, 255};   // water - cyjan
        default: return {128, 128, 128, 255}; // inne - szary
    }
}

void drawCircle(SDL_Renderer* renderer, int x, int y, int radius) {
    for (int w = 0; w < radius * 2; w++) {
        for (int h = 0; h < radius * 2; h++) {
            int dx = radius - w;
            int dy = radius - h;
            if ((dx * dx + dy * dy) <= (radius * radius)) {
                SDL_RenderDrawPoint(renderer, x + dx, y + dy);
            }
        }
    }
}

// Funkcja sprawdzająca czy punkt (px, py) znajduje się w odległości <= radius od (cx, cy)
bool isPointInCircle(int px, int py, int cx, int cy, int radius) {
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

void drawDigit(SDL_Renderer* renderer, int x, int y, int digit) {
    static const int patterns[10][5][3] = {
        {{1,1,1},{1,0,1},{1,0,1},{1,0,1},{1,1,1}}, // 0
        {{0,1,0},{1,1,0},{0,1,0},{0,1,0},{1,1,1}}, // 1
        {{1,1,1},{0,0,1},{1,1,1},{1,0,0},{1,1,1}}, // 2
        {{1,1,1},{0,0,1},{1,1,1},{0,0,1},{1,1,1}}, // 3
        {{1,0,1},{1,0,1},{1,1,1},{0,0,1},{0,0,1}}, // 4
        {{1,1,1},{1,0,0},{1,1,1},{0,0,1},{1,1,1}}, // 5
        {{1,1,1},{1,0,0},{1,1,1},{1,0,1},{1,1,1}}, // 6
        {{1,1,1},{0,0,1},{0,0,1},{0,0,1},{0,0,1}}, // 7
        {{1,1,1},{1,0,1},{1,1,1},{1,0,1},{1,1,1}}, // 8
        {{1,1,1},{1,0,1},{1,1,1},{0,0,1},{1,1,1}}  // 9
    };
    
    for (int row = 0; row < 5; row++) {
        for (int col = 0; col < 3; col++) {
            if (patterns[digit][row][col]) {
                SDL_RenderDrawPoint(renderer, x + col, y + row);
            }
        }
    }
}

void drawNumber(SDL_Renderer* renderer, int x, int y, int number) {
    if (number < 0) return;
    if (number < 10) {
        drawDigit(renderer, x, y, number);
    } else if (number < 100) {
        drawDigit(renderer, x, y, number / 10);
        drawDigit(renderer, x + 4, y, number % 10);
    } else if (number < 1000) {
        drawDigit(renderer, x, y, number / 100);
        drawDigit(renderer, x + 4, y, (number / 10) % 10);
        drawDigit(renderer, x + 8, y, number % 10);
    }
}

void drawLetter(SDL_Renderer* renderer, int x, int y, char letter) {
    static const std::vector<std::vector<int>> patterns = {
        {1,1,1,1,1, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0}, // T
        {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,1,1,1,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1}, // A
        {1,0,0,0,1, 1,0,0,0,1, 0,1,0,1,0, 0,0,1,0,0, 0,1,0,1,0, 1,0,0,0,1, 1,0,0,0,1}, // X
        {1,1,1,1,1, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 0,0,1,0,0, 1,1,1,1,1}, // I
        {1,1,1,1,0, 1,0,0,0,1, 1,1,1,1,0, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,1,1,1,0}, // B
        {1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // U
        {0,1,1,1,0, 1,0,0,0,1, 1,0,0,0,0, 0,1,1,1,0, 0,0,0,0,1, 1,0,0,0,1, 0,1,1,1,0}, // S
        {1,0,0,0,1, 1,1,0,1,1, 1,0,1,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1}, // M
        {1,1,1,1,1, 1,0,0,0,0, 1,1,1,1,0, 1,0,0,0,0, 1,0,0,0,0, 1,0,0,0,0, 1,1,1,1,1}, // E
        {1,1,1,1,0, 1,0,0,0,1, 1,1,1,1,0, 1,1,0,0,0, 1,0,1,0,0, 1,0,0,1,0, 1,0,0,0,1}, // R
        {1,0,0,0,1, 1,0,0,0,1, 1,0,0,0,1, 1,0,1,0,1, 1,0,1,0,1, 1,1,0,1,1, 1,0,0,0,1}, // W
    };
    
    int idx = -1;
    switch(letter) {
        case 'T': idx = 0; break;
        case 'A': idx = 1; break;
        case 'X': idx = 2; break;
        case 'I': idx = 3; break;
        case 'B': idx = 4; break;
        case 'U': idx = 5; break;
        case 'S': idx = 6; break;
        case 'M': idx = 7; break;
        case 'E': idx = 8; break;
        case 'R': idx = 9; break;
        case 'W': idx = 10; break;
    }
    
    if (idx < 0) return;
    const auto& pattern = patterns[idx];
    for (int row = 0; row < 7; row++) {
        for (int col = 0; col < 5; col++) {
            if (pattern[row * 5 + col]) {
                SDL_RenderDrawPoint(renderer, x + col, y + row);
            }
        }
    }
}

void drawText(SDL_Renderer* renderer, int x, int y, const char* text) {
    int offset = 0;
    while (*text) {
        drawLetter(renderer, x + offset, y, *text);
        offset += 6;
        text++;
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        MessageBoxA(NULL, SDL_GetError(), "SDL Init Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Scotland Yard - Graph Viewer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    
    if (!window) {
        MessageBoxA(NULL, SDL_GetError(), "Window Creation Error", MB_ICONERROR | MB_OK);
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    
    if (!renderer) {
        MessageBoxA(NULL, SDL_GetError(), "Renderer Creation Error", MB_ICONERROR | MB_OK);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::string pos_file = "nodes_with_station.csv";
    std::string con_file = "polaczenia.csv";
    GraphManager graph(199);
    graph.LoadData(pos_file, con_file);

    int maxX = graph.getBoundsX(graph.GetNodeCount());
    int maxY = graph.getBoundsY(graph.GetNodeCount());

    std::cout << "Grid size: " << maxX << " x " << maxY << std::endl;
    std::cout << "Controls:" << std::endl;
    std::cout << "  L - Toggle legend" << std::endl;
    std::cout << "  Left Click - Select/Deselect node" << std::endl;
    std::cout << "  ESC - Exit" << std::endl;

    bool running = true;
    bool showLegend = true;
    bool showAllConnections = false;
    int selectedNodeId = -1; // -1 oznacza brak zaznaczonego węzła
    SDL_Event event;

    std::map<int, SDL_Color> nodeColors;

    float scaleX = (WINDOW_WIDTH - 2 * PADDING) / static_cast<float>(maxX);
    float scaleY = (WINDOW_HEIGHT - 2 * PADDING) / static_cast<float>(maxY);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.keysym.sym == SDLK_l) {
                    showLegend = !showLegend;
                    std::cout << "Legend: " << (showLegend ? "ON" : "OFF") << std::endl;
                }
                if (event.key.keysym.sym == SDLK_t) {
                    showAllConnections = !showAllConnections;
                    std::cout << "All connections: " << (showAllConnections ? "ON" : "OFF") << std::endl;
                }
            }
            
            // Obsługa kliknięcia myszy
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;
                
                // Sprawdź czy kliknięto na jakiś węzeł
                bool nodeClicked = false;
                for (int i = 1; i <= graph.GetNodeCount(); ++i) {
                    if (i == 200) continue;
                    Node* node = graph.GetNode(i);
                    if (!node) continue;
                    
                    int cx = static_cast<int>(node->x * scaleX) + PADDING;
                    int cy = static_cast<int>(node->y * scaleY) + PADDING;
                    
                    if (isPointInCircle(mouseX, mouseY, cx, cy, CLICK_RADIUS)) {
                        // Kliknięto na węzeł
                        if (selectedNodeId == i) {
                            // Odznacz jeśli już zaznaczony
                            selectedNodeId = -1;
                            std::cout << "Node " << i << " deselected" << std::endl;
                        } else {
                            // Zaznacz nowy węzeł
                            selectedNodeId = i;
                            std::cout << "Node " << i << " selected" << std::endl;
                        }
                        nodeClicked = true;
                        break;
                    }
                }
                
                // Jeśli kliknięto poza węzłami, odznacz wszystko
                if (!nodeClicked && selectedNodeId != -1) {
                    std::cout << "Node " << selectedNodeId << " deselected" << std::endl;
                    selectedNodeId = -1;
                }
            }
        }

        // Rysowanie
        SDL_SetRenderDrawColor(renderer, 20, 20, 30, 255);
        SDL_RenderClear(renderer);

        // Rysuj połączenia
        if (showAllConnections) {
            // Rysuj wszystkie połączenia
            for (int i = 1; i <= graph.GetNodeCount(); ++i) {
                if (i == 200) continue;
                Node* node = graph.GetNode(i);
                if (!node) continue;
                
                int x1 = static_cast<int>(node->x * scaleX) + PADDING;
                int y1 = static_cast<int>(node->y * scaleY) + PADDING;
                
                int connCount = node->connectionCount();
                for (int j = 0; j < connCount; ++j) {
                    Node* neighbor = node->otherNode(j);
                    Edge* edge = node->getEdge(j);
                    
                    if (neighbor && edge && neighbor->id > i) { // Rysuj tylko raz każdą krawędź
                        int x2 = static_cast<int>(neighbor->x * scaleX) + PADDING;
                        int y2 = static_cast<int>(neighbor->y * scaleY) + PADDING;
                        
                        SDL_Color color = getColorForType(edge->type);
                        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                        SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                    }
                }
            }
        } else if (selectedNodeId != -1) {
    // Wyczyść mapę kolorów
    nodeColors.clear();
    
    // Rysuj połączenia tylko dla zaznaczonego węzła
    Node* selectedNode = graph.GetNode(selectedNodeId);
    if (selectedNode) {
        int x1 = static_cast<int>(selectedNode->x * scaleX) + PADDING;
        int y1 = static_cast<int>(selectedNode->y * scaleY) + PADDING;
        
        int connCount = selectedNode->connectionCount();
        for (int j = 0; j < connCount; ++j) {
            Node* neighbor = selectedNode->otherNode(j);
            Edge* edge = selectedNode->getEdge(j);
            
            if (neighbor && edge) {
                int x2 = static_cast<int>(neighbor->x * scaleX) + PADDING;
                int y2 = static_cast<int>(neighbor->y * scaleY) + PADDING;
                
                SDL_Color color = getColorForType(edge->type);
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                
                // NOWY KOD - zapisz kolor dla połączonego węzła
                nodeColors[neighbor->id] = color;
            }
        }
    }
}


        // Rysuj węzły
        for (int i = 1; i <= graph.GetNodeCount(); ++i) {
            if (i == 200) continue;
            Node* node = graph.GetNode(i);
            if (!node) continue;
            
            int cx = static_cast<int>(node->x * scaleX) + PADDING;
            int cy = static_cast<int>(node->y * scaleY) + PADDING;
            
            //kolorowanie węzłów
            if (i == selectedNodeId) {
                SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255); // Czerwony dla zaznaczonego
            } else if (nodeColors.find(i) != nodeColors.end()) {
                // Węzeł połączony - użyj koloru połączenia
                SDL_Color color = nodeColors[i];
                SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            } else {
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // Biały dla pozostałych
            }
            drawCircle(renderer, cx, cy, NODE_RADIUS);
            
            // Narysuj numer węzła
            SDL_SetRenderDrawColor(renderer, 255, 255, 100, 255);
            int textX = cx;
            int textY = cy + NODE_RADIUS + 2;
            
            if (i >= 100) textX -= 6;
            else if (i >= 10) textX -= 2;
            else textX -= 1;
            
            drawNumber(renderer, textX, textY, i);
        }


        // Legenda
        if (showLegend) {
            int legendX = 10;
            int legendY = WINDOW_HEIGHT - 85;
            
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_RenderDrawLine(renderer, legendX, legendY, legendX + 30, legendY);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            drawText(renderer, legendX + 35, legendY - 3, "TAXI");
            
            legendY += 20;
            SDL_SetRenderDrawColor(renderer, 0, 128, 0, 255);
            SDL_RenderDrawLine(renderer, legendX, legendY, legendX + 30, legendY);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            drawText(renderer, legendX + 35, legendY - 3, "BUS");
            
            legendY += 20;
            SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
            SDL_RenderDrawLine(renderer, legendX, legendY, legendX + 30, legendY);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            drawText(renderer, legendX + 35, legendY - 3, "METRO");
            
            legendY += 20;
            SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
            SDL_RenderDrawLine(renderer, legendX, legendY, legendX + 30, legendY);
            SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
            drawText(renderer, legendX + 35, legendY - 3, "WATER");
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    return SDL_main(__argc, __argv);
}
