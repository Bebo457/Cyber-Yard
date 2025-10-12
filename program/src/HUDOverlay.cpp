#include "HUDOverlay.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "Application.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "../external/stb_image.h"

namespace ScotlandYard {
    namespace UI {

        namespace {
            HUDStyle                g_HUDStyle{};
            int                     g_i_ViewportWidth = 1280;
            int                     g_i_ViewportHeight = 720;
            ScotlandYard::Core::Application* g_pApp = nullptr;

            static GLuint g_TexCamera = 0;
            static int    g_TexCamW = 0, g_TexCamH = 0;
            static std::function<void()> g_OnCameraToggle;
            static float g_CamBtnNdcX0 = 0, g_CamBtnNdcY0 = 0, g_CamBtnNdcX1 = 0, g_CamBtnNdcY1 = 0;

            // number of round todo changing + clock
            int g_iRound = 1;

            // OpenGL shaders and buffers
            GLuint g_ShaderProgram_Rounded = 0;
            GLuint g_ShaderProgram_Texture = 0;

            GLuint g_VAO_Rounded = 0;
            GLuint g_VBO_Rounded = 0;
            GLuint g_VAO_Texture = 0;
            GLuint g_VBO_Texture = 0;

            // layout/state
            float pxToNDC(float px) { return (2.0f * px) / float(g_i_ViewportHeight); }

            std::vector<TicketSlot> g_vec_TicketSlots(k_TicketSlotCount);

            
            std::vector<std::string> g_vec_PillLabels = { "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus" };
            std::vector<Color>       g_vec_PillColors = {
                {0.0f / 255.0f,   0.0f / 255.0f,   0.0f / 255.0f,   1.0f}, // Black
                {0xE2 / 255.0f,  0x70 / 255.0f,  0x3F / 255.0f,  1.0f}, // 2x
                {0xED / 255.0f,  0xD1 / 255.0f,  0x00 / 255.0f,  1.0f}, // TAXI
                {0xF5 / 255.0f,  0x51 / 255.0f,  0xAE / 255.0f,  1.0f}, // Metro
                {0x41 / 255.0f,  0x84 / 255.0f,  0x3D / 255.0f,  1.0f}, // Bus
            };

            // SHADERS
            const char* VS_R = R"(#version 330 core
        layout(location=0) in vec2 aPos;
        out vec2 vPos;
        void main(){ vPos=aPos; gl_Position=vec4(aPos,0.0,1.0); })";

            const char* FS_R = R"(#version 330 core
        in vec2 vPos;
        uniform vec4 uRect;   // x0,y0,x1,y1
        uniform float uRadius;
        uniform vec4 uColor;
        out vec4 FragColor;
        float sdRoundBox(in vec2 p, in vec2 b, in float r){
            vec2 d = abs(p) - b + vec2(r);
            return length(max(d,0.0)) - r;
        }
        void main(){
            vec2 c  = 0.5*(uRect.xy + uRect.zw);
            vec2 hs = 0.5*vec2(uRect.z - uRect.x, uRect.w - uRect.y);
            vec2 lp = vPos - c;
            float d = sdRoundBox(lp, hs, uRadius);
            float aa = fwidth(d) * 1.5;
            float alpha = 1.0 - smoothstep(0.0, aa, d);
            vec4 col = vec4(uColor.rgb, uColor.a*alpha);
            if (col.a <= 0.001) discard;
            FragColor = col;
        })";

