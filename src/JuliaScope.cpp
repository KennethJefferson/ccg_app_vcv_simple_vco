#include "plugin.hpp"
#include <cmath>
#include <algorithm>

static const int RING_BUFFER_SIZE = 2048;
static const int DISPLAY_WIDTH = 200;
static const int DISPLAY_HEIGHT = 155;

struct JuliaScope : Module {
    enum ParamId {
        C_REAL_PARAM,
        C_IMAG_PARAM,
        ZOOM_PARAM,
        ITER_PARAM,
        COLOR_PARAM,
        MOD_PARAM,
        SPEED_PARAM,
        TILT_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        LEFT_INPUT,
        RIGHT_INPUT,
        RE_CV_INPUT,
        IM_CV_INPUT,
        ZOOM_CV_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    // Ring buffer for audio samples
    float leftBuffer[RING_BUFFER_SIZE] = {};
    float rightBuffer[RING_BUFFER_SIZE] = {};
    int bufferIndex = 0;

    // Envelope followers
    float leftEnvelope = 0.f;
    float rightEnvelope = 0.f;

    // Pitch detection
    float lastSample = 0.f;
    int zeroCrossings = 0;
    int samplesSinceReset = 0;
    float detectedFreq = 440.f;
    float smoothFreq = 440.f;
    static const int FREQ_WINDOW = 2048;

    // Smoothed modulation values
    float smoothCReal = -0.7f;
    float smoothCImag = 0.27015f;
    float smoothZoom = 1.f;
    float smoothTilt = 0.f;

    // Frame counter for display update
    int frameCounter = 0;

    // Pixel buffer for fractal (RGBA)
    uint8_t pixels[DISPLAY_WIDTH * DISPLAY_HEIGHT * 4];
    bool pixelsDirty = true;

    float sampleRate = 44100.f;

    JuliaScope() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(C_REAL_PARAM, -2.f, 2.f, -0.7f, "C Real");
        configParam(C_IMAG_PARAM, -2.f, 2.f, 0.27015f, "C Imaginary");
        configParam(ZOOM_PARAM, 0.5f, 4.f, 1.f, "Zoom");
        configParam(ITER_PARAM, 16.f, 256.f, 64.f, "Max Iterations");
        configParam(COLOR_PARAM, 0.f, 4.f, 0.f, "Color Palette");
        configParam(MOD_PARAM, 0.f, 1.f, 0.5f, "Modulation Depth", "%", 0.f, 100.f);
        configParam(SPEED_PARAM, 0.01f, 1.f, 0.1f, "Response Speed");
        configParam(TILT_PARAM, 0.f, 1.f, 0.5f, "Frequency->Tilt", "%", 0.f, 100.f);

        configInput(LEFT_INPUT, "Left Audio");
        configInput(RIGHT_INPUT, "Right Audio");
        configInput(RE_CV_INPUT, "C Real CV");
        configInput(IM_CV_INPUT, "C Imaginary CV");
        configInput(ZOOM_CV_INPUT, "Zoom CV");

        // Initialize pixel buffer to black
        std::memset(pixels, 0, sizeof(pixels));
    }

