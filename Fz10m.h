#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "IControls.h"

const int kNumPresets = 1;

enum EWavePreset
{
  kPresetSine = 0,
  kPresetTriangle,
  kPresetSawtooth,
  kPresetSquare,
  kPresetPulse,
  kPresetDoubleSine,
  kPresetSawPulse,
  kPresetRandom,
  kNumWavePresets
};

enum EParams
{
  kParamGain = 0,
  kParamAttack,
  kParamDecay,
  kParamSustain,
  kParamRelease,
  kParamCutoff,
  kParamResonance,
  kParamLoFiCharacter,
  kParamLoFiRate,
  kParamLoFiBits,
  kParamFilterStep,
  kParamFEnvAttack,
  kParamFEnvDecay,
  kParamFEnvSustain,
  kParamFEnvRelease,
  kParamFEnvAmount,
  kParamWavePreset,
  kNumParams
};

enum EControlTags
{
  kCtrlTagWavetable = 0,
  kCtrlTagKeyboard,
  kNumCtrlTags
};

enum EMessageTags
{
  kMsgTagWavetableChanged = 0,
};

// Shared between UI (IVMultiSliderControl template arg) and DSP (wavetable buffer).
// Changing this requires matching the IVMultiSliderControl<N> in Fz10m.cpp.
constexpr int kWavetableSize = 128;

#if IPLUG_DSP
#include "Fz10m_DSP.h"
#endif

using namespace iplug;
using namespace igraphics;

class Fz10m final : public Plugin
{
public:
  Fz10m(const InstanceInfo& info);

  bool SerializeState(IByteChunk& chunk) const override;
  int UnserializeState(const IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;

#if IPLUG_DSP
public:
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void ProcessMidiMsg(const IMidiMsg& msg) override;
  void OnReset() override;
  void OnParamChange(int paramIdx) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

private:
  Fz10mDSP<sample> mDSP;
#endif
};
