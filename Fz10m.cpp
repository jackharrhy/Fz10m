#include "Fz10m.h"
#include "IPlug_include_in_plug_src.h"

// Generate a preset waveform as 0..1 UI slider values into vals[kWavetableSize].
static void GenerateWavePreset(int preset, float* vals)
{
  constexpr int N = kWavetableSize;
  for (int i = 0; i < N; ++i)
  {
    double v = 0.0;
    const double phase = static_cast<double>(i) / N;
    switch (preset)
    {
      case kPresetSine:
        v = std::sin(2.0 * M_PI * phase);
        break;
      case kPresetTriangle:
        v = 2.0 / M_PI * std::asin(std::sin(2.0 * M_PI * phase));
        break;
      case kPresetSawtooth:
        v = 2.0 * phase - 1.0;
        break;
      case kPresetSquare:
        v = phase < 0.5 ? 1.0 : -1.0;
        break;
      case kPresetPulse:
        v = phase < 0.25 ? 1.0 : -1.0;
        break;
      case kPresetDoubleSine:
        v = std::sin(4.0 * M_PI * phase);
        break;
      case kPresetSawPulse:
        v = phase < 0.5 ? (4.0 * phase - 1.0) : -1.0;
        break;
      case kPresetRandom:
        v = (static_cast<double>(std::rand()) / RAND_MAX) * 2.0 - 1.0;
        break;
    }
    // Map -1..+1 to 0..1 for UI sliders
    vals[i] = static_cast<float>(v * 0.5 + 0.5);
  }
}