    void process(const ProcessArgs& args) override {
        sampleRate = args.sampleRate;

        // Get audio input (right normalizes to left)
        float leftSample = inputs[LEFT_INPUT].getVoltage() / 5.f;
        float rightSample = inputs[RIGHT_INPUT].isConnected() ?
            inputs[RIGHT_INPUT].getVoltage() / 5.f : leftSample;

        // Store in ring buffer
        leftBuffer[bufferIndex] = leftSample;
        rightBuffer[bufferIndex] = rightSample;
        bufferIndex = (bufferIndex + 1) % RING_BUFFER_SIZE;

        // Zero-crossing pitch detection
        if ((lastSample <= 0.f && leftSample > 0.f) || (lastSample >= 0.f && leftSample < 0.f)) {
            zeroCrossings++;
        }
        lastSample = leftSample;
        samplesSinceReset++;

        if (samplesSinceReset >= FREQ_WINDOW) {
            // Calculate frequency from zero crossings
            // Each cycle has 2 zero crossings
            float freq = (zeroCrossings / 2.f) * (args.sampleRate / (float)FREQ_WINDOW);
            freq = clamp(freq, 20.f, 5000.f);
            detectedFreq = freq;
            zeroCrossings = 0;
            samplesSinceReset = 0;
        }

        // Envelope follower with adjustable speed
        float speed = params[SPEED_PARAM].getValue();
        float attackTime = 0.001f + (1.f - speed) * 0.1f;
        float releaseTime = 0.01f + (1.f - speed) * 0.5f;
        float attackCoeff = 1.f - std::exp(-args.sampleTime / attackTime);
        float releaseCoeff = 1.f - std::exp(-args.sampleTime / releaseTime);

        float leftAbs = std::abs(leftSample);
        float rightAbs = std::abs(rightSample);

        if (leftAbs > leftEnvelope)
            leftEnvelope += attackCoeff * (leftAbs - leftEnvelope);
        else
            leftEnvelope += releaseCoeff * (leftAbs - leftEnvelope);

        if (rightAbs > rightEnvelope)
            rightEnvelope += attackCoeff * (rightAbs - rightEnvelope);
        else
            rightEnvelope += releaseCoeff * (rightAbs - rightEnvelope);

        // Get base parameters
        float baseCReal = params[C_REAL_PARAM].getValue();
        float baseCImag = params[C_IMAG_PARAM].getValue();
        float baseZoom = params[ZOOM_PARAM].getValue();

        // Add CV modulation
        baseCReal += inputs[RE_CV_INPUT].getVoltage() * 0.2f;
        baseCImag += inputs[IM_CV_INPUT].getVoltage() * 0.2f;
        baseZoom += inputs[ZOOM_CV_INPUT].getVoltage() * 0.1f;
        baseZoom = clamp(baseZoom, 0.1f, 10.f);

        // Add audio modulation
        float modDepth = params[MOD_PARAM].getValue();
        float targetCReal = baseCReal + leftEnvelope * modDepth * 0.5f;
        float targetCImag = baseCImag + rightEnvelope * modDepth * 0.5f;

        // Map frequency to tilt angle (-1 to 1)
        // Log scale: 20Hz -> -1, ~450Hz -> 0, 5000Hz -> 1
        float freqNorm = (std::log(detectedFreq) - std::log(20.f)) / (std::log(5000.f) - std::log(20.f));
        freqNorm = clamp(freqNorm, 0.f, 1.f);
        float tiltAmount = params[TILT_PARAM].getValue();
        float targetTilt = (freqNorm * 2.f - 1.f) * tiltAmount;

        // Smooth the values
        float smoothCoeff = 1.f - std::exp(-args.sampleTime / (0.01f + (1.f - speed) * 0.1f));
        smoothCReal += smoothCoeff * (targetCReal - smoothCReal);
        smoothCImag += smoothCoeff * (targetCImag - smoothCImag);
        smoothZoom += smoothCoeff * (baseZoom - smoothZoom);
        smoothTilt += smoothCoeff * (targetTilt - smoothTilt);
        smoothFreq += smoothCoeff * (detectedFreq - smoothFreq);

        // Mark pixels dirty (actual rendering happens in widget)
        pixelsDirty = true;
    }

    float getCReal() { return smoothCReal; }
    float getCImag() { return smoothCImag; }
    float getZoom() { return smoothZoom; }
    float getTilt() { return smoothTilt; }
    float getFreq() { return smoothFreq; }
    int getMaxIter() { return (int)params[ITER_PARAM].getValue(); }
    int getColorMode() { return (int)params[COLOR_PARAM].getValue(); }
};

