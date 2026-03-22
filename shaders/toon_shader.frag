#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoords;

uniform sampler2D texture_diffuse1;
uniform vec3 lightPos;

const int BANDS = 4;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(vec3(1.0, 1.0, 1.0));

    float diff = max(dot(norm, lightDir), 0.0);
    float stepped = floor(diff * float(BANDS)) / float(BANDS);

    vec4 texColor = texture(texture_diffuse1, TexCoords);

    vec3 ambient = 0.15 * texColor.rgb;
    vec3 result = ambient + texColor.rgb * stepped;

    FragColor = vec4(result, texColor.a);
}