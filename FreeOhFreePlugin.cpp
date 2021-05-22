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

class FreeOhFreePluginProcessor  : public AudioProcessor {
private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FreeOhFreePluginProcessor)

    AudioProcessorValueTreeState state;

public:
    FreeOhFreePluginProcessor() :
        AudioProcessor(BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
        state(*this, nullptr, "state", {
            // Parameter definitions go here
        }) {
            
    }

    ~FreeOhFreePluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override {
        // Inform any subcomponents about samplerate
    }

    void processBlock (AudioBuffer<float>& audio, MidiBuffer& midi) override {
        // Write your processing code here
    }

    void releaseResources() override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override {
        return (layouts.getMainOutputChannels() == 2);
    }

    // Change these to add an editor
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }

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
        auto stateToSave = state.copyState();
        std::unique_ptr<XmlElement> xml (stateToSave.createXml());
        copyXmlToBinary(*xml, destData);
    }

    // Restore state from binary block
    void setStateInformation (const void* data, int sizeInBytes) override {
        std::unique_ptr<XmlElement> xmlState (getXmlFromBinary(data, sizeInBytes));
        if (xmlState != nullptr) {
            if (xmlState->hasTagName(state.state.getType())) {
                state.replaceState(ValueTree::fromXml(*xmlState));
            }
        }
    }
};

AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FreeOhFreePluginProcessor();
}
