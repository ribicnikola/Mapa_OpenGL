# 2D Map - OpenGL

A simple interactive 2D map made in C++ using OpenGL 3.3 for a Computer Graphics university project.

The application uses a map of Novi Sad and has two modes: walking and distance measurement.

## Features

**Walking mode**
- Move around the map using WASD keys.
- The location pin stays in the center while the map moves.
- The map is zoomed in during walking.
- Traveled distance is calculated and displayed in meters or kilometers.
- Reset the traveled distance without changing the current position.

**Distance measurement mode**
- View the whole map.
- Add points by left-clicking on the map.
- Points are connected with lines.
- Click on an existing point to remove it.
- The total distance between the points is calculated automatically.

Distances are calculated using geographic coordinates and the Haversine formula.

## Controls

| Key / Action | Function |
|---|---|
| W, A, S, D | Move around the map |
| R | Switch between modes |
| Left mouse click | Add or remove points in measurement mode |
| Reset Distance button | Reset traveled distance |
| Esc | Exit application |

## Technologies

- C++
- OpenGL 3.3 Core Profile
- GLFW
- GLEW
- GLSL (vertex and fragment shaders)
- CMake

The map and UI elements are rendered using OpenGL. Text is displayed using a font texture atlas, while buttons and icons are drawn using basic 2D shapes.

## Project Structure

```text
Mapa_OpenGL/
├── src/          # C++ source files
├── shaders/      # Vertex and fragment shaders
├── res/          # Map, font and cursor images
├── vendor/       # GLFW and GLEW libraries
└── CMakeLists.txt
```

## Running the Project

The project was developed on Windows using Visual Studio and CMake.

Open the project folder in Visual Studio, let CMake configure the project, then build and run it.

## Map Credits

Map data: OpenStreetMap contributors.