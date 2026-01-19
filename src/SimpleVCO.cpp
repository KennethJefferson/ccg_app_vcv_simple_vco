#include "plugin.hpp"

struct SimpleVCO : Module {
    enum ParamId {
        FREQ_PARAM,
        FINE_PARAM,
        FM_PARAM,
        PARAMS_LEN
    };
    enum InputId {
        VOCT_INPUT,
        FM_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        SINE_OUTPUT,
        TRI_OUTPUT,
        SAW_OUTPUT,
        SQUARE_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        LIGHTS_LEN
    };

    float phase = 0.f;

    SimpleVCO() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(FREQ_PARAM, -3.f, 3.f, 0.f, "Frequency", " octaves");
        configParam(FINE_PARAM, -0.5f, 0.5f, 0.f, "Fine tune", " semitones", 0.f, 12.f);
        configParam(FM_PARAM, -1.f, 1.f, 0.f, "FM amount", "%", 0.f, 100.f);
        configInput(VOCT_INPUT, "V/Oct");
        configInput(FM_INPUT, "FM");
        configOutput(SINE_OUTPUT, "Sine");
        configOutput(TRI_OUTPUT, "Triangle");
        configOutput(SAW_OUTPUT, "Saw");
        configOutput(SQUARE_OUTPUT, "Square");
    }

    void process(const ProcessArgs& args) override {
        float freqParam = params[FREQ_PARAM].getValue();
        float fineParam = params[FINE_PARAM].getValue();
        float fmParam = params[FM_PARAM].getValue();

        float pitch = freqParam + fineParam / 12.f;
        pitch += inputs[VOCT_INPUT].getVoltage();
        pitch += inputs[FM_INPUT].getVoltage() * fmParam;

        float freq = dsp::FREQ_C4 * std::pow(2.f, pitch);
        freq = clamp(freq, 0.f, args.sampleRate / 2.f);

        phase += freq * args.sampleTime;
        if (phase >= 1.f)
            phase -= 1.f;

        // Generate waveforms
        if (outputs[SINE_OUTPUT].isConnected()) {
            float sine = std::sin(2.f * M_PI * phase);
            outputs[SINE_OUTPUT].setVoltage(5.f * sine);
        }

        if (outputs[TRI_OUTPUT].isConnected()) {
            float tri = 4.f * std::abs(phase - 0.5f) - 1.f;
            outputs[TRI_OUTPUT].setVoltage(5.f * tri);
        }

        if (outputs[SAW_OUTPUT].isConnected()) {
            float saw = 2.f * phase - 1.f;
            outputs[SAW_OUTPUT].setVoltage(5.f * saw);
        }

        if (outputs[SQUARE_OUTPUT].isConnected()) {
            float square = phase < 0.5f ? 1.f : -1.f;
            outputs[SQUARE_OUTPUT].setVoltage(5.f * square);
        }
    }
};

struct SimpleVCOWidget : ModuleWidget {
    SimpleVCOWidget(SimpleVCO* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/SimpleVCO.svg")));

        // Screws
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // Knobs - positioned in center column
        addParam(createParamCentered<RoundBigBlackKnob>(mm2px(Vec(15.24, 30)), module, SimpleVCO::FREQ_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15.24, 50)), module, SimpleVCO::FINE_PARAM));
        addParam(createParamCentered<RoundSmallBlackKnob>(mm2px(Vec(15.24, 65)), module, SimpleVCO::FM_PARAM));

        // Inputs
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(7.62, 85)), module, SimpleVCO::VOCT_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(22.86, 85)), module, SimpleVCO::FM_INPUT));

        // Outputs - bottom row
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 100)), module, SimpleVCO::SINE_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 100)), module, SimpleVCO::TRI_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(7.62, 115)), module, SimpleVCO::SAW_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(22.86, 115)), module, SimpleVCO::SQUARE_OUTPUT));
    }
};

Model* modelSimpleVCO = createModel<SimpleVCO, SimpleVCOWidget>("SimpleVCO");
