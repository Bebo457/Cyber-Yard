#include "GameSetupState.h"
#include "Application.h"
#include "StateManager.h"
#include "GameSettings.h"
#include "HUDOverlay.h"
#include <algorithm> // std::max

namespace ScotlandYard {
    namespace States {

        static inline float rowY(float base, float lineH, int rowIndex) {
            // drawing from down of the screen
            return base + rowIndex * lineH;
        }

        void GameSetupState::OnEnter(Core::Application* p_App) {
            m_i_Mode = 0;
            m_i_Map = 0;
            m_i_AIType = 0;
            m_i_Human = 0;
            m_i_AIMrX = 0;
            m_i_AIDet = 0;
            m_i_Hover = -1;

            m_Page = Page::Main;
        }

        void GameSetupState::Layout(Core::Application* app) {
            m_vec_Buttons.clear();

            const int W = app->GetVirtualWidth();
            const int H = app->GetVirtualHeight();


            const float defaultBtnW = 340.0f;
            const float btnH = 54.0f;
            const float rowGap = 45.0f;
            const float btnGap = 30.0f;

            // text scale
            const float f_SafetyMultiplier = 1.15f;
            const float f_TextScale = ((btnH * 0.60f) / 48.0f) * f_SafetyMultiplier;
            const auto& characters = app->GetCharacterMap();

            // width text 
            auto GetTextW = [&](const char* text) -> float {
                float w = 0.0f;
                std::string s(text);
                for (char c : s) {
                    auto it = characters.find(c);
                    if (it != characters.end()) {
                        w += (it->second.m_i_Advance >> 6) * f_TextScale;
                    }
                }
                return w;
                };

            int rows = 0;
            switch (m_Page) {
            case Page::Main:      rows = IsPvBot() ? 4 : 3; break;
            case Page::AIType:    rows = 2; break;
            case Page::HumanSide: rows = 2; break;
            case Page::Algorithms:
                if (m_i_AIType == 1) { // ML Mode (Grid 4+4)
                    rows = IsPvBot() ? 3 : 5;
                }
                else {
                    // Heuristic Mode
                    rows = IsPvBot() ? 2 : 3;
                }
                break;
            default:              rows = 2; break;
            }

            const float lineH = btnH + rowGap;
            const float blockH = rows * lineH;
            const float topY = (H + blockH) * 0.5f;

            
            auto rowYTopDown = [&](int idxFromTop) {
                return topY - (idxFromTop + 1) * lineH + (lineH - btnH);
                };

            

            // 3 buttons row
            auto placeRow3 = [&](Row row, int idxFromTop, const char* t0, const char* t1, const char* t2) {
                float y = rowYTopDown(idxFromTop);

                float maxTextW = std::max({ GetTextW(t0), GetTextW(t1), GetTextW(t2) });
                float actualBtnW = std::max(defaultBtnW, maxTextW + 60.0f);

                float availableW = (float)W - 40.0f;
                float requiredW = 3.0f * actualBtnW + 2.0f * btnGap;

                if (requiredW > availableW) {
                    // safety check to fit buttons
                    actualBtnW = (availableW - 2.0f * btnGap) / 3.0f;
                }

                // centering
                float totalW = 3.0f * actualBtnW + 2.0f * btnGap;
                float x0 = (W - totalW) * 0.5f;

                m_vec_Buttons.push_back({ row, 0, x0 + 0.0f * (actualBtnW + btnGap), y, actualBtnW, btnH, t0 });
                m_vec_Buttons.push_back({ row, 1, x0 + 1.0f * (actualBtnW + btnGap), y, actualBtnW, btnH, t1 });
                m_vec_Buttons.push_back({ row, 2, x0 + 2.0f * (actualBtnW + btnGap), y, actualBtnW, btnH, t2 });
                };

            // 2 buttons row
            auto placeRow2 = [&](Row row, int idxFromTop, const char* t0, const char* t1) {
                float y = rowYTopDown(idxFromTop);

                float maxTextW = std::max({ GetTextW(t0), GetTextW(t1) });
                float actualBtnW = std::max(defaultBtnW, maxTextW + 60.0f);

                float availableW = (float)W - 40.0f;
                float requiredW = 2.0f * actualBtnW + 1.0f * btnGap;
                if (requiredW > availableW) {
                    actualBtnW = (availableW - 1.0f * btnGap) / 2.0f;
                }

                float totalW = 2.0f * actualBtnW + 1.0f * btnGap;
                float x0 = (W - totalW) * 0.5f;

                m_vec_Buttons.push_back({ row, 0, x0 + 0.0f * (actualBtnW + btnGap), y, actualBtnW, btnH, t0 });
                m_vec_Buttons.push_back({ row, 1, x0 + 1.0f * (actualBtnW + btnGap), y, actualBtnW, btnH, t1 });
                };

            // 5 buttons row
            auto placeRow5 = [&](Row row, int idxFromTop,
                const char* t0, const char* t1, const char* t2, const char* t3, const char* t4) {

                    float y = rowYTopDown(idxFromTop);
                    const float baseSmallW = 240.0f;
                    const float gap = 18.0f;

                    float maxTextW = std::max({ GetTextW(t0), GetTextW(t1), GetTextW(t2), GetTextW(t3), GetTextW(t4) });
                    float actualBtnW = std::max(baseSmallW, maxTextW + 40.0f);

                    float totalReq = 5.0f * actualBtnW + 4.0f * gap;
                    if (totalReq > W - 40.0f) {
                        actualBtnW = (W - 40.0f - 4.0f * gap) / 5.0f;
                    }

                    float totalW = 5.0f * actualBtnW + 4.0f * gap;
                    float x0 = (W - totalW) * 0.5f;

                    const char* T[5] = { t0,t1,t2,t3,t4 };
                    for (int i = 0; i < 5; ++i) {
                        m_vec_Buttons.push_back({ row, i, x0 + i * (actualBtnW + gap), y, actualBtnW, btnH, T[i] });
                    }
                };

            auto placeGrid8 = [&](Row row, int startIdxFromTop,
                const char* t0, const char* t1, const char* t2, const char* t3, const char* t4,
                const char* t5, const char* t6, const char* t7) -> int {

                    const char* Row1[4] = { t0, t1, t2, t3 };
                    const char* Row2[4] = { t4, t5, t6, t7 };

                    const float baseSmallW = 220.0f;
                    const float gap = 20.0f;

                    float maxW1 = 0.0f;
                    for (auto t : Row1) maxW1 = std::max(maxW1, GetTextW(t));

                    float maxW2 = 0.0f;
                    for (auto t : Row2) maxW2 = std::max(maxW2, GetTextW(t));

                    float actualBtnW = std::max({ baseSmallW, maxW1 + 40.0f, maxW2 + 40.0f });

                    float totalReq = 4.0f * actualBtnW + 3.0f * gap;
                    if (totalReq > W - 40.0f) {
                        actualBtnW = (W - 40.0f - 3.0f * gap) / 4.0f;
                    }

                    float totalW = 4.0f * actualBtnW + 3.0f * gap;
                    float x0 = (W - totalW) * 0.5f;

                    float y1 = rowYTopDown(startIdxFromTop);
                    for (int i = 0; i < 4; ++i) {
                        m_vec_Buttons.push_back({ row, i, x0 + i * (actualBtnW + gap), y1, actualBtnW, btnH, Row1[i] });
                    }

                    float y2 = rowYTopDown(startIdxFromTop + 1);
                    for (int i = 0; i < 4; ++i) {
                        m_vec_Buttons.push_back({ row, 4 + i, x0 + i * (actualBtnW + gap), y2, actualBtnW, btnH, Row2[i] });
                    }

                    return 2;
                };



            int idx = 0;

            if (m_Page == Page::Main) {
                placeRow3(Row::Mode, idx++, "Player vs Player", "Player vs Bot", "Bot vs Bot");
                if (IsPvBot()) { // m_i_Mode == 1
                    placeRow2(Row::Human, idx++, "Mr X", "Detectives");
                }
                placeRow3(Row::Map, idx++, "Default Map", "Map Generator", "Random Environment");

                PlaceFooter(idx++, "Back to Menu", HasBot() ? "Next" : "Start Game", W, rowYTopDown, btnH);
                return;
            }


            if (m_Page == Page::AIType) {
                placeRow2(Row::AIType, idx++, "Heuristic", "ML");
                PlaceFooter(idx++, "Back", "Next", W, rowYTopDown, btnH);
                return;
            }

            if (m_Page == Page::HumanSide) {
                placeRow2(Row::Human, idx++, "Mr X", "Detectives");
                PlaceFooter(idx++, "Back", "Next", W, rowYTopDown, btnH);
                return;
            }

            if (m_Page == Page::Algorithms) {
                // m_i_AIType: 0 = Heuristic (5 options), 1 = ML (all 8 options)
                if (m_i_AIType == 1) {
                    // ML mode - show ALL options (heuristic + ML) split into 4+4 grids
                    if (IsPvBot()) {
                        if (m_i_Human == 0) {
                            // Human = MrX, Bot = Detectives
                            idx += placeGrid8(Row::Detectives, idx,
                                "D: Random", "D: MC", "D: Minimax", "D: GSP",
                                "D: FSE", "D: PPO", "D: MAPPO", "D: SAC");
                        }
                        else {
                            // Human = Detectives, Bot = MrX
                            idx += placeGrid8(Row::MrX, idx,
                                "X: Random", "X: DistMax", "X: Decoy", "X: MC",
                                "X: DFS", "X: PPO", "X: MAPPO", "X: SAC");
                        }
                    }
                    else {
                        // BotvBot - show all 8 options for both
                        idx += placeGrid8(Row::MrX, idx,
                            "X: Random", "X: DistMax", "X: Decoy", "X: MC",
                            "X: DFS", "X: PPO", "X: MAPPO", "X: SAC");

                        idx += placeGrid8(Row::Detectives, idx,
                            "D: Random", "D: MC", "D: Minimax", "D: GSP",
                            "D: FSE", "D: PPO", "D: MAPPO", "D: SAC");
                    }
                }
                else {
                    // Heuristic mode - show only 5 heuristic algorithm choices (single row)
                    if (IsPvBot()) {
                        if (m_i_Human == 0) {
                            placeRow5(Row::Detectives, idx++, "D: Random", "D: Monte Carlo", "D: Minimax", "D: GSP", "D: FSE");
                        }
                        else {
                            placeRow5(Row::MrX, idx++, "X: Random", "X: Dist Max", "X: Decoy Mov", "X: Monte Carlo", "X: DFS");
                        }
                    }
                    else {
                        // BotvBot
                        placeRow5(Row::MrX, idx++, "X: Random", "X: Dist Max", "X: Decoy Mov", "X: Monte Carlo", "X: DFS");
                        placeRow5(Row::Detectives, idx++, "D: Random", "D: Monte Carlo", "D: Minimax", "D: GSP", "D: FSE");
                    }
                }

                PlaceFooter(idx++, "Back", "Start Game", W, rowYTopDown, btnH);
                return;
            }
        }


