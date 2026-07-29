#version 330 core
// Instanced PBR Vertex Shader
// Supports per-instance model matrices and material parameters

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

// Instance attributes (divisor = 1)
layout (location = 3) in mat4 aModel;          // 4 vec4 attributes for mat4
layout (location = 7) in vec3 aInstanceColor;  // Instance-specific color
layout (location = 8) in float aRoughness;     // Instance-specific roughness
layout (location = 9) in float aMetallic;      // Instance-specific metallic
layout (location = 10) in uint aEntityId;       // Entity ID for picking
layout (location = 11) in uint aIsSelected;    // Selection flag

out vec3 WorldPos;
out vec3 Normal;
out vec2 TexCoords;
flat out uint InstanceId;  // flat to avoid interpolation
flat out uint IsSelected;  // flat to avoid interpolation

uniform mat4 view;
uniform mat4 projection;

void main()
{
    // Calculate world position using instance model matrix
    vec4 worldPos = aModel * vec4(aPos, 1.0);
    WorldPos = worldPos.xyz;
    
    // Calculate normal matrix in shader (inverse-transpose of model)
    // This saves vertex attribute slots and GPU bandwidth
    mat3 normalMatrix = transpose(inverse(mat3(aModel)));
    Normal = normalize(normalMatrix * aNormal);
    
    TexCoords = aTexCoords;
    
    // Pass instance data to fragment shader
    InstanceId = aEntityId;
    IsSelected = aIsSelected;

    gl_Position = projection * view * worldPos;
}
