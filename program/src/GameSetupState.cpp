#include "GameSetupState.h"
#include "Application.h"
#include "StateManager.h"
#include "GameSettings.h"
#include "HUDOverlay.h"

namespace ScotlandYard {
    namespace States {

        static inline float rowY(float base, float lineH, int rowIndex) {
            // drawing from down of the screen
            return base + rowIndex * lineH;
        }

        void GameSetupState::OnEnter() {
            // default setup
            m_i_Mode = 0;
            m_i_AIMrX = 0;
            m_i_AIDet = 0;
            m_vec_Buttons.clear();
        }

        void GameSetupState::Layout(Core::Application* app) {
            m_vec_Buttons.clear();

            const int   W = app->GetWidth();
            const int   H = app->GetHeight();

            // setup
            const int   cols = 3;
            const float gridW = std::min((float)W * 0.92f, 1120.0f);
            const float gridX = (W - gridW) * 0.5f;
            const float cellW = gridW / cols;
            const float cellGap = 14.0f;
            const float btnW = cellW - 2 * cellGap;
            const float btnH = 54.0f;

            // vertical gap
            const float lineH = btnH + 45.0f;

            const bool  hasHumanRow = (m_i_Mode == 1); // 1 = PvBot
            const int   rowsAboveFooter = hasHumanRow ? 4 : 3;
            const int   totalRows = rowsAboveFooter + 1; // +Footer
            const float blockH = totalRows * lineH + hasHumanRow;

            const float topY = (H + blockH) * 0.5f;

            auto rowYTopDown = [&](int idxFromTop) {
                float y = topY - (idxFromTop + 1) * lineH + (lineH - btnH);
                return y;
                };

            auto placeRow3 = [&](Row row, int idxFromTop,
                const char* t0, const char* t1, const char* t2) {
                    float y = rowYTopDown(idxFromTop);
                    for (int c = 0; c < 3; ++c) {
                        float x = gridX + c * cellW + cellGap;
                        const char* text = (c == 0 ? t0 : (c == 1 ? t1 : t2));
                        m_vec_Buttons.push_back({ row, c, x, y, btnW, btnH, text });
                    }
                };

            // ROW LAYOUT
            // 0: MODE
            placeRow3(Row::Mode, 0, "Player vs Player", "Player vs Bot", "Bot vs Bot");

            int idx = 1;

            // 1: HUMAN (PvBot)
            if (m_i_Mode == 1) {
                const float y = rowYTopDown(idx);
                const float gap = 2.0f * cellGap;
                const float groupW = 2.0f * btnW + gap;
                const float center = W * 0.5f;
                const float xL = center - groupW * 0.5f;
                const float xR = xL + btnW + gap;

                // buttons to choose a player
                m_vec_Buttons.push_back({ Row::Human, 0, xL, y, btnW, btnH, "Mr X" });
                m_vec_Buttons.push_back({ Row::Human, 1, xR, y, btnW, btnH, "Detectives" });


                idx += 1;
            }
            // Mr X AI
            placeRow3(Row::MrX, idx++, "Mr X AI: Random", "Mr X AI: Greedy", "Mr X AI: Neural");
            // Detectives AI
            placeRow3(Row::Detectives, idx++, "Detectives AI: Random", "Detectives AI: Greedy", "Detectives AI: Neural");

            // Footer
            const float yF = rowYTopDown(idx);
            const float fBtnW = 250.0f;
            const float fGap = 60.0f;
            const float totalFW = fBtnW * 2 + fGap;
            const float fx0 = (W - totalFW) * 0.5f;

            m_vec_Buttons.push_back({ Row::Footer, 0, fx0,                  yF, fBtnW, btnH, "Back to Menu" });
            m_vec_Buttons.push_back({ Row::Footer, 1, fx0 + fBtnW + fGap,   yF, fBtnW, btnH, "Start Game" });
        }


        bool GameSetupState::IsRowDisabled(Row row) const {
            if (row == Row::Footer || row == Row::Mode || row == Row::Human) return false;

            if (m_i_Mode == 0) { // PvP
                return (row == Row::MrX || row == Row::Detectives);
            }
            if (m_i_Mode == 1) { // PvBot
                // off human side
                return (m_i_Human == 0) ? (row == Row::MrX) : (row == Row::Detectives);
            }
            return false; // BotvBot
        }


