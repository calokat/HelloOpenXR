#version 430
layout (location=0) in vec3 in_position;
layout (location=2) in vec2 aTexCoord;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 TexCoord;
void main()
{
    gl_Position = projection * view * model * vec4(in_position, 1);
    TexCoord = aTexCoord;
}
