#version 440

// High detail stacked waveform: rendergraph port of res/shaders/stackedsignal.frag
// (without its disabled drawBorder branches). See waveformtexturedfiltered.frag
// for the texture layout and conventions.

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

    float currentIndex =
            floor(ubuf.firstVisualIndex + uv.x * (ubuf.lastVisualIndex - ubuf.firstVisualIndex)) * 2.0;
    // Bottom half shows the right channel.
    if (uv.y < 0.5) {
        currentIndex += 1.0;
    }

    vec4 outputColor = vec4(0.0);
    bool showing = false;
    vec4 showingColor = vec4(0.0);
    vec4 mixColor = vec4(0.0);
    float alpha = 0.75;

    if (currentIndex >= 0.0 && currentIndex <= ubuf.waveformLength - 1.0) {
        // The (low, mid, high) magnitude is the waveform height; rescale the
        // maximum height to 1.
        const float scaleFactor = 1.0 / sqrt(3.0);

        vec4 dataUnscaled = getWaveformData(currentIndex) * ubuf.allGain;
        dataUnscaled.xyz *= scaleFactor;

        vec4 data = dataUnscaled;
        data.x *= ubuf.lowGain;
        data.y *= ubuf.midGain;
        data.z *= ubuf.highGain;

        vec3 dataTop = max(data.xyz, dataUnscaled.xyz);

        // [0, 1] distance of this pixel from the centre line.
        float ourDistance = abs((uv.y - 0.5) * 2.0);

        showing = true;
        if (ourDistance <= data.x) {
            showingColor = ubuf.lowColor;
        } else if (ourDistance <= dataUnscaled.x) {
            showingColor = ubuf.lowFilteredColor;
            alpha = 0.6;
        } else if (ourDistance <= dataTop.x + data.y) {
            showingColor = ubuf.midColor;
        } else if (ourDistance <= dataTop.x + dataTop.y) {
            showingColor = ubuf.midFilteredColor;
            alpha = 0.6;
        } else if (ourDistance <= dataTop.x + dataTop.y + data.z) {
            showingColor = ubuf.highColor;
        } else if (ourDistance <= dataTop.x + dataTop.y + dataUnscaled.z) {
            showingColor = ubuf.highFilteredColor;
            alpha = 0.6;
        } else {
            showing = false;
        }

        // Combine the band colours by the unfiltered levels for the tone.
        mixColor = ubuf.lowColor * dataUnscaled.x +
                ubuf.midColor * dataUnscaled.y +
                ubuf.highColor * dataUnscaled.z;
        float showingMax = max(mixColor.x, max(mixColor.y, mixColor.z));
        if (showingMax > 0.0) {
            mixColor = mixColor / showingMax;
        }
        mixColor.w = 1.0;
    }

    // Axis, two device pixels tall, as the lowest layer.
    if (abs(uv.y - 0.5) * ubuf.size.y <= 1.0) {
        outputColor.xyz = mix(outputColor.xyz, ubuf.axesColor.xyz, ubuf.axesColor.w);
        outputColor.w = 1.0;
    }

    if (showing) {
        outputColor.xyz = mix(outputColor.xyz, showingColor.xyz, alpha);
        // Mix in the sum colour to smooth the look and give the general tone.
        outputColor.xyz = mix(outputColor.xyz, mixColor.xyz, 0.53);
        outputColor.w = 1.0;
    }

    // rendergraph blends premultiplied.
    fragColor = vec4(outputColor.xyz * outputColor.w, outputColor.w);
}
