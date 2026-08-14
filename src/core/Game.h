#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <string>
#include <memory>
#include "../puzzle/Segmentation.h"
#include "../puzzle/PuzzleBoard.h"

enum class GameState {
    MENU,
    PLAYING,
    CLEARED
};

class Game {
public:
    Game();
    ~Game();

    bool init(const char* title, int width, int height);
    void run();
    void clean();
    
    void load_image(const std::string& filepath);

private:
    void handle_events();
    void update();
    void render();

    bool is_running;
    SDL_Window* window;
    SDL_Renderer* renderer;

    GameState state;

    Segmentation segmentation;
    std::unique_ptr<PuzzleBoard> board;

    int window_width;
    int window_height;

    // Interaction state
    int selected_piece_id;
    int last_mouse_x;
    int last_mouse_y;
};

#endif // GAME_H
