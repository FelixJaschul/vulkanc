#version 450

layout(location = 0) in vec3 position;
layout(location = 1) in vec2 tex_coord;
layout(location = 2) in vec4 color;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
} push_constants;

layout(location = 0) out vec2 frag_tex_coord;
layout(location = 1) out vec4 frag_color;

void main() {
    gl_Position = push_constants.mvp * vec4(position, 1.0);
    frag_tex_coord = tex_coord;
    frag_color = color;
}