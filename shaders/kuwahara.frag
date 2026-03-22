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

vec3 structureTensor(sampler2D tex, vec2 uv, vec2 texelSize)
{
    float tl = dot(texture(tex, uv + vec2(-1,-1)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float tm = dot(texture(tex, uv + vec2( 0,-1)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float tr = dot(texture(tex, uv + vec2( 1,-1)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float ml = dot(texture(tex, uv + vec2(-1, 0)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float mr = dot(texture(tex, uv + vec2( 1, 0)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float bl = dot(texture(tex, uv + vec2(-1, 1)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float bm = dot(texture(tex, uv + vec2( 0, 1)*texelSize).rgb, vec3(0.299,0.587,0.114));
    float br = dot(texture(tex, uv + vec2( 1, 1)*texelSize).rgb, vec3(0.299,0.587,0.114));

    float gx = -tl - 2.0*ml - bl + tr + 2.0*mr + br;
    float gy = -tl - 2.0*tm - tr + bl + 2.0*bm + br;

    return vec3(gx*gx, gx*gy, gy*gy);
}

vec3 smoothTensor(sampler2D tex, vec2 uv, vec2 texelSize, int radius)
{
    vec3  sum   = vec3(0.0);
    float total = 0.0;
    for (int y = -radius; y <= radius; y++)
    for (int x = -radius; x <= radius; x++)
    {
        float w = exp(-float(x*x + y*y) * 0.5);
        sum   += structureTensor(tex, uv + vec2(x,y)*texelSize, texelSize) * w;
        total += w;
    }
    return sum / total;
}

vec3 anisotropicKuwahara(sampler2D tex, vec2 uv, vec2 texelSize, float radius)
{
    vec3 T = smoothTensor(tex, uv, texelSize, 4);
    float Jxx = T.x, Jxy = T.y, Jyy = T.z;

    float tmp = sqrt((Jxx - Jyy)*(Jxx - Jyy) + 4.0*Jxy*Jxy);
    float L1  = 0.5*(Jxx + Jyy + tmp);
    float L2  = 0.5*(Jxx + Jyy - tmp);

    float A     = (L1 + L2 < 0.0001) ? 0.0 : (L1 - L2) / (L1 + L2);
    float angle = 0.5 * atan(2.0*Jxy, Jxx - Jyy);

    float stretch   = mix(1.0, 4.0, A);
    vec2  majorAxis = vec2(cos(angle), sin(angle)) * radius * stretch;
    vec2  minorAxis = vec2(-sin(angle), cos(angle)) * radius / stretch;

    const int SECTORS = 8;
    vec3  sectorMean[8];
    float sectorVar[8];

    vec3  center    = texture(tex, uv).rgb;
    float centerLum = dot(center, vec3(0.299, 0.587, 0.114));

    for (int s = 0; s < SECTORS; s++)
    {
        float sAngle = float(s) * 3.14159 * 2.0 / float(SECTORS);
        vec3  mean   = vec3(0.0);
        vec3  meanSq = vec3(0.0);
        float count  = 0.0;

        int steps = 10;
        for (int i = 0; i <= steps; i++)
        for (int j = 0; j <= steps; j++)
        {
            float u  = float(i) / float(steps);
            float v  = float(j) / float(steps);
            float sa = sAngle + v * (3.14159 * 2.0 / float(SECTORS));
            float sr = u;

            vec2 localOffset = vec2(cos(sa), sin(sa)) * sr;
            vec2 offset      = localOffset.x * majorAxis + localOffset.y * minorAxis;

            vec3  c   = texture(tex, uv + offset * texelSize).rgb;
            float lum = dot(c, vec3(0.299, 0.587, 0.114));

            // Down-weight samples much darker than center
            float darkPenalty = smoothstep(0.0, 0.3, lum / max(centerLum, 0.001));
            float w = darkPenalty;

            mean   += c * w;
            meanSq += c * c * w;
            count  += w;
        }

        // Avoid divide by zero if all samples were rejected
        if (count < 0.0001)
        {
            sectorMean[s] = center;
            sectorVar[s]  = 1e9; // high variance = won't be picked
        }
        else
        {
            mean   /= count;
            meanSq /= count;
            sectorMean[s] = mean;
            sectorVar[s]  = dot(meanSq - mean*mean, vec3(1.0));
        }
    }

    float minVar = 1e9;
    vec3  result = center; // fallback to center if everything is rejected
    for (int s = 0; s < SECTORS; s++)
    {
        if (sectorVar[s] < minVar)
        {
            minVar = sectorVar[s];
            result = sectorMean[s];
        }
    }

    return result;
}

vec3 quantizeColor(vec3 col, float levels)
{
    return floor(col * levels + 0.5) / levels;
}

void main()
{
    vec2  texSize   = vec2(textureSize(colorTex, 0));
    vec2  texelSize = 1.0 / texSize;

    float d       = texture(depthTex, TexCoords).r;
    float linearD = linearizeDepth(d);
    float t       = smoothstep(0.0, 1.0, clamp(linearD / 8.0, 0.0, 1.0));

    float radius = mix(10.0, 22.0, t);
    vec3 painted = anisotropicKuwahara(colorTex, TexCoords, texelSize, radius);

    float levels = mix(10.0, 3.0, t);
    painted      = quantizeColor(painted, levels);

    vec3 original = texture(colorTex, TexCoords).rgb;
    painted       = mix(original, painted, mix(0.9, 1.0, t));

    FragColor = vec4(painted, 1.0);
}