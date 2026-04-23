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
    const IRECT knobRow = b.ReduceFromTop(140.f);
    const IRECT kbBounds = b.ReduceFromBottom(180.f);
    const IRECT wtBounds = b; // whatever's left in between

    // Top row: 7 knobs in a 1×7 grid. GetGridCell has two overloads; we use
    // the 4-arg (row, col, nRows, nCols) form explicitly to avoid the 3-arg
    // (cellIndex, nRows, nCols) overload that would collapse to nRows=0.
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 0, 1, 7).GetCentredInside(100), kParamGain, "Gain"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 1, 1, 7).GetCentredInside(100), kParamAttack, "Attack"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 2, 1, 7).GetCentredInside(100), kParamDecay, "Decay"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 3, 1, 7).GetCentredInside(100), kParamSustain, "Sustain"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 4, 1, 7).GetCentredInside(100), kParamRelease, "Release"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 5, 1, 7).GetCentredInside(100), kParamCutoff, "Cutoff"));
    pGraphics->AttachControl(new IVKnobControl(knobRow.GetGridCell(0, 6, 1, 7).GetCentredInside(100), kParamResonance, "Res"));

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
