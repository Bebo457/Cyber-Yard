#include "HUDOverlay.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <unordered_map>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#define HUD_NO_TEXT 1 // without font yet

namespace ScotlandYard {
namespace UI {

    namespace {
        HUDStyle                g_HUDStyle{};
        int                     g_i_ViewportWidth = 1280;
        int                     g_i_ViewportHeight = 720;

        // OpenGL shaders and buffers
        GLuint g_ShaderProgram_Rounded = 0;
        GLuint g_ShaderProgram_Texture = 0;

        GLuint g_VAO_Rounded = 0;
        GLuint g_VBO_Rounded = 0;
        GLuint g_VAO_Texture = 0;
        GLuint g_VBO_Texture = 0;

        float pxToNDC(float px) { return (2.0f * px) / float(g_i_ViewportHeight); }

        std::vector<TicketSlot> g_vec_TicketSlots(k_TicketSlotCount);

        std::vector<std::string> g_vec_PillLabels = {
            "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus"
        };
        std::vector<Color> g_vec_PillColors = {
            {0.0f / 255.0f, 0.0f / 255.0f, 0.0f / 255.0f, 1.0f },   // Black
            {233.0f / 255.0f, 145.0f / 255.0f, 83.0f / 255.0f, 1.0f },   // 2x (orange)
            {250.0f / 255.0f, 219.0f / 255.0f, 55.0f / 255.0f, 1.0f },   // TAXI (yellow)
            {214.0f / 255.0f, 102.0f / 255.0f, 168.0f / 255.0f, 1.0f },   // Metro (magenta)
            { 95.0f / 255.0f, 146.0f / 255.0f, 88.0f / 255.0f, 1.0f },   // Bus (green)
        };

        // SHADERS
        const char* VS_R = R"(#version 330 core
        layout(location=0) in vec2 aPos; // NDC quad nad prostokątem
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
            vec2 lp = vPos - c;                 // lokalnie względem środka
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
            GLuint id = glCreateShader(t); glShaderSource(id, 1, &s, nullptr); glCompileShader(id);
            GLint ok = 0; glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
            if (!ok) { char log[1024]; glGetShaderInfoLog(id, 1024, nullptr, log); std::fprintf(stderr, "[HUD] %s\n", log); }
            return id;
        }
        GLuint link(GLuint vs, GLuint fs) {
            GLuint p = glCreateProgram(); glAttachShader(p, vs); glAttachShader(p, fs); glLinkProgram(p);
            GLint ok = 0; glGetProgramiv(p, GL_LINK_STATUS, &ok);
            if (!ok) { char log[1024]; glGetProgramInfoLog(p, 1024, nullptr, log); std::fprintf(stderr, "[HUD] %s\n", log); }
            glDetachShader(p, vs); glDetachShader(p, fs); glDeleteShader(vs); glDeleteShader(fs);
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

#if !HUD_NO_TEXT
        struct GlyphTex { GLuint tex = 0; int w = 0, h = 0; };
        static std::unordered_map<std::string, GlyphTex> g_textCache;

        static GLuint surfaceToTexture(SDL_Surface* s) {
            if (!s) return 0;
            SDL_Surface* conv = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_ABGR8888, 0);
            if (!conv) return 0;
            GLuint tex = 0;
            glGenTextures(1, &tex);
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, conv->w, conv->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, conv->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glBindTexture(GL_TEXTURE_2D, 0);
            SDL_FreeSurface(conv);
            return tex;
        }

