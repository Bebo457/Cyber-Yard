#pragma once
#include "IGameState.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <string>
#include <vector>

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
    namespace States {

        class MapGeneratorState : public Core::IGameState {
        public:
            MapGeneratorState() = default;
            ~MapGeneratorState() override = default;

            void OnEnter() override;
            void OnExit() override {}
            void OnPause() override {}
            void OnResume() override {}
            void Update(float) override {}
            void Render(Core::Application* p_App) override;
            void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;

        private:
            struct Field {
                std::string label;
                std::string value;
                SDL_Rect    rect{};
                bool        focused = false;
                bool        numeric = false;
                int         maxLen = 16;
            };

            struct Slider {
                std::string label;
                float value;
                float minv, maxv;
                float step;
                SDL_Rect track{};
                bool dragging = false;
                int  knobW = 14;
            };

            std::vector<Field> m_Fields;
            std::vector<Slider> m_Sliders;
            int        m_Focused = -1;


            // Buttons
            SDL_Rect   m_BtnGenerate{};
            SDL_Rect   m_BtnBack{};

            // Info label
            std::string m_InfoText;

            // Optional preview
            GLuint     m_PreviewTex = 0;
            SDL_Rect   m_PreviewRect{};
            bool       m_HasPreview = false;

        private:
            void layoutUI(int W, int H, Core::Application* p_App);
            void layoutSliders(int startY, int cardX, int cardW, int pad, int gap);
            float xToVal(const Slider& s, int mx) const;
            int   valToX(const Slider& s) const;
            void focusField(int idx);
            void blurAll();
            void appendTextToFocused(const char* utf8);
            void backspaceFocused();
            void tryGenerate();                 // TODO: podpiac realny generator 
            void makeDummyPreview(int W, int H);
        };

    }
} // namespaces
