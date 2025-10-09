#ifndef SCOTLANDYARD_CORE_IGAMESTATE_H
#define SCOTLANDYARD_CORE_IGAMESTATE_H

#include <SDL2/SDL.h>
#include <GL/glew.h> 

namespace ScotlandYard {
namespace Core {

class IGameState {
public:
    virtual ~IGameState() = default;

    virtual void OnEnter() = 0;
    virtual void OnExit() = 0;
    virtual void OnPause() = 0;
    virtual void OnResume() = 0;
    virtual void Update(float deltaTime) = 0;
    virtual void Render() = 0;
    virtual void HandleEvent(const SDL_Event& event) = 0;
};

} // namespace Core
} // namespace ScotlandYard

#endif // SCOTLANDYARD_CORE_IGAMESTATE_H
