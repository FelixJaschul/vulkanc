#version 450

layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_color;

layout(location = 0) out vec2 frag_uv;
layout(location = 1) out vec4 frag_color;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    vec4 tint_color;
    float tiling;
} pc;

void main()
{
    gl_Position = pc.mvp * vec4(in_pos, 1.0);
    frag_uv = in_uv;
    frag_color = in_color;
}
