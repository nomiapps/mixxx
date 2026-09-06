#version 440

// High detail filtered waveform: rendergraph port of res/shaders/filteredsignal.frag.
// The waveform is a texture of (low, mid, high, all) bytes, one texel per
// visual sample, interleaved stereo. vTexcoord.x spans the visible range,
// vTexcoord.y is 1 at the top (left channel) and 0 at the bottom (right).

layout(std140, binding = 0) uniform buf {
    mat4 matrix;
    vec4 axesColor;
    vec4 lowColor;
    vec4 midColor;
    vec4 highColor;
    vec4 lowFilteredColor;
    vec4 midFilteredColor;
    vec4 highFilteredColor;
    vec2 size;
    float allGain;
    float lowGain;
    float midGain;
    float highGain;
    float firstVisualIndex;
    float lastVisualIndex;
    float waveformLength;
    float textureStride;
    float splitStereoSignal;
}
ubuf;

layout(binding = 1) uniform sampler2D waveformTexture;
layout(location = 0) in highp vec2 vTexcoord;
layout(location = 0) out highp vec4 fragColor;

vec4 getWaveformData(float index) {
    float row = floor(index / ubuf.textureStride);
    float column = index - row * ubuf.textureStride;
    // Sample at the texel centre; the texture is square (stride x stride).
    return texture(waveformTexture, (vec2(column, row) + 0.5) / ubuf.textureStride);
}

void main() {
    vec2 uv = vTexcoord;

    float currentIndex =
            floor(ubuf.firstVisualIndex + uv.x * (ubuf.lastVisualIndex - ubuf.firstVisualIndex)) * 2.0;
    // Bottom half shows the right channel.
    if (uv.y < 0.5) {
        currentIndex += 1.0;
    }

    vec4 outputColor = vec4(0.0);
    bool lowShowing = false;
    bool midShowing = false;
    bool highShowing = false;
    bool maxShowingUnscaled = false;
    // Keep going when the sample is out of range: the axis still draws.
    if (currentIndex >= 0.0 && currentIndex <= ubuf.waveformLength - 1.0) {
        vec4 dataUnscaled = getWaveformData(currentIndex) * ubuf.allGain;
        vec4 data = dataUnscaled;
        data.x *= ubuf.lowGain;
        data.y *= ubuf.midGain;
        data.z *= ubuf.highGain;

        // [0, 1] distance of this pixel from the centre line; a band shows
        // where its level reaches past that.
        float ourDistance = abs((uv.y - 0.5) * 2.0);
        vec4 signalDistance = data - ourDistance;
        lowShowing = signalDistance.x >= 0.0;
        midShowing = signalDistance.y >= 0.0;
        highShowing = signalDistance.z >= 0.0;
        maxShowingUnscaled = dataUnscaled.x - ourDistance >= 0.0 ||
                dataUnscaled.y - ourDistance >= 0.0 ||
                dataUnscaled.z - ourDistance >= 0.0;
    }

    // Axis, two device pixels tall, as the lowest layer.
    if (abs(uv.y - 0.5) * ubuf.size.y <= 1.0) {
        outputColor = ubuf.axesColor;
    } else if (maxShowingUnscaled) {
        outputColor.xyz = ubuf.axesColor.xyz;
        outputColor.w = ubuf.axesColor.w * 0.2;
    }

    if (lowShowing) {
        outputColor.xyz = mix(outputColor.xyz, ubuf.lowColor.xyz, 0.8);
        outputColor.w = 1.0;
    }
    if (midShowing) {
        outputColor.xyz = mix(outputColor.xyz, ubuf.midColor.xyz, 0.85);
        outputColor.w = 1.0;
    }
    if (highShowing) {
        outputColor.xyz = mix(outputColor.xyz, ubuf.highColor.xyz, 0.9);
        outputColor.w = 1.0;
    }

    // rendergraph blends premultiplied.
    fragColor = vec4(outputColor.xyz * outputColor.w, outputColor.w);
}
