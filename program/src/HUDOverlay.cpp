#include "HUDOverlay.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <unordered_map>
#include <algorithm>
#include <cstdio>

namespace UI {

    namespace {
        HUDStyle                g_style{};
        int                     g_vpW = 1280, g_vpH = 720;
        TTF_Font* g_font = nullptr;

        std::vector<TicketSlot> g_slots(24);

        std::vector<std::string> g_pills = {
            "Runda ...", "Black", "2x", "TAXI", "Metro", "Bus"
        };
        std::vector<Color> g_pillColors = {
            {0.0f / 255.f, 0.0f / 255.f, 0.0f / 255.f, 1.0f },   // Black
            {233 / 255.f,145 / 255.f, 83 / 255.f, 1.0f },   // 2x (orange)
            {250 / 255.f,219 / 255.f, 55 / 255.f, 1.0f },   // TAXI (yellow)
            {214 / 255.f,102 / 255.f,168 / 255.f, 1.0f },   // Metro (magenta)
            { 95 / 255.f,146 / 255.f, 88 / 255.f, 1.0f },   // Bus (green)
        };

        // OpenGL shaders and bufors
        GLuint g_progRounded = 0;
        GLuint g_progTex = 0;

        GLuint g_vaoRounded = 0, g_vboRounded = 0;
        GLuint g_vaoTex = 0, g_vboTex = 0;

        float pxToNDC(float px) { return (2.0f * px) / float(g_vpH); } 

        
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
            if (!g_progRounded) {
                GLuint vs = compile(GL_VERTEX_SHADER, VS_R);
                GLuint fs = compile(GL_FRAGMENT_SHADER, FS_R);
                g_progRounded = link(vs, fs);
            }
            if (!g_progTex) {
                GLuint vs = compile(GL_VERTEX_SHADER, VS_TEX);
                GLuint fs = compile(GL_FRAGMENT_SHADER, FS_TEX);
                g_progTex = link(vs, fs);
            }

            // VAO/VBO for rounded-rect (2D positions)
            if (!g_vaoRounded) {
                glGenVertexArrays(1, &g_vaoRounded);
                glGenBuffers(1, &g_vboRounded);
                glBindVertexArray(g_vaoRounded);
                glBindBuffer(GL_ARRAY_BUFFER, g_vboRounded);
                glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
                glEnableVertexAttribArray(0);
                glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
                glBindVertexArray(0);
                glBindBuffer(GL_ARRAY_BUFFER, 0);
            }

            // VAO/VBOfor text (position + UV)
            if (!g_vaoTex) {
                glGenVertexArrays(1, &g_vaoTex);
                glGenBuffers(1, &g_vboTex);
                glBindVertexArray(g_vaoTex);
                glBindBuffer(GL_ARRAY_BUFFER, g_vboTex);
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
            glUseProgram(g_progRounded);
            glUniform4f(glGetUniformLocation(g_progRounded, "uRect"), x0, y0, x1, y1);
            glUniform1f(glGetUniformLocation(g_progRounded, "uRadius"), radiusNDC);
            glUniform4f(glGetUniformLocation(g_progRounded, "uColor"), c.r, c.g, c.b, c.a);

            glBindVertexArray(g_vaoRounded);
            glBindBuffer(GL_ARRAY_BUFFER, g_vboRounded);
            glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
            glUseProgram(0);
        }

        struct GlyphTex { GLuint tex = 0; int w = 0, h = 0; };
        std::unordered_map<std::string, GlyphTex> g_textCache;

