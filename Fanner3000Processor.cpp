#include "Fanner3000Processor.h"
#include <cmath>

Fanner3000Processor::Fanner3000Processor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout Fanner3000Processor::createLayout()
{
    using P    = juce::AudioParameterFloat;
    using NR   = juce::NormalisableRange<float>;
    using Attr = juce::AudioParameterFloatAttributes;

    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    // Velocidad de las aspas (cortes por segundo). Skew para que sea mas fino abajo.
    params.push_back (std::make_unique<P> (juce::ParameterID { "speed", 1 }, "Speed",
                                           NR (0.5f, 80.0f, 0.01f, 0.4f), 25.0f,
                                           Attr().withLabel ("Hz")));

    // Cuanto se corta la voz (1 = silencio total entre aspas)
    params.push_back (std::make_unique<P> (juce::ParameterID { "depth", 1 }, "Depth",
                                           NR (0.0f, 1.0f, 0.01f), 0.9f));

    // 0 = onda suave tipo tremolo, 1 = corte seco tipo aspa
    params.push_back (std::make_unique<P> (juce::ParameterID { "shape", 1 }, "Shape",
                                           NR (0.0f, 1.0f, 0.01f), 0.6f));

    // Desfase entre izquierda y derecha (efecto de ventilador girando)
    params.push_back (std::make_unique<P> (juce::ParameterID { "stereo", 1 }, "Stereo",
                                           NR (0.0f, 1.0f, 0.01f), 0.0f));

    // Irregularidad en la velocidad, como un ventilador de verdad
    params.push_back (std::make_unique<P> (juce::ParameterID { "wobble", 1 }, "Wobble",
                                           NR (0.0f, 1.0f, 0.01f), 0.3f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "mix", 1 }, "Mix",
                                           NR (0.0f, 1.0f, 0.01f), 1.0f));

    params.push_back (std::make_unique<P> (juce::ParameterID { "output", 1 }, "Output",
                                           NR (-24.0f, 12.0f, 0.1f), 0.0f,
                                           Attr().withLabel ("dB")));

    return { params.begin(), params.end() };
}

void Fanner3000Processor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    smoothSpeed.reset (sampleRate, 0.05);
    smoothDepth.reset (sampleRate, 0.02);
    smoothShape.reset (sampleRate, 0.02);
    smoothMix.reset (sampleRate, 0.02);
    smoothOutput.reset (sampleRate, 0.02);

    smoothSpeed.setCurrentAndTargetValue (apvts.getRawParameterValue ("speed")->load());
    smoothDepth.setCurrentAndTargetValue (apvts.getRawParameterValue ("depth")->load());
    smoothShape.setCurrentAndTargetValue (apvts.getRawParameterValue ("shape")->load());
    smoothMix.setCurrentAndTargetValue (apvts.getRawParameterValue ("mix")->load());
    smoothOutput.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));

    phase = 0.0;
    noise = 0.0f;
    noiseTarget = 0.0f;
    noiseCounter = 0;
}

bool Fanner3000Processor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void Fanner3000Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numChannels = juce::jmin (buffer.getNumChannels(), 2);
    const int numSamples  = buffer.getNumSamples();

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    smoothSpeed.setTargetValue (apvts.getRawParameterValue ("speed")->load());
    smoothDepth.setTargetValue (apvts.getRawParameterValue ("depth")->load());
    smoothShape.setTargetValue (apvts.getRawParameterValue ("shape")->load());
    smoothMix.setTargetValue (apvts.getRawParameterValue ("mix")->load());
    smoothOutput.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));

    const float stereo = apvts.getRawParameterValue ("stereo")->load();
    const float wobble = apvts.getRawParameterValue ("wobble")->load();

    const float sr = static_cast<float> (currentSampleRate);
    const float noiseCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3.0f / sr);
    const int noisePeriod = juce::jmax (1, static_cast<int> (0.12f * sr));

    float* channelData[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numChannels; ++ch)
        channelData[ch] = buffer.getWritePointer (ch);

    for (int i = 0; i < numSamples; ++i)
    {
        const float speed  = smoothSpeed.getNextValue();
        const float depth  = smoothDepth.getNextValue();
        const float shape  = smoothShape.getNextValue();
        const float mix    = smoothMix.getNextValue();
        const float output = smoothOutput.getNextValue();

        // Irregularidad lenta: cada ~0.12 s elige un objetivo nuevo y lo sigue suavemente
        if (--noiseCounter <= 0)
        {
            noiseTarget  = rng.nextFloat() * 2.0f - 1.0f;
            noiseCounter = noisePeriod;
        }
        noise += noiseCoeff * (noiseTarget - noise);

        // Avanza el giro del ventilador
        const double hz = static_cast<double> (speed) * (1.0 + 0.15 * static_cast<double> (wobble * noise));
        phase += hz / currentSampleRate;
        if (phase >= 1.0)
            phase -= 1.0;

        // Shape: tanh empuja la senoidal hacia una onda cuadrada (corte seco)
        const float k     = 1.0f + 24.0f * shape;
        const float normK = 1.0f / std::tanh (k);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float p = static_cast<float> (phase) + (ch == 1 ? stereo * 0.5f : 0.0f);
            p -= std::floor (p);

            const float s      = std::sin (juce::MathConstants<float>::twoPi * p);
            const float shaped = std::tanh (k * s) * normK;   // -1..1
            const float unipolar = 0.5f * (shaped + 1.0f);    // 0..1
            const float gain   = 1.0f - depth * (1.0f - unipolar);

            const float dry = channelData[ch][i];
            const float wet = dry * gain;

            channelData[ch][i] = (dry * (1.0f - mix) + wet * mix) * output;
        }
    }
}

juce::AudioProcessorEditor* Fanner3000Processor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void Fanner3000Processor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void Fanner3000Processor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Fanner3000Processor();
}
