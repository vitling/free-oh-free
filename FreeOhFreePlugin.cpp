/*
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <JuceHeader.h>



#define TAU MathConstants<float>::twoPi
#define PI MathConstants<float>::pi

class OscCycler {
private:
    float angle = 0.0f;
    float frequency = 440.0f;
    float currentSampleRate = 44100;
public:
    void setSampleRate(float sampleRate) {
        currentSampleRate = sampleRate;
    }

    float next() {
        angle += (frequency / currentSampleRate) * TAU;
        if (angle > TAU) angle -= TAU;
        return angle;
    }

    void reset() {
        angle = 0.0f;
    }

    void setFrequency(float freq) {
        frequency = freq;
    }
};

class Envelope {
private:
    double sampleRate = 44100;
    double value = 0.0;
    double decayTime = 0.1;
public:
    void setSampleRate(double _sampleRate) {
        sampleRate = _sampleRate;
    }

    void trigger(double _decayTime) {
        decayTime = _decayTime;
        value = 1.0;
    }

    double next() {
        if (value >= 0.0) {
            value -= (1/(sampleRate * decayTime));
        }
        if (value < 0) value = 0;

        return value;
    }

    bool isActive() {
        return (value != 0.0);
    }
};


class WanderingParameter {
private:
    double value, min, max, scaleFactor, diff, scale;
    Random random;

public:
    WanderingParameter(double min, double max, double scaleFactor):min(min), max(max), scaleFactor(scaleFactor) {
        value = (min + max) / 2;
        diff = 0.0;
        scale = scaleFactor * (max - min);
    }

    double step() {
        diff *= 0.98;
        diff += (random.nextDouble() - 0.5) * scale;
        value += diff;

        if (value > min + 0.8 * (max - min)) {
            diff -= random.nextDouble() * scale;
        } else if (value < min + 0.2 * (max - min)) {
            diff += random.nextDouble() * scale;
        }

        return value;
    }

    double get() {
        return value;
    }
};

inline std::unique_ptr<AudioParameterFloat> param(const String& id, const String& name, float low, float hi) {
    auto interval = pow(10, int(log10((hi - low) / 256)));
    std::cout << "Parameter " << name << " low " << low << " high " << hi << "interval" << interval << "\n";
    return std::make_unique<AudioParameterFloat>(id, name, NormalisableRange<float>(low, hi, interval), (low+hi)/2);
}

std::unique_ptr<AudioProcessorParameterGroup> wanderable(const String& id, const String& name, float low, float hi) {
    return std::make_unique<AudioProcessorParameterGroup>(
            id,
            name,
            "---",
            param(id + "_centre", name, low, hi),
            param(id + "_wander", name + " Drift", 0, 1));
}

class WanderController {
private:

    AudioProcessorValueTreeState &state;
    float currentValue;
    float range;
public:
    WanderingParameter p;
    String centreId;
    String driftId;
    String name;

    WanderController(float range, const String &idPrefix, AudioProcessorValueTreeState &state):
    p(-range,range,1/400.0f),
    centreId(idPrefix + "_centre"),
    driftId(idPrefix + "_wander"),
    state(state),
    range(range),
    name(idPrefix) {
        currentValue = *state.getRawParameterValue(centreId);
    }

    void step() {
        auto centreRange = state.getParameterRange(centreId);
        auto centre = float(state.getParameterAsValue(centreId).getValue());
        auto drift = p.step() * float(state.getParameterAsValue(driftId).getValue());
        currentValue = centreRange.snapToLegalValue(centre + drift);
    }

    float value() {
        return currentValue;
    }

    float centre() {
        return float(state.getParameterAsValue(centreId).getValue());
    }

    float driftMin() {
        auto centreRange = state.getParameterRange(centreId);
        return centreRange.snapToLegalValue(centre() - range * float(state.getParameterAsValue(driftId).getValue()));
    }

    float driftMax() {
        auto centreRange = state.getParameterRange(centreId);
        return centreRange.snapToLegalValue(centre() + range * float(state.getParameterAsValue(driftId).getValue()));
    }

    float min() {
        return state.getParameterRange(centreId).start;
    }

    float max() {
        return state.getParameterRange(centreId).end;
    }

};

class ParameterHandler {
private:
    AudioProcessorValueTreeState state;

public:
    WanderController cutoffW,envmodW,resonanceW,decayW;

    ParameterHandler(AudioProcessor &proc):
    state(proc, nullptr, "state", {
        wanderable("cutoff", "Cutoff", 30, 800),
        wanderable("envmod", "Env Mod", 0.0,5.0),
        wanderable("resonance", "Resonance", 0.1,20.0),
        wanderable("decay", "Decay", 0.1, 0.9)
    }),
    cutoffW(500.0f, "cutoff", state),
    envmodW(3.0, "envmod", state),
    resonanceW(10.0, "resonance", state),
    decayW(0.5, "decay", state)
    {}

    void step() {
        cutoffW.step();
        envmodW.step();
        resonanceW.step();
        decayW.step();
    }

    float cutoff() {
        return cutoffW.value();
    }

    float envMod() {
        return envmodW.value();
    }

    float resonance() {
        return resonanceW.value();
    }

    float decay() {
        return decayW.value();
    }

    AudioProcessorValueTreeState & getState() {
        return state;
    }
};

class FreeOhVoice: public SynthesiserVoice {
private:
    OscCycler oscCycler;
    Envelope envelope;
    Envelope fEnv;
    dsp::StateVariableTPTFilter<float> filter;
    double sampleRate = 44100;
    bool isPlaying = false;
    bool isAccent = false;
    Envelope release;
    ParameterHandler& params;
public:
    FreeOhVoice(ParameterHandler& params): params(params) {

    }

    bool canPlaySound(SynthesiserSound *) override {
        return true;
    }


    void startNote(int midiNoteNumber, float velocity, SynthesiserSound *sound, int currentPitchWheelPosition) override {
        isAccent = velocity >= 0.7;

        oscCycler.setSampleRate(getSampleRate());
        envelope.setSampleRate(getSampleRate());
        envelope.trigger(0.2);
        fEnv.trigger(isAccent ? params.decay() / 3 : params.decay());
        oscCycler.reset();
        oscCycler.setFrequency(MidiMessage::getMidiNoteInHertz(midiNoteNumber));
        isPlaying = true;
        filter.prepare({getSampleRate(), 2048, 1});
    }

    void stopNote(float velocity, bool allowTailOff) override {
        isPlaying = false;
        if (allowTailOff) {
            release.setSampleRate(getSampleRate());
            release.trigger(0.01);
        }
    }

    void pitchWheelMoved(int newPitchWheelValue) override {

    }

    void renderNextBlock(AudioBuffer<float> &outputBuffer, int startSample, int numSamples) override {
        auto cutoff = params.cutoff();
        auto resonance = params.resonance() * (isAccent ? 2.0 : 0.8);
        auto envMod = params.envMod();


        if (isPlaying || release.isActive()) {
            auto left = outputBuffer.getWritePointer(0);
            auto right = outputBuffer.getWritePointer(1);
            for (int i = startSample; i < startSample + numSamples; i++) {
                filter.setCutoffFrequency(cutoff * (pow(2, fEnv.next() * envMod)));
                filter.setResonance(resonance);
                auto sawtooth = (oscCycler.next() - PI) / PI;
                auto gained = sawtooth * 0.1 * (envelope.next() + 1.0) * (isPlaying ? 1.0 : release.next());
                auto filtered = filter.processSample(0, gained);

                left[i] = filtered;
                right[i] = filtered;
            }
        }
    }

    void controllerMoved(int controllerNumber, int newControllerValue) override {

    }
};
class FreeOhSound: public SynthesiserSound {
public:
    bool appliesToNote(int midiNoteNumber) override { return true; }
    bool appliesToChannel(int midiChannel) override { return true; }
};

class BassSynthesiser: public Synthesiser {
private:
    FreeOhVoice* voice;

public:
    const FreeOhVoice* getVoice() {
        return voice;
    }
    BassSynthesiser(ParameterHandler& params) {
        voice = new FreeOhVoice(params);
        addSound(new FreeOhSound);
        addVoice(voice);
    }

    virtual ~BassSynthesiser() = default;
};

class FreeOhFreePluginProcessor  : public AudioProcessor {
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeOhFreePluginProcessor)

    BassSynthesiser bassSynthesiser;
    ParameterHandler params;

    double timePerParameterStep = 0.1;
    long blockNumber = 0;
    int blocksPerParameterStep = 10;

public:
    FreeOhFreePluginProcessor() :
        AudioProcessor(BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
        bassSynthesiser(params),
        params(*this) {
            
    }

    ~FreeOhFreePluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        blocksPerParameterStep = int((timePerParameterStep * sampleRate) / samplesPerBlock) + 1;
        blockNumber = 0;
        // Inform any subcomponents about samplerate
        bassSynthesiser.setCurrentPlaybackSampleRate(sampleRate);
    }

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midi) override {
        if (blockNumber ++ % blocksPerParameterStep == 0) {
            params.step();
        }
        bassSynthesiser.renderNextBlock(audio,midi,0,audio.getNumSamples());
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {
        return (layouts.getMainOutputChannels() == 2);
    }

    // Change these to add an editor
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    const String getName() const override { return "FreeOhFree";}
    bool acceptsMidi() const override {return true;}
    bool producesMidi() const override {return false;}
    bool isMidiEffect() const override {return false;}
    double getTailLengthSeconds() const override {
        return 0.0;
    }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 1; }
    void setCurrentProgram (int index) override {}
    const String getProgramName (int index) override { return "Default Program"; }
    void changeProgramName (int index, const String& newName) override { }

    // Save state to binary block (used eg. for saving state inside Ableton project)
    void getStateInformation (MemoryBlock& destData) override {
        auto stateToSave = params.getState().copyState();
        std::unique_ptr<XmlElement> xml (stateToSave.createXml());
        copyXmlToBinary(*xml, destData);
    }

    // Restore state from binary block
    void setStateInformation (const void* data, int sizeInBytes) override {
        std::unique_ptr<XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
        if (xmlState != nullptr) {
            if (xmlState->hasTagName(params.getState().state.getType())) {
                params.getState().replaceState(ValueTree::fromXml(*xmlState));
            }
        }
    }
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreeOhFreePluginProcessor();
}


class WanderParameterIndicator : public Component, Timer {
    WanderController& ctrl;
public:
    WanderParameterIndicator(WanderController & ctrl): ctrl(ctrl) {
        startTimerHz(10);
    }

    void paint(Graphics &g) override {
        g.setColour(Colours::black);
        g.fillRect(0,0,getWidth(), getHeight());

        auto min = ctrl.min();
        auto max = ctrl.max();

        auto scaledValue = (ctrl.value() - min) / (max - min);
        auto scaledCentre = (ctrl.centre() - min) / (max - min);
        auto scaledDriftMin = (ctrl.driftMin() - min) / (max - min);
        auto scaledDriftMax = (ctrl.driftMax() - min) / (max - min);

        g.setColour(Colour::fromRGBA(32,32,255,100));
        g.fillRect(0.0f, (1-scaledDriftMax) * float(getHeight()), float(getWidth()), (scaledDriftMax - scaledDriftMin) * float(getHeight()));

        g.setColour(Colour::fromRGBA(32,32,255,200));
        g.fillRect(0.0f, (1-scaledDriftMin) * float(getHeight()), float(getWidth()), 2.0f);
        g.fillRect(0.0f, (1-scaledDriftMax) * float(getHeight()), float(getWidth()), 2.0f);

        g.setColour(Colours::yellow.withAlpha(0.7f));
        g.fillRect(0.0f, (1-scaledCentre) * float(getHeight()), float(getWidth()), 2.0f);

        g.setColour(Colours::white);
        g.fillRect(0.0f, (1-scaledValue) * float(getHeight()), float(getWidth()), 2.0f);

    }

    void timerCallback() override {
        repaint();
    }

};

class WanderParameterEditor: public Component{
private:
    Slider centreSlider;
    Slider driftSlider;
    Label centreLabel;
    Label driftLabel;
    AudioProcessorValueTreeState::SliderAttachment centreAttachment;
    AudioProcessorValueTreeState::SliderAttachment driftAttachment;
    WanderParameterIndicator indicator;
    WanderController& controller;
public:
    WanderParameterEditor(AudioProcessorValueTreeState &state, WanderController & controller):
        centreSlider(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox),
        driftSlider(Slider::RotaryHorizontalVerticalDrag, Slider::NoTextBox),
        centreAttachment(state, controller.centreId, centreSlider),
        driftAttachment(state, controller.driftId, driftSlider),
        indicator(controller),
        controller(controller)
    {
        centreLabel.setText(controller.name + " centre", NotificationType::dontSendNotification);
        centreLabel.setJustificationType(Justification::centred);
        driftLabel.setText(controller.name + " drift", NotificationType::dontSendNotification);
        driftLabel.setJustificationType(Justification::centred);

        addAndMakeVisible(centreSlider);
        addAndMakeVisible(driftSlider);
        addAndMakeVisible(centreLabel);
        addAndMakeVisible(driftLabel);
        addAndMakeVisible(indicator);
        resized();
    }

    void resized() override {
        auto w = getWidth();
        auto h = getHeight();
        auto labelSize = 40;

        centreSlider.setBounds(0,0,w/2,h/2 - labelSize);
        centreLabel.setBounds(0,h/2-labelSize,w/2,labelSize);

        driftSlider.setBounds(0,h/2,w/2,h/2 - labelSize);
        driftLabel.setBounds(0,h-labelSize,w/2,labelSize);

        indicator.setBounds(w/2,0,w/2,h);
    }

};


class FreeOhFreeEditor: public AudioProcessorEditor {
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FreeOhFreeEditor)
    ParameterHandler& params;
    FreeOhFreePluginProcessor& processor;

    WanderParameterEditor cutoff;
    WanderParameterEditor envMod;
    WanderParameterEditor decay;
    WanderParameterEditor resonance;

public:
    explicit FreeOhFreeEditor(FreeOhFreePluginProcessor &processor, ParameterHandler &params):
        AudioProcessorEditor(processor),
        processor(processor),
        params(params),
        cutoff(params.getState(), params.cutoffW),
        envMod(params.getState(), params.envmodW),
        decay(params.getState(), params.decayW),
        resonance(params.getState(), params.resonanceW) {
        setSize(400, 400);
        cutoff.setBounds(0,0,200,200);
        resonance.setBounds(200,0,200,200);
        decay.setBounds(0,200,200,200);
        envMod.setBounds(200,200,200,200);
        addAndMakeVisible(cutoff);
        addAndMakeVisible(resonance);
        addAndMakeVisible(decay);
        addAndMakeVisible(envMod);
    }
};

juce::AudioProcessorEditor * FreeOhFreePluginProcessor::createEditor() {
    return new FreeOhFreeEditor(*this, params);
}