// Color palette functions
struct ColorPalette {
    static void getColor(int mode, int iter, int maxIter, float brightness, uint8_t& r, uint8_t& g, uint8_t& b) {
        if (iter >= maxIter) {
            r = g = b = 0;
            return;
        }

        float t = (float)iter / (float)maxIter;

        float br = 0.f, bg = 0.f, bb = 0.f;

        switch (mode) {
            case 0: // Classic blue-white
                br = 9 * (1 - t) * t * t * t;
                bg = 15 * (1 - t) * (1 - t) * t * t;
                bb = 8.5f * (1 - t) * (1 - t) * (1 - t) * t;
                break;
            case 1: // Fire
                br = std::min(1.f, t * 3.f);
                bg = std::max(0.f, std::min(1.f, t * 3.f - 1.f));
                bb = std::max(0.f, t * 3.f - 2.f);
                break;
            case 2: // Ocean
                br = t * t;
                bg = t;
                bb = std::sqrt(t);
                break;
            case 3: // Rainbow
                {
                    float h = t * 6.f;
                    int i = (int)h;
                    float f = h - i;
                    switch (i % 6) {
                        case 0: br = 1.f; bg = f; bb = 0.f; break;
                        case 1: br = 1.f - f; bg = 1.f; bb = 0.f; break;
                        case 2: br = 0.f; bg = 1.f; bb = f; break;
                        case 3: br = 0.f; bg = 1.f - f; bb = 1.f; break;
                        case 4: br = f; bg = 0.f; bb = 1.f; break;
                        case 5: br = 1.f; bg = 0.f; bb = 1.f - f; break;
                    }
                }
                break;
            case 4: // Grayscale
            default:
                br = bg = bb = t;
                break;
        }

        // Apply brightness (3D lighting)
        br *= brightness;
        bg *= brightness;
        bb *= brightness;

        r = (uint8_t)(clamp(br, 0.f, 1.f) * 255);
        g = (uint8_t)(clamp(bg, 0.f, 1.f) * 255);
        b = (uint8_t)(clamp(bb, 0.f, 1.f) * 255);
    }
};

struct JuliaScopeDisplay : TransparentWidget {
    JuliaScope* module = nullptr;
    int nvgImage = -1;
    int frameSkip = 0;
    static const int FRAME_SKIP_COUNT = 2;

    uint8_t localPixels[DISPLAY_WIDTH * DISPLAY_HEIGHT * 4];

    // Height buffer for 3D effect
    float heightMap[DISPLAY_WIDTH * DISPLAY_HEIGHT];

    JuliaScopeDisplay() {
        box.size = mm2px(Vec(70.f, 54.f));
        std::memset(localPixels, 0, sizeof(localPixels));
        std::memset(heightMap, 0, sizeof(heightMap));
    }

    ~JuliaScopeDisplay() {
        // Image cleanup handled by NanoVG context
    }

