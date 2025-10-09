#include "MenuState.h"
#include "Application.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

namespace ScotlandYard {
namespace States {

MenuState::MenuState()
    : m_i_SelectedOption(0)
    , m_p_TitleTexture(nullptr)
    , m_i_TitleWidth(0)
    , m_i_TitleHeight(0)
    , m_b_TexturesCreated(false)
{
    m_Buttons[0] = {0, 0, 200, 60, "Rozpocznij", nullptr, 0, 0};
    m_Buttons[1] = {0, 0, 200, 60, "Ustawienia", nullptr, 0, 0};
    m_Buttons[2] = {0, 0, 200, 60, "Wyjdz", nullptr, 0, 0};
}

MenuState::~MenuState() {
    DestroyTextures();
}

void MenuState::OnEnter() {
    m_i_SelectedOption = 0;
}

void MenuState::OnExit() {
    DestroyTextures();
}

void MenuState::OnPause() {
}

void MenuState::OnResume() {
}

void MenuState::Update(float f_DeltaTime) {
}

void MenuState::Render(Core::Application* p_App) {
    SDL_Renderer* p_Renderer = p_App->GetRenderer();
    if (!p_Renderer) return;
    
    if (!m_b_TexturesCreated) {
        CreateTextures(p_App);
    }
    
    SDL_SetRenderDrawColor(p_Renderer, 255, 255, 255, 255);
    SDL_RenderClear(p_Renderer);
    
    InitializeButtons(p_App);
    
    if (m_p_TitleTexture) {
        SDL_Rect titleRect = {
            p_App->GetWidth() / 2 - m_i_TitleWidth / 2,
            50,
            m_i_TitleWidth,
            m_i_TitleHeight
        };
        SDL_RenderCopy(p_Renderer, m_p_TitleTexture, nullptr, &titleRect);
    }
    
    for (int i = 0; i < 3; i++) {
        RenderButton(p_App, m_Buttons[i], i == m_i_SelectedOption);
    }
    
    SDL_RenderPresent(p_Renderer);
}

void MenuState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
    if (event.type == SDL_KEYDOWN) {
        switch (event.key.keysym.sym) {
            case SDLK_UP:
                m_i_SelectedOption = (m_i_SelectedOption - 1 + 3) % 3;
                break;
            case SDLK_DOWN:
                m_i_SelectedOption = (m_i_SelectedOption + 1) % 3;
                break;
            case SDLK_RETURN:
                switch (m_i_SelectedOption) {
                    case 0:
                        break;
                    case 1:
                        break;
                    case 2: 
                        p_App->RequestExit();
                        break;
                }
                break;
            case SDLK_ESCAPE:
                p_App->RequestExit();
                break;
        }
    }
    else if (event.type == SDL_MOUSEBUTTONDOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            int i_MouseX = event.button.x;
            int i_MouseY = event.button.y;
            
            for (int i = 0; i < 3; i++) {
                if (i_MouseX >= m_Buttons[i].i_X && i_MouseX <= m_Buttons[i].i_X + m_Buttons[i].i_Width &&
                    i_MouseY >= m_Buttons[i].i_Y && i_MouseY <= m_Buttons[i].i_Y + m_Buttons[i].i_Height) {
                    m_i_SelectedOption = i;
                    switch (i) {
                        case 0: 
                            break;
                        case 1: 
                            break;
                        case 2: 
                            p_App->RequestExit();
                            break;
                    }
                    break;
                }
            }
        }
    }
    else if (event.type == SDL_MOUSEMOTION) {
        int i_MouseX = event.motion.x;
        int i_MouseY = event.motion.y;
        
        for (int i = 0; i < 3; i++) {
            if (i_MouseX >= m_Buttons[i].i_X && i_MouseX <= m_Buttons[i].i_X + m_Buttons[i].i_Width &&
                i_MouseY >= m_Buttons[i].i_Y && i_MouseY <= m_Buttons[i].i_Y + m_Buttons[i].i_Height) {
                m_i_SelectedOption = i;
                break;
            }
        }
    }
}

