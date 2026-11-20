#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

class Fanner3000Processor : public juce::AudioProcessor
{
public:
    Fanner3000Processor();
    ~Fanner3000Processor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Fanner 3000"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::LinearSmoothedValue<float> smoothSpeed  { 25.0f };
    juce::LinearSmoothedValue<float> smoothDepth  { 0.9f };
    juce::LinearSmoothedValue<float> smoothShape  { 0.6f };
    juce::LinearSmoothedValue<float> smoothMix    { 1.0f };
    juce::LinearSmoothedValue<float> smoothOutput { 1.0f };

    double currentSampleRate = 44100.0;

    // Estado del "ventilador"
    double phase = 0.0;          // fase de giro (0..1)
    juce::Random rng;
    float noise = 0.0f;          // irregularidad suavizada (-1..1)
    float noiseTarget = 0.0f;
    int noiseCounter = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Fanner3000Processor)
};
