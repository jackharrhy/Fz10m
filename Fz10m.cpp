#include "Fz10m.h"
#include "IPlug_include_in_plug_src.h"

Fz10m::Fz10m(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kParamGain)->InitDouble("Gain", 100., 0., 100., 0.01, "%");
  GetParam(kParamAttack)->InitDouble("Attack", 10., 1., 1000., 0.1, "ms",
                                     IParam::kFlagsNone, "ADSR",
                                     IParam::ShapePowCurve(3.));
  GetParam(kParamDecay)->InitDouble("Decay", 10., 1., 1000., 0.1, "ms",
                                    IParam::kFlagsNone, "ADSR",
                                    IParam::ShapePowCurve(3.));
  GetParam(kParamSustain)->InitDouble("Sustain", 50., 0., 100., 1., "%",
                                      IParam::kFlagsNone, "ADSR");
  GetParam(kParamRelease)->InitDouble("Release", 10., 2., 1000., 0.1, "ms",
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
  GetParam(kParamFilterStep)->InitDouble("Step", 1., 1., 512., 1., "smp",
                                         IParam::kFlagsNone, "Synth");

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

    // Carve the full rect into three non-overlapping horizontal slices from
    // top to bottom: knobs row, wavetable, keyboard. ReduceFromTop/Bottom
    // mutates `b` to the remaining space each time.
    const IRECT knobRow = b.ReduceFromTop(170.f);
    const IRECT kbBounds = b.ReduceFromBottom(180.f);
    const IRECT wtBounds = b; // whatever's left in between

    // Top row: 11 knobs split into 3 groups (Synth 4, ADSR 4, LoFi 3).
    // Divide into 11 equal slices, then union slices into group rects with
    // 5px inset on each side so group borders don't overlap each other.
    const float kGap = 5.f;
    const IRECT synthRect = knobRow.SubRectHorizontal(11, 0).Union(knobRow.SubRectHorizontal(11, 3)).GetPadded(-kGap);
    const IRECT adsrRect  = knobRow.SubRectHorizontal(11, 4).Union(knobRow.SubRectHorizontal(11, 7)).GetPadded(-kGap);
    const IRECT lofiRect  = knobRow.SubRectHorizontal(11, 8).Union(knobRow.SubRectHorizontal(11, 10)).GetPadded(-kGap);

    // Synth group: Gain, Cutoff, Resonance, Step
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 0, 1, 4).GetCentredInside(90), kParamGain, "Gain"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 1, 1, 4).GetCentredInside(90), kParamCutoff, "Cutoff"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 2, 1, 4).GetCentredInside(90), kParamResonance, "Res"), kNoTag, "Synth");
    pGraphics->AttachControl(new IVKnobControl(synthRect.GetGridCell(0, 3, 1, 4).GetCentredInside(90), kParamFilterStep, "Step"), kNoTag, "Synth");

    // ADSR group: Attack, Decay, Sustain, Release
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 0, 1, 4).GetCentredInside(90), kParamAttack, "Attack"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 1, 1, 4).GetCentredInside(90), kParamDecay, "Decay"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 2, 1, 4).GetCentredInside(90), kParamSustain, "Sustain"), kNoTag, "ADSR");
    pGraphics->AttachControl(new IVKnobControl(adsrRect.GetGridCell(0, 3, 1, 4).GetCentredInside(90), kParamRelease, "Release"), kNoTag, "ADSR");

    // LoFi group: Character, Rate, Bits
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 0, 1, 3).GetCentredInside(90), kParamLoFiCharacter, "Char"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 1, 1, 3).GetCentredInside(90), kParamLoFiRate, "Rate"), kNoTag, "LoFi");
    pGraphics->AttachControl(new IVKnobControl(lofiRect.GetGridCell(0, 2, 1, 3).GetCentredInside(90), kParamLoFiBits, "Bits"), kNoTag, "LoFi");

    // Group borders (attached after knobs so they auto-size around them)
    // Padding: left, top, right, bottom. Extra top padding avoids group label
    // overlapping with knob labels inside.
    pGraphics->AttachControl(new IVGroupControl("Synth", "Synth", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("ADSR", "ADSR", 5.f, 20.f, 5.f, 5.f));
    pGraphics->AttachControl(new IVGroupControl("LoFi", "LoFi", 5.f, 20.f, 5.f, 5.f));

    // Middle: wavetable drawing surface (128 sliders).
    auto* pWT = new IVMultiSliderControl<kWavetableSize>(wtBounds, "Wavetable",
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
