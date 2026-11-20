#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include "Util.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

static const char* STUDENT = "NIKOLA RIBIC -  SV41/2023";
// Geografske granice tacno ove mape
// Sirina: sever 45.34554228, jug 45.19620113; duzina: zapad 19.65375255, istok 20.01423661.
// Nisu kalibrisane rucno prema nasumicnim ulicama.
static constexpr double MAP_NORTH = 45.34554228;
static constexpr double MAP_SOUTH = 45.19620113;
static constexpr double MAP_WEST = 19.65375255;
static constexpr double MAP_EAST = 20.01423661;

// Brzina virtuelnog pomeranja kroz veliku mapu, a ne realna brzina pesaka.
static constexpr float MAP_NAVIGATION_METERS_PER_SEC = 400.0f;
static constexpr float PI = 3.14159265358979323846f;

struct Vec2 {
    float x = 0.f, y = 0.f;
};
static Vec2 operator+(Vec2 a, Vec2 b) { return { a.x + b.x, a.y + b.y }; }
static Vec2 operator-(Vec2 a, Vec2 b) { return { a.x - b.x, a.y - b.y }; }
static Vec2 operator*(Vec2 a, float f) { return { a.x * f, a.y * f }; }
static float length(Vec2 a) { return std::sqrt(a.x * a.x + a.y * a.y); }
static float clampf(float v, float lo, float hi) { 
    return std::max(lo, std::min(v, hi));
}
struct Color { 
    float r, g, b, a;
};
static constexpr Color WHITE{ 1,1,1,1 }, DARK{ .055f,.09f,.15f,.91f }, CYAN{ .1f,.82f,1.f,1.f };

struct App {
    GLFWwindow* window = nullptr;
    int fbW = 1280, fbH = 720;
    int windowW = 1280, windowH = 720;
    int mapW = 3000, mapH = 1600;
    GLuint shader = 0, vao = 0, vbo = 0, circleVao = 0, circleVbo = 0;
    GLuint mapTexture = 0, fontTexture = 0;
    GLFWcursor* compass = nullptr;
    bool walking = true;
    Vec2 walkCenter{ 0.5f, 0.5f };
    float walkedMeters = 0;
    std::vector<Vec2> points; // MERENJE: niz uredjenih tacaka; polilinija se racuna iz njega.
    double lastTime = 0;
    float zoom = 5.f; // vidljivo ~1/50 ukupne povrsine mape.
} g;

static Vec2 cursorPx() {
    double x = 0, y = 0;
    glfwGetCursorPos(g.window, &x, &y);
    return { float(x * g.fbW / std::max(1,g.windowW)),float(y * g.fbH / std::max(1,g.windowH)) };
}
static Vec2 mapSizeOnScreen() {
    float scale = std::min(float(g.fbW) / g.mapW, float(g.fbH) / g.mapH);
    return { g.mapW * scale, g.mapH * scale };
}
static Vec2 mapOrigin() {
    Vec2 s = mapSizeOnScreen();
    return { (g.fbW - s.x) * .5f,(g.fbH - s.y) * .5f };
}
static Vec2 toScreen(Vec2 uv) {
    Vec2 s = mapSizeOnScreen(), o = mapOrigin();
    return { o.x + uv.x * s.x,o.y + uv.y * s.y };
}
static bool fromScreen(Vec2 pixel, Vec2& uv) {
    Vec2 s = mapSizeOnScreen(), o = mapOrigin();
    uv = { (pixel.x - o.x) / s.x,(pixel.y - o.y) / s.y };
    return uv.x >= 0 && uv.x <= 1 && uv.y >= 0 && uv.y <= 1;
}
// Karta potice sa OpenStreetMap; vertikalna osa prati Web Mercator projekciju.
// UV: x=0 zapad, x=1 istok; y=0 sever, y=1 jug.
// Inverzna Mercator transformacija: iz koordinata slike do lat/lon.
static double rad(double deg) { 
    return deg * (3.14159265358979323846 / 180.0);
}
static double mercatorY(double latitudeDeg) {
    return std::log(std::tan(3.14159265358979323846 / 4.0 + rad(latitudeDeg) / 2.0));
}
struct GeoPoint { double latitude, longitude; };
static GeoPoint uvToGeo(Vec2 uv) {
    const double mercNorth = mercatorY(MAP_NORTH);
    const double mercSouth = mercatorY(MAP_SOUTH);
    const double merc = mercNorth + double(uv.y) * (mercSouth - mercNorth);
    const double latitude = (2.0 * std::atan(std::exp(merc)) - 3.14159265358979323846 / 2.0)
        * 180.0 / 3.14159265358979323846;
    const double longitude = MAP_WEST + double(uv.x) * (MAP_EAST - MAP_WEST);
    return { latitude,longitude }; 
}
static Vec2 geoToUv(double latitude, double longitude) {
    float u = float((longitude - MAP_WEST) / (MAP_EAST - MAP_WEST));
    float v = float((mercatorY(latitude) - mercatorY(MAP_NORTH)) /
        (mercatorY(MAP_SOUTH) - mercatorY(MAP_NORTH)));
    return { u,v };
}
// Haversine: najkraca udaljenost na Zemljinoj povrsini izmedju dve tacke.
// Ne meri automobilsku ili pesacku putanju kroz ulice.
static float segmentMeters(Vec2 a, Vec2 b) {
    GeoPoint p = uvToGeo(a), q = uvToGeo(b);
    const double dLat = rad(q.latitude - p.latitude);
    const double dLon = rad(q.longitude - p.longitude);
    const double h = std::pow(std::sin(dLat / 2.0), 2.0)
        + std::cos(rad(p.latitude)) * std::cos(rad(q.latitude))
        * std::pow(std::sin(dLon / 2.0), 2.0);
    return float(2.0 * 6371000.0 * std::asin(std::sqrt(std::clamp(h, 0.0, 1.0))));
}
static float totalMeters() { 
    float s = 0;
    for (size_t i = 1;i < g.points.size();++i)
        s += segmentMeters(g.points[i - 1], g.points[i]);
    return s;
}
static std::string distanceLabel(float m) {
    char b[100];
    if (m < 1000.f)
        std::snprintf(b, sizeof(b), "%.1f m", m);
    else 
        std::snprintf(b, sizeof(b), "%.3f km", m / 1000.f);
    return b;
}

