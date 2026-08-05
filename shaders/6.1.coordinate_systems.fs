#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
in float TexID;

uniform sampler2D texDirt;
uniform sampler2D texStone;

void main()
{
    if (TexID < 0.5) {
        FragColor = texture(texDirt, TexCoord);
    }
    else {
        FragColor = texture(texStone, TexCoord);
    }
}