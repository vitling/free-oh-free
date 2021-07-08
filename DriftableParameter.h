//
// Created by David Whiting on 2021-05-25.
//

#ifndef FREE_OH_FREE_PLUGIN_DRIFTABLEPARAMETER_H
#define FREE_OH_FREE_PLUGIN_DRIFTABLEPARAMETER_H

#include <JuceHeader.h>

#include <memory>

inline std::unique_ptr<AudioParameterFloat> param(const String& id, const String& name, float low, float hi) {
    auto interval = pow(10, int(log10((hi - low) / 256)));
    std::cout << "Parameter " << name << " low " << low << " high " << hi << "interval" << interval << "\n";
    return std::make_unique<AudioParameterFloat>(id, name, NormalisableRange<float>(low, hi, interval), (low+hi)/2);
}

class DriftableParameter  {
private:
    const String &id, &name;
    float low, high;
    AudioParameterFloat *centreUnsafe = nullptr;
public:
    DriftableParameter(const String &id, const String &name, float low, float high): id(id), name(name), low(low), high(high) {

    }

    std::unique_ptr<AudioProcessorParameterGroup> createParameters() {
        auto centre = param(id, name, low, high);

        return std::make_unique<AudioProcessorParameterGroup>(id, name,"--", std::move(centre))
    }
};

#endif //FREE_OH_FREE_PLUGIN_DRIFTABLEPARAMETER_H
