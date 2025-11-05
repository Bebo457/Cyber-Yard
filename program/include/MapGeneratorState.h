#pragma once

#include "IGameState.h"
#include "MapGenerator.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <GL/gl.h>
#include <memory>

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
        std::string s_Label;
        std::string s_Value;
        SDL_Rect rect{};
        bool b_Focused = false;
        bool b_Numeric = false;
        int i_MaxLength = 16;
    };

    struct Slider {
        std::string s_Label;
        float f_Value;
        float f_MinValue, f_MaxValue;
        float f_Step;
        SDL_Rect track{};
        bool b_Dragging = false;
        int i_KnobWidth = 14;
    };

    Core::Application* m_pApp;

    std::vector<Field> m_vec_Fields;
    std::vector<Slider> m_vec_Sliders;
    int m_i_FocusedFieldIndex = -1;

    SDL_Rect m_BtnGenerate{};
    SDL_Rect m_BtnBack{};

    std::string m_s_InfoText;

    GLuint m_GLuint_PreviewTexture = 0;
    SDL_Rect m_rect_PreviewArea{};
    bool m_b_HasPreview = false;

    std::vector<MapGen::Point> m_vec_GridPoints;
    std::vector<MapGen::Point> m_vec_RiverPath;
    std::vector<MapGen::Point> m_vec_ControlPoints;
    std::vector<MapGen::Park> m_vec_Parks;
    int m_i_CurrentCorner = 0;

private:
    void LayoutUI(int i_Width, int i_Height, Core::Application* p_App);
    void LayoutSliders(int i_StartY, int i_CardX, int i_CardWidth, int i_Padding, int i_Gap);
    float XPositionToValue(const Slider& slider, int i_MouseX) const;
    int ValueToXPosition(const Slider& slider) const;
    void FocusField(int i_Index);
    void BlurAllFields();
    void AppendTextToFocusedField(const char* p_Utf8Text);
    void BackspaceInFocusedField();
    void TryGenerateMap();
    void MakeDummyPreview(int i_Width, int i_Height);
    void ShowMapInNewWindow(int i_Width, int i_Height);
    
    void GenerateMapPreview(int i_Width, int i_Height);
    void RenderMapToTexture(int i_Width, int i_Height);
};

} // namespace States
} // namespace ScotlandYard
