#version 330 core
in vec2 chUV;
out vec4 outCol;
uniform vec4 uColor;
uniform bool uTextured;
uniform sampler2D uTex;
void main() {
    outCol = uTextured ? texture(uTex,chUV)*uColor : uColor;
}
