#pragma once
#include "IGameState.h"

namespace ScotlandYard { namespace Core { class Application; } }

namespace ScotlandYard {
    namespace States {

        class MapGeneratorState : public Core::IGameState {
        public:
            MapGeneratorState() = default;
            ~MapGeneratorState() override = default;

            void OnEnter() override {}
            void OnExit() override {}
            void OnPause() override {}
            void OnResume() override {}
            void Update(float) override {}
            void Render(Core::Application* p_App) override;
            void HandleEvent(const SDL_Event& event, Core::Application* p_App) override;
        };

    }
}
