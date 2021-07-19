#ifndef FULLSCREEN_QUAD_H
#define FULLSCREEN_QUAD_H

#include "core/resources/mesh.h"
#include "core/resources/texture.h"
#include "core/render/shader.h"

class FullscreenQuad {

public:

    struct DrawTexturedParam {
        bool toneMap = false;
        bool gamma = false;

        float exposure = 1.0f;
        float gammaVal = 2.2f;

        DrawTexturedParam() { }
    };

    // Draw the quad, simply bind and submit geometry. No render state is affected
    static void draw();

    // Bind a shader to render a fullscreen texture and draw the quad. Only affects active shader state
    static void drawTextured(const Texture* pTexture, DrawTexturedParam param = DrawTexturedParam());

private:

    static Shader* textureShader;
    static Mesh* fullscreenQuad;

    static bool initialized;

    static void initialize();

};

#endif // FULLSCREEN_QUAD_H
