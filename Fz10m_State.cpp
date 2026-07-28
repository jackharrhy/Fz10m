#include "Fz10m.h"

namespace {

constexpr int kStateVersion = 2;
constexpr int kVST3TrailerSize = sizeof(int32_t);

constexpr int kV001ParamMap[] = {
  kParamGain, kParamAttack, kParamDecay, kParamSustain, kParamRelease,
  kParamCutoff, kParamResonance
};

constexpr int kV03ParamMap[] = {
  kParamGain, kParamAttack, kParamDecay, kParamSustain, kParamRelease,
  kParamCutoff, kParamResonance, kParamLoFiCharacter, kParamLoFiRate,
  kParamLoFiBits, kParamFilterStep
};

constexpr int kV04ParamMap[] = {
  kParamGain, kParamAttack, kParamDecay, kParamSustain, kParamRelease,
  kParamCutoff, kParamResonance, kParamLoFiCharacter, kParamLoFiRate,
  kParamLoFiBits, kParamFilterStep, kParamFEnvAttack, kParamFEnvDecay,
  kParamFEnvSustain, kParamFEnvRelease, kParamFEnvAmount, kParamWavePreset
};

static_assert(std::size(kV001ParamMap) == 7);
static_assert(std::size(kV03ParamMap) == 11);
static_assert(std::size(kV04ParamMap) == 17);

int MatchLegacyParamCount(int remainingBytes, const int* pCounts, int nCounts)
{
  for (int i = 0; i < nCounts; ++i)
  {
    const int paramBytes = pCounts[i] * static_cast<int>(sizeof(double));
    if (remainingBytes == paramBytes || remainingBytes == paramBytes + kVST3TrailerSize)
      return pCounts[i];
  }
  return 0;
}

void GenerateSine(float* vals)
{
  for (int i = 0; i < kWavetableSize; ++i)
    vals[i] = static_cast<float>(0.5 + 0.5 * std::sin(2.0 * M_PI * i / kWavetableSize));
}

void AnalyzeHarmonics(const float* vals, float* harmonics)
{
  float maxAmplitude = 0.f;
  for (int k = 0; k < kNumHarmonics; ++k)
  {
    double amplitude = 0.0;
    for (int i = 0; i < kWavetableSize; ++i)
    {
      const double sample = vals[i] * 2.0 - 1.0;
      amplitude += sample * std::sin(2.0 * M_PI * (k + 1) * i / kWavetableSize);
    }

    harmonics[k] = std::max(0.f, static_cast<float>(amplitude * 2.0 / kWavetableSize));
    maxAmplitude = std::max(maxAmplitude, harmonics[k]);
  }

  if (maxAmplitude > 0.f)
  {
    for (int k = 0; k < kNumHarmonics; ++k)
      harmonics[k] /= maxAmplitude;
  }
}

} // namespace

bool Fz10m::SerializeState(IByteChunk& chunk) const
{
  chunk.Put(&kStateVersion);

#if IPLUG_DSP
  const auto& wt = mDSP.GetWavetable();
  for (int i = 0; i < kWavetableSize; ++i)
  {
    const double value = wt[i];
    chunk.Put(&value);
  }
#endif

  for (const float harmonic : mHarmonics)
  {
    const double value = harmonic;
    chunk.Put(&value);
  }

  return SerializeParams(chunk);
}

int Fz10m::UnserializeState(const IByteChunk& chunk, int startPos)
{
  const int unversionedCounts[] = {7, 11};
  const int unversionedParamCount = MatchLegacyParamCount(
    chunk.Size() - startPos, unversionedCounts, static_cast<int>(std::size(unversionedCounts)));

  if (unversionedParamCount > 0)
  {
    double values[11] = {};
    for (int i = 0; i < unversionedParamCount; ++i)
    {
      startPos = chunk.Get(&values[i], startPos);
      if (startPos < 0)
        return -1;
    }

    mHarmonics.fill(0.f);
    mHarmonics[0] = 1.f;
#if IPLUG_DSP
    float sine[kWavetableSize];
    GenerateSine(sine);
    mDSP.UpdateWavetable(sine, kWavetableSize);
#endif

    ENTER_PARAMS_MUTEX
    for (int i = 0; i < kNumParams; ++i)
      GetParam(i)->Set(GetParam(i)->GetDefault());
    for (int i = 0; i < unversionedParamCount; ++i)
    {
      const int paramIdx = unversionedParamCount == 7 ? kV001ParamMap[i] : kV03ParamMap[i];
      GetParam(paramIdx)->Set(values[i]);
    }
    OnParamReset(kPresetRecall);
    LEAVE_PARAMS_MUTEX
    return startPos;
  }

  int version = 0;
  startPos = chunk.Get(&version, startPos);
  if (startPos < 0 || (version != 1 && version != kStateVersion))
    return -1;

  float vals01[kWavetableSize];
  for (int i = 0; i < kWavetableSize; ++i)
  {
    double value = 0.0;
    startPos = chunk.Get(&value, startPos);
    if (startPos < 0)
      return -1;
    vals01[i] = static_cast<float>(value * 0.5 + 0.5);
  }

  if (version == 1)
  {
    const int versionedCounts[] = {11, 17, 19, 20};
    const int legacyParamCount = MatchLegacyParamCount(
      chunk.Size() - startPos, versionedCounts, static_cast<int>(std::size(versionedCounts)));
    if (legacyParamCount == 0)
      return -1;

    double values[20] = {};
    for (int i = 0; i < legacyParamCount; ++i)
    {
      startPos = chunk.Get(&values[i], startPos);
      if (startPos < 0)
        return -1;
    }

#if IPLUG_DSP
    mDSP.UpdateWavetable(vals01, kWavetableSize);
#endif

    ENTER_PARAMS_MUTEX
    for (int i = 0; i < kNumParams; ++i)
      GetParam(i)->Set(GetParam(i)->GetDefault());
    for (int i = 0; i < legacyParamCount; ++i)
    {
      int paramIdx = i;
      if (legacyParamCount == 11)
        paramIdx = kV03ParamMap[i];
      else if (legacyParamCount == 17)
        paramIdx = kV04ParamMap[i];
      GetParam(paramIdx)->Set(values[i]);
    }

    mHarmonics.fill(0.f);
    mHarmonics[0] = 1.f;
    if (GetParam(kParamWaveMode)->Bool())
      AnalyzeHarmonics(vals01, mHarmonics.data());
    OnParamReset(kPresetRecall);
    LEAVE_PARAMS_MUTEX
    return startPos;
  }

  for (int i = 0; i < kNumHarmonics; ++i)
  {
    double value = 0.0;
    startPos = chunk.Get(&value, startPos);
    if (startPos < 0)
      return -1;
    mHarmonics[i] = static_cast<float>(std::clamp(value, 0.0, 1.0));
  }

#if IPLUG_DSP
  mDSP.UpdateWavetable(vals01, kWavetableSize);
#endif

  return UnserializeParams(chunk, startPos);
}
