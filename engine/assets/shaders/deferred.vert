#version 330 core
layout (location = 0) in vec3 aPos;
// Note: our Vertex layout has UV at location 2
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;

void main()
{
    TexCoords = aTexCoords;
    gl_Position = vec4(aPos, 1.0);
}
