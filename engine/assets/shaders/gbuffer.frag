#version 330 core
layout (location = 0) out vec4 gAlbedo;
layout (location = 1) out vec4 gNormal;
layout (location = 2) out vec4 gMaterial;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform vec3 objectColor;
uniform float metallic;
uniform float roughness;
uniform float ao;

void main()
{
    float checker = mod(floor(TexCoords.x * 10.0) + floor(TexCoords.y * 10.0), 2.0);
    vec3 albedo = objectColor * mix(0.5, 1.0, checker);

    gAlbedo = vec4(albedo, 1.0);
    gNormal = vec4(normalize(Normal), 1.0);
    gMaterial = vec4(metallic, roughness, ao, 1.0);
}
