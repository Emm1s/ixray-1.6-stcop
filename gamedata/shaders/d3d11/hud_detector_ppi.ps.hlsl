#include "common.hlsli"

// Engine: Blender_Recorder_StandartBinding (ppi_radar_params). x = sweep phase scale, y = beam touch (rad), zw reserved.
uniform float4 ppi_radar_params;

struct ui_vert_out
{
    float2 tc0 : TEXCOORD0;
};

Texture2D s_noise;
Texture2D s_data;

static const float kPi = 3.14159265359f;
static const float kTwoPi = 6.28318530718f;
// Procedural marker strength (addon PPI). Main game keeps palette; addon uses shader icons from s_data.
static const float kPpiShaderBlipFromDataWeight = 1.0f;
static const float kPpiMarkerBrightnessMul = 11.0f;

// Map angle to (-pi, pi]; branchless fmod fixup (avoids divergent if in per-pixel hot path).
float WrapAngleMinusPiPi(float angle)
{
    float w = fmod(angle + kPi, kTwoPi);
    w += kTwoPi * (1.0f - step(0.0f, w));
    return w - kPi;
}

float4 main(ui_vert_out input) : SV_Target
{
    float2 uv = input.tc0;
    float2 centeredUv = uv * 2.0f - 1.0f;

    float radius = length(centeredUv);
    if (radius > 1.0f)
    {
        discard;
    }

    float4 baseColor = s_base.Sample(smp_base, uv);
    float noise = s_noise.Sample(smp_base, uv * 1.35f + timers.xx * float2(0.11f, -0.07f)).r;

    const float sweepScale = ppi_radar_params.x;
    const float sweepSafe = lerp(0.6f, sweepScale, step(1e-5f, sweepScale));
    float sweepPhase = frac(timers.x * sweepSafe);
    float sweepAngle = sweepPhase * kTwoPi;
    float polarAngle = atan2(centeredUv.y, centeredUv.x);
    float deltaAngle = abs(WrapAngleMinusPiPi(polarAngle - sweepAngle));

    float beamCore = smoothstep(0.035f, 0.0f, deltaAngle);
    float beamTail = smoothstep(0.70f, 0.0f, deltaAngle);
    float radialFade = smoothstep(1.0f, 0.10f, radius);
    float sweepIntensity = saturate(beamCore * 1.38f + beamTail * 0.48f) * radialFade;

    float ring1 = smoothstep(0.008f, 0.0f, abs(radius - 0.33f));
    float ring2 = smoothstep(0.008f, 0.0f, abs(radius - 0.66f));
    float ring3 = smoothstep(0.010f, 0.0f, abs(radius - 0.92f));
    float gridIntensity = (ring1 + ring2 + ring3) * 0.18f;

    float noiseIntensity = (noise - 0.5f) * 0.08f;

    float3 sweepColor = float3(0.0f, 1.0f, 188.0f / 255.0f) * (1.58f * sweepIntensity);
    float3 gridColor = float3(0.01f, 0.20f, 0.05f) * gridIntensity;

    float3 blipAccum = 0.0f;

    static const int kPpiMaxPoints = 32;
    static const float kIconCoreR = 0.011f;
    static const float kIconHaloR = 0.034f;
    static const float kBlipVisEps = 1.0f / 255.0f;

    [unroll]
    for (int i = 0; i < kPpiMaxPoints; ++i)
    {
        float4 d = s_data.Load(int3(i, 0, 0));

        // Alpha: CPU steady envelope after first sweep hit (same phase scale as ppi_radar_params.x).
        float markerVis = d.a;
        // Mask instead of continue: keeps warps convergent (same control flow per pixel).
        float blipActive = step(kBlipVisEps, markerVis);

        float isZone = step(0.5f, d.b);
        float3 blipColor = lerp(float3(0.0f, 1.0f, 188.0f / 255.0f), float3(1.0f, 0.62f, 0.10f), isZone);

        float2 targetCenteredUv = d.rg * 2.0f - 1.0f;

        float dist = length(centeredUv - targetCenteredUv);
        float iconCore = smoothstep(kIconCoreR, 0.0f, dist);
        float iconHalo = smoothstep(kIconHaloR, kIconCoreR * 1.4f, dist) * (1.0f - smoothstep(kIconCoreR * 0.45f, 0.0f, dist));
        float iconBody = saturate(iconCore * 1.25f + iconHalo * 0.42f);

        float intensity = iconBody * markerVis * blipActive;
        blipAccum += blipColor * intensity;
    }

    float4 result = baseColor;
    result.rgb += sweepColor + gridColor + blipAccum * kPpiShaderBlipFromDataWeight * kPpiMarkerBrightnessMul + noiseIntensity;
    // noise is (tex - 0.5) * 0.08; on dark baseColor sum can dip slightly below 0 before gamma.
    result.rgb = max(result.rgb, 0.0f);
    result.rgb = PushGamma(result.rgb);

    return result;
}