        // todo turn on after adding font
        static void drawTextureCentered(GLuint tex, int texW, int texH,
            float x0, float y0, float x1, float y1,
            Color mul = { 1,1,1,1 })
        {
            if (!tex) return;

            // slot (px)
            auto ndcToPx = [&](float x, float y) {
                int px = int((x * 0.5f + 0.5f) * g_i_ViewportWidth);
                int py = int((1.0f - (y * 0.5f + 0.5f)) * g_i_ViewportHeight);
                return std::pair<int, int>(px, py);
                };
            auto [lx, ty] = ndcToPx(x0, y1);
            auto [rx, by] = ndcToPx(x1, y0);
            int sw = rx - lx, sh = by - ty;

            // height ~ 60% recta
            float targetH = std::max(14.0f, sh * 0.60f);
            float scale = targetH / float(texH);
            float qw = texW * scale;
            float qh = texH * scale;

            float cx = lx + sw * 0.5f;
            float cy = ty + sh * 0.5f;

            auto pxToNdcX = [&](float p) { return (p / float(g_i_ViewportWidth)) * 2.0f - 1.0f; };
            auto pxToNdcY = [&](float p) { return 1.0f - (p / float(g_i_ViewportHeight)) * 2.0f; };

            float qx0 = pxToNdcX(cx - qw * 0.5f);
            float qx1 = pxToNdcX(cx + qw * 0.5f);
            float qy0 = pxToNdcY(cy + qh * 0.5f);
            float qy1 = pxToNdcY(cy - qh * 0.5f);

            float verts[24] = {
                qx0,qy1, 0,1,  qx1,qy1, 1,1,  qx0,qy0, 0,0,
                qx1,qy1, 1,1,  qx1,qy0, 1,0,  qx0,qy0, 0,0
            };

            glUseProgram(g_ShaderProgram_Texture);
            glUniform4f(glGetUniformLocation(g_ShaderProgram_Texture, "uColor"), mul.r, mul.g, mul.b, mul.a);
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

        
        static void drawTextCentered(const std::string&, float, float, float, float, Color) { /* NO-OP */ }
#else
        
        static void drawTextCentered(const std::string&, float, float, float, float, Color) { /* NO-OP */ }
#endif

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

        void drawTopBar(float x0, float y0, float x1, float y1) {
            drawRoundedRect(x0, y0, x1, y1, g_HUDStyle.barColor, pxToNDC(g_HUDStyle.barRadiusPx));

            // layout of tickets
            const float padY = pxToNDC(g_HUDStyle.pillsPadYPx);
            const float padX = pxToNDC(g_HUDStyle.pillsPadXPx);
            const float gap = pxToNDC(g_HUDStyle.pillsGapPx);

            float innerX0 = x0 + gap;
            float innerX1 = x1 - gap;
            float innerY0 = y0 + padY;
            float innerY1 = y1 - padY;
            float h = innerY1 - innerY0;

            float cur = innerX0;

            for (size_t i = 0; i < g_vec_PillLabels.size(); ++i) {
                const std::string& txt = g_vec_PillLabels[i];
                const Color pc = g_vec_PillColors[i < g_vec_PillColors.size() ? i : 0];

                float minCapW = h * 1.8f;
                float approxW = minCapW + gap;

                float capX0 = cur;
                float capX1 = std::min(cur + approxW, innerX1);
                if (capX1 <= capX0) break;

                drawRoundedRect(capX0, innerY0, capX1, innerY1, { pc.r,pc.g,pc.b, 1.0f }, pxToNDC(g_HUDStyle.slotRadiusPx));

                // text inside (not working yet)
                drawTextCentered(txt, capX0 + padX * 0.5f, innerY0, capX1 - padX * 0.5f, innerY1, g_HUDStyle.textColor);

                cur = capX1 + gap;
                if (cur >= innerX1) break;
            }
        }

        constexpr float k_BottomBarGapPx = 10.0f;
        constexpr float k_BottomBarMarginPx = 6.0f;
        constexpr float k_BottomBarGapMultiplier = 0.6f;

        void drawBottomBar(float x0, float y0, float x1, float y1) {
            drawRoundedRect(x0, y0, x1, y1, g_HUDStyle.barColor, pxToNDC(g_HUDStyle.barRadiusPx));

            // slots from 1..24 (for tickets of Mr X)
            const float gap = pxToNDC(k_BottomBarGapPx);
            const float margin = pxToNDC(k_BottomBarMarginPx);

            float sx0 = x0 + gap;
            float sx1 = x1 - gap;
            float sy0 = y0 + gap * k_BottomBarGapMultiplier;
            float sy1 = y1 - gap * k_BottomBarGapMultiplier;

            float totalW = (sx1 - sx0) - (k_TicketSlotCount - 1) * margin;
            float slotW = totalW / float(k_TicketSlotCount);

            float x = sx0;
            for (int i = 0; i < k_TicketSlotCount; ++i) {
                Color c = g_vec_TicketSlots[i].color;
                if (c.a < 0.0f) c = g_HUDStyle.slotColor;

                drawRoundedRect(x, sy0, x + slotW, sy1, c, pxToNDC(g_HUDStyle.slotRadiusPx));

                // number (not working, todo)
                char buf[4]; std::snprintf(buf, sizeof(buf), "%d", i + 1);
                drawTextCentered(buf, x, sy0, x + slotW, sy1, g_HUDStyle.textColor);

                x += slotW + margin;
            }
        }

    } // namespace

    // API
    bool InitHUD() {
        ensurePipelines();
        return g_ShaderProgram_Rounded && g_ShaderProgram_Texture;
    }

    void ShutdownHUD() {
#if !HUD_NO_TEXT
        for (auto& kv : g_textCache) {
            if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
        }
        g_textCache.clear();
#endif

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

    // font not working yet — stub
    void SetFont(void* /*font*/) {
        // NO-OP
    }

    void SetTicketStates(const std::vector<TicketSlot>& slots) {
        g_vec_TicketSlots = slots;
        if (g_vec_TicketSlots.size() < k_TicketSlotCount) g_vec_TicketSlots.resize(k_TicketSlotCount);
        if (g_vec_TicketSlots.size() > k_TicketSlotCount) g_vec_TicketSlots.resize(k_TicketSlotCount);
    }

    void SetTopBar(const std::vector<std::string>& labels,
        const std::vector<Color>& pillColors)
    {
        if (!labels.empty()) g_vec_PillLabels = labels;
        if (!pillColors.empty()) g_vec_PillColors = pillColors;
    }

    void RenderHUD() {
        if (!g_ShaderProgram_Rounded || !g_ShaderProgram_Texture) if (!InitHUD()) return;

        GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
        GLboolean blendWas = glIsEnabled(GL_BLEND);
        if (depthWas)  glDisable(GL_DEPTH_TEST);
        if (!blendWas) glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        float tX0, tX1, tY0, tY1, bX0, bX1, bY0, bY1;
        computeBars(tX0, tX1, tY0, tY1, bX0, bX1, bY0, bY1);

        drawTopBar(tX0, tY0, tX1, tY1);
        drawBottomBar(bX0, bY0, bX1, bY1);

        if (!blendWas) glDisable(GL_BLEND);
        if (depthWas)  glEnable(GL_DEPTH_TEST);
    }

} // namespace UI
} // namespace ScotlandYard