        bool GameSetupState::IsRowDisabled(Row row) const {
            if (row == Row::Footer || row == Row::Mode || row == Row::Map) return false;

            // PvP
            if (m_Page == Page::Algorithms && m_i_Mode == 0) {
                return (row == Row::AIType || row == Row::Human || row == Row::MrX || row == Row::Detectives);
            }

            // PvBot
            if (m_Page == Page::Algorithms && m_i_Mode == 1) {
                if (row == Row::AIType || row == Row::Human) return false;
                if (row == Row::MrX)        return (m_i_Human == 0); // MrX = human
                if (row == Row::Detectives) return (m_i_Human == 1); // Detectives = human
                return false;
            }

            // BotvBot
            if (m_i_Mode == 2) {
                if (row == Row::AIType) return false;
                return false;
            }

            return false;
        }

        void GameSetupState::PlaceFooter(int idxFromTop, const char* left, const char* right,
            int W,
            const std::function<float(int)>& rowYTopDown,
            float btnH) {
            float y = rowYTopDown(idxFromTop);
            const float fBtnW = 250.0f;
            const float fGap = 60.0f;
            const float totalFW = fBtnW * 2 + fGap;
            const float fx0 = (W - totalFW) * 0.5f;
            m_vec_Buttons.push_back({ Row::Footer, 0, fx0,                y, fBtnW, btnH, left });
            m_vec_Buttons.push_back({ Row::Footer, 1, fx0 + fBtnW + fGap, y, fBtnW, btnH, right });
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
                if (e.key.keysym.sym == SDLK_ESCAPE) app->GetStateManager()->ChangeState("menu", app);
                else if (e.key.keysym.sym == SDLK_RETURN) StartGame(app);
                return;
            }