        void GameSetupState::DrawButton(const Button& b, bool selected, Core::Application* app) {
            const bool disabled = IsRowDisabled(b.e_Row);
            const bool hovered = (&b - m_vec_Buttons.data()) == m_i_Hover && !disabled;

            SDL_Rect r{ (int)std::lround(b.f_X), (int)std::lround(b.f_Y),
                         (int)std::lround(b.f_W), (int)std::lround(b.f_H) };

            if (disabled) {
                ScotlandYard::UI::Color bg{ 0.30f,0.32f,0.34f,0.85f };
                ScotlandYard::UI::Color tx{ 0.75f,0.75f,0.75f,0.9f };
                ScotlandYard::UI::DrawRoundedRectScreen((float)r.x, (float)r.y, (float)(r.x + r.w), (float)(r.y + r.h), bg, 12.f, app);
                ScotlandYard::UI::DrawTextCenteredPx(b.p_Text, (float)r.x, (float)r.y, (float)(r.x + r.w), (float)(r.y + r.h), tx, app, -4.f);
                return;
            }

            ScotlandYard::UI::DrawMenuLikeButton(r, b.p_Text, app, hovered);

            if (selected && b.e_Row != Row::Footer) {
                ScotlandYard::UI::Color accent{ 1.f, 0.84f, 0.0f, 1.f };
                ScotlandYard::UI::DrawRoundedRectScreen((float)r.x, (float)(r.y + r.h - 4),
                    (float)(r.x + r.w), (float)(r.y + r.h),
                    accent, 12.f, app);
            }
        }


        void GameSetupState::HandleEvent(const SDL_Event& e, Core::Application* app) {
            if (e.type == SDL_KEYDOWN) {
                if (e.key.keysym.sym == SDLK_ESCAPE) app->GetStateManager()->ChangeState("menu");
                else if (e.key.keysym.sym == SDLK_RETURN) StartGame(app);
                return;
            }

            // HOVER
            if (e.type == SDL_MOUSEMOTION) {
                float mx = (float)e.motion.x;
                float my = (float)e.motion.y;
                float myBL = (float)app->GetHeight() - my;

                m_i_Hover = -1;
                for (int i = 0; i < (int)m_vec_Buttons.size(); ++i) {
                    const auto& b = m_vec_Buttons[i];
                    if (mx >= b.f_X && mx <= b.f_X + b.f_W && myBL >= b.f_Y && myBL <= b.f_Y + b.f_H) {
                        if (!IsRowDisabled(b.e_Row)) m_i_Hover = i;
                        break;
                    }
                }
                return;
            }

            // CLICK
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                float mx = (float)e.button.x;
                float my = (float)e.button.y;
                float myBL = (float)app->GetHeight() - my;

                for (const auto& b : m_vec_Buttons) {
                    if (mx >= b.f_X && mx <= b.f_X + b.f_W && myBL >= b.f_Y && myBL <= b.f_Y + b.f_H) {
                        if (IsRowDisabled(b.e_Row)) return; // ignored

                        if (b.e_Row == Row::Mode) { m_i_Mode = b.i_Col; return; }
                        if (b.e_Row == Row::Human) { m_i_Human = b.i_Col; return; }
                        if (b.e_Row == Row::MrX) { m_i_AIMrX = b.i_Col; return; }
                        if (b.e_Row == Row::Detectives) { m_i_AIDet = b.i_Col; return; }
                        if (b.e_Row == Row::Footer) {
                            if (b.i_Col == 0) app->GetStateManager()->ChangeState("menu");
                            else            StartGame(app);
                            return;
                        }
                    }
                }
            }


        }