        GLuint surfaceToTexture(SDL_Surface* s) {
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

        GlyphTex bakeTextTexture(const std::string& text, int px, SDL_Color color) {
            GlyphTex gt;
            if (!g_font || text.empty()) return gt;


        #if SDL_TTF_VERSION_ATLEAST(2,20,0)
                    TTF_SetFontSize(g_font, px);
        #endif
                    SDL_Surface* surf = TTF_RenderUTF8_Blended(g_font, text.c_str(), color);
                    if (!surf) return gt;
                    gt.tex = surfaceToTexture(surf);
                    gt.w = surf->w; gt.h = surf->h;
                    SDL_FreeSurface(surf);
                    return gt;
                }

        void drawTextureCentered(GLuint tex, int texW, int texH,
            float x0, float y0, float x1, float y1,
            Color mul = { 1,1,1,1 })
        {
            if (!tex) return;

            // slot (px)
            auto ndcToPx = [&](float x, float y) {
                int px = int((x * 0.5f + 0.5f) * g_vpW);
                int py = int((1.0f - (y * 0.5f + 0.5f)) * g_vpH);
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

            auto pxToNdcX = [&](float p) { return (p / float(g_vpW)) * 2.0f - 1.0f; };
            auto pxToNdcY = [&](float p) { return 1.0f - (p / float(g_vpH)) * 2.0f; };

            float qx0 = pxToNdcX(cx - qw * 0.5f);
            float qx1 = pxToNdcX(cx + qw * 0.5f);
            float qy0 = pxToNdcY(cy + qh * 0.5f);
            float qy1 = pxToNdcY(cy - qh * 0.5f);

            float verts[24] = {
                qx0,qy1, 0,1,  qx1,qy1, 1,1,  qx0,qy0, 0,0,
                qx1,qy1, 1,1,  qx1,qy0, 1,0,  qx0,qy0, 0,0
            };

            glUseProgram(g_progTex);
            glUniform4f(glGetUniformLocation(g_progTex, "uColor"), mul.r, mul.g, mul.b, mul.a);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glUniform1i(glGetUniformLocation(g_progTex, "uTex"), 0);

            glBindVertexArray(g_vaoTex);
            glBindBuffer(GL_ARRAY_BUFFER, g_vboTex);
            glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_DYNAMIC_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUseProgram(0);

        }

        void drawTextCentered(const std::string& text, float x0, float y0, float x1, float y1,
            Color col = { 1,1,1,1 })
        {
            if (!g_font || text.empty()) return;

            auto ndcToPxY = [&](float y) {
                return int((1.0f - (y * 0.5f + 0.5f)) * g_vpH);
                };
            int rectHpx = ndcToPxY(y0) - ndcToPxY(y1);
            int fontPx = std::max(12, int(rectHpx * 0.60f));

            SDL_Color sdlCol = { Uint8(col.r * 255), Uint8(col.g * 255), Uint8(col.b * 255), Uint8(col.a * 255) };
            std::string key = text + "@" + std::to_string(fontPx);
            GlyphTex gt{};
            auto it = g_textCache.find(key);
            if (it == g_textCache.end()) {
                gt = bakeTextTexture(text, fontPx, sdlCol);
                if (gt.tex) g_textCache.emplace(key, gt);
            }
            else {
                gt = it->second;
            }
            if (gt.tex) drawTextureCentered(gt.tex, gt.w, gt.h, x0, y0, x1, y1, { 1,1,1,1 });
        }

        // LAYOUT
        void computeBars(float& tX0, float& tX1, float& tY0, float& tY1,
            float& bX0, float& bX1, float& bY0, float& bY1)
        {
            const float iy = std::clamp(g_style.insetY, 0.0f, 0.20f);
            const float it = std::clamp(g_style.topInsetX, 0.0f, 0.45f);
            const float ib = std::clamp(g_style.botInsetX, 0.0f, 0.45f);

            tX0 = -1.0f + it; tX1 = 1.0f - it;
            tY0 = g_style.topY0 - iy; tY1 = g_style.topY1 - iy;

            bX0 = -1.0f + ib; bX1 = 1.0f - ib;
            bY0 = g_style.botY0 + iy; bY1 = g_style.botY1 + iy;
        }

        void drawTopBar(float x0, float y0, float x1, float y1) {
            drawRoundedRect(x0, y0, x1, y1, g_style.barColor, pxToNDC(g_style.barRadiusPx));

            // layout of tcikets
            const float padY = pxToNDC(g_style.pillsPadYPx);
            const float padX = pxToNDC(g_style.pillsPadXPx);
            const float gap = pxToNDC(g_style.pillsGapPx);

            float innerX0 = x0 + gap;
            float innerX1 = x1 - gap;
            float innerY0 = y0 + padY;
            float innerY1 = y1 - padY;
            float h = innerY1 - innerY0;

            float cur = innerX0;

            for (size_t i = 0; i < g_pills.size(); ++i) {
                const std::string& txt = g_pills[i];
                const Color pc = g_pillColors[i < g_pillColors.size() ? i : 0];

                float minCapW = h * 1.8f;
                float approxW = minCapW + gap;

                float capX0 = cur;
                float capX1 = std::min(cur + approxW, innerX1);
                if (capX1 <= capX0) break;

                drawRoundedRect(capX0, innerY0, capX1, innerY1, { pc.r,pc.g,pc.b, 1.0f }, pxToNDC(g_style.slotRadiusPx));

                // text inside (not working yet)
                drawTextCentered(txt, capX0 + padX * 0.5f, innerY0, capX1 - padX * 0.5f, innerY1, g_style.textColor);

                cur = capX1 + gap;
                if (cur >= innerX1) break;
            }
        }

        void drawBottomBar(float x0, float y0, float x1, float y1) {
            drawRoundedRect(x0, y0, x1, y1, g_style.barColor, pxToNDC(g_style.barRadiusPx));

            // slots from 1..24 (for tickets of Mr X)
            const int   N = 24;
            const float gap = pxToNDC(10.0f);
            const float margin = pxToNDC(6.0f);

            float sx0 = x0 + gap;
            float sx1 = x1 - gap;
            float sy0 = y0 + gap * 0.6f;
            float sy1 = y1 - gap * 0.6f;

            float totalW = (sx1 - sx0) - (N - 1) * margin;
            float slotW = totalW / float(N);

            float x = sx0;
            for (int i = 0; i < N; ++i) {
                Color c = g_slots[i].color;
                if (c.a < 0.0f) c = g_style.slotColor;

                drawRoundedRect(x, sy0, x + slotW, sy1, c, pxToNDC(g_style.slotRadiusPx));

                // number (not working, todo)
                char buf[4]; std::snprintf(buf, sizeof(buf), "%d", i + 1);
                drawTextCentered(buf, x, sy0, x + slotW, sy1, g_style.textColor);

                x += slotW + margin;
            }
        }

    } // namespace

    // API
    bool InitHUD() {
        ensurePipelines();
        return g_progRounded && g_progTex;
    }

    void ShutdownHUD() {
        for (auto& kv : g_textCache) {
            if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
        }
        g_textCache.clear();

        if (g_vboRounded) { glDeleteBuffers(1, &g_vboRounded); g_vboRounded = 0; }
        if (g_vaoRounded) { glDeleteVertexArrays(1, &g_vaoRounded); g_vaoRounded = 0; }
        if (g_vboTex) { glDeleteBuffers(1, &g_vboTex);     g_vboTex = 0; }
        if (g_vaoTex) { glDeleteVertexArrays(1, &g_vaoTex); g_vaoTex = 0; }

        g_font = nullptr;
    }

    void SetViewport(int w, int h) {
        g_vpW = std::max(1, w);
        g_vpH = std::max(1, h);
    }

    void SetHUDStyle(const HUDStyle& style) { g_style = style; }

    void SetFont(TTF_Font* font) { g_font = font; }

    void SetTicketStates(const std::vector<TicketSlot>& slots) {
        g_slots = slots;
        if (g_slots.size() < 24) g_slots.resize(24);
        if (g_slots.size() > 24) g_slots.resize(24);
    }

    void SetTopBar(const std::vector<std::string>& labels,
        const std::vector<Color>& pillColors)
    {
        if (!labels.empty()) g_pills = labels;
        if (!pillColors.empty()) g_pillColors = pillColors;
    }

    void RenderHUD() {
        if (!g_progRounded || !g_progTex) if (!InitHUD()) return;

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