    void renderJuliaSet() {
        if (!module) return;

        float cReal = module->getCReal();
        float cImag = module->getCImag();
        float zoom = module->getZoom();
        float tilt = module->getTilt();
        int maxIter = module->getMaxIter();
        int colorMode = module->getColorMode();

        float aspectRatio = (float)DISPLAY_WIDTH / (float)DISPLAY_HEIGHT;
        float xMin = -2.f / zoom * aspectRatio;
        float xMax = 2.f / zoom * aspectRatio;
        float yMin = -2.f / zoom;
        float yMax = 2.f / zoom;

        // Tilt creates a perspective transformation
        // tilt = -1: looking from bottom, tilt = 1: looking from top
        float tiltAngle = tilt * 0.5f; // radians

        // First pass: compute iteration counts (height map)
        for (int py = 0; py < DISPLAY_HEIGHT; py++) {
            for (int px = 0; px < DISPLAY_WIDTH; px++) {
                // Apply perspective based on tilt
                float normY = (float)py / (float)DISPLAY_HEIGHT;
                float perspectiveScale = 1.f + tiltAngle * (normY - 0.5f) * 0.5f;

                float x0 = xMin + (xMax - xMin) * px / (float)DISPLAY_WIDTH;
                float y0 = yMin + (yMax - yMin) * py / (float)DISPLAY_HEIGHT;

                // Apply perspective to y
                y0 *= perspectiveScale;

                float x = x0;
                float y = y0;
                int iter = 0;

                while (x * x + y * y <= 4.f && iter < maxIter) {
                    float xTemp = x * x - y * y + cReal;
                    y = 2.f * x * y + cImag;
                    x = xTemp;
                    iter++;
                }

                heightMap[py * DISPLAY_WIDTH + px] = (float)iter / (float)maxIter;
            }
        }

        // Second pass: compute normals and apply lighting
        float lightDir[3] = {-0.3f - tilt * 0.5f, -0.5f, 1.f};
        float lightLen = std::sqrt(lightDir[0]*lightDir[0] + lightDir[1]*lightDir[1] + lightDir[2]*lightDir[2]);
        lightDir[0] /= lightLen;
        lightDir[1] /= lightLen;
        lightDir[2] /= lightLen;

        for (int py = 0; py < DISPLAY_HEIGHT; py++) {
            for (int px = 0; px < DISPLAY_WIDTH; px++) {
                float h = heightMap[py * DISPLAY_WIDTH + px];
                int iter = (int)(h * maxIter);

                // Compute normal from height map gradient
                float hL = px > 0 ? heightMap[py * DISPLAY_WIDTH + (px-1)] : h;
                float hR = px < DISPLAY_WIDTH-1 ? heightMap[py * DISPLAY_WIDTH + (px+1)] : h;
                float hU = py > 0 ? heightMap[(py-1) * DISPLAY_WIDTH + px] : h;
                float hD = py < DISPLAY_HEIGHT-1 ? heightMap[(py+1) * DISPLAY_WIDTH + px] : h;

                float nx = (hL - hR) * 2.f;
                float ny = (hU - hD) * 2.f;
                float nz = 0.1f;
                float nLen = std::sqrt(nx*nx + ny*ny + nz*nz);
                nx /= nLen; ny /= nLen; nz /= nLen;

                // Diffuse lighting
                float diffuse = std::max(0.f, nx * lightDir[0] + ny * lightDir[1] + nz * lightDir[2]);

                // Ambient + diffuse
                float brightness = 0.3f + 0.7f * diffuse;

                // Add specular highlight for high iteration areas
                if (h > 0.5f) {
                    float spec = std::pow(diffuse, 8.f) * 0.5f;
                    brightness += spec;
                }

                int idx = (py * DISPLAY_WIDTH + px) * 4;
                uint8_t r = 0, g = 0, b = 0;
                ColorPalette::getColor(colorMode, iter, maxIter, brightness, r, g, b);
                localPixels[idx + 0] = r;
                localPixels[idx + 1] = g;
                localPixels[idx + 2] = b;
                localPixels[idx + 3] = 255;
            }
        }
    }

    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer != 1) return;

        // Frame skip for performance
        frameSkip++;
        if (frameSkip >= FRAME_SKIP_COUNT) {
            frameSkip = 0;
            renderJuliaSet();
        }

        // Create or update NanoVG image
        if (nvgImage == -1) {
            nvgImage = nvgCreateImageRGBA(args.vg, DISPLAY_WIDTH, DISPLAY_HEIGHT, 0, localPixels);
        } else {
            nvgUpdateImage(args.vg, nvgImage, localPixels);
        }

        if (nvgImage != -1) {
            NVGpaint paint = nvgImagePattern(args.vg, 0, 0, box.size.x, box.size.y, 0, nvgImage, 1.0f);
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
            nvgFillPaint(args.vg, paint);
            nvgFill(args.vg);
        }

        // Draw border
        nvgBeginPath(args.vg);
        nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
        nvgStrokeColor(args.vg, nvgRGB(80, 80, 80));
        nvgStrokeWidth(args.vg, 1.f);
        nvgStroke(args.vg);

        // Draw placeholder if no module
        if (!module) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0, 0, box.size.x, box.size.y);
            nvgFillColor(args.vg, nvgRGB(30, 30, 40));
            nvgFill(args.vg);

            nvgFontSize(args.vg, 12.f);
            nvgFillColor(args.vg, nvgRGB(100, 100, 120));
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(args.vg, box.size.x / 2, box.size.y / 2, "JULIA SET 3D", nullptr);
        }
    }
};

