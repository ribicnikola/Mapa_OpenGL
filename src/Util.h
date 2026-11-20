#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
unsigned int createShader(const char* vertPath,const char* fragPath);
unsigned int loadImageToTexture(const char* path,int* width,int* height);
GLFWcursor* loadImageToCursor(const char* path);
