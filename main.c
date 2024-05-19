#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

#include <emscripten/emscripten.h>

// Compilation
// emcc -o game.html main.c -Os -Wall /opt/webRaylib/raylib-master/src/web/libraylib.a -I. -I /opt/webRaylib/raylib-master/src -L. -L /opt/webRaylib/raylib-master/src/web -s USE_GLFW=3 --shell-file ./shell.html -DPLATFORM_WEB -sGL_ENABLE_GET_PROC_ADDRESS --preload-file mapImages -s ALLOW_MEMORY_GROWTH=1 -s NO_EXIT_RUNTIME=1 -s 'EXPORTED_RUNTIME_METHODS=[ccall]'

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
int mousePositionXOld = 0;
int mousePositionYOld = 0;

// If the mouse is over a corner/wall node
bool isMouseOverCorner = false;

// Read the name
typedef enum MAPEDITORMODE
{
    MAP_NONE,
    MAP_PLACEWALLS,
    MAP_PLAY
} MAPEDITORMODE;
MAPEDITORMODE mapEditorMode = MAP_PLACEWALLS;

// If the first vertex of a wall has been placed
bool wallPlacementStarted = false;
// If box selection has been started
bool boxSelectionStarted = false;

typedef enum WALLSTATE
{
    WALL_NONE,
    WALL_PLACED,
    WALL_MARKED
} WALLSTATE;

// Wall struct
typedef struct Wall
{
    WALLSTATE state;
    short startX;
    short startY;
    short endX;
    short endY;
} WALL;

// Wall color
bool wallColorToggle = true;

// Wall variable
const short maxWallCount = 512;
WALL walls[512];
short placeWallIndex = 0;
short selectedWallIndex = -1;

// Bitwise boolean conditions array
typedef enum BITCONDITION
{
    CON_DEAD = 1,
    CON_BLIND = 1 << 1,
    CON_CHARMED = 1 << 2,
    CON_DEAFENED = 1 << 3,
    CON_FRIGHTENED = 1 << 4,
    CON_GRAPPLED = 1 << 5,
    CON_INCAPACITATED = 1 << 6,
    CON_INVISIBLE = 1 << 7,
    CON_PARALYZED = 1 << 8,
    CON_PETRIFIED = 1 << 9,
    CON_POISONED = 1 << 10,
    CON_PRONE = 1 << 11,
    CON_RESTRAINED = 1 << 12,
    CON_STUNNED = 1 << 13,
    CON_UNCONSCIOUS = 1 << 14,
    CON_EXTRA_01 = 1 << 15,
    CON_EXTRA_02 = 1 << 16,
    CON_EXTRA_03 = 1 << 17,
    CON_EXTRA_04 = 1 << 18,
    CON_EXTRA_05 = 1 << 19,
    CON_EXTRA_06 = 1 << 20,
    CON_EXTRA_07 = 1 << 21,
    CON_EXTRA_08 = 1 << 22,
    CON_EXTRA_09 = 1 << 23,
    CON_EXTRA_10 = 1 << 24,
    CON_EXTRA_11 = 1 << 25,
    CON_EXTRA_12 = 1 << 26,
    CON_EXTRA_13 = 1 << 27,
    CON_EXTRA_14 = 1 << 28,
    CON_EXTRA_15 = 1 << 29,
    CON_EXTRA_16 = 1 << 30,
    CON_EXTRA_17 = 1 << 31
} BITCONDITION;

typedef enum TOKENSTATE
{
    TOKEN_NONE,
    TOKEN_PLACED,
    TOKEN_HOVER,
    TOKEN_SELECTED
} TOKENSTATE;

// Token struct
typedef struct Token
{
    TOKENSTATE state;
    short x;
    short y;
    char width;
    char height;
    uint32_t bitConditions;
    Color color;
} TOKEN;

// Token variable
const short maxTokenCount = 512;
TOKEN tokens[512];
short activeToken = -1;

bool drawFov = true;

// Map texture
Texture2D mapTexture;

inline int max(int a, int b)
{
    return (a > b) * a + (a <= b) * b;
}

inline int min(int a, int b)
{
    return (a < b) * a + (a >= b) * b;
}

// Point rectangle collision
inline bool PointRectCollision(int x, int y, int rx, int ry, int rx2, int ry2)
{
    return x >= min(rx, rx2) &&
           x <= max(rx, rx2) &&
           y >= min(ry, ry2) &&
           y <= max(ry, ry2);
}