            // HOVER
            if (e.type == SDL_MOUSEMOTION) {
                float mx = (float)e.motion.x;
                float my = (float)e.motion.y;
                float myBL = (float)app->GetVirtualHeight() - my;

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
                // Transform mouse coordinates to virtual space
                float mx, my;
                app->TransformMouseToVirtual(e.button.x, e.button.y, mx, my);
                float myBL = (float)app->GetVirtualHeight() - my;

                for (const auto& b : m_vec_Buttons) {
                    if (mx >= b.f_X && mx <= b.f_X + b.f_W && myBL >= b.f_Y && myBL <= b.f_Y + b.f_H) {
                        if (IsRowDisabled(b.e_Row)) return; // ignored

                        if (b.e_Row == Row::Mode) {
                            m_i_Mode = b.i_Col;
                            m_i_AIType = 0;
                            m_i_Human = 0;
                            m_i_AIMrX = 0;
                            m_i_AIDet = 0;
                            return;
                        }

                        if (b.e_Row == Row::Map) { m_i_Map = b.i_Col; return; }
                        if (b.e_Row == Row::AIType) { m_i_AIType = b.i_Col; return; }

                        if (b.e_Row == Row::Human) { m_i_Human = b.i_Col; return; }

                        if (b.e_Row == Row::MrX) { m_i_AIMrX = b.i_Col; return; }
                        if (b.e_Row == Row::Detectives) { m_i_AIDet = b.i_Col; return; }

                        if (b.e_Row == Row::Footer) {
                            if (b.i_Col == 0) {
                                if (m_Page == Page::Main) {
                                    app->GetStateManager()->ChangeState("menu", app);
                                    return;
                                }
                                // back to previous
                                if (m_Page == Page::AIType) { m_Page = Page::Main; return; }
                                if (m_Page == Page::HumanSide) { m_Page = Page::AIType; return; }
                                if (m_Page == Page::Algorithms) { m_Page = Page::AIType; return; }
                                return;
                            }
                            else {
                                if (m_Page == Page::Main) {
                                    if (m_i_Map == 2) {
                                        app->GetStateManager()->ChangeState("emptyenv", app);
                                        return;
                                    }
                                    if (m_i_Map == 1) {
                                        app->GetStateManager()->ChangeState("mapgen", app);
                                        return;
                                    }

                                    if (!HasBot()) {
                                        // PvP - no more pages, just game
                                        StartGame(app);
                                        return;
                                    }

                                    m_Page = Page::AIType;
                                    return;
                                }

                                if (m_Page == Page::AIType) {
                                    m_Page = Page::Algorithms;
                                    return;
                                }


                                if (m_Page == Page::HumanSide) {
                                    m_Page = Page::Algorithms;
                                    return;
                                }

                                if (m_Page == Page::Algorithms) {
                                    StartGame(app);
                                    return;
                                }
                            }
                        }

                    }
                }
            }


        }