static void keyCallback(GLFWwindow* w, int key, int, int action, int) {
    if (action != GLFW_PRESS)
        return;
    if (key == GLFW_KEY_ESCAPE)
        glfwSetWindowShouldClose(w, GL_TRUE);
    if (key == GLFW_KEY_R)
        g.walking = !g.walking; // Oba stanja ostaju u App strukturi.
}
static bool modeButtonHit(Vec2 p) { 
    return p.x >= 18 && p.x <= 250 && p.y >= 18 && p.y <= 76;
}
// Dugme za reset postoji samo u rezimu hodanja.
static bool resetDistanceHit(Vec2 p) {
    return p.x >= 18 && p.x <= 262 && p.y >= 92 && p.y <= 144;
}
static void mouseCallback(GLFWwindow*, int button, int action, int) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)return;
    Vec2 p = cursorPx();
    if (modeButtonHit(p)) {
        g.walking = !g.walking;
        return; 
    }
    if (g.walking && resetDistanceHit(p)) {
        g.walkedMeters = 0.f;
        return;
    }
    if (g.walking)return;
    Vec2 uv;
    if (!fromScreen(p, uv))
        return;
    // Klik na postojecu tacku brise je; susedi postaju automatski povezani.
    for (size_t i = 0;i < g.points.size();++i) {
        if (length(toScreen(g.points[i]) - p) <= 13.f) {
            g.points.erase(g.points.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
    g.points.push_back(uv);
}
static void resizeCallback(GLFWwindow*, int w, int h) {
    g.fbW = std::max(1, w);
    g.fbH = std::max(1, h);
    glViewport(0, 0, g.fbW, g.fbH);
}
static void windowSizeCallback(GLFWwindow*, int w, int h) { 
    g.windowW = std::max(1, w);
    g.windowH = std::max(1, h);
}

// Crtanje jednim VAO i VBO: svi objekti koriste isti jedini quad.
static void drawQuad(Vec2 p, Vec2 axisX, Vec2 axisY, Color col, GLuint tex = 0,
    Vec2 uvMin = { 0,0 }, Vec2 uvMax = { 1,1 }, bool circle = false) {
    glUseProgram(g.shader);
    glUniform2f(glGetUniformLocation(g.shader, "uScreen"), float(g.fbW), float(g.fbH));
    glUniform2f(glGetUniformLocation(g.shader, "uPos"), p.x, p.y);
    glUniform2f(glGetUniformLocation(g.shader, "uAxisX"), axisX.x, axisX.y);
    glUniform2f(glGetUniformLocation(g.shader, "uAxisY"), axisY.x, axisY.y);
    glUniform4f(glGetUniformLocation(g.shader, "uColor"), col.r, col.g, col.b, col.a);
    glUniform1i(glGetUniformLocation(g.shader, "uTextured"), tex != 0);
    glUniform2f(glGetUniformLocation(g.shader, "uUvMin"), uvMin.x, uvMin.y);
    glUniform2f(glGetUniformLocation(g.shader, "uUvMax"), uvMax.x, uvMax.y);
    glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D, tex);
    glBindVertexArray(circle ? g.circleVao : g.vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, circle ? 66 : 4);
}
static void rect(float x, float y, float w, float h, Color c) {
    drawQuad({ x,y }, { w,0 }, { 0,h }, c);
}
static void textureRect(float x, float y, float w, float h, GLuint tex, Color tint = WHITE, Vec2 a = { 0,0 }, Vec2 b = { 1,1 }) {
    drawQuad({ x,y }, { w,0 }, { 0,h }, tint, tex, a, b);
}
static void circle(float x, float y, float radius, Color c) {
    drawQuad({ x - radius,y - radius }, { radius * 2,0 }, { 0,radius * 2 }, c, 0, { 0,0 }, { 1,1 }, true);
}
static void line(Vec2 a, Vec2 b, float thickness, Color c) {
    Vec2 d = b - a;
    float len = length(d);
    if (len < .0001f)
        return;
    Vec2 n = { -d.y / len * thickness * .5f,d.x / len * thickness * .5f };
    drawQuad(a - n, d, n * 2.f, c);
}
// Atlas: ASCII 32..126, 16 kolona x 6 redova; svaka celija 32x48 piksela.
static void textAt(const std::string& str, float x, float y, float height, Color c = WHITE) {
    const float width = height * 32.f / 48.f;
    float startX = x;
    for (unsigned char ch : str) {
        if (ch == '\n') {
            x = startX;
            y += height;
            continue;
        }
        if (ch < 32 || ch>126)
            ch = '?';
        const int idx = int(ch) - 32, col = idx % 16, row = idx / 16;
        Vec2 a = { col / 16.f,row / 6.f };
        Vec2 b = { (col + 1) / 16.f,(row + 1) / 6.f };
        textureRect(x, y, width, height, g.fontTexture, c, a, b);
        x += width * .68f;
    }
}
static void modeSymbol(float cx, float cy, bool walk) {
    if (walk) {
        circle(cx, cy - 10, 5, WHITE);
        line({ cx,cy - 3 }, { cx,cy + 9 }, 3, WHITE);
        line({ cx,cy + 1 }, { cx - 8,cy + 6 }, 3, WHITE);
        line({ cx,cy + 1 }, { cx + 8,cy - 5 }, 3, WHITE);
        line({ cx,cy + 9 }, { cx - 7,cy + 20 }, 3, WHITE);
        line({ cx,cy + 9 }, { cx + 9,cy + 17 }, 3, WHITE);
    }
    else {
        rect(cx - 17, cy - 6, 34, 13, { .95f,.84f,.32f,1 });
        for (int i = 0;i < 6;++i)
            line({ cx - 13 + i * 5.2f,cy - 6 }, { cx - 13 + i * 5.2f,cy + (i % 2 ? 0 : 3) }, 1.6f, DARK);
    }
}
static void drawHud() {
    rect(14, 14, 252, 67, DARK);
    rect(18, 18, 244, 59, { .14f,.27f,.37f,.93f });
    modeSymbol(44, 43, g.walking);
    textAt(g.walking ? "REZIM: HODANJE" : "REZIM: MERENJE", 70, 25, 26);
    textAt("R / KLIK ZA PROMENU", 70, 51, 18, { .72f,.9f,.96f,1 });
    if (g.walking) {
        // Klik na dugme menja samo predjenu razdaljinu; pozicija pina ostaje ista.
        rect(14, 88, 252, 60, DARK);
        rect(18, 92, 244, 52, { .17f,.40f,.51f,.96f });
        textAt("RESET DISTANCE", 37, 104, 28, WHITE);
    }
    rect(14, g.fbH - 100, 520, 86, DARK);
    textAt(g.walking ? "PREDJENO: " + distanceLabel(g.walkedMeters) : "UKUPNO: " + distanceLabel(totalMeters()), 28, g.fbH - 92, 34);
    textAt(g.walking ? "W A S D  -  POMERANJE MAPE" : "LEVI KLIK: DODAJ / OBRISI TACKU", 28, g.fbH - 56, 24, { .8f,.9f,1,1 });
    // Poluprovidna identifikaciona tekstura (tekst se renderuje iz teksturnog atlasa).
    const float iw = std::min(float(g.fbW) - 40.f, 430.f);
    rect(g.fbW - iw - 14, 14, iw, 41, { .04f,.09f,.13f,.59f });
    textAt(STUDENT, g.fbW - iw, 22, 23, { 1,1,1,.78f });
}
static void render() {
    glClearColor(.83f, .86f, .88f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    if (g.walking) {
        // Originalne UV koordinate, sa crop-om oko sredisnjeg pina.
        // Uv odnos cuva proporcije mape bez istezanja i na razlicitim ekranima.
        float halfU = .5f / g.zoom;
        float halfV = halfU * (float(g.fbH) / g.fbW) * (float(g.mapW) / g.mapH);
        halfV = std::min(halfV, .48f);
        g.walkCenter.x = clampf(g.walkCenter.x, halfU, 1 - halfU);
        g.walkCenter.y = clampf(g.walkCenter.y, halfV, 1 - halfV);
        Vec2 a = { g.walkCenter.x - halfU,g.walkCenter.y - halfV };
        Vec2 b = { g.walkCenter.x + halfU,g.walkCenter.y + halfV };
        textureRect(0, 0, float(g.fbW), float(g.fbH), g.mapTexture, WHITE, a, b);
        // Pin ostaje u sredini iako se UV crop pomera.
        float cx = g.fbW * .5f, cy = g.fbH * .5f;
        circle(cx, cy - 13, 19, { 0,0,0,.28f });
        circle(cx, cy - 17, 16, { .92f,.19f,.22f,1 });
        circle(cx, cy - 17, 6, WHITE);
        line({ cx,cy - 4 }, { cx,cy + 19 }, 8, { .92f,.19f,.22f,1 });
        circle(cx, cy + 19, 4, { .92f,.19f,.22f,1 });
    }
    else {
        Vec2 s = mapSizeOnScreen(), o = mapOrigin();
        textureRect(o.x, o.y, s.x, s.y, g.mapTexture);
        for (size_t i = 1;i < g.points.size();++i) {
            Vec2 a = toScreen(g.points[i - 1]), b = toScreen(g.points[i]);
            line(a, b, 6.f, { .04f,.24f,.39f,.7f });
            line(a, b, 3.f, CYAN);
        }
        for (Vec2 uv : g.points) {
            Vec2 p = toScreen(uv);
            circle(p.x, p.y, 10, { .04f,.24f,.39f,.85f });
            circle(p.x, p.y, 6, WHITE);
        }
    }
    drawHud();
}
static void updateWalk(float dt) {
    if (!g.walking)
        return;
    Vec2 d;
    if (glfwGetKey(g.window, GLFW_KEY_W) == GLFW_PRESS)
        d.y -= 1;
    if (glfwGetKey(g.window, GLFW_KEY_S) == GLFW_PRESS)
        d.y += 1;
    if (glfwGetKey(g.window, GLFW_KEY_A) == GLFW_PRESS)
        d.x -= 1;
    if (glfwGetKey(g.window, GLFW_KEY_D) == GLFW_PRESS)
        d.x += 1;
    float L = length(d);
    if (L == 0)
        return;
    d = d * (1.f / L);
    Vec2 old = g.walkCenter;
    // Konverzija metara u UV pomeraj na trenutnoj geografskoj sirini.
    // Skaliranje dijagonale: WASD ostaje konstantne brzine na mapi.
    const GeoPoint here = uvToGeo(g.walkCenter);
    constexpr double EARTH_RADIUS = 6371000.0;
    const double metersPerUvX = rad(MAP_EAST - MAP_WEST) * EARTH_RADIUS * std::cos(rad(here.latitude));
    const double metersPerUvY = std::abs(mercatorY(MAP_NORTH) - mercatorY(MAP_SOUTH))
        * EARTH_RADIUS * std::cos(rad(here.latitude));

    g.walkCenter.x += float(d.x * MAP_NAVIGATION_METERS_PER_SEC * dt / metersPerUvX);
    g.walkCenter.y += float(d.y * MAP_NAVIGATION_METERS_PER_SEC * dt / metersPerUvY);

    const float halfU = .5f / g.zoom;
    const float halfV = std::min(.48f, halfU * float(g.fbH) / g.fbW * float(g.mapW) / g.mapH);

    g.walkCenter.x = clampf(g.walkCenter.x, halfU, 1 - halfU);
    g.walkCenter.y = clampf(g.walkCenter.y, halfV, 1 - halfV);
    g.walkedMeters += segmentMeters(old, g.walkCenter);
}
int main() {
    if (!glfwInit()) { std::cerr << "GLFW nije inicijalizovan.\n";return 1; }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
    
    if (!mode){ 
        std::cerr << "Ne postoji video rezim.\n";
        glfwTerminate();
        return 1;
    }
    
    g.window = glfwCreateWindow(mode->width, mode->height, "2D MAPA - Racunarska grafika", monitor, nullptr);
    if (!g.window) { 
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(g.window);
    glfwSwapInterval(0); // Limiter od tacno najvise 75 FPS.
    
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { 
        std::cerr << "GLEW nije inicijalizovan.\n";
        glfwTerminate();
        return 1;
    }
    glGetError();
    glfwGetFramebufferSize(g.window, &g.fbW, &g.fbH);
    glfwGetWindowSize(g.window, &g.windowW, &g.windowH);
    glViewport(0, 0, g.fbW, g.fbH);
    glfwSetKeyCallback(g.window, keyCallback);
    glfwSetMouseButtonCallback(g.window, mouseCallback);
    glfwSetFramebufferSizeCallback(g.window, resizeCallback);
    glfwSetWindowSizeCallback(g.window, windowSizeCallback);

    g.shader = createShader("shaders/basic.vert", "shaders/basic.frag");
    if (!g.shader) { 
        glfwTerminate();
        return 1;
    }
    glUseProgram(g.shader);
    glUniform1i(glGetUniformLocation(g.shader, "uTex"), 0);

    // Kvadrat: atribut 0 = koordinata temena, atribut 1 = UV.
    float q[] = { 0,0,0,0, 0,1,0,1, 1,1,1,1, 1,0,1,0 };
    glGenVertexArrays(1, &g.vao);
    glGenBuffers(1, &g.vbo);
    glBindVertexArray(g.vao);
    glBindBuffer(GL_ARRAY_BUFFER, g.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(q), q, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    std::vector<float> cv;
    cv.reserve(66 * 4);
    cv.insert(cv.end(), { .5f,.5f,.5f,.5f });
    for (int i = 0;i <= 64;++i) { 
        float a = 2 * PI * i / 64;
        float u = .5f + .5f * std::cos(a), v = .5f + .5f * std::sin(a);
        cv.insert(cv.end(), { u,v,u,v });
    }
    glGenVertexArrays(1, &g.circleVao);
    glGenBuffers(1, &g.circleVbo);
    glBindVertexArray(g.circleVao);
    glBindBuffer(GL_ARRAY_BUFFER, g.circleVbo);
    glBufferData(GL_ARRAY_BUFFER, cv.size() * sizeof(float), cv.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    g.mapTexture = loadImageToTexture("res/map.jpg", &g.mapW, &g.mapH);
    // Pocetna pozicija je centar Novog Sada, a ne centar cele siroke mape.
    g.walkCenter = geoToUv(45.2629, 19.8297);
    g.fontTexture = loadImageToTexture("res/font.png", nullptr, nullptr);
    g.compass = loadImageToCursor("res/compass.png");
    if (g.compass)
        glfwSetCursor(g.window, g.compass);
    if (!g.mapTexture || !g.fontTexture) { 
        std::cerr << "Nedostaju res/map.jpg ili res/font.png\n";
        glfwTerminate();
        return 1;
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    const double interval = 1.0 / 75.0;
    g.lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(g.window)) {
        double begin = glfwGetTime();
        float dt = float(std::min(.06, begin - g.lastTime));g.lastTime = begin;
        glfwPollEvents();
        updateWalk(dt);
        render();
        glfwSwapBuffers(g.window);
        double elapsed = glfwGetTime() - begin;
        if (elapsed < interval)
            std::this_thread::sleep_for(std::chrono::duration<double>(interval - elapsed));
    }
    glfwDestroyCursor(g.compass);
    glDeleteTextures(1, &g.mapTexture);
    glDeleteTextures(1, &g.fontTexture);
    glDeleteBuffers(1, &g.vbo);
    glDeleteVertexArrays(1, &g.vao);
    glDeleteBuffers(1, &g.circleVbo);
    glDeleteVertexArrays(1, &g.circleVao);
    glDeleteProgram(g.shader);
    glfwDestroyWindow(g.window);
    glfwTerminate();
    return 0;
}
