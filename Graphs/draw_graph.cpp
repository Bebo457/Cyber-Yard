#include <windows.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_main.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include "graph_manage.h"  // zakładamy, że zawiera klasę Node i GraphManager

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;
const int NODE_RADIUS = 6;

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

// Główna funkcja aplikacji
int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        MessageBoxA(NULL, SDL_GetError(), "SDL Init Error", MB_ICONERROR | MB_OK);
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow("Graph Viewer",
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

    GraphManager graph(200);
    graph.LoadData(pos_file, con_file);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
        }

        SDL_SetRenderDrawColor(renderer, 30, 30, 30, 255); // tło
        SDL_RenderClear(renderer);

        // Draw cons
        for (int i = 1; i <= graph.GetNodeCount(); ++i) {
            Node* node = graph.GetNode(i);
            if (!node) continue;

            int x1 = node->x * 5 + 50; 
            int y1 = node->y * 5 + 50;

            //TODO rescaling to fot the window size - see xRange, yRange

            int connCount = node->connectionCount();
            for (int j = 0; j < connCount; ++j) {
                Node* neighbor = node->otherNode(j);
                int type = 1;
                //TODO getting info about type of connection and changing the color of line

                if (neighbor && node->id < neighbor->id) { // rysuj tylko raz dla każdej pary
                    int x2 = neighbor->x * 20 + 20;
                    int y2 = neighbor->y * 20 + 20;

                    SDL_Color color = getColorForType(type);
                    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
                    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
                }
            }
        }

        
        SDL_SetRenderDrawColor(renderer, 255, 120, 120, 255);
        drawCircle(renderer, 800, 600, NODE_RADIUS);

        int xRange = 14; //graph.getBoundsX(graph.GetNodeCount());
        int yRange = 14; //graph.getBoundsY(graph.GetNodeCount());

        for (int i = 1; i <= graph.GetNodeCount(); ++i) {
            Node* node = graph.GetNode(i);
            if (!node) continue;

            int cx = node->x*20 + 20;
            int cy = node->y*20 + 20; //node->y

            // int cx = node->x/xRange*WINDOW_WIDTH;
            // int cy = node->y/yRange*WINDOW_HEIGHT;

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // biały
            drawCircle(renderer, cx, cy, NODE_RADIUS);
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

// WinMain — punkt wejścia dla aplikacji GUI
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) {
    return SDL_main(__argc, __argv);  // SDL redefiniuje main() jako SDL_main
}