        void GameSetupState::Render(Core::Application* app) {
            if (app->IsTrainingMode()) return;

            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            Layout(app);

            // TITLE
            ScotlandYard::UI::DrawTextCenteredPx("Setup Your Game",
                20, app->GetVirtualHeight() - 120, app->GetVirtualWidth() - 20, app->GetVirtualHeight() - 60, { 1,1,1,1 }, app, 0.f);

            // buttons drawing with selected option
            for (const auto& b : m_vec_Buttons) {
                bool selected = false;
                if (b.e_Row == Row::Mode)    selected = (b.i_Col == m_i_Mode);
                if (b.e_Row == Row::Map)     selected = (b.i_Col == m_i_Map);
                if (b.e_Row == Row::AIType)  selected = (b.i_Col == m_i_AIType);
                if (b.e_Row == Row::Human)   selected = (b.i_Col == m_i_Human);
                if (b.e_Row == Row::MrX)     selected = (b.i_Col == m_i_AIMrX);
                if (b.e_Row == Row::Detectives) selected = (b.i_Col == m_i_AIDet);

                DrawButton(b, selected, app);
            }

            // tooltip for PvP
            if (m_i_Mode == 0) {
                bool hasMrX = false, hasDet = false;
                float mrxLower = 1e9f;
                float detUpper = -1e9f;

                for (const auto& b : m_vec_Buttons) {
                    if (b.e_Row == Row::MrX) { hasMrX = true; mrxLower = std::min(mrxLower, b.f_Y); }
                    else if (b.e_Row == Row::Detectives) { hasDet = true; detUpper = std::max(detUpper, b.f_Y + b.f_H); }
                }

                if (hasMrX && hasDet && detUpper < mrxLower)
                {
                    const float padY = 8.0f;
                    const float minH = 34.0f; // minimal height
                    float y0 = detUpper + padY;
                    float y1 = mrxLower - padY;
                    if (y1 - y0 < minH) y1 = y0 + minH;

                    const float x0 = 40.0f;
                    const float x1 = (float)app->GetVirtualWidth() - 40.0f;

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
            if (m_Page == Page::Algorithms && m_i_Mode == 1) {
                // if human is Mr X -> Mr X AI should be off (the same logic with Detectives)
                const Row disabledRow = (m_i_Human == 0) ? Row::MrX : Row::Detectives;

                float humanTop = -1e9f;
                for (const auto& b : m_vec_Buttons) {
                    if (b.e_Row == Row::Human) humanTop = std::max(humanTop, b.f_Y + b.f_H);
                }
                if (humanTop > -1e8f) {
                    ScotlandYard::UI::DrawTextCenteredPx(
                        "Choose who you want to play:", 0, humanTop + 2.0f, (float)app->GetVirtualWidth(), humanTop + 35.0f,
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
                    float x1 = std::min((float)app->GetVirtualWidth(), rowMaxX + marginX);
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

            switch (m_i_Mode) {
            case 0: S.e_Mode = GameMode::PvP;     break;
            case 1: S.e_Mode = GameMode::PvBot;   break;
            case 2: S.e_Mode = GameMode::BotvBot; break;
            }

            // mapping for Mr X (8 options: 0-4 heuristic, 5-7 ML)
            auto mapAI_MrX = [](int idx) {
                switch (idx) {
                case 0: return AIAlgorithm::Random;
                case 1: return AIAlgorithm::DistanceMaximizationMrX;
                case 2: return AIAlgorithm::DecoyMovementMrX;
                case 3: return AIAlgorithm::MonteCarloMrX;
                case 4: return AIAlgorithm::DFSMrX;
                case 5: return AIAlgorithm::PPOMrX;
                case 6: return AIAlgorithm::MAPPOMrX;
                case 7: return AIAlgorithm::DiscreteSACMrX;
                default: return AIAlgorithm::Random;
                }
                };

            // mapping for Detectives (8 options: 0-4 heuristic, 5-7 ML)
            auto mapAI_Detectives = [](int idx) {
                switch (idx) {
                case 0: return AIAlgorithm::Random;
                case 1: return AIAlgorithm::MonteCarloPolice;
                case 2: return AIAlgorithm::MinimaxPolice;
                case 3: return AIAlgorithm::GreedyShortestPathPolice;
                case 4: return AIAlgorithm::FrontSearchEncirclementPolice;
                case 5: return AIAlgorithm::PPOPolice;
                case 6: return AIAlgorithm::MAPPOPolice;
                case 7: return AIAlgorithm::DiscreteSACPolice;
                default: return AIAlgorithm::Random;
                }
                };

            if (S.e_Mode == GameMode::PvP) {
                // AI ignored
            }
            else if (S.e_Mode == GameMode::PvBot) {
                if (m_i_Human == 0) { // human = Mr X, bot = Detectives
                    S.e_AIDetectives = mapAI_Detectives(m_i_AIDet);
                    S.e_AIMisterX = AIAlgorithm::Random;
                }
                else { // human = Detectives, bot = MrX
                    S.e_AIMisterX = mapAI_MrX(m_i_AIMrX);
                    S.e_AIDetectives = AIAlgorithm::Random;
                }
            }
            else { // BotvBot
                S.e_AIMisterX = mapAI_MrX(m_i_AIMrX);
                S.e_AIDetectives = mapAI_Detectives(m_i_AIDet);
            }

            S.e_PvBotHuman = (m_i_Human == 0 ? HumanSide::MrX : HumanSide::Detectives);

            app->GetStateManager()->ChangeState("game", app);
        }


    }
} // namespaces

