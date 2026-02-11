#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "rendering/framebuffer.hpp"
#include "rendering/mask_buffer.hpp"
#include "rendering/primitives.hpp"
#include "rendering/patterns.hpp"
#include "rendering/bezier.hpp"
#include "rendering/vector_font.hpp"
#include "rendering/animation.hpp"
#include "st7305.hpp"

static const char* TAG = "main";

using namespace rendering;

// Get current time in seconds
static float getTime() {
    return static_cast<float>(esp_timer_get_time()) / 1000000.0f;
}

// Demo 1: Primitives
static void demoPrimitives(IFramebuffer& fb) {
    ESP_LOGI(TAG, "Demo: Primitives");

    fb.clear(WHITE);

    // Draw lines
    for (int i = 0; i < 10; i++) {
        drawLine(fb, 10 + i * 10, 10, 10 + i * 10, 50, BLACK);
    }

    // Draw rectangles
    drawRect(fb, 120, 10, 60, 40, BLACK);
    fillRect(fb, 130, 20, 40, 20, BLACK);

    // Draw circles
    drawCircle(fb, 250, 30, 25, BLACK);
    fillCircle(fb, 320, 30, 20, BLACK);

    // Draw polygon
    Point pentagon[] = {
        {50, 100}, {80, 80}, {110, 100}, {100, 130}, {60, 130}
    };
    drawPolygon(fb, pentagon, 5, BLACK);

    // Fill polygon
    Point hexagon[] = {
        {180, 80}, {210, 90}, {220, 120}, {200, 140}, {170, 130}, {160, 100}
    };
    fillPolygon(fb, hexagon, 6, BLACK);
}

// Demo 2: Patterns
static void demoPatterns(IFramebuffer& fb) {
    ESP_LOGI(TAG, "Demo: Patterns");

    fb.clear(WHITE);

    // Draw pattern rectangles
    int x = 20;
    int w = 60;
    int h = 50;
    int spacing = 70;

    fillRectPattern(fb, x, 20, w, h, Pattern::SolidBlack);
    x += spacing;
    fillRectPattern(fb, x, 20, w, h, Pattern::Dense);
    x += spacing;
    fillRectPattern(fb, x, 20, w, h, Pattern::Medium);
    x += spacing;
    fillRectPattern(fb, x, 20, w, h, Pattern::Sparse);
    x += spacing;
    fillRectPattern(fb, x, 20, w, h, Pattern::SolidWhite);
    drawRect(fb, x, 20, w, h, BLACK);

    // Pattern circles
    fillCirclePattern(fb, 60, 150, 40, Pattern::Dense);
    fillCirclePattern(fb, 150, 150, 40, Pattern::Medium);
    fillCirclePattern(fb, 240, 150, 40, Pattern::Sparse);

    // Pattern polygon
    Point diamond[] = {
        {350, 120}, {380, 150}, {350, 180}, {320, 150}
    };
    fillPolygonPattern(fb, diamond, 4, Pattern::Medium);
}

// Demo 3: Bezier curves
static void demoBezier(IFramebuffer& fb) {
    ESP_LOGI(TAG, "Demo: Bezier");

    fb.clear(WHITE);

    // Simple curve
    PointF curve1[] = {
        {20, 50}, {100, 20}, {180, 80}, {260, 40}
    };
    drawBezierCurve(fb, curve1, 4, 0.5f, BLACK, 1.0f);

    // Texture ball stroke
    PointF curve2[] = {
        {20, 150}, {120, 100}, {220, 180}, {320, 130}, {380, 160}
    };
    strokeBezierTextureBall(fb, curve2, 5, 0.5f, 3.0f);

    // Another curve with different smoothness
    PointF curve3[] = {
        {20, 250}, {80, 200}, {160, 280}, {240, 210}, {320, 260}, {380, 220}
    };
    strokeBezierTextureBall(fb, curve3, 6, 0.3f, 4.0f);
}

// Demo 4: Vector font
static void demoFont(IFramebuffer& fb) {
    ESP_LOGI(TAG, "Demo: Vector Font");

    fb.clear(WHITE);

    // Numbers
    renderString(fb, "0123456789", 20, 20, 30, 40, 5, 2, BLACK);

    // Alphabet
    renderString(fb, "ABCDEFGHIJKLM", 20, 80, 24, 32, 4, 2, BLACK);
    renderString(fb, "NOPQRSTUVWXYZ", 20, 120, 24, 32, 4, 2, BLACK);

    // Special characters
    renderString(fb, "12:34 -50.7%", 20, 170, 20, 28, 4, 2, BLACK);

    // Centered text
    renderStringCentered(fb, "CENTERED", 200, 220, 24, 32, 4, 3, BLACK);

    // Right-aligned text
    renderStringRight(fb, "RIGHT", 380, 260, 20, 28, 4, 2, BLACK);
}

