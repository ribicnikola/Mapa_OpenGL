#include "Util.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

static std::string readText(const char* path) {
    std::ifstream file(path);
    if(!file){std::cerr<<"Ne mogu da otvorim sejder: "<<path<<'\n';return {};}
    std::ostringstream s;s<<file.rdbuf();return s.str();
}
static GLuint compileOne(GLenum type,const std::string& code) {
    GLuint sh=glCreateShader(type);const char* p=code.c_str();glShaderSource(sh,1,&p,nullptr);
    glCompileShader(sh);GLint status=GL_FALSE;glGetShaderiv(sh,GL_COMPILE_STATUS,&status);
    if(!status){char log[2048]={};glGetShaderInfoLog(sh,sizeof(log),nullptr,log);
        std::cerr<<"Greska u sejderu: "<<log<<'\n';glDeleteShader(sh);return 0;}
    return sh;
}
unsigned int createShader(const char* vp,const char* fp) {
    std::string v=readText(vp),f=readText(fp);if(v.empty()||f.empty())return 0;
    GLuint vs=compileOne(GL_VERTEX_SHADER,v),fs=compileOne(GL_FRAGMENT_SHADER,f);
    if(!vs||!fs){glDeleteShader(vs);glDeleteShader(fs);return 0;}
    GLuint program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);
    glLinkProgram(program);GLint ok=GL_FALSE;glGetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok){char log[2048]={};glGetProgramInfoLog(program,sizeof(log),nullptr,log);
        std::cerr<<"Greska linkovanja: "<<log<<'\n';glDeleteProgram(program);program=0;}
    glDeleteShader(vs);glDeleteShader(fs);return program;
}
unsigned int loadImageToTexture(const char* path,int* w,int* h) {
    int width=0,height=0,channels=0;
    // stb ucitava redove od vrha. OpenGL UV (0,0) je dno: preokrecemo sliku.
    stbi_set_flip_vertically_on_load(1);
    unsigned char* bytes=stbi_load(path,&width,&height,&channels,4);
    if(!bytes){std::cerr<<"Ne mogu da ucitam teksturu: "<<path<<'\n';return 0;}
    GLuint id=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
    glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,bytes);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    stbi_image_free(bytes);if(w)*w=width;if(h)*h=height;return id;
}
GLFWcursor* loadImageToCursor(const char* path) {
    int w,h,n;stbi_set_flip_vertically_on_load(0);
    unsigned char* bytes=stbi_load(path,&w,&h,&n,4);
    if(!bytes){std::cerr<<"Ne mogu da ucitam kursor: "<<path<<'\n';return nullptr;}
    GLFWimage img{w,h,bytes};
    // Vrh kompasa je klik tacka, crvena igla je usmerena gore-levo.
    GLFWcursor* result=glfwCreateCursor(&img,w/2,h/2);
    stbi_image_free(bytes);return result;
}
