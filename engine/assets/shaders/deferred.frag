#version 330 core
out vec4 FragColor;

in vec2 TexCoords;

uniform sampler2D gAlbedo;
uniform sampler2D gNormal;
uniform sampler2D gMaterial;
// Note: gDepth is not explicitly sampled in basic deferred unless we need position reconstruction.
// We can reconstruct view-space position from depth, or just pass FragPos in another RT.
// Since we only have 3 RTs, let's reconstruct FragPos from Depth!
uniform sampler2D gDepth;

uniform mat4 projection;
uniform mat4 inverseProj;

// Lighting
uniform vec3 lightPos[3]; // View-space light positions!
uniform vec3 lightColor[3];

const float PI = 3.14159265359;

// (Skipping full Cook-Torrance BRDF for brevity here; we will implement SSAO in next step. 
// For now, let's do simple BRDF)
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
    
    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
    
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;
    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

void main()
{
    // Retrieve data from G-buffer
    vec3 albedo = texture(gAlbedo, TexCoords).rgb;
    vec3 Normal = texture(gNormal, TexCoords).rgb;
    vec4 material = texture(gMaterial, TexCoords);
    float metallic = material.r;
    float roughness = material.g;
    float ao = material.b;

    // Reconstruct view-space position from depth
    float depth = texture(gDepth, TexCoords).r;
    float ndcDepth = depth * 2.0 - 1.0;
    vec4 clipSpacePos = vec4(TexCoords * 2.0 - 1.0, ndcDepth, 1.0);
    vec4 viewSpacePos = inverseProj * clipSpacePos;
    vec3 FragPos = viewSpacePos.xyz / viewSpacePos.w;

    // Since lighting is in view-space, View position is (0,0,0)
    vec3 V = normalize(-FragPos);
    vec3 N = normalize(Normal);

    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 Lo = vec3(0.0);
    for(int i = 0; i < 3; ++i) 
    {
        vec3 L = normalize(lightPos[i] - FragPos);
        vec3 H = normalize(V + L);
        
        float dist = length(lightPos[i] - FragPos);
        float attenuation = 1.0 / (dist * dist);
        vec3 radiance = lightColor[i] * attenuation;        

        float NDF = DistributionGGX(N, H, roughness);        
        float G = GeometrySmith(N, V, L, roughness);      
        vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);       

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        vec3 numerator = NDF * G * F;
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;  
            
        float NdotL = max(dot(N, L), 0.0);                
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }   

    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    FragColor = vec4(color, 1.0);
}
