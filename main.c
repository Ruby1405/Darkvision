#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

#include <emscripten/emscripten.h>

// Compilation
// emcc -o game.html main.c -Os -Wall /opt/webRaylib/raylib-master/src/web/libraylib.a -I. -I /opt/webRaylib/raylib-master/src -L. -L /opt/webRaylib/raylib-master/src/web -s USE_GLFW=3 --shell-file ./shell.html -DPLATFORM_WEB -sGL_ENABLE_GET_PROC_ADDRESS --preload-file mapImages -s ALLOW_MEMORY_GROWTH=1

// Screen dimensions
int screenWidth = 1000;
int screenHeight = 1000;

// Game board grid dimensions
short gameBoardGridWidth = 16;
short gameBoardGridHeight = 28;

// Tile size (automatically calculated)
float tileSize = 20;

// Distance from a point to be considered as the point 
int mouseSensitivityDistance = 6;
// Mouse position
int mousePositionX = 0;
int mousePositionY = 0;
// If the mouse is over a corner/wall node
bool isMouseOverCorner = false;

// Read the name
typedef enum MAPEDITORMODE
{
    NONE,
    PLACEWALLS
} MAPEDITORMODE;
MAPEDITORMODE mapEditorMode = PLACEWALLS;

// If the first vertex of a wall has been placed
bool wallPlacementStarted = false;

// Wall struct
typedef struct Wall
{
    bool exists;
    short startX;
    short startY;
    short endX;
    short endY;
} WALL;

// Wall variable
const short maxWallCount = 500;
WALL walls[500];
short wallIndex = 0;
short selectedWallIndex = -1;

// Map texture
Texture2D mapTexture;

// Game loop
void UpdateDrawFrame()
{
    // Update variables
    mousePositionX = GetMouseX();
    mousePositionY = GetMouseY();

    int mouseGridPosX = (int)round(mousePositionX / tileSize);
    int mouseGridPosY = (int)round(mousePositionY / tileSize);

    isMouseOverCorner =
        ((mousePositionX + mouseSensitivityDistance) % (int)tileSize) <= mouseSensitivityDistance * 2 &&
        ((mousePositionY + mouseSensitivityDistance) % (int)tileSize) <= mouseSensitivityDistance * 2;

    switch (mapEditorMode)
    {
    case NONE:
        break;
    case PLACEWALLS:
        if (!wallPlacementStarted)
        {
            selectedWallIndex = -1;
            for (short i = 0; i < maxWallCount; i++)
            {
                if (walls[i].exists)
                {
                    if (CheckCollisionPointLine(
                            (Vector2){mousePositionX, mousePositionY},
                            (Vector2){walls[i].startX * tileSize, walls[i].startY * tileSize},
                            (Vector2){walls[i].endX * tileSize, walls[i].endY * tileSize},
                            mouseSensitivityDistance))
                    {
                        selectedWallIndex = i;
                        break;
                    }
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (isMouseOverCorner)
            {
                if (wallPlacementStarted)
                {
                    // Place the end of the wall
                    walls[wallIndex].endX = mouseGridPosX;
                    walls[wallIndex].endY = mouseGridPosY;
                    
                    // Check if a new wall can be made
                    bool wallFound = false;
                    for (short i = 0; i < maxWallCount; i++)
                    {
                        if (!walls[i].exists)
                        {
                            wallIndex = i;
                            wallFound = true;
                            break;
                        }
                    }
                    if (!wallFound)
                    {
                        wallPlacementStarted = false;
                        // cry about it (TODO: Make an error message)
                    }
                    else
                    {
                        // Make a new wall
                        walls[wallIndex].startX = mouseGridPosX;
                        walls[wallIndex].startY = mouseGridPosY;
                        walls[wallIndex].endX = mouseGridPosX;
                        walls[wallIndex].endY = mouseGridPosY;
                        walls[wallIndex].exists = true;
                    }
                }
                else
                {
                    // Check if a new wall can be made
                    bool wallFound = false;
                    for (short i = 0; i < maxWallCount; i++)
                    {
                        if (!walls[i].exists)
                        {
                            wallIndex = i;
                            wallFound = true;
                            break;
                        }
                    }
                    if (!wallFound)
                    {
                        // cry about it
                    }
                    else
                    {
                        selectedWallIndex = -1;
                        // Make a new wall
                        walls[wallIndex].startX = mouseGridPosX;
                        walls[wallIndex].startY = mouseGridPosY;
                        walls[wallIndex].endX = mouseGridPosX;
                        walls[wallIndex].endY = mouseGridPosY;
                        walls[wallIndex].exists = true;
                        wallPlacementStarted = true;
                    }
                }
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            if (wallPlacementStarted)
            {
                // Stop placing the wall
                walls[wallIndex].exists = false;
                wallPlacementStarted = false;
            }
            else if (selectedWallIndex != -1)
            {
                // Remove the wall
                walls[selectedWallIndex].exists = false;
            }
        }
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawTextureEx(mapTexture, (Vector2){0, 0}, 0, 0.5, WHITE);
    
    // Draw wall nodes (tile corners)
    for (short x = 0; x < gameBoardGridWidth; x++)
    {
        for (short y = 0; y < gameBoardGridHeight; y++)
        {
            DrawPoly((Vector2){x * tileSize, y * tileSize}, 4, 3, 0, BLACK);   
        }
    }
    
    // Draw highlighted wall node
    if (isMouseOverCorner)
    {
        DrawPoly(
            (Vector2){mouseGridPosX * tileSize, mouseGridPosY * tileSize}, 
            4, mouseSensitivityDistance, 0, PURPLE );
    }

    // Draw walls that exist
    for (short i = 0; i < maxWallCount; i++)
    {
        if(walls[i].exists)
        {
            DrawLineEx(
                (Vector2){walls[i].startX * tileSize, walls[i].startY * tileSize},
                (Vector2){walls[i].endX * tileSize, walls[i].endY * tileSize},
                3, i == selectedWallIndex ? RED : GREEN
            );
        }
    }

    // Draw the wall being placed
    if (wallPlacementStarted)
    {
        DrawLineEx(
            (Vector2){walls[wallIndex].startX * tileSize, walls[wallIndex].startY * tileSize},
            (Vector2){mousePositionX, mousePositionY},
            3,
            PURPLE
        );
    }

    // Debug text
    // DrawText(TextFormat("(%d %% %d) - %d = %d \n\n\n\n%d", mousePositionX, (int)tileSize, mouseSensitivityDistance, ((mousePositionX + mouseSensitivityDistance)% (int)tileSize), isMouseOverCorner), 10, 10, 50, RED);
    // DrawText(TextFormat("%d", wallPlacementStarted), 10, 150, 50, RED);

    EndDrawing();
}

int main ()
{
    // Initialize variables

    for (short i = 0; i < maxWallCount; i++)
    {
        walls[i].exists = false;
    }

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    mapTexture = LoadTexture("mapImages/The_handy_hag_upstairs.png");

    screenWidth = mapTexture.width/2;
    screenHeight = mapTexture.height/2;

    SetWindowSize(screenWidth, screenHeight);
    
    tileSize = 
        (screenWidth / gameBoardGridWidth) * (screenWidth <= screenHeight) +
        (screenHeight / gameBoardGridHeight) * (screenWidth > screenHeight);

    // Start the main loop
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);

    return 0;
}