void MenuState::InitializeButtons(Core::Application* p_App) {
    int i_WindowWidth = p_App->GetWidth();
    int i_WindowHeight = p_App->GetHeight();
    
    int i_ButtonSpacing = 80;
    int i_StartY = i_WindowHeight / 2 - (3 * m_Buttons[0].i_Height + 2 * i_ButtonSpacing) / 2;
    
    for (int i = 0; i < 3; i++) {
        m_Buttons[i].i_X = i_WindowWidth / 2 - m_Buttons[i].i_Width / 2;
        m_Buttons[i].i_Y = i_StartY + i * (m_Buttons[i].i_Height + i_ButtonSpacing);
    }
}

void MenuState::CreateTextures(Core::Application* p_App) {
    SDL_Renderer* p_Renderer = p_App->GetRenderer();
    TTF_Font* p_Font = p_App->GetFont();
    
    if (!p_Renderer || !p_Font) return;
    
    SDL_Color black = {0, 0, 0, 255};
    
    SDL_Surface* p_TitleSurface = TTF_RenderText_Solid(p_Font, "MENU", black);
    if (p_TitleSurface) {
        m_p_TitleTexture = SDL_CreateTextureFromSurface(p_Renderer, p_TitleSurface);
        m_i_TitleWidth = p_TitleSurface->w;
        m_i_TitleHeight = p_TitleSurface->h;
        SDL_FreeSurface(p_TitleSurface);
    }
    
    for (int i = 0; i < 3; i++) {
        SDL_Surface* p_TextSurface = TTF_RenderText_Solid(p_Font, m_Buttons[i].p_Text, black);
        if (p_TextSurface) {
            m_Buttons[i].p_Texture = SDL_CreateTextureFromSurface(p_Renderer, p_TextSurface);
            m_Buttons[i].i_TextWidth = p_TextSurface->w;
            m_Buttons[i].i_TextHeight = p_TextSurface->h;
            SDL_FreeSurface(p_TextSurface);
        }
    }
    
    m_b_TexturesCreated = true;
}

void MenuState::DestroyTextures() {
    if (m_p_TitleTexture) {
        SDL_DestroyTexture(m_p_TitleTexture);
        m_p_TitleTexture = nullptr;
    }
    
    for (int i = 0; i < 3; i++) {
        if (m_Buttons[i].p_Texture) {
            SDL_DestroyTexture(m_Buttons[i].p_Texture);
            m_Buttons[i].p_Texture = nullptr;
        }
    }
    
    m_b_TexturesCreated = false;
}

void MenuState::RenderButton(Core::Application* p_App, const Button& button, bool b_Selected) {
    SDL_Renderer* p_Renderer = p_App->GetRenderer();
    if (!p_Renderer) return;
    
    if (b_Selected) {
        SDL_SetRenderDrawColor(p_Renderer, 200, 200, 200, 255); 
    } else {
        SDL_SetRenderDrawColor(p_Renderer, 240, 240, 240, 255);
    }
    
    SDL_Rect buttonRect = {button.i_X, button.i_Y, button.i_Width, button.i_Height};
    SDL_RenderFillRect(p_Renderer, &buttonRect);
    
    SDL_SetRenderDrawColor(p_Renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(p_Renderer, &buttonRect);
    
    if (button.p_Texture) {
        SDL_Rect textRect = {
            button.i_X + button.i_Width / 2 - button.i_TextWidth / 2,
            button.i_Y + button.i_Height / 2 - button.i_TextHeight / 2,
            button.i_TextWidth,
            button.i_TextHeight
        };
        SDL_RenderCopy(p_Renderer, button.p_Texture, nullptr, &textRect);
    }
}

} // namespace States
} // namespace ScotlandYard
