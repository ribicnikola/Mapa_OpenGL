#version 330 core
layout(location=0) in vec2 inPos;
layout(location=1) in vec2 inUV;
uniform vec2 uScreen;
uniform vec2 uPos;
uniform vec2 uAxisX;
uniform vec2 uAxisY;
uniform vec2 uUvMin;
uniform vec2 uUvMax;
out vec2 chUV;
void main() {
    vec2 pixel = uPos + inPos.x*uAxisX + inPos.y*uAxisY;
    gl_Position = vec4(pixel.x/uScreen.x*2.0-1.0, 1.0-pixel.y/uScreen.y*2.0,0.0,1.0);
    chUV = vec2(mix(uUvMin.x,uUvMax.x,inUV.x),1.0-mix(uUvMin.y,uUvMax.y,inUV.y));
}