Fz10m::Fz10m(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100., 0.01, "%");
  GetParam(kParamAttack)->InitDouble("Attack", 10., 5., 1000., 0.1, "ms",
                                     IParam::kFlagsNone, "ADSR",
                                     IParam::ShapePowCurve(3.));
  GetParam(kParamDecay)->InitDouble("Decay", 50., 20., 1000., 0.1, "ms",
                                     IParam::kFlagsNone, "ADSR",
                                     IParam::ShapePowCurve(3.));
  GetParam(kParamSustain)->InitDouble("Sustain", 50., 0., 100., 1., "%",
                                      IParam::kFlagsNone, "ADSR");
  GetParam(kParamRelease)->InitDouble("Release", 50., 20., 1000., 0.1, "ms",
                                       IParam::kFlagsNone, "ADSR",
                                       IParam::ShapePowCurve(3.));
  GetParam(kParamCutoff)->InitFrequency("Cutoff", 20000., 20., 20000.);
  GetParam(kParamResonance)->InitDouble("Resonance", 0.707, 0.5, 10., 0.01);
  GetParam(kParamLoFiCharacter)->InitDouble("Character", 100., 0., 100., 0.1, "%",
                                            IParam::kFlagsNone, "LoFi");
  GetParam(kParamLoFiRate)->InitDouble("Rate", 36000., 9000., 48000., 1., "Hz",
                                       IParam::kFlagsNone, "LoFi",
                                       IParam::ShapePowCurve(2.));
  GetParam(kParamLoFiBits)->InitDouble("Bits", 8., 4., 16., 1., "bits",
                                       IParam::kFlagsNone, "LoFi");
  GetParam(kParamLoFiPost)->InitBool("Post", false, "", IParam::kFlagsNone, "LoFi");
  GetParam(kParamFilterStep)->InitDouble("Step", 1., 1., 512., 1., "smp",
                                            IParam::kFlagsNone, "Synth");
  GetParam(kParamFilterMode)->InitEnum("Filter", 0, 5,
                                        "", IParam::kFlagsNone, "Synth",
                                        "LowPass", "HighPass", "BandPass", "Notch", "Peak");
  GetParam(kParamFEnvAttack)->InitDouble("FAtk", 10., 5., 1000., 0.1, "ms",
                                          IParam::kFlagsNone, "FiltEnv",
                                          IParam::ShapePowCurve(3.));
  GetParam(kParamFEnvDecay)->InitDouble("FDec", 200., 20., 1000., 0.1, "ms",
                                         IParam::kFlagsNone, "FiltEnv",
                                         IParam::ShapePowCurve(3.));
  GetParam(kParamFEnvSustain)->InitDouble("FSus", 0., 0., 100., 1., "%",
                                           IParam::kFlagsNone, "FiltEnv");
  GetParam(kParamFEnvRelease)->InitDouble("FRel", 50., 20., 1000., 0.1, "ms",
                                           IParam::kFlagsNone, "FiltEnv",
                                           IParam::ShapePowCurve(3.));
  GetParam(kParamFEnvAmount)->InitDouble("FAmt", 0., -100., 100., 0.1, "%",
                                          IParam::kFlagsNone, "FiltEnv");
  GetParam(kParamWavePreset)->InitEnum("Wave", kPresetSine, kNumWavePresets,
                                        "", IParam::kFlagsNone, "",
                                        "Sine", "Triangle", "Sawtooth", "Square",
                                        "Pulse", "Dbl Sine", "Saw/Pulse", "Random");

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS,
                        GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(COLOR_GRAY);
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    IRECT b = pGraphics->GetBounds().GetPadded(-20.f);

    // Reset button in top-right corner.
    const IRECT resetBounds = b.GetFromTop(24.f).GetFromRight(70.f);

    // Carve the full rect into three non-overlapping horizontal slices from
    // top to bottom: knobs row, wavetable, keyboard. ReduceFromTop/Bottom
    // mutates `b` to the remaining space each time.
    // Two rows of knobs, 80px each, with grouped sections.
    const float kGap = 5.f;
    const float kKnobSize = 80.f;
    const IRECT row1 = b.ReduceFromTop(150.f);
    const IRECT row2 = b.ReduceFromTop(150.f);

    const IRECT kbBounds = b.ReduceFromBottom(180.f);
    const IRECT wtBounds = b; // whatever's left in between

    // Row 1: Synth (5) + ADSR (4) = 9 columns
    const IRECT synthRect = row1.SubRectHorizontal(9, 0).Union(row1.SubRectHorizontal(9, 4)).GetPadded(-kGap);
    const IRECT adsrRect  = row1.SubRectHorizontal(9, 5).Union(row1.SubRectHorizontal(9, 8)).GetPadded(-kGap);

    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 0, 1, 5).GetCentredInside(kKnobSize), kParamGain, "Gain"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 1, 1, 5).GetCentredInside(kKnobSize), kParamCutoff, "Cutoff"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 2, 1, 5).GetCentredInside(kKnobSize), kParamResonance, "Res"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 3, 1, 5).GetCentredInside(kKnobSize), kParamFilterStep, "Step"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVMenuButtonControl(synthRect.GetGridCell(0, 4, 1, 5).GetCentredInside(kKnobSize, 40.f), kParamFilterMode, "Mode", DEFAULT_STYLE), kNoTag, "Synth");

    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 0, 1, 4).GetCentredInside(kKnobSize), kParamAttack, "Attack"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 1, 1, 4).GetCentredInside(kKnobSize), kParamDecay, "Decay"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 2, 1, 4).GetCentredInside(kKnobSize), kParamSustain, "Sustain"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 3, 1, 4).GetCentredInside(kKnobSize), kParamRelease, "Release"), kNoTag, "ADSR");

    // Row 2: FiltEnv (5) + LoFi (3) = 8 knobs in an 8-column grid
    const IRECT fenvRect = row2.SubRectHorizontal(8, 0).Union(row2.SubRectHorizontal(8, 4)).GetPadded(-kGap);
    const IRECT lofiRect = row2.SubRectHorizontal(8, 5).Union(row2.SubRectHorizontal(8, 7)).GetPadded(-kGap);

    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 0, 1, 5).GetCentredInside(kKnobSize), kParamFEnvAttack, "FAtk"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 1, 1, 5).GetCentredInside(kKnobSize), kParamFEnvDecay, "FDec"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 2, 1, 5).GetCentredInside(kKnobSize), kParamFEnvSustain, "FSus"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 3, 1, 5).GetCentredInside(kKnobSize), kParamFEnvRelease, "FRel"), kNoTag, "FiltEnv");
    pGraphics->AttachControl(new IVKnobControl(fenvRect.GetGridCell(0, 4, 1, 5).GetCentredInside(kKnobSize), kParamFEnvAmount, "FAmt"), kNoTag, "FiltEnv");

    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 0, 1, 4).GetCentredInside(kKnobSize), kParamLoFiCharacter, "Char"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 1, 1, 4).GetCentredInside(kKnobSize), kParamLoFiRate, "Rate"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 2, 1, 4).GetCentredInside(kKnobSize), kParamLoFiBits, "Bits"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVToggleControl(lofiRect.GetGridCell(0, 3, 1, 4).GetCentredInside(kKnobSize, 40.f), kParamLoFiPost, "Position", DEFAULT_STYLE, "Pre", "Post"), kNoTag, "LoFi");

    // Group borders
    pGraphics->AttachControl(new IVGroupControl("Synth", "Synth", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("ADSR", "ADSR", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("Filter Env", "FiltEnv", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("LoFi", "LoFi", 5.f, 20.f, 5.f, 5.f));

    // Reset button (positioned at top-right, bounds computed above).
    pGraphics->AttachControl(new IVButtonControl(resetBounds,
      [pGraphics](IControl* pCaller) {
        // Reset all parameters to defaults.
        auto* pDelegate = pGraphics->GetDelegate();
        for (int i = 0; i < kNumParams; ++i)
        {
          pDelegate->BeginInformHostOfParamChangeFromUI(i);
          pDelegate->SendParameterValueFromUI(i, pDelegate->GetParam(i)->GetDefault(true));
          pDelegate->EndInformHostOfParamChangeFromUI(i);
        }
        // Reset wavetable to sine (matching default kParamWavePreset).
        auto* pWT = pGraphics->GetControlWithTag(kCtrlTagWavetable);
        float vals[kWavetableSize];
        GenerateWavePreset(kPresetSine, vals);
        for (int i = 0; i < kWavetableSize; ++i)
          pWT->SetValue(vals[i], i);
        pDelegate->SendArbitraryMsgFromUI(kMsgTagWavetableChanged,
                                          kCtrlTagWavetable,
                                          sizeof(vals), vals);
        // Push updated param values back to knob controls.
        pDelegate->SendCurrentParamValuesFromDelegate();
      }, "Reset", DEFAULT_STYLE));

    // Wavetable preset dropdown above the wavetable, top-left.
    const IRECT presetRow = b.ReduceFromTop(50.f);
    pGraphics->AttachControl(new IVMenuButtonControl(presetRow.GetFromLeft(120.f),
                                                      kParamWavePreset, "Wave", DEFAULT_STYLE));

    // Full-width wavetable drawing surface.
    const IRECT wtDrawBounds = b;
    auto* pWT = new IVMultiSliderControl<kWavetableSize>(wtDrawBounds, "Wavetable",
                                                         DEFAULT_STYLE);
    pGraphics->AttachControl(pWT, kCtrlTagWavetable);
    // Initialize to a sine (slider values are 0..1; actual wavetable samples
    // get remapped to -1..+1 on the DSP side in Fz10mDSP::UpdateWavetable).
    for (int i = 0; i < kWavetableSize; ++i)
    {
      const double v = 0.5 + 0.5 * std::sin(2.0 * M_PI * i / kWavetableSize);
      pWT->SetValue(v, i);
    }
    pWT->SetActionFunction([pGraphics](IControl* pCaller) {
      float vals[kWavetableSize];
      for (int i = 0; i < kWavetableSize; ++i)
        vals[i] = static_cast<float>(pCaller->GetValue(i));
      pGraphics->GetDelegate()->SendArbitraryMsgFromUI(kMsgTagWavetableChanged,
                                                      kCtrlTagWavetable,
                                                      sizeof(vals), vals);
    });

    // Bottom: on-screen keyboard for testing.
    pGraphics->AttachControl(new IVKeyboardControl(kbBounds), kCtrlTagKeyboard);

    pGraphics->SetQwertyMidiKeyHandlerFunc([pGraphics](const IMidiMsg& msg) {
      pGraphics->GetControlWithTag(kCtrlTagKeyboard)
        ->As<IVKeyboardControl>()
        ->SetNoteFromMidi(msg.NoteNumber(), msg.StatusMsg() == IMidiMsg::kNoteOn);
    });
  };
#endif
}

// State chunk format version. Bump this when the serialized layout changes.
static constexpr int kStateVersion = 1;

bool Fz10m::SerializeState(IByteChunk& chunk) const
{
  // Version header (always first).
  chunk.Put(&kStateVersion);

#if IPLUG_DSP
  // Wavetable: 128 doubles in -1..+1 range.
  const auto& wt = mDSP.GetWavetable();
  for (int i = 0; i < kWavetableSize; ++i)
    chunk.Put(&wt[i]);
#endif

  return SerializeParams(chunk);
}

int Fz10m::UnserializeState(const IByteChunk& chunk, int startPos)
{
  int version = 0;
  startPos = chunk.Get(&version, startPos);

  if (version == 1)
  {
#if IPLUG_DSP
    // Read 128 doubles (-1..+1), convert to 0..1 floats for UpdateWavetable.
    float vals01[kWavetableSize];
    for (int i = 0; i < kWavetableSize; ++i)
    {
      double v;
      startPos = chunk.Get(&v, startPos);
      vals01[i] = static_cast<float>(v * 0.5 + 0.5);
    }
    mDSP.UpdateWavetable(vals01, kWavetableSize);
#endif
  }
  // Unknown version: skip gracefully. Parameters will still load via
  // UnserializeParams below, and wavetable stays at its current state.

  return UnserializeParams(chunk, startPos);
}

void Fz10m::OnUIOpen()
{
  // Base class pushes all parameter values to knob controls.
  // Fully qualified to avoid ambiguity with CLAP helpers' Plugin class.
  iplug::Plugin::OnUIOpen();

#if IPLUG_DSP
  // Push the current wavetable state to the UI multi-slider control.
  // (Wavetable is custom state, not a parameter, so the base class doesn't handle it.)
  if (GetUI())
  {
    auto* pWT = GetUI()->GetControlWithTag(kCtrlTagWavetable);
    if (pWT)
    {
      const auto& wt = mDSP.GetWavetable();
      for (int i = 0; i < kWavetableSize; ++i)
        pWT->SetValue(wt[i] * 0.5 + 0.5, i);
      pWT->SetDirty(false);
    }
  }
#endif
}

#if IPLUG_DSP
void Fz10m::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  mDSP.ProcessBlock(outputs, 2, nFrames);
}

