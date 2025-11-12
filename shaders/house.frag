#version 450
#extension GL_ARB_separate_shader_objects : enable

// Matches the engine's MeshUBO layout used by model shaders
layout(binding = 0) uniform UniformBufferObject
{
    mat4    model;
    mat4    view;
    mat4    proj;
    vec4    color;      // color modulation (rgba)
    vec4    camera;     // camera position (xyz)
    vec4    lightPos;   // world-space light position (xyz)
    vec4    lightColor; // rgb light color
} ubo;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec4 colorMod;
layout(location = 2) in vec3 fragNormal;
layout(location = 3) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

void main()
{
    vec4 texColor = texture(texSampler, fragTexCoord);
    vec3 normal = normalize(fragNormal);
    vec3 lightDirection = normalize(ubo.lightPos.xyz - fragWorldPos);
    vec3 viewDirection = normalize(ubo.camera.xyz - fragWorldPos);
    vec3 halfVector = normalize(lightDirection + viewDirection);

    float diffuseStrength = max(dot(normal, lightDirection), 0.0);
    float specularStrength = pow(max(dot(normal, halfVector), 0.0), 16.0);

    // Slightly warmer ambient for houses
    vec3 ambient = 0.25 * texColor.rgb;
    vec3 diffuse = diffuseStrength * texColor.rgb;
    vec3 specular = specularStrength * vec3(0.25);

    vec3 lightTint = ubo.lightColor.rgb;
    vec3 colorTint = colorMod.rgb * ubo.color.rgb;
    vec3 litColor = (ambient + diffuse) * lightTint + specular * lightTint;

    outColor = vec4(litColor * colorTint, texColor.a * colorMod.a * ubo.color.a);
}
