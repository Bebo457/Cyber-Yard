#pragma once
#include "IGameState.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
    namespace States {

        class GameSetupState : public Core::IGameState {
        public:
            void OnEnter() override;
            void OnExit() override {}
            void OnPause() override {}
            void OnResume() override {}
            void Update(float) override {}
            void Render(Core::Application* p_App) override;
            void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

        private:
            enum class Row { Mode = 0, Human = 1, MrX = 2, Detectives = 3, Footer = 4 };
            struct Button {
                Row row;
                int col; // column
                float x, y, w, h; // position
                const char* text;
            };

            // game player choices
            int m_iMode = 0;   // 0: PvP, 1: PvBot, 2: BotvBot
            int m_iAIMrX = 0;  // 0: Random, 1: Greedy, 2: Neural
            int m_iAIDet = 0;  // 0: Random, 1: Greedy, 2: Neural
            int m_iHuman = 0;
            int m_iHover = -1;

            std::vector<Button> m_btns;

            bool isRowDisabled(Row row) const;
            void layout(Core::Application* app);
            void drawButton(const Button& b, bool selected, Core::Application* app);
            void startGame(Core::Application* app);
        };

    }
} // namespaces