        void GameSetupState::Render(Core::Application* app) {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Layout(app);

            // TITLE
            ScotlandYard::UI::DrawTextCenteredPx("Setup Your Game",
                20, app->GetHeight() - 120, app->GetWidth() - 20, app->GetHeight() - 60, { 1,1,1,1 }, app, 0.f);

            // buttons drawing with selected option
            for (const auto& b : m_vec_Buttons) {
                bool selected = false;
                if (b.e_Row == Row::Mode)       selected = (b.i_Col == m_i_Mode);
                if (b.e_Row == Row::MrX)        selected = (b.i_Col == m_i_AIMrX);
                if (b.e_Row == Row::Detectives) selected = (b.i_Col == m_i_AIDet);
                if (b.e_Row == Row::Human)      selected = (b.i_Col == m_i_Human);
                DrawButton(b, selected, app);
            }

            // tooltip for PvP
            if (m_i_Mode == 0) {
                float mrxLower = 1e9f;
                float detUpper = -1e9f;

                // to have it centred in 2 rows
                for (const auto& b : m_vec_Buttons) {
                    if (b.e_Row == Row::MrX) {
                        mrxLower = std::min(mrxLower, b.f_Y);
                    }
                    else if (b.e_Row == Row::Detectives) {
                        detUpper = std::max(detUpper, b.f_Y + b.f_H);
                    }
                }

                // gap checking
                if (detUpper < mrxLower) {
                    const float padY = 8.0f;
                    const float minH = 34.0f; // minimal height
                    float y0 = detUpper + padY;
                    float y1 = mrxLower - padY;
                    if (y1 - y0 < minH) y1 = y0 + minH;

                    const float x0 = 40.0f;
                    const float x1 = (float)app->GetWidth() - 40.0f;

                    // background of tooltip
                    ScotlandYard::UI::Color plate{ 0.f, 0.f, 0.f, 0.35f };
                    ScotlandYard::UI::DrawRoundedRectScreen(x0, y0, x1, y1, plate, 10.f, app);

                    ScotlandYard::UI::Color info{ 0.92f, 0.92f, 0.92f, 1.f };
                    ScotlandYard::UI::DrawTextCenteredPx(
                        "AI settings are disabled in Player vs Player mode",
                        x0, y0, x1, y1, info, app, -3.0f
                    );
                }
            }
            // tooltip for PvBot
            if (m_i_Mode == 1) {
                // if human is Mr X -> Mr X AI should be off (the same logic with Detectives)
                const Row disabledRow = (m_i_Human == 0) ? Row::MrX : Row::Detectives;

                float humanTop = -1e9f;
                for (const auto& b : m_vec_Buttons) {
                    if (b.e_Row == Row::Human) humanTop = std::max(humanTop, b.f_Y + b.f_H);
                }
                if (humanTop > -1e8f) {
                    ScotlandYard::UI::DrawTextCenteredPx(
                        "Choose who you want to play:", 0, humanTop + 2.0f, (float)app->GetWidth(), humanTop + 35.0f,
                        { 1,1,1,0.85f }, app, -4.0f
                    );
                }

                float rowMinX = 1e9f, rowMaxX = -1e9f;
                float rowMinY = 1e9f, rowMaxY = -1e9f;
                for (const auto& b : m_vec_Buttons) {
                    if (b.e_Row == disabledRow) {
                        rowMinX = std::min(rowMinX, b.f_X);
                        rowMaxX = std::max(rowMaxX, b.f_X + b.f_W);
                        rowMinY = std::min(rowMinY, b.f_Y);
                        rowMaxY = std::max(rowMaxY, b.f_Y + b.f_H);
                    }
                }

                if (rowMaxX > rowMinX && rowMaxY > rowMinY) {
                    // centred tooltip
                    const float marginX = 40.0f;
                    float x0 = std::max(0.f, rowMinX - marginX);
                    float x1 = std::min((float)app->GetWidth(), rowMaxX + marginX);
                    const float midY = rowMinY + (rowMaxY - rowMinY) * 0.5f;
                    const float hBox = 34.0f;
                    float y0 = midY - hBox * 0.5f;
                    float y1 = midY + hBox * 0.5f;

                    ScotlandYard::UI::Color plate{ 0.f, 0.f, 0.f, 0.35f };
                    ScotlandYard::UI::DrawRoundedRectScreen(x0, y0, x1, y1, plate, 10.f, app);

                    const char* msg = (disabledRow == Row::MrX)
                        ? "Mr X is controlled by player"
                        : "Detectives are controlled by player";

                    ScotlandYard::UI::Color info{ 0.92f, 0.92f, 0.92f, 1.f };
                    ScotlandYard::UI::DrawTextCenteredPx(msg, x0, y0, x1, y1, info, app, -3.0f);
                }
            }




            SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
        }

        void GameSetupState::StartGame(Core::Application* app) {
            using namespace ScotlandYard::Core;

            auto& S = Settings();
            // mode mapping
            switch (m_i_Mode) {
            case 0: S.e_Mode = GameMode::PvP;     break;
            case 1: S.e_Mode = GameMode::PvBot;   break;
            case 2: S.e_Mode = GameMode::BotvBot; break;
            }

            auto mapAI = [](int idx) {
                switch (idx) {
                case 1: return AIAlgorithm::GreedyShortestPath;
                case 2: return AIAlgorithm::NeuralNet;
                default: return AIAlgorithm::Random;
                }
                };
            S.e_AIMisterX = mapAI(m_i_AIMrX);
            S.e_AIDetectives = mapAI(m_i_AIDet);
            S.e_PvBotHuman = (m_i_Human == 0 ? HumanSide::MrX : HumanSide::Detectives);

            app->GetStateManager()->ChangeState("game");
        }


    }
} // namespaces
