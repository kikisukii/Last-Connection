#include <raylib.h>
#include <cstring>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace {
float elapsed = 0.0f;
void frame() {
    elapsed += GetFrameTime();
    BeginDrawing();
    ClearBackground(Color{18, 24, 33, 255});
    DrawText("LAST CONNECTION", 64, 60, 38, RAYWHITE);
    DrawText("PHASE 1 / C++20 + raylib", 64, 115, 22, LIGHTGRAY);
    DrawRectanglePro(Rectangle{480, 280, 90, 90}, Vector2{45, 45}, elapsed * 45.0f,
                     Color{104, 226, 180, 255});
    DrawText("Native / WebAssembly smoke test", 64, 410, 24, RAYWHITE);
    DrawText(TextFormat("Render loop: %.2f s", elapsed), 64, 450, 20, LIGHTGRAY);
    EndDrawing();
}
}

int main(int argc, char** argv) {
    InitWindow(960, 540, "LAST CONNECTION - Phase 1");
#ifdef __EMSCRIPTEN__
    (void)argc;
    (void)argv;
    emscripten_set_main_loop(frame, 0, 1);
#else
    SetTargetFPS(60);
    const bool smoke = argc > 1 && std::strcmp(argv[1], "--smoke") == 0;
    int frames = 0;
    while (!WindowShouldClose() && (!smoke || frames < 120)) {
        frame();
        ++frames;
    }
    if (smoke) TakeScreenshot("smoke.png");
    CloseWindow();
#endif
    return 0;
}
