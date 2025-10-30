#include "MapGeneratorState.h"
#include "Application.h"
#include "StateManager.h"
#include "HUDOverlay.h"

namespace ScotlandYard {
    namespace States {

        void MapGeneratorState::Render(Core::Application* p_App) {
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            int W = p_App->GetWidth();
            int H = p_App->GetHeight();

            // background
            ScotlandYard::UI::DrawRoundedRectScreen(0, 0, (float)W, (float)H, { 0.06f,0.07f,0.10f,1.0f }, 0, p_App);

            // title
            ScotlandYard::UI::DrawTextCenteredPx("MAP GENERATOR", 0, H * 0.60f, W, H * 0.70f, { 1,1,1,1 }, p_App, -8.0f);

            // Back button
            float bw = 240.0f, bh = 48.0f;
            float bx0 = (W - bw) * 0.5f, by0 = H * 0.25f;
            float bx1 = bx0 + bw, by1 = by0 + bh;
            ScotlandYard::UI::DrawRoundedRectScreen(bx0, by0, bx1, by1, { 0.0f,0.6f,0.2f,1.0f }, 10, p_App);
            ScotlandYard::UI::DrawTextCenteredPx("BACK", bx0, by0, bx1, by1, { 1,1,1,1 }, p_App, -4.0f);

            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void MapGeneratorState::HandleEvent(const SDL_Event& event, Core::Application* p_App) {
            if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    p_App->GetStateManager()->ChangeState("menu");
                }
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int W = p_App->GetWidth();
                int H = p_App->GetHeight();
                float bw = 240.0f, bh = 48.0f;
                float bx0 = (W - bw) * 0.5f, by0 = H * 0.25f;
                float bx1 = bx0 + bw, by1 = by0 + bh;

                float mx = (float)event.button.x;
                float my = (float)event.button.y;
                my = (float)H - my;

                if (mx >= bx0 && mx <= bx1 && my >= by0 && my <= by1) {
                    p_App->GetStateManager()->ChangeState("menu");
                }
            }
        }

    }
}
