#pragma once

struct SPpiRadarMarkerParams
{
    float sweepPhaseScale = 0.6f;
    float beamTouchAngleRad = 0.10f;
    float markerFlashSec = 0.055f;
    float peakSec = 0.035f;
    float fadeSec = 0.14f;
    float lingerAlpha = 0.22f;
    float sectorSoftRad = 0.07f;
    float waveFreq = 52.0f;
    float waveAmp = 0.11f;
    float waveDecay = 8.0f;
    float touchThreshold = 0.22f;
    float touchBlend = 0.92f;
};

namespace PpiRadarRuntime
{
bool IsDetectorPpiShaderName(pcstr shaderName);
void BuildInvViewMapFromCameraYaw(Fmatrix& invMap);
float ComputeMarkerVisibility(float gameTime, float targetAngleRad, float& lastHitTime, const SPpiRadarMarkerParams& params);
}

