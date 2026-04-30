#include "StdAfx.h"

#include "PpiRadarRuntime.h"

namespace
{
float SweepPhaseMatchTimersX(float gameTime, float sweepPhaseScale)
{
    const float v = gameTime * sweepPhaseScale;
    return v - floorf(v);
}

float Smoothstep01(float edge0, float edge1, float x)
{
    const float span = edge1 - edge0;
    if (fis_zero(span))
    {
        return x >= edge1 ? 1.0f : 0.0f;
    }
    const float t = clampr((x - edge0) / span, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float SafeBeamTouchAngleRad(float beamTouchAngleRad)
{
    const float minW = 1e-5f;
    return beamTouchAngleRad > minW ? beamTouchAngleRad : minW;
}

float WrapAngleMinusPiOpen(float angleRad)
{
    float a = angleRad;
    while (a > PI)
    {
        a -= PI_MUL_2;
    }
    while (a <= -PI)
    {
        a += PI_MUL_2;
    }
    if (a >= PI - 1e-5f)
    {
        a -= PI_MUL_2;
    }
    return a;
}

bool AngleInHighlightSector(float angleRad)
{
    const float a = WrapAngleMinusPiOpen(angleRad);
    return (a <= 0.0f && a >= -PI);
}

float SectorWeight01(float angleRad, float edgeRad)
{
    if (!AngleInHighlightSector(angleRad))
    {
        return 0.0f;
    }
    if (edgeRad <= 1e-5f)
    {
        return 1.0f;
    }
    const float a = WrapAngleMinusPiOpen(angleRad);
    float w = 1.0f;
    if (a < -PI + edgeRad)
    {
        w *= Smoothstep01(-PI, -PI + edgeRad, a);
    }
    if (a > -edgeRad)
    {
        w *= 1.0f - Smoothstep01(-edgeRad, 0.0f, a);
    }
    return clampr(w, 0.0f, 1.0f);
}

float MarkerEnvelopeSteady01(float age, float flashSec, float peakSec, float fadeSec, float lingerAlpha)
{
    if (age < 0.0f)
    {
        return 0.0f;
    }

    const float la = clampr(lingerAlpha, 0.0f, 1.0f);
    float sf = flashSec;
    float sp = peakSec;
    float sd = fadeSec;
    const float sum = sf + sp + sd;
    const float maxBurst = 2.0f;
    if (sum > maxBurst && sum > 1e-5f)
    {
        const float g = maxBurst / sum;
        sf *= g;
        sp *= g;
        sd *= g;
    }

    const float flashEnd = sf;
    const float peakEnd = flashEnd + sp;
    const float fadeEnd = peakEnd + sd;

    if (age < flashEnd)
    {
        return Smoothstep01(0.0f, flashEnd, age);
    }
    if (age < peakEnd)
    {
        return 1.0f;
    }
    if (age < fadeEnd)
    {
        const float span = fadeEnd - peakEnd;
        const float u = fis_zero(span) ? 1.0f : Smoothstep01(0.0f, span, age - peakEnd);
        return 1.0f * (1.0f - u) + la * u;
    }
    return la;
}
} // namespace

bool PpiRadarRuntime::IsDetectorPpiShaderName(pcstr shaderName)
{
    if (shaderName == nullptr || shaderName[0] == 0)
    {
        return false;
    }

    if (0 == _stricmp(shaderName, "hud\\detector_ppi"))
    {
        return true;
    }
    if (0 == _stricmp(shaderName, "hud/detector_ppi"))
    {
        return true;
    }

    return false;
}

void PpiRadarRuntime::BuildInvViewMapFromCameraYaw(Fmatrix& invMap)
{
    float h = 0.0f;
    float p = 0.0f;
    Device.vCameraDirection.getHP(h, p);
    Fmatrix Mc;
    Mc.setHPB(h, 0.0f, 0.0f);
    Mc.c.set(Device.vCameraPosition);
    invMap.invert(Mc);
}

float PpiRadarRuntime::ComputeMarkerVisibility(float gameTime, float targetAngleRad, float& lastHitTime, const SPpiRadarMarkerParams& params)
{
    const float sweepAngle = SweepPhaseMatchTimersX(gameTime, params.sweepPhaseScale) * PI_MUL_2;
    const float angDist = angle_difference(sweepAngle, targetAngleRad);
    const float touch = 1.0f - clampr(angDist / SafeBeamTouchAngleRad(params.beamTouchAngleRad), 0.0f, 1.0f);

    if (lastHitTime < 0.0f && touch > params.touchThreshold)
    {
        lastHitTime = gameTime;
    }

    if (lastHitTime < 0.0f)
    {
        return 0.0f;
    }

    const float age = gameTime - lastHitTime;
    const float sectorW = SectorWeight01(targetAngleRad, params.sectorSoftRad);
    const float env = MarkerEnvelopeSteady01(age, params.markerFlashSec, params.peakSec, params.fadeSec, params.lingerAlpha);
    const float waveOsc = sinf(age * params.waveFreq) * params.waveAmp * expf(-age * params.waveDecay);
    const float gated = sectorW * (env + waveOsc);
    const float touchBoost = touch * params.touchBlend;
    const float blended = gated > touchBoost ? gated : touchBoost;
    return clampr(blended, 0.0f, 1.0f);
}

