#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject
{
    mat4    model;
    mat4    view;
    mat4    proj;
    vec4    color;
    vec4    camera;
    vec4    lightPos;
    vec4    lightColor;
} ubo;

out gl_PerVertex
{
    vec4 gl_Position;
};

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexCoord;

layout(location = 0) out vec2 fragTexCoord;
layout(location = 1) out vec4 colorMod;
layout(location = 2) out vec3 fragNormal;
layout(location = 3) out vec3 fragWorldPos;

void main()
{
    vec4 worldPosition = ubo.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(ubo.model)));

    fragWorldPos = worldPosition.xyz;
    fragNormal = normalize(normalMatrix * inNormal);
    colorMod = ubo.color;
    fragTexCoord = inTexCoord;

    gl_Position = ubo.proj * ubo.view * worldPosition;
}
