#version 330 core
out vec4 FragColor;
in vec2 TexCoords;

uniform sampler2D colorTex;
uniform sampler2D depthTex;
uniform float near;
uniform float far;

float linearizeDepth(float d)
{
    return (2.0 * near * far) / (far + near - d * (far - near));
}

// Smooth blur in a spiral pattern — avoids grid artifacts
vec3 spiralBlur(sampler2D tex, vec2 uv, vec2 texelSize, float radius, int samples)
{
    vec3 col   = vec3(0.0);
    float total = 0.0;
    float angle = 2.399; // golden angle in radians

    for (int i = 0; i < samples; i++)
    {
        float fi = float(i);
        float r  = radius * sqrt(fi / float(samples));
        float a  = fi * angle;
        vec2  offset = vec2(cos(a), sin(a)) * r * texelSize;
        float weight = 1.0 - (r / radius); // closer samples weighted more
        col   += texture(tex, uv + offset).rgb * weight;
        total += weight;
    }
    return col / total;
}

// Directional smear along a random angle — creates brush stroke feel
vec3 directionalSmear(sampler2D tex, vec2 uv, vec2 texelSize, float length, float angle, int samples)
{
    vec3  col   = vec3(0.0);
    vec2  dir   = vec2(cos(angle), sin(angle));
    for (int i = 0; i < samples; i++)
    {
        float t      = (float(i) / float(samples - 1)) - 0.5;
        vec2  offset = dir * t * length * texelSize;
        float weight = 1.0 - abs(t) * 2.0; // center weighted
        col += texture(tex, uv + offset).rgb * max(weight, 0.1);
    }
    return col / float(samples);
}

// Pseudo random
float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec3 quantizeColor(vec3 color, float levels)
{
    return floor(color * levels + 0.5) / levels;
}

void main()
{
    vec2  texSize   = vec2(textureSize(colorTex, 0));
    vec2  texelSize = 1.0 / texSize;

    float d       = texture(depthTex, TexCoords).r;
    float linearD = linearizeDepth(d);
    float t       = clamp(linearD / 8.0, 0.0, 1.0);
    // Smooth t so transition isn't abrupt
    t = smoothstep(0.0, 1.0, t);

    // Base blur — grows a lot with distance
    // Controls softness of the base layer. 1.0 = sharp up close, 18.0 = very soft at distance. 
    // Raise the max for more painterly smearing at distance, raise the min to make everything softer.
    float blurRadius = mix(14.0, 18.0, t);
    vec3  blurred    = spiralBlur(colorTex, TexCoords, texelSize, blurRadius, 32);

    // Directional smear on top — stroke angle varies per region smoothly
    // Size of the stroke direction regions. Bigger = larger areas share the same stroke angle = bigger coherent brush strokes.
    // Smaller = more chaotic varied directions.
    float regionScale = mix(0.1, 0.1, t);
    vec2  regionUV    = floor(TexCoords * texSize / regionScale) / (texSize / regionScale);
    float strokeAngle = hash(regionUV) * 3.14159 * 2.0;
    
    // Length of the directional smear strokes. Higher max = longer more visible streaks at distance. 
    // Raise the min to get stroke streaking even up close.
    float strokeLen   = mix(20.0, 30.0, t);
    vec3  smeared     = directionalSmear(colorTex, TexCoords, texelSize, strokeLen, strokeAngle, 16);

    // Blend blur and smear — more smear at distance
    vec3 painted = mix(blurred, smeared, mix(0.6, 1.0, t));

    // Color quantization — fewer levels = more abstract
    float levels = mix(8.0, 4.0, t);
    painted      = quantizeColor(painted, levels);

    FragColor = vec4(painted, 1.0);
}