#version 440

// High detail RGB waveform: rendergraph port of res/shaders/rgbsignal.frag.
// See waveformtexturedfiltered.frag for the texture layout and conventions.

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
    return texture(waveformTexture, (vec2(column, row) + 0.5) / ubuf.textureStride);
}

void main() {
    vec2 uv = vTexcoord;

    float indexRange = ubuf.lastVisualIndex - ubuf.firstVisualIndex;
    float currentIndex = floor(ubuf.firstVisualIndex + uv.x * indexRange) * 2.0;

    vec4 outputColor = vec4(0.0);
    bool showing = false;
    bool shadow = false;

    if (currentIndex >= 0.0 && currentIndex <= ubuf.waveformLength - 1.0) {
        vec4 dataUnscaled;
        if (ubuf.splitStereoSignal > 0.5) {
            // Bottom half shows the right channel.
            float stereoIndex = (uv.y < 0.5) ? currentIndex + 1.0 : currentIndex;
            dataUnscaled = getWaveformData(stereoIndex);
        } else {
            vec4 left = getWaveformData(currentIndex);
            vec4 right = getWaveformData(currentIndex + 1.0);
            dataUnscaled = max(left, right);
        }

        dataUnscaled *= ubuf.allGain;
        vec3 data = dataUnscaled.xyz * vec3(ubuf.lowGain, ubuf.midGain, ubuf.highGain);

        // [0, 1] distance of this pixel from the centre line.
        float ourDistance = abs(uv.y - 0.5) * 2.0;

        float sumUnscaled = dataUnscaled.x + dataUnscaled.y + dataUnscaled.z;
        float sumScaled = data.x + data.y + data.z;

        float signalDistance = dataUnscaled.w;
        if (sumUnscaled > 0.0) {
            signalDistance *= sumScaled / sumUnscaled;
        }

        showing = signalDistance >= ourDistance;
        shadow = !showing && (dataUnscaled.w >= ourDistance);

        // Combine the band colours by the band levels.
        if (showing) {
            outputColor = ubuf.lowColor * data.x +
                    ubuf.midColor * data.y +
                    ubuf.highColor * data.z;
        } else if (shadow) {
            outputColor = ubuf.lowColor * dataUnscaled.x +
                    ubuf.midColor * dataUnscaled.y +
                    ubuf.highColor * dataUnscaled.z;
        }

        float maxComponent = max(outputColor.x, max(outputColor.y, outputColor.z));
        if (maxComponent > 0.0) {
            outputColor.xyz /= maxComponent;
        }
    }

    if (showing) {
        outputColor.w = 1.0;
    } else if (abs(uv.y - 0.5) * ubuf.size.y <= 1.0) {
        // Axis, two device pixels tall, as the lowest layer.
        outputColor = ubuf.axesColor;
    } else if (shadow) {
        outputColor.w = 0.4;
    }

    // rendergraph blends premultiplied.
    fragColor = vec4(outputColor.xyz * outputColor.w, outputColor.w);
}
