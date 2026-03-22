#version 330 core
out vec4 FragColor;

uniform vec3 uColor;

void main()
{
    // Use the uniform color instead of the hardcoded numbers
    FragColor = vec4(uColor, 1.0); 
}