void Fz10m::ProcessMidiMsg(const IMidiMsg& msg)
{
  const int status = msg.StatusMsg();
  switch (status)
  {
    case IMidiMsg::kNoteOn:
    case IMidiMsg::kNoteOff:
    case IMidiMsg::kPolyAftertouch:
    case IMidiMsg::kControlChange:
    case IMidiMsg::kProgramChange:
    case IMidiMsg::kChannelAftertouch:
    case IMidiMsg::kPitchWheel:
      mDSP.ProcessMidiMsg(msg);
      SendMidiMsg(msg);
      break;
    default:
      break;
  }
}

void Fz10m::OnReset()
{
  mDSP.Reset(GetSampleRate(), GetBlockSize());
}

void Fz10m::OnParamChange(int paramIdx)
{
  if (paramIdx == kParamWavePreset)
  {
    // Generate the preset waveform and push to DSP + UI.
    float vals[kWavetableSize];
    GenerateWavePreset(static_cast<int>(GetParam(kParamWavePreset)->Value()), vals);
    mDSP.UpdateWavetable(vals, kWavetableSize);

    if (GetUI())
    {
      auto* pWT = GetUI()->GetControlWithTag(kCtrlTagWavetable);
      if (pWT)
      {
        for (int i = 0; i < kWavetableSize; ++i)
          pWT->SetValue(vals[i], i);
        pWT->SetDirty(false);
      }
    }
    return;
  }

  mDSP.SetParam(paramIdx, GetParam(paramIdx)->Value());
}

bool Fz10m::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  if (msgTag == kMsgTagWavetableChanged && ctrlTag == kCtrlTagWavetable)
  {
    const int count = dataSize / static_cast<int>(sizeof(float));
    mDSP.UpdateWavetable(static_cast<const float*>(pData), count);
    return true;
  }
  return false;
}
#endif