struct JuliaScopeWidget : ModuleWidget {
    JuliaScopeWidget(JuliaScope* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/JuliaScope.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Display widget - centered at top
        JuliaScopeDisplay* display = createWidget<JuliaScopeDisplay>(mm2px(Vec(5.64, 14)));
        display->module = module;
        addChild(display);

        // Row 1: C REAL and C IMAG knobs (y=75mm)
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(20.32, 75)), module, JuliaScope::C_REAL_PARAM));
        addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(60.96, 75)), module, JuliaScope::C_IMAG_PARAM));

        // Row 2: ZOOM, ITER, COLOR knobs (y=90mm)
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15.24, 90)), module, JuliaScope::ZOOM_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(40.64, 90)), module, JuliaScope::ITER_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(66.04, 90)), module, JuliaScope::COLOR_PARAM));

        // Row 3: MOD, TILT, SPEED knobs (y=102mm)
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15.24, 102)), module, JuliaScope::MOD_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(40.64, 102)), module, JuliaScope::TILT_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(66.04, 102)), module, JuliaScope::SPEED_PARAM));

        // Inputs row (y=117mm)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10.16, 117)), module, JuliaScope::LEFT_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(23.5, 117)), module, JuliaScope::RIGHT_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(40.64, 117)), module, JuliaScope::RE_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(53.98, 117)), module, JuliaScope::IM_CV_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(71.12, 117)), module, JuliaScope::ZOOM_CV_INPUT));
    }

    void drawLabel(NVGcontext* vg, float x, float y, const char* text, NVGcolor color, float size = 10.f) {
        nvgFontSize(vg, size);
        nvgFillColor(vg, color);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, x, y, text, nullptr);
    }

    void draw(const DrawArgs& args) override {
        ModuleWidget::draw(args);

        NVGcolor white = nvgRGB(170, 170, 170);
        NVGcolor blue = nvgRGB(136, 136, 255);
        NVGcolor green = nvgRGB(153, 255, 153);
        NVGcolor cyan = nvgRGB(102, 204, 255);
        NVGcolor orange = nvgRGB(255, 153, 102);
        NVGcolor gray = nvgRGB(85, 85, 119);

        nvgFontFaceId(args.vg, APP->window->uiFont->handle);

        // Title
        drawLabel(args.vg, mm2px(40.64f), mm2px(7.f), "JULIA SCOPE", blue, 14.f);

        // Row 1 labels
        drawLabel(args.vg, mm2px(20.32f), mm2px(70.f), "C REAL", white, 9.f);
        drawLabel(args.vg, mm2px(60.96f), mm2px(70.f), "C IMAG", white, 9.f);

        // Row 2 labels
        drawLabel(args.vg, mm2px(15.24f), mm2px(85.f), "ZOOM", white, 8.f);
        drawLabel(args.vg, mm2px(40.64f), mm2px(85.f), "ITER", white, 8.f);
        drawLabel(args.vg, mm2px(66.04f), mm2px(85.f), "COLOR", white, 8.f);

        // Row 3 labels
        drawLabel(args.vg, mm2px(15.24f), mm2px(97.f), "MOD", white, 8.f);
        drawLabel(args.vg, mm2px(40.64f), mm2px(97.f), "TILT", green, 8.f);
        drawLabel(args.vg, mm2px(66.04f), mm2px(97.f), "SPEED", white, 8.f);

        // Input labels
        drawLabel(args.vg, mm2px(10.16f), mm2px(112.f), "L IN", cyan, 7.f);
        drawLabel(args.vg, mm2px(23.5f), mm2px(112.f), "R IN", cyan, 7.f);
        drawLabel(args.vg, mm2px(40.64f), mm2px(112.f), "Re CV", orange, 7.f);
        drawLabel(args.vg, mm2px(53.98f), mm2px(112.f), "Im CV", orange, 7.f);
        drawLabel(args.vg, mm2px(71.12f), mm2px(112.f), "Z CV", orange, 7.f);

        // Brand
        drawLabel(args.vg, mm2px(40.64f), mm2px(125.f), "Tony Baloney", gray, 7.f);
    }
};

Model* modelJuliaScope = createModel<JuliaScope, JuliaScopeWidget>("JuliaScope");
