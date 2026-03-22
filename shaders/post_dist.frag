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

    // First, snap UV to the block center using a base pixel size
    // We use maxPixelSize blocks to find the center — coarse grid
    vec2 blockUV = floor(TexCoords * texSize / maxPixelSize) * maxPixelSize / texSize;
    vec2 blockCenter = blockUV + (maxPixelSize * 0.5) * texelSize;

    // Sample depth at block center
    float d = texture(depthTex, blockCenter).r;
    float linearD = linearizeDepth(d);

    float t = smoothstep(0.0, 1.0, clamp(linearD / 20.0, 0.0, 1.0));
    float pixelSize = mix(minPixelSize, maxPixelSize, t);

    // Now pixelate color using that block's depth-driven pixel size
    vec2 pixelatedUV = floor(TexCoords * texSize / pixelSize) * pixelSize / texSize;

    FragColor = texture(colorTex, pixelatedUV);
}