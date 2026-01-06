#include <SDL2/SDL.h>
#include <iostream>
#include "MapGenerator.h"

void DrawMap(SDL_Renderer* renderer, const CityGen::MapGenerator& generator) {
    int width = generator.GetWidth();
    int height = generator.GetHeight();
    
    // Rysuj mapę gęstości (szarości)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float density = generator.GetDensityAt(x, y);
            Uint8 gray = static_cast<Uint8>((1.0f - density) * 255);
            
            SDL_SetRenderDrawColor(renderer, gray, gray, gray, 255);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }
    
    // Pobierz węzły, drogi i highways
    const auto& nodes = generator.GetRoadNodes();
    const auto& roads = generator.GetRoads();        // ZMIENIONE: pobierz Roads
    const auto& highways = generator.GetHighways();  // To są Highways
    
    // Paleta 10 kolorów dla highways
    SDL_Color highwayColors[10] = {
        {255, 0, 0, 255},      // Czerwony
        {0, 255, 0, 255},      // Zielony
        {0, 0, 255, 255},      // Niebieski
        {255, 255, 0, 255},    // Żółty
        {255, 0, 255, 255},    // Magenta
        {0, 255, 255, 255},    // Cyan
        {255, 128, 0, 255},    // Pomarańczowy
        {128, 0, 255, 255},    // Fioletowy
        {255, 192, 203, 255},  // Różowy
        {0, 128, 128, 255}     // Ciemny cyan
    };

    // Rysuj każdy highway innym kolorem
    for (size_t hwIdx = 0; hwIdx < highways.size(); ++hwIdx) {
        const auto& highway = highways[hwIdx];
        
        // Wybierz kolor (zapętlaj po 10 kolorach)
        SDL_Color color = highwayColors[hwIdx % 10];
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        
        // Rysuj wszystkie roads tego highway
        for (int roadIdx : highway.roadIndices) {
            if (roadIdx < 0 || roadIdx >= (int)roads.size()) continue;  // ZMIENIONE: sprawdź roads
            
            const auto& road = roads[roadIdx];  // ZMIENIONE: pobierz z roads
            if (road.isDeleted) continue;
            
            const auto& start = nodes[road.startNodeIdx];
            const auto& end = nodes[road.endNodeIdx];
            
            SDL_RenderDrawLine(
                renderer,
                static_cast<int>(start.x),
                static_cast<int>(start.y),
                static_cast<int>(end.x),
                static_cast<int>(end.y)
            );
        }
    }
    
    // Rysuj skrzyżowania jako żółte kółka
    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
    for (const auto& node : nodes) {
        if (node.isIntersection) {
            // Rysuj wypełnione kółko (promień 4px)
            for (int dx = -4; dx <= 4; ++dx) {
                for (int dy = -4; dy <= 4; ++dy) {
                    if (dx*dx + dy*dy <= 16) { // r² = 16
                        SDL_RenderDrawPoint(renderer, 
                            static_cast<int>(node.x) + dx,
                            static_cast<int>(node.y) + dy);
                    }
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::cout << "=== City Map Generator ===" << std::endl;

    const int WINDOW_WIDTH = 800;
    const int WINDOW_HEIGHT = 600;
    
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cout << "ERROR: SDL_Init failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    std::cout << "SDL initialized OK" << std::endl;
    
    SDL_Window* window = SDL_CreateWindow(
        "City Map Generator - Highways",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cout << "ERROR: Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }
    std::cout << "Window created OK" << std::endl;
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cout << "ERROR: Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    std::cout << "Renderer created OK" << std::endl;
    
    CityGen::MapGenerator generator(WINDOW_WIDTH, WINDOW_HEIGHT);
    std::cout << "Map generator created" << std::endl;

    generator.Generate();
    std::cout << "Initial map generated" << std::endl;
    
    std::cout << "Window is open! Press ESC to exit, SPACE to regenerate." << std::endl;
    
    bool running = true;
    SDL_Event event;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    std::cout << "\n=== Regenerating map ===" << std::endl;
                    generator.Generate();
                }
            }
        }
        
        // Białe tło
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);
        
        // Rysuj mapę gęstości i highways
        DrawMap(renderer, generator);
        
        SDL_RenderPresent(renderer);
        
        SDL_Delay(16);
    }
    
    std::cout << "\nClosing..." << std::endl;
    
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    std::cout << "Done!" << std::endl;
    return 0;
}