            const char* VS_TEX = R"(#version 330 core
        layout(location=0) in vec2 aPos; // NDC
        layout(location=1) in vec2 aUV;
        out vec2 vUV;
        void main(){ vUV=aUV; gl_Position=vec4(aPos,0.0,1.0); })";

            const char* FS_TEX = R"(#version 330 core
        in vec2 vUV; uniform sampler2D uTex; uniform vec4 uColor;
        out vec4 FragColor;
        void main(){ vec4 t = texture(uTex, vUV); FragColor = vec4(uColor.rgb, uColor.a) * t; })";

            GLuint compile(GLenum t, const char* s) {
                GLuint id = glCreateShader(t);
                glShaderSource(id, 1, &s, nullptr);
                glCompileShader(id);
                GLint ok = 0; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
                if (!ok) { char log[1024]; glGetShaderInfoLog(id, 1024, nullptr, log); std::fprintf(stderr, "[HUD] %s\n", log); }
                return id;
            }
            GLuint link(GLuint vs, GLuint fs) {
                GLuint p = glCreateProgram();
                glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
                GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
                if (!ok) { char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log); std::fprintf(stderr, "[HUD] %s\n", log); }
                glDetachShader(p, vs); glDetachShader(p, fs);
                glDeleteShader(vs); glDeleteShader(fs);
                return p;
            }

            void ensurePipelines() {
                if (!g_ShaderProgram_Rounded) {
                    GLuint vs = compile(GL_VERTEX_SHADER, VS_R);
                    GLuint fs = compile(GL_FRAGMENT_SHADER, FS_R);
                    g_ShaderProgram_Rounded = link(vs, fs);
                }
                if (!g_ShaderProgram_Texture) {
                    GLuint vs = compile(GL_VERTEX_SHADER, VS_TEX);
                    GLuint fs = compile(GL_FRAGMENT_SHADER, FS_TEX);
                    g_ShaderProgram_Texture = link(vs, fs);
                }

                // VAO/VBO for rounded-rect (2D positions)
                if (!g_VAO_Rounded) {
                    glGenVertexArrays(1, &g_VAO_Rounded);
                    glGenBuffers(1, &g_VBO_Rounded);
                    glBindVertexArray(g_VAO_Rounded);
                    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Rounded);
                    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
                    glBindVertexArray(0);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                }

                // VAO/VBO for text (position + UV)
                if (!g_VAO_Texture) {
                    glGenVertexArrays(1, &g_VAO_Texture);
                    glGenBuffers(1, &g_VBO_Texture);
                    glBindVertexArray(g_VAO_Texture);
                    glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Texture);
                    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
                    glEnableVertexAttribArray(0);
                    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
                    glEnableVertexAttribArray(1);
                    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
                    glBindVertexArray(0);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                }
            }

            static void drawIcon(GLuint tex, float x0, float y0, float x1, float y1)
            {
                if (!tex) return;
                float verts[24] = {
                    x0,y0, 0,0,  x1,y0, 1,0,  x0,y1, 0,1,
                    x1,y0, 1,0,  x1,y1, 1,1,  x0,y1, 0,1
                };
                glUseProgram(g_ShaderProgram_Texture);
                glUniform4f(glGetUniformLocation(g_ShaderProgram_Texture, "uColor"), 1, 1, 1, 1);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, tex);
                glUniform1i(glGetUniformLocation(g_ShaderProgram_Texture, "uTex"), 0);
                glBindVertexArray(g_VAO_Texture);
                glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Texture);
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                glUseProgram(0);
            }

            void drawRoundedRect(float x0, float y0, float x1, float y1, Color c, float radiusNDC) {
                const float verts[] = { x0,y0,  x1,y0,  x0,y1,  x1,y0,  x1,y1,  x0,y1 };
                glUseProgram(g_ShaderProgram_Rounded);
                glUniform4f(glGetUniformLocation(g_ShaderProgram_Rounded, "uRect"), x0, y0, x1, y1);
                glUniform1f(glGetUniformLocation(g_ShaderProgram_Rounded, "uRadius"), radiusNDC);
                glUniform4f(glGetUniformLocation(g_ShaderProgram_Rounded, "uColor"), c.r, c.g, c.b, c.a);
                glBindVertexArray(g_VAO_Rounded);
                glBindBuffer(GL_ARRAY_BUFFER, g_VBO_Rounded);
                glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
                glUseProgram(0);
            }

            // TEXT
            inline float ndcX_to_px(float x) { return (x * 0.5f + 0.5f) * g_i_ViewportWidth; }
            inline float ndcY_to_px(float y) { return (y * 0.5f + 0.5f) * g_i_ViewportHeight; }
            inline float px_to_ndcX(float p) { return (p / float(g_i_ViewportWidth)) * 2.0f - 1.0f; }
            inline float px_to_ndcY(float p) { return 1.0f - (p / float(g_i_ViewportHeight)) * 2.0f; }

            float textWidthPx(const std::string& s, float scale) {
                if (!g_pApp) return 0.0f;
                const auto& chars = g_pApp->GetCharacterMap();
                float w = 0.0f;
                for (char c : s) {
                    auto it = chars.find(c);
                    if (it == chars.end()) continue;
                    w += float(it->second.m_i_Advance >> 6) * scale;
                }
                return w;
            }

            void drawTextPx(const std::string& s, float x_px, float baseline_y_px, float scale,
                float r, float g, float b)
            {
                if (!g_pApp) return;
                const auto& chars = g_pApp->GetCharacterMap();
                GLuint prog = g_pApp->GetTextShaderProgram();
                GLuint vao = g_pApp->GetTextVAO();
                GLuint vbo = g_pApp->GetTextVBO();

                glUseProgram(prog);
                glUniform3f(glGetUniformLocation(prog, "textColor"), r, g, b);
                glm::mat4 P = glm::ortho(0.0f, (float)g_i_ViewportWidth, 0.0f, (float)g_i_ViewportHeight);
                glUniformMatrix4fv(glGetUniformLocation(prog, "projection"), 1, GL_FALSE, glm::value_ptr(P));

                glActiveTexture(GL_TEXTURE0);
                glUniform1i(glGetUniformLocation(prog, "text"), 0);
                glBindVertexArray(vao);

                float penX = x_px;
                for (char c : s) {
                    auto it = chars.find(c); if (it == chars.end()) continue;
                    const ScotlandYard::Core::Character& ch = it->second;

                    float xpos = penX + ch.m_i_BearingX * scale;
                    float ypos = baseline_y_px - (ch.m_i_Height - ch.m_i_BearingY) * scale;
                    float w = ch.m_i_Width * scale, h = ch.m_i_Height * scale;

                    float verts[6][4] = {
                        { xpos,     ypos,     0.0f, 1.0f },
                        { xpos,     ypos + h,   0.0f, 0.0f },
                        { xpos + w,   ypos + h,   1.0f, 0.0f },
                        { xpos,     ypos,     0.0f, 1.0f },
                        { xpos + w,   ypos + h,   1.0f, 0.0f },
                        { xpos + w,   ypos,     1.0f, 1.0f },
                    };

                    glBindTexture(GL_TEXTURE_2D, ch.m_TextureID);
                    glBindBuffer(GL_ARRAY_BUFFER, vbo);
                    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
                    glBindBuffer(GL_ARRAY_BUFFER, 0);
                    glDrawArrays(GL_TRIANGLES, 0, 6);

                    penX += (ch.m_i_Advance >> 6) * scale;
                }
                glBindVertexArray(0);
                glBindTexture(GL_TEXTURE_2D, 0);
                glUseProgram(0);
            }


            
            
            void drawTextCentered(const std::string& s, float x0, float y0, float x1, float y1, Color col)
            {
                float left_px = ndcX_to_px(x0);
                float right_px = ndcX_to_px(x1);
                float bottom_px = ndcY_to_px(y0);
                float top_px = ndcY_to_px(y1);

                float rectH_px = std::max(1.0f, top_px - bottom_px);
                float targetH_px = std::max(14.0f, rectH_px * 0.60f);
                float scale = targetH_px / 48.0f; // in Application FT font height = 48 px

                // baseline with correction
                float baseline = bottom_px + (rectH_px * 0.20f) + targetH_px * 0.8f - 10.0f;

                float tw = textWidthPx(s, scale);
                float tx = (left_px + right_px) * 0.5f - tw * 0.5f;

                drawTextPx(s, tx, baseline, scale, col.r, col.g, col.b);
            }

            // LAYOUT
            void computeBars(float& tX0, float& tX1, float& tY0, float& tY1,
                float& bX0, float& bX1, float& bY0, float& bY1)
            {
                const float iy = std::clamp(g_HUDStyle.insetY, 0.0f, k_DefaultInsetYMax);
                const float it = std::clamp(g_HUDStyle.topInsetX, 0.0f, k_DefaultInsetXMax);
                const float ib = std::clamp(g_HUDStyle.botInsetX, 0.0f, k_DefaultInsetXMax);

                tX0 = -1.0f + it; tX1 = 1.0f - it;
                tY0 = g_HUDStyle.topY0 - iy; tY1 = g_HUDStyle.topY1 - iy;

                bX0 = -1.0f + ib; bX1 = 1.0f - ib;
                bY0 = g_HUDStyle.botY0 + iy; bY1 = g_HUDStyle.botY1 + iy;
            }

            // TOP BAR
            void drawTopBar(float x0, float y0, float x1, float y1) {
                drawRoundedRect(x0, y0, x1, y1, g_HUDStyle.barColor, pxToNDC(g_HUDStyle.barRadiusPx));

                const float padY = pxToNDC(g_HUDStyle.pillsPadYPx);
                const float padX = pxToNDC(g_HUDStyle.pillsPadXPx);
                const float gap = pxToNDC(g_HUDStyle.pillsGapPx);

                float innerX0 = x0 + gap;
                float innerX1 = x1 - gap;
                float innerY0 = y0 + padY;
                float innerY1 = y1 - padY;
                float hNdc = innerY1 - innerY0;

                // ROUND TEXT
                int clampedRound = std::clamp(g_iRound, 1, 24);
                std::string sRound = "Round " + std::to_string(clampedRound);

                float topH_px = std::max(18.0f, hNdc * 0.60f * g_i_ViewportHeight);
                float scale = (topH_px * 0.75f) / 48.0f;

                float left_px = (innerX0 * 0.5f + 0.5f) * g_i_ViewportWidth;
                float cY_px = 0.5f * (ndcY_to_px(innerY0) + ndcY_to_px(innerY1));
                float baseline = cY_px + topH_px * 0.38f - 20.0f;

                drawTextPx(sRound, left_px, baseline, scale,
                    g_HUDStyle.textColor.r, g_HUDStyle.textColor.g, g_HUDStyle.textColor.b);


                // CAMERA ICON SETTINGS
                float capAreaRight = innerX1;

                if (g_TexCamera)
                {
                    float btnH = innerY1 - innerY0;
                    float btnW = btnH;
                    float btnX1 = innerX1;
                    float btnX0 = btnX1 - btnW;


                    g_CamBtnNdcX0 = btnX0; g_CamBtnNdcX1 = btnX1;
                    g_CamBtnNdcY0 = innerY0; g_CamBtnNdcY1 = innerY1;

                    drawIcon(g_TexCamera, btnX0, innerY0, btnX1, innerY1);

                    capAreaRight = btnX0 - gap;
                }

                // layout of tickets
                float curRight = capAreaRight;
                for (int i = int(g_vec_PillLabels.size()) - 1; i >= 1; --i) {
                    const std::string& label = g_vec_PillLabels[i];
                    Color pc = g_vec_PillColors[i - 1 < (int)g_vec_PillColors.size() ? i - 1 : 0];

                    float neededTextW = textWidthPx(label, scale);
                    float padX_px = std::max(8.0f, g_HUDStyle.pillsPadXPx);
                    float capH_px = hNdc * g_i_ViewportHeight;
                    float capW_px = std::max(capH_px * 1.8f, neededTextW + 2.0f * padX_px);
                    float capW_ndc = (capW_px / g_i_ViewportWidth) * 2.0f;

                    float capX1 = curRight;
                    float capX0 = capX1 - capW_ndc;
                    if (capX0 < innerX0) capX0 = innerX0;

                    drawRoundedRect(capX0, innerY0, capX1, innerY1, pc, pxToNDC(g_HUDStyle.slotRadiusPx));
                    drawTextCentered(label, capX0, innerY0, capX1, innerY1, { 1,1,1,1 });

                    curRight = capX0 - gap;
                    if (curRight <= innerX0) break;
                }
            }

            // BOTTOM BAR
            constexpr float k_BottomBarGapPx = 10.0f;
            constexpr float k_BottomBarMarginPx = 6.0f;

            void drawBottomBar(float x0, float y0, float x1, float y1) {
                drawRoundedRect(x0, y0, x1, y1, g_HUDStyle.barColor, pxToNDC(g_HUDStyle.barRadiusPx));

                const float gap = pxToNDC(k_BottomBarGapPx);
                const float margin = pxToNDC(k_BottomBarMarginPx);

                float sx0 = x0 + gap;
                float sx1 = x1 - gap;
                float sy0 = y0 + gap * 0.6f;
                float sy1 = y1 - gap * 0.6f;

                float totalW = (sx1 - sx0) - (k_TicketSlotCount - 1) * margin;
                float slotW = totalW / float(k_TicketSlotCount);

                float x = sx0;
                for (int i = 0; i < k_TicketSlotCount; ++i) {
                    Color c = g_vec_TicketSlots[i].color;
                    if (c.a < 0.0f) c = g_HUDStyle.slotColor;

                    drawRoundedRect(x, sy0, x + slotW, sy1, c, pxToNDC(g_HUDStyle.slotRadiusPx));

                    char buf[4]; std::snprintf(buf, sizeof(buf), "%d", i + 1);
                    drawTextCentered(buf, x, sy0, x + slotW, sy1, g_HUDStyle.textColor);

                    x += slotW + margin;
                }
            }

        } // namespace

        // API

        void LoadCameraIconPNG(const char* path)
        {
            if (g_TexCamera) { glDeleteTextures(1, &g_TexCamera); g_TexCamera = 0; }
            int n = 0;
            unsigned char* data = stbi_load(path, &g_TexCamW, &g_TexCamH, &n, 4);
            if (!data) {
                std::fprintf(stderr, "[HUD] Camera icon load FAILED: %s\n", path);
                return;
            }
            std::fprintf(stderr, "[HUD] Camera icon loaded: %s (%dx%d, ch=%d)\n", path, g_TexCamW, g_TexCamH, n);

            glGenTextures(1, &g_TexCamera);
            glBindTexture(GL_TEXTURE_2D, g_TexCamera);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_TexCamW, g_TexCamH, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            stbi_image_free(data);
        }

        void SetCameraToggleCallback(std::function<void()> cb) { g_OnCameraToggle = std::move(cb); }

        void HandleMouseClick(int x_px, int y_px)
        {
            // click change (px, origin top-left) -> NDC
            float nx = (x_px / float(g_i_ViewportWidth)) * 2.0f - 1.0f;
            float ny = 1.0f - (y_px / float(g_i_ViewportHeight)) * 2.0f;
            if (nx >= g_CamBtnNdcX0 && nx <= g_CamBtnNdcX1 &&
                ny >= g_CamBtnNdcY0 && ny <= g_CamBtnNdcY1)
            {
                if (g_OnCameraToggle) g_OnCameraToggle();
            }
        }



        bool InitHUD() {
            ensurePipelines();
            return g_ShaderProgram_Rounded && g_ShaderProgram_Texture;
        }

        void DrawRoundedRectScreen(float x0, float y0, float x1, float y1, Color c, int radiusPx) {
            ensurePipelines();
            auto pxToNdcX = [&](float px) { return (px / float(g_i_ViewportWidth)) * 2.0f - 1.0f; };
            auto pxToNdcY = [&](float py) { return (py / float(g_i_ViewportHeight)) * 2.0f - 1.0f; };
            drawRoundedRect(pxToNdcX(x0), pxToNdcY(y0), pxToNdcX(x1), pxToNdcY(y1), c, pxToNDC(radiusPx));
        }

        void ShutdownHUD() {
            if (g_VBO_Rounded) { glDeleteBuffers(1, &g_VBO_Rounded); g_VBO_Rounded = 0; }
            if (g_VAO_Rounded) { glDeleteVertexArrays(1, &g_VAO_Rounded); g_VAO_Rounded = 0; }
            if (g_VBO_Texture) { glDeleteBuffers(1, &g_VBO_Texture); g_VBO_Texture = 0; }
            if (g_VAO_Texture) { glDeleteVertexArrays(1, &g_VAO_Texture); g_VAO_Texture = 0; }
        }

        void SetViewport(int w, int h) {
            g_i_ViewportWidth = std::max(1, w);
            g_i_ViewportHeight = std::max(1, h);
        }

        void SetHUDStyle(const HUDStyle& style) { g_HUDStyle = style; }

        void SetTicketStates(const std::vector<TicketSlot>& slots) {
            g_vec_TicketSlots = slots;
            if (g_vec_TicketSlots.size() < k_TicketSlotCount) g_vec_TicketSlots.resize(k_TicketSlotCount);
            if (g_vec_TicketSlots.size() > k_TicketSlotCount) g_vec_TicketSlots.resize(k_TicketSlotCount);
        }

        void SetTopBar(const std::vector<std::string>& labels,
            const std::vector<Color>& pillColors)
        {
            if (!labels.empty())     g_vec_PillLabels = labels;
            if (!pillColors.empty()) g_vec_PillColors = pillColors;
        }

        // number of round
        void SetRound(int r) { g_iRound = std::clamp(r, 1, 24); }

        // import font from Application
        void BindTextFromApp(ScotlandYard::Core::Application* p_App) { g_pApp = p_App; }

        void RenderHUD() {
            if (!g_ShaderProgram_Rounded || !g_ShaderProgram_Texture) if (!InitHUD()) return;

            GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
            GLboolean blendWas = glIsEnabled(GL_BLEND);
            GLboolean sRGBWas = glIsEnabled(GL_FRAMEBUFFER_SRGB);

            if (depthWas)  glDisable(GL_DEPTH_TEST);
            if (!blendWas) glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


            if (sRGBWas) glDisable(GL_FRAMEBUFFER_SRGB);
            float tX0, tX1, tY0, tY1, bX0, bX1, bY0, bY1;
            computeBars(tX0, tX1, tY0, tY1, bX0, bX1, bY0, bY1);

            drawTopBar(tX0, tY0, tX1, tY1);
            drawBottomBar(bX0, bY0, bX1, bY1);

            if (sRGBWas)  glEnable(GL_FRAMEBUFFER_SRGB);
            if (!blendWas) glDisable(GL_BLEND);
            if (depthWas)  glEnable(GL_DEPTH_TEST);
        }

        // new version
        void RenderHUD(ScotlandYard::Core::Application* p_App) {
            BindTextFromApp(p_App);
            RenderHUD();
        }

    } // namespace UI
} // namespace ScotlandYard
