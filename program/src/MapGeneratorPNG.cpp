#include "MapGenerator.h"
#include <SDL2/SDL.h>
#include <vector>
#include <cstring>

namespace ScotlandYard {
namespace MapGen {

bool ExportMapToPNG(const std::string& s_Filename, 
                    int i_Width, int i_Height,
                    const std::vector<Point>& vec_GridPoints,
                    const std::vector<Point>& vec_RiverPath,
                    const std::vector<Park>& vec_Parks,
                    const std::vector<GraphNode>* p_Nodes) {
    
    // Create surface
    SDL_Surface* surface = SDL_CreateRGBSurface(0, i_Width, i_Height, 32,
                                                 0xFF000000, 0x00FF0000, 
                                                 0x0000FF00, 0x000000FF);
    if (!surface) return false;
    
    SDL_LockSurface(surface);
    Uint32* pixels = (Uint32*)surface->pixels;
    
    // Fill background (black)
    memset(pixels, 0, i_Width * i_Height * sizeof(Uint32));
    
    // Draw connections if provided
    if (p_Nodes) {
        // Taxi = yellow, Bus = red, Metro = blue, Ferry = cyan
        Uint32 taxiColor = SDL_MapRGB(surface->format, 255, 255, 100);
        Uint32 busColor = SDL_MapRGB(surface->format, 255, 100, 100);
        Uint32 metroColor = SDL_MapRGB(surface->format, 100, 100, 255);
        Uint32 ferryColor = SDL_MapRGB(surface->format, 100, 255, 255);
        
        auto drawConnection = [&](int x0, int y0, int x1, int y1, Uint32 color) {
            // Bresenham's line algorithm
            int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
            int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
            int err = dx + dy, e2;
            
            while (true) {
                if (x0 >= 0 && x0 < i_Width && y0 >= 0 && y0 < i_Height) {
                    pixels[y0 * i_Width + x0] = color;
                }
                if (x0 == x1 && y0 == y1) break;
                e2 = 2 * err;
                if (e2 >= dy) { err += dy; x0 += sx; }
                if (e2 <= dx) { err += dx; y0 += sy; }
            }
        };
        
        for (const auto& node : *p_Nodes) {
            int x0 = (int)node.position.x;
            int y0 = (int)node.position.y;
            
            // Draw taxi connections
            for (int targetID : node.set_TaxiConnections) {
                if (targetID <= node.i_ID) continue;
                const auto& target = (*p_Nodes)[targetID - 1];
                drawConnection(x0, y0, (int)target.position.x, (int)target.position.y, taxiColor);
            }
            
            // Draw bus connections
            for (int targetID : node.set_BusConnections) {
                if (targetID <= node.i_ID) continue;
                const auto& target = (*p_Nodes)[targetID - 1];
                drawConnection(x0, y0, (int)target.position.x, (int)target.position.y, busColor);
            }
            
            // Draw metro connections
            for (int targetID : node.set_MetroConnections) {
                if (targetID <= node.i_ID) continue;
                const auto& target = (*p_Nodes)[targetID - 1];
                drawConnection(x0, y0, (int)target.position.x, (int)target.position.y, metroColor);
            }
            
            // Draw ferry connections
            for (int targetID : node.set_FerryConnections) {
                if (targetID <= node.i_ID) continue;
                const auto& target = (*p_Nodes)[targetID - 1];
                drawConnection(x0, y0, (int)target.position.x, (int)target.position.y, ferryColor);
            }
        }
    }
    
    // Draw parks (green)
    Uint32 parkColor = SDL_MapRGB(surface->format, 34, 139, 34);
    for (const auto& park : vec_Parks) {
        int minX = std::max(0, (int)(park.center.x - park.f_BaseRadius * 1.5f));
        int maxX = std::min(i_Width - 1, (int)(park.center.x + park.f_BaseRadius * 1.5f));
        int minY = std::max(0, (int)(park.center.y - park.f_BaseRadius * 1.5f));
        int maxY = std::min(i_Height - 1, (int)(park.center.y + park.f_BaseRadius * 1.5f));
        
        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                if (park.ContainsPoint(x, y)) {
                    pixels[y * i_Width + x] = parkColor;
                }
            }
        }
    }
    
    // Draw river (blue)
    Uint32 riverColor = SDL_MapRGB(surface->format, 50, 150, 255);
    for (size_t i = 0; i < vec_RiverPath.size(); ++i) {
        for (int offset = -Config::RIVER_WIDTH/2; offset <= Config::RIVER_WIDTH/2; ++offset) {
            int x = (int)vec_RiverPath[i].x + offset;
            int y = (int)vec_RiverPath[i].y;
            if (x >= 0 && x < i_Width && y >= 0 && y < i_Height) {
                pixels[y * i_Width + x] = riverColor;
            }
        }
    }
    
    // Draw grid points (white) - make them bigger
    Uint32 gridColor = SDL_MapRGB(surface->format, 255, 255, 255);
    for (const auto& p : vec_GridPoints) {
        for (int dy = -4; dy <= 4; ++dy) {
            for (int dx = -4; dx <= 4; ++dx) {
                if (dx*dx + dy*dy <= 16) {
                    int x = (int)p.x + dx;
                    int y = (int)p.y + dy;
                    if (x >= 0 && x < i_Width && y >= 0 && y < i_Height) {
                        pixels[y * i_Width + x] = gridColor;
                    }
                }
            }
        }
    }
    
    SDL_UnlockSurface(surface);
    
    // Save to BMP (SDL native) or use SDL_image for PNG
    int result = SDL_SaveBMP(surface, s_Filename.c_str());
    SDL_FreeSurface(surface);
    
    return result == 0;
}

} // namespace MapGen
} // namespace ScotlandYard
