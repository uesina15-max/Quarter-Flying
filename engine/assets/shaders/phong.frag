#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;

#define MAX_LIGHTS 3

uniform vec3 lightPos[MAX_LIGHTS];
uniform vec3 lightColor[MAX_LIGHTS];
uniform vec3 viewPos;
uniform vec3 objectColor;

void main() {
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);
    
    vec3 result = vec3(0.0);
    
    for(int i = 0; i < MAX_LIGHTS; ++i) {
        // Ambient
        float ambientStrength = 0.15;
        vec3 ambient = ambientStrength * lightColor[i];

        // Diffuse
        vec3 lightDir = normalize(lightPos[i] - FragPos);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = diff * lightColor[i];

        // Specular
        float specularStrength = 0.5;
        vec3 halfDir = normalize(lightDir + viewDir);
        float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);
        vec3 specular = specularStrength * spec * lightColor[i];

        result += (ambient + diffuse + specular);
    }

    vec3 linearColor = result * objectColor;
    
    // Gamma correction
    vec3 gamma = vec3(1.0 / 2.2);
    FragColor = vec4(pow(linearColor, gamma), 1.0);
}
