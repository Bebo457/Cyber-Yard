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
                Row e_Row;
                int i_Col;
                float f_X, f_Y, f_W, f_H;
                const char* p_Text;
            };

            int m_i_Mode = 0;
            int m_i_AIMrX = 0;
            int m_i_AIDet = 0;
            int m_i_Human = 0;
            int m_i_Hover = -1;

            std::vector<Button> m_vec_Buttons;

            bool IsRowDisabled(Row e_Row) const;
            void Layout(Core::Application* p_App);
            void DrawButton(const Button& s_Button, bool b_Selected, Core::Application* p_App);
            void StartGame(Core::Application* p_App);
        };

    }
} // namespaces