inline Vector2 FoVEndpoint(short x, short y)
{
    return (Vector2){
        x + 10 * ((x * tileSize) - (tokens[activeToken].x * tileSize) + (tileSize * 0.5)),
        y + 10 * ((y * tileSize) - (tokens[activeToken].y * tileSize) + (tileSize * 0.5))};
}

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
    case MAP_NONE:
        break;
    case MAP_PLACEWALLS:
    {
        if (!wallPlacementStarted)
        {
            if (boxSelectionStarted)
            {
                // Select walls in the box
                for (short i = 0; i < maxWallCount; i++)
                {
                    if (walls[i].state)
                    {
                        walls[i].state = WALL_PLACED;

                        // First pass: Rectangle - point collision wall start
                        if (PointRectCollision(
                                walls[i].startX * tileSize, walls[i].startY * tileSize,
                                mousePositionXOld, mousePositionYOld,
                                mousePositionX, mousePositionY))
                        {
                            walls[i].state = WALL_MARKED;
                            continue;
                        }

                        // Second pass: Rectangle - point collision wall end
                        if (PointRectCollision(
                                walls[i].endX * tileSize, walls[i].endY * tileSize,
                                mousePositionXOld, mousePositionYOld,
                                mousePositionX, mousePositionY))
                        {
                            walls[i].state = WALL_MARKED;
                            continue;
                        }

                        // Third pass: Line - Line collision for three box edges
                        Vector2 boxEdges[4] = {
                            (Vector2){mousePositionXOld, mousePositionYOld},
                            (Vector2){mousePositionX, mousePositionYOld},
                            (Vector2){mousePositionX, mousePositionY},
                            (Vector2){mousePositionXOld, mousePositionY}};

                        Vector2 wallStartVector = (Vector2){walls[i].startX * tileSize, walls[i].startY * tileSize};
                        Vector2 wallEndVector = (Vector2){walls[i].endX * tileSize, walls[i].endY * tileSize};

                        for (short ii = 0; ii < 3; ii++)
                        {
                            if (CheckCollisionLines(
                                    boxEdges[ii], boxEdges[ii + 1],
                                    wallStartVector, wallEndVector,
                                    NULL))
                            {
                                walls[i].state = WALL_MARKED;
                                break;
                            }
                        }
                    }
                }
            }
            else
            {
                selectedWallIndex = -1;
                for (short i = 0; i < maxWallCount; i++)
                {
                    if (walls[i].state)
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
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON))
        {
            if (isMouseOverCorner)
            {
                if (wallPlacementStarted)
                {
                    // Place the end of the wall
                    walls[placeWallIndex].endX = mouseGridPosX;
                    walls[placeWallIndex].endY = mouseGridPosY;

                    // Check if a new wall can be made
                    bool wallFound = false;
                    for (short i = 0; i < maxWallCount; i++)
                    {
                        if (!walls[i].state)
                        {
                            placeWallIndex = i;
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
                        walls[placeWallIndex].startX = mouseGridPosX;
                        walls[placeWallIndex].startY = mouseGridPosY;
                        walls[placeWallIndex].endX = mouseGridPosX;
                        walls[placeWallIndex].endY = mouseGridPosY;
                        walls[placeWallIndex].state = WALL_PLACED;
                    }
                }
                else
                {
                    // Check if a new wall can be made
                    bool wallFound = false;
                    for (short i = 0; i < maxWallCount; i++)
                    {
                        if (!walls[i].state)
                        {
                            placeWallIndex = i;
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
                        walls[placeWallIndex].startX = mouseGridPosX;
                        walls[placeWallIndex].startY = mouseGridPosY;
                        walls[placeWallIndex].endX = mouseGridPosX;
                        walls[placeWallIndex].endY = mouseGridPosY;
                        walls[placeWallIndex].state = WALL_PLACED;
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
                walls[placeWallIndex].state = WALL_NONE;
                wallPlacementStarted = false;
            }
            mousePositionXOld = mousePositionX;
            mousePositionYOld = mousePositionY;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !boxSelectionStarted)
        {
            if (abs(mousePositionX - mousePositionXOld) > mouseSensitivityDistance ||
                abs(mousePositionY - mousePositionYOld) > mouseSensitivityDistance)
            {
                boxSelectionStarted = true;
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
        {
            if (boxSelectionStarted)
            {
                // delete selected walls
                for (short i = 0; i < maxWallCount; i++)
                {
                    if (walls[i].state == WALL_MARKED)
                    {
                        walls[i].state = WALL_NONE;
                    }
                }
                boxSelectionStarted = false;
            }
            else if (selectedWallIndex != -1)
            {
                // Remove the wall
                walls[selectedWallIndex].state = WALL_NONE;
            }
        }
        break;
    }
    case MAP_PLAY:
    {
        // Select tokens
        if (boxSelectionStarted)
        {
            // Select tokens in the box
            for (short i = 0; i < maxTokenCount; i++)
            {
                if (tokens[i].state)
                {
                    if (tokens[i].state == TOKEN_SELECTED)
                    {
                        continue;
                    }

                    tokens[i].state = TOKEN_PLACED;

                    if (CheckCollisionRecs(
                            (Rectangle){
                                min(mousePositionXOld, mousePositionX) * tileSize,
                                min(mousePositionYOld, mousePositionY) * tileSize,
                                abs(mousePositionX - mousePositionXOld) * tileSize,
                                abs(mousePositionY - mousePositionYOld) * tileSize},
                            (Rectangle){
                                tokens[i].x * tileSize,
                                tokens[i].y * tileSize,
                                tokens[i].width * tileSize,
                                tokens[i].height * tileSize}))
                    {
                        tokens[i].state = TOKEN_HOVER;
                    }
                }
            }
        }
        else
        {
            for (short i = 0; i < maxTokenCount; i++)
            {
                if (tokens[i].state == TOKEN_HOVER)
                {
                    tokens[i].state = TOKEN_PLACED;
                }
                if (tokens[i].state == TOKEN_PLACED)
                {
                    if (PointRectCollision(
                            mousePositionX,
                            mousePositionY,
                            tokens[i].x * tileSize,
                            tokens[i].y * tileSize,
                            (tokens[i].x + tokens[i].width) * tileSize,
                            (tokens[i].y + tokens[i].height) * tileSize))
                    {
                        tokens[i].state = TOKEN_HOVER;
                    }
                }
            }
        }
        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            mousePositionXOld = mousePositionX;
            mousePositionYOld = mousePositionY;
        }
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !boxSelectionStarted)
        {
            if (abs(mousePositionX - mousePositionXOld) > mouseSensitivityDistance ||
                abs(mousePositionY - mousePositionYOld) > mouseSensitivityDistance)
            {
                boxSelectionStarted = true;
            }
        }
        if (IsMouseButtonReleased(MOUSE_BUTTON_RIGHT))
        {
            if (boxSelectionStarted)
            {
                // select tokens in the box
                for (short i = 0; i < maxTokenCount; i++)
                {
                    if (tokens[i].state == TOKEN_HOVER)
                    {
                        tokens[i].state = TOKEN_SELECTED;
                    }
                }
                boxSelectionStarted = false;
            }
            else
            {
                if (activeToken != -1)
                {
                    tokens[activeToken].state = TOKEN_PLACED;
                    activeToken = -1;
                }
                for (short i = 0; i < maxTokenCount; i++)
                {
                    if (tokens[i].state)
                    {
                        if (PointRectCollision(
                                mousePositionX,
                                mousePositionY,
                                tokens[i].x * tileSize,
                                tokens[i].y * tileSize,
                                (tokens[i].x + tokens[i].width) * tileSize,
                                (tokens[i].y + tokens[i].height) * tileSize))
                        {
                            tokens[i].state = TOKEN_SELECTED;
                            activeToken = i;
                        }
                    }
                }
            }
        }
        if (IsKeyPressed(KEY_UP))
        {
            for (short i = 0; i < maxTokenCount; i++)
            {
                if (tokens[i].state == TOKEN_SELECTED)
                {
                    tokens[i].y--;
                }
            }
        }
        if (IsKeyPressed(KEY_DOWN))
        {
            for (short i = 0; i < maxTokenCount; i++)
            {
                if (tokens[i].state == TOKEN_SELECTED)
                {
                    tokens[i].y++;
                }
            }
        }
        if (IsKeyPressed(KEY_LEFT))
        {
            for (short i = 0; i < maxTokenCount; i++)
            {
                if (tokens[i].state == TOKEN_SELECTED)
                {
                    tokens[i].x--;
                }
            }
        }
        if (IsKeyPressed(KEY_RIGHT))
        {
            for (short i = 0; i < maxTokenCount; i++)
            {
                if (tokens[i].state == TOKEN_SELECTED)
                {
                    tokens[i].x++;
                }
            }
        }
        break;
    }
    default:
        break;
    }

    BeginDrawing();
    ClearBackground(RAYWHITE);

    DrawTextureEx(mapTexture, (Vector2){0, 0}, 0, 0.5, WHITE);

    // Draw FoV shadows
    if (drawFov && activeToken != -1)
    {
        for (short i = 0; i < maxWallCount; i++)
        {
            if (!walls[i].state)
                continue;

            Vector2 a = (Vector2){
                walls[i].startX * tileSize,
                walls[i].startY * tileSize};
            Vector2 b = (Vector2){
                walls[i].endX * tileSize,
                walls[i].endY * tileSize};
            Vector2 c = (Vector2){
                FoVEndpoint(walls[i].startX, walls[i].startY).x,
                FoVEndpoint(walls[i].startX, walls[i].startY).y};
            Vector2 d = (Vector2){
                FoVEndpoint(walls[i].endX, walls[i].endY).x,
                FoVEndpoint(walls[i].endX, walls[i].endY).y};

            if ( (b.x - a.x) * (c.y - b.y) - (b.y - a.y) * (c.x - b.x) > 0.0f )
            {
                // clockwise
                DrawTriangle( a, c, d, BLACK);
                DrawTriangle( a, d, b, BLACK);
            }
            else
            {
                // counter-clockwise.
                DrawTriangle( a, d, c, BLACK);
                DrawTriangle( a, b, d, BLACK);
            }
        }
    }

    // Draw Tokens
    for (short i = 0; i < maxTokenCount; i++)
    {
        switch (tokens[i].state)
        {
        case TOKEN_PLACED:
        {
            DrawRectangle(
                tokens[i].x * tileSize, tokens[i].y * tileSize,
                tokens[i].width * tileSize, tokens[i].height * tileSize,
                BLACK);
            DrawRectangle(
                (tokens[i].x * tileSize) + 1, (tokens[i].y * tileSize) + 1,
                (tokens[i].width * tileSize) - 2, (tokens[i].height * tileSize) - 2,
                tokens[i].color);
            break;
        }
        case TOKEN_HOVER:
        {
            DrawRectangle(
                tokens[i].x * tileSize, tokens[i].y * tileSize,
                tokens[i].width * tileSize, tokens[i].height * tileSize,
                WHITE);
            DrawRectangle(
                (tokens[i].x * tileSize) + 1, (tokens[i].y * tileSize) + 1,
                (tokens[i].width * tileSize) - 2, (tokens[i].height * tileSize) - 2,
                tokens[i].color);
            break;
        }
        case TOKEN_SELECTED:
        {
            DrawRectangle(
                tokens[i].x * tileSize, tokens[i].y * tileSize,
                tokens[i].width * tileSize, tokens[i].height * tileSize,
                WHITE);
            DrawRectangle(
                (tokens[i].x * tileSize) + 3, (tokens[i].y * tileSize) + 3,
                (tokens[i].width * tileSize) - 6, (tokens[i].height * tileSize) - 6,
                RED);
            break;
        }

        default:
            break;
        }
        // DrawText(TextFormat("%d", tokens[i].state), tokens[i].x * tileSize + 5, tokens[i].y * tileSize + 5, tileSize, BLUE);
    }

    if (mapEditorMode == MAP_PLACEWALLS)
    {
        // Draw wall nodes (tile corners)
        for (short x = 0; x < gameBoardGridWidth; x++)
        {
            for (short y = 0; y < gameBoardGridHeight; y++)
            {
                DrawPoly((Vector2){x * tileSize, y * tileSize}, 4, 3, 0, BLACK);
                DrawPoly((Vector2){x * tileSize, y * tileSize}, 4, 2, 0, WHITE);
            }
        }

        // Draw highlighted wall node
        if (isMouseOverCorner)
        {
            DrawPoly(
                (Vector2){mouseGridPosX * tileSize, mouseGridPosY * tileSize},
                4, mouseSensitivityDistance, 0, BLACK);
            DrawPoly(
                (Vector2){mouseGridPosX * tileSize, mouseGridPosY * tileSize},
                4, mouseSensitivityDistance - 1, 0, PINK);
        }
    }

    // Draw walls that exist
    for (short i = 0; i < maxWallCount; i++)
    {
        if (walls[i].state)
        {
            DrawLineEx(
                (Vector2){walls[i].startX * tileSize, walls[i].startY * tileSize},
                (Vector2){walls[i].endX * tileSize, walls[i].endY * tileSize},
                5, BLACK);
        }
    }
    for (short i = 0; i < maxWallCount; i++)
    {
        if (walls[i].state)
        {
            Color wallColor = wallColorToggle ? GREEN : BLUE;
            DrawLineEx(
                (Vector2){walls[i].startX * tileSize, walls[i].startY * tileSize},
                (Vector2){walls[i].endX * tileSize, walls[i].endY * tileSize},
                3, i == selectedWallIndex || walls[i].state == WALL_MARKED ? RED : wallColor);
        }
    }

    // Draw the wall being placed
    if (wallPlacementStarted)
    {
        DrawLineEx(
            (Vector2){walls[placeWallIndex].startX * tileSize, walls[placeWallIndex].startY * tileSize},
            (Vector2){mousePositionX, mousePositionY},
            5, BLACK);
        DrawLineEx(
            (Vector2){walls[placeWallIndex].startX * tileSize, walls[placeWallIndex].startY * tileSize},
            (Vector2){mousePositionX, mousePositionY},
            3,
            PINK);
    }

    // Draw box selection
    if (boxSelectionStarted)
    {
        DrawRectangle(
            mousePositionXOld * (mousePositionXOld <= mousePositionX) + mousePositionX * (mousePositionXOld > mousePositionX),
            mousePositionYOld * (mousePositionYOld <= mousePositionY) + mousePositionY * (mousePositionYOld > mousePositionY),
            (mousePositionX - mousePositionXOld) * (mousePositionXOld <= mousePositionX) + (mousePositionXOld - mousePositionX) * (mousePositionXOld > mousePositionX),
            (mousePositionY - mousePositionYOld) * (mousePositionYOld <= mousePositionY) + (mousePositionYOld - mousePositionY) * (mousePositionYOld > mousePositionY),
            Fade(PURPLE, 0.4));
        DrawRectangleLines(
            mousePositionXOld * (mousePositionXOld <= mousePositionX) + mousePositionX * (mousePositionXOld > mousePositionX),
            mousePositionYOld * (mousePositionYOld <= mousePositionY) + mousePositionY * (mousePositionYOld > mousePositionY),
            (mousePositionX - mousePositionXOld) * (mousePositionXOld <= mousePositionX) + (mousePositionXOld - mousePositionX) * (mousePositionXOld > mousePositionX),
            (mousePositionY - mousePositionYOld) * (mousePositionYOld <= mousePositionY) + (mousePositionYOld - mousePositionY) * (mousePositionYOld > mousePositionY),
            PURPLE);
    }

    // Debug text
    // DrawText(TextFormat("(%d %% %d) - %d = %d \n\n\n\n%d", mousePositionX, (int)tileSize, mouseSensitivityDistance, ((mousePositionX + mouseSensitivityDistance)% (int)tileSize), isMouseOverCorner), 10, 10, 50, RED);
    // DrawText(TextFormat("%d", activeToken), 10, 150, 50, RED);

    EndDrawing();
}

EMSCRIPTEN_KEEPALIVE
bool ChangeWallColor()
{
    wallColorToggle = !wallColorToggle;
    return wallColorToggle;
}

EMSCRIPTEN_KEEPALIVE
bool ChangeMapMode()
{
    if (mapEditorMode == MAP_PLACEWALLS)
    {
        mapEditorMode = MAP_PLAY;
        boxSelectionStarted = false;
        return true;
    }
    else
    {
        mapEditorMode = MAP_PLACEWALLS;
        boxSelectionStarted = false;
        return false;
    }
}

int main()
{
    for (short i = 0; i < maxWallCount; i++)
    {
        walls[i].state = WALL_NONE;
    }

    for (short i = 0; i < 5; i++)
    {
        tokens[i].state = TOKEN_PLACED;
        tokens[i].x = i;
        tokens[i].y = i;
        tokens[i].width = 1;
        tokens[i].height = 1;
        tokens[i].bitConditions = 0u;
        tokens[i].color = PINK;
    }

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    mapTexture = LoadTexture("mapImages/The_handy_hag_upstairs.png");

    screenWidth = mapTexture.width / 2;
    screenHeight = mapTexture.height / 2;

    SetWindowSize(screenWidth, screenHeight);

    tileSize =
        (screenWidth / gameBoardGridWidth) * (screenWidth <= screenHeight) +
        (screenHeight / gameBoardGridHeight) * (screenWidth > screenHeight);

    // Start the main loop
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);

    return 0;
}