#ifndef GAME_H
#define GAME_H

#include "../puzzle/PuzzleBoard.h"
#include "../puzzle/Segmentation.h"
#include <SDL2/SDL.h>
#include <memory>
#include <string>
#include <unordered_map>

struct PieceTexture {
  SDL_Texture *texture = nullptr;
  int min_x = 0;
  int min_y = 0;
  int w = 0;
  int h = 0;
};

enum class GameState { MENU, PLAYING, CLEARED };

class Game {
public:
  Game();
  ~Game();

  bool init(const char *title, int width, int height);
  void run();
  void clean();

  void load_image(const std::string &filepath);

private:
  void handle_events();
  void update();
  void render();

  bool is_running;
  SDL_Window *window;
  SDL_Renderer *renderer;

  GameState state;

  // Audio state
  SDL_AudioDeviceID audio_device;
  void init_audio();
  void play_snap_sound();

  Segmentation segmentation;
  std::unique_ptr<PuzzleBoard> board;

  int window_width;
  int window_height;

  // Interaction state
  int selected_piece_id;
  int last_mouse_x;
  int last_mouse_y;

  // Camera state
  float camera_x;
  float camera_y;
  float camera_zoom;
  bool is_panning;

  float grab_offset_x;
  float grab_offset_y;

  // UI state
  int sidebar_width;
  int inventory_page;
  std::vector<int> inventory_pieces;
  std::vector<bool> in_inventory;

  // Texture Cache
  std::unordered_map<int, PieceTexture> piece_textures;

  // Helper functions
  bool is_mouse_over_sidebar(int x);
  void open_file_dialog();
  void render_menu_buttons();
  void render_inventory();
  void render_playground();
  void get_inventory_slot_rect(int slot_index, SDL_Rect &rect);
  void move_piece_to_board(int piece_id, int mouse_x, int mouse_y);
  void generate_texture(int piece_id);
  void clean_textures();
  void draw_text(const char* text, int x, int y, int scale = 2, Uint8 r = 40, Uint8 g = 40, Uint8 b = 40);
};

#endif // GAME_H
