#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D colorTex;
uniform sampler2D depthTex;
uniform float near;
uniform float far;
uniform float minPixelSize;
uniform float maxPixelSize;

float linearizeDepth(float d)
{
    return (2.0 * near * far) / (far + near - d * (far - near));
}

void main()
{
    vec2 texSize = vec2(textureSize(colorTex, 0));
    vec2 texelSize = 1.0 / texSize;

    // Sample depth once at screen center — one value drives the whole frame
    float d = texture(depthTex, vec2(0.5, 0.5)).r;
    float linearD = linearizeDepth(d);

    float t = clamp(linearD / 20.0, 0.0, 1.0);
    float pixelSize = mix(minPixelSize, maxPixelSize, t);

    vec2 pixelatedUV = floor(TexCoords * texSize / pixelSize) * pixelSize / texSize;

    FragColor = texture(colorTex, pixelatedUV);
}