// Demo 5: Animation
static void demoAnimation(IFramebuffer& fb, st7305::Display& display) {
    ESP_LOGI(TAG, "Demo: Animation");

    AnimationState anim(getTime());

    // Animated circle with breathing effect
    for (int frame = 0; frame < 100; frame++) {
        anim.update(getTime());

        fb.clear(WHITE);

        // Breathing circle
        float scale = anim.breathingScale(0.8f, 1.2f, 2.0f);
        int16_t radius = static_cast<int16_t>(30 * scale);
        fillCircle(fb, 100, 100, radius, BLACK);

        // Breathing offset
        float offset = anim.breathingOffset(20.0f, 1.5f);
        fillCircle(fb, static_cast<int16_t>(250 + offset), 100, 25, BLACK);

        // Wiggling polygon
        Point baseHex[] = {
            {180, 200}, {210, 190}, {230, 210}, {220, 240}, {190, 250}, {170, 230}
        };
        Point wiggledHex[6];
        wigglePoints(baseHex, 6, wiggledHex, 3.0f, 5.0f, anim.elapsed(), 12345);
        fillPolygon(fb, wiggledHex, 6, BLACK);

        // Transitioning shapes (circle to square-ish)
        float progress = anim.progress(3.0f);  // 3 second transition
        PointF shapeA[] = {
            {320, 180}, {350, 200}, {340, 230}, {310, 230}, {300, 200}
        };
        PointF shapeB[] = {
            {300, 180}, {350, 180}, {350, 240}, {300, 240}, {300, 200}
        };
        PointF transitioned[5];
        transitionPoints(shapeA, shapeB, 5, transitioned, progress, easeInOut);

        Point transitionedInt[5];
        for (int i = 0; i < 5; i++) {
            transitionedInt[i] = transitioned[i].toPoint();
        }
        fillPolygon(fb, transitionedInt, 5, BLACK);

        // Frame counter
        char frameStr[16];
        snprintf(frameStr, sizeof(frameStr), "F:%d", frame);
        renderString(fb, frameStr, 10, 270, 16, 20, 3, 1, BLACK);

        display.show(fb);
        vTaskDelay(pdMS_TO_TICKS(33));  // ~30 FPS
    }
}

// Demo 6: Mask buffer clipping
static void demoMaskBuffer(IFramebuffer& fb, st7305::Display& display) {
    ESP_LOGI(TAG, "Demo: Mask Buffer");

    MaskBuffer400x300 mask;

    // Test 1: Circle mask - only draw inside circle
    mask.clear(WHITE);  // WHITE = blocked everywhere
    fillCircle(mask, 200, 150, 100, BLACK);  // BLACK = allowed

    fb.setMask(&mask);
    fb.clear(WHITE);

    // Draw pattern that will be clipped to circle
    fillRectPattern(fb, 0, 0, 400, 300, Pattern::Medium);

    // Add label outside mask area (won't be clipped because we disable mask)
    fb.setMask(nullptr);
    renderString(fb, "CIRCLE MASK", 120, 260, 18, 24, 3, 2, BLACK);

    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 2: Inverted mask - draw outside circle (cutout effect)
    mask.invert();
    fb.setMask(&mask);
    fb.clear(WHITE);
    fillRectPattern(fb, 0, 0, 400, 300, Pattern::Dense);

    fb.setMask(nullptr);
    renderString(fb, "CUTOUT", 165, 145, 18, 24, 3, 2, BLACK);

    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Test 3: Polygon mask
    mask.clear(WHITE);
    Point star[] = {
        {200, 50}, {230, 120}, {300, 130}, {250, 180},
        {270, 250}, {200, 210}, {130, 250}, {150, 180},
        {100, 130}, {170, 120}
    };
    fillPolygon(mask, star, 10, BLACK);

    fb.setMask(&mask);
    fb.clear(WHITE);
    fillRectPattern(fb, 0, 0, 400, 300, Pattern::Sparse);

    fb.setMask(nullptr);
    renderString(fb, "STAR MASK", 135, 270, 18, 24, 3, 2, BLACK);

    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Cleanup
    fb.setMask(nullptr);
}

// Demo 7: Lowercase letters
static void demoLowercase(IFramebuffer& fb) {
    ESP_LOGI(TAG, "Demo: Lowercase Letters");

    fb.clear(WHITE);

    // Full lowercase alphabet
    renderString(fb, "abcdefghijklm", 10, 10, 22, 32, 2, 2, BLACK);
    renderString(fb, "nopqrstuvwxyz", 10, 50, 22, 32, 2, 2, BLACK);

    // Mixed case examples
    renderString(fb, "Hello World", 10, 100, 20, 28, 3, 2, BLACK);
    renderString(fb, "ESP32-S3 Demo", 10, 135, 20, 28, 3, 2, BLACK);

    // Descenders test (g, j, p, q, y)
    renderString(fb, "gyp jumping joy", 10, 180, 18, 26, 2, 2, BLACK);

    // Quick brown fox
    renderString(fb, "The quick brown", 10, 220, 16, 22, 2, 2, BLACK);
    renderString(fb, "fox jumps lazy", 10, 250, 16, 22, 2, 2, BLACK);
}

// Run all demos
static void runDemos(IFramebuffer& fb, st7305::Display& display) {
    // Primitives
    demoPrimitives(fb);
    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Patterns
    demoPatterns(fb);
    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Bezier
    demoBezier(fb);
    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Font
    demoFont(fb);
    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Animation
    demoAnimation(fb, display);

    // Mask buffer
    demoMaskBuffer(fb, display);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Lowercase letters
    demoLowercase(fb);
    display.show(fb);
    vTaskDelay(pdMS_TO_TICKS(2000));
}

extern "C" void app_main() {
    ESP_LOGI(TAG, "ESP32-S3 Rendering Toolkit Demo");

    // Initialize framebuffer
    Framebuffer400x300 fb;
    if (!fb.buffer()) {
        ESP_LOGE(TAG, "Failed to create framebuffer");
        return;
    }

    // Initialize display
    st7305::Config displayConfig;
    st7305::Display display(displayConfig);
    display.init();

    ESP_LOGI(TAG, "Running demos...");

    while (true) {
        runDemos(fb, display);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
