#version 440

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

layout(location = 0) in highp vec4 position;
layout(location = 1) in highp vec2 texcoord;
layout(location = 0) out highp vec2 vTexcoord;

void main() {
    vTexcoord = texcoord;
    gl_Position = ubuf.matrix * position;
}
