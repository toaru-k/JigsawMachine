#include "Game.h"
#include "../graphics/Font.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <windows.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define PIECE_GRAB_MARGIN 5.0f
#define DATA_MASK 0xf0

#define UI_OFFSET 10
#define INVENTORY_COL_NUM 2
#define INVENTORY_ROW_NUM 4

Game::Game()
    : is_running(false), window(nullptr), renderer(nullptr),
      state(GameState::MENU), audio_device(0), selected_piece_id(-1),
      window_width(0), window_height(0), camera_x(0.0f), camera_y(0.0f),
      camera_zoom(1.0f), is_panning(false), grab_offset_x(0.0f),
      grab_offset_y(0.0f), sidebar_width(200), inventory_page(0) {}

Game::~Game() { clean(); }

bool Game::init(const char *title, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError()
              << std::endl;
    return false;
  }

  window =
      SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {
    std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    return false;
  }

  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    return false;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  window_width = width;
  window_height = height;
  is_running = true;

  std::cout
      << "Select the image (.jpg or .png, .bmp) from the File Explorer to play"
      << std::endl;

  init_audio();

  return true;
}

void Game::init_audio() {
  SDL_AudioSpec want, have;
  SDL_zero(want);
  want.freq = 44100;
  want.format = AUDIO_S16SYS;
  want.channels = 1;
  want.samples = 2048;
  want.callback = NULL;

  audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  if (audio_device != 0) {
    SDL_PauseAudioDevice(audio_device, 0);
  }
}

void Game::play_snap_sound() {
  if (audio_device == 0)
    return;

  const int sample_rate = 44100;
  const int duration_ms = 30; // 30ms snap
  const int num_samples = (sample_rate * duration_ms) / 1000;
  std::vector<int16_t> buffer(num_samples);

  float phase = 0.0f;
  float freq = 1400.0f; // Higher starting frequency

  for (int i = 0; i < num_samples; ++i) {
    // Basic envelope
    float volume = 1.0f - (float)i / num_samples;

    // Square wave for 8-bit retro sound
    float wave = (sin(phase) > 0.0f) ? 1.0f : -1.0f;

    buffer[i] = (int16_t)(16000.0f * volume * wave);

    phase += 2.0f * M_PI * freq / sample_rate;
    freq -= 1000.0f / num_samples; // Pitch sweep down
  }

  SDL_QueueAudio(audio_device, buffer.data(), buffer.size() * sizeof(int16_t));
}

void Game::draw_text(const char *text, int x, int y, int scale, Uint8 r,
                     Uint8 g, Uint8 b) {
  SDL_SetRenderDrawColor(renderer, r, g, b, 255);
  int cursor_x = x;
  while (*text) {
    uint64_t mask = get_char_bitmask(*text);
    if (mask != 0) {
      for (int row = 0; row < 7; ++row) {
        int shift = 30 - (row * 5);
        for (int col = 0; col < 5; ++col) {
          if ((mask >> (shift + 4 - col)) & 1) {
            SDL_Rect p = {cursor_x + col * scale, y + row * scale, scale,
                          scale};
            SDL_RenderFillRect(renderer, &p);
          }
        }
      }
    }
    cursor_x += 6 * scale; // 5px width + 1px spacing
    text++;
  }
}

void Game::generate_texture(int piece_id) {
  if (piece_textures.find(piece_id) != piece_textures.end()) {
    if (piece_textures[piece_id].texture) {
      SDL_DestroyTexture(piece_textures[piece_id].texture);
    }
  }

  const auto &p = segmentation.get_pieces()[piece_id];
  if (p.num_pixels == 0)
    return;

  int min_x = 100000, max_x = -1, min_y = 100000, max_y = -1;
  for (const auto &pix : p.pixels) {
    if (pix.first < min_x)
      min_x = pix.first;
    if (pix.first > max_x)
      max_x = pix.first;
    if (pix.second < min_y)
      min_y = pix.second;
    if (pix.second > max_y)
      max_y = pix.second;
  }
  int w = max_x - min_x + 1;
  int h = max_y - min_y + 1;

  SDL_Surface *surface =
      SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_RGBA32);
  if (!surface)
    return;

  SDL_FillRect(surface, NULL, SDL_MapRGBA(surface->format, 0, 0, 0, 0));

  SDL_LockSurface(surface);
  Uint32 *pixels = (Uint32 *)surface->pixels;
  int pitch = surface->pitch / 4;

  std::vector<bool> in_piece(w * h, false);
  for (const auto &pix : p.pixels) {
    int lx = pix.first - min_x;
    int ly = pix.second - min_y;
    in_piece[ly * w + lx] = true;
  }

  for (const auto &pix : p.pixels) {
    int lx = pix.first - min_x;
    int ly = pix.second - min_y;

    bool top = ly == 0 || !in_piece[(ly - 1) * w + lx];
    bool left = lx == 0 || !in_piece[ly * w + (lx - 1)];
    bool bottom = ly == h - 1 || !in_piece[(ly + 1) * w + lx];
    bool right = lx == w - 1 || !in_piece[ly * w + (lx + 1)];

    int orig_idx = pix.first + pix.second * segmentation.w;
    unsigned char r = segmentation.data[orig_idx * 3 + 0] & DATA_MASK;
    unsigned char g = segmentation.data[orig_idx * 3 + 1] & DATA_MASK;
    unsigned char b = segmentation.data[orig_idx * 3 + 2] & DATA_MASK;

    if (top || left) {
      r = std::min(255, r + 60);
      g = std::min(255, g + 60);
      b = std::min(255, b + 60);
    } else if (bottom || right) {
      r = std::max(0, r - 60);
      g = std::max(0, g - 60);
      b = std::max(0, b - 60);
    }

    pixels[ly * pitch + lx] = SDL_MapRGBA(surface->format, r, g, b, 255);
  }
  SDL_UnlockSurface(surface);

  SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
  SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
  SDL_FreeSurface(surface);

  PieceTexture pt;
  pt.texture = tex;
  pt.min_x = min_x;
  pt.min_y = min_y;
  pt.w = w;
  pt.h = h;
  piece_textures[piece_id] = pt;
}

void Game::clean_textures() {
  auto &uf = segmentation.get_uf();
  for (auto it = piece_textures.begin(); it != piece_textures.end();) {
    int id = it->first;
    if (uf.find(id) != id) {
      if (it->second.texture) {
        SDL_DestroyTexture(it->second.texture);
      }
      it = piece_textures.erase(it);
    } else {
      ++it;
    }
  }
}

void Game::load_image(const std::string &filepath) {
  for (auto &pair : piece_textures) {
    if (pair.second.texture)
      SDL_DestroyTexture(pair.second.texture);
  }
  piece_textures.clear();
  segmentation.cleanup();

  std::cout << "Loading image: " << filepath << std::endl;
  if (segmentation.init(filepath.c_str())) {
    board = std::make_unique<PuzzleBoard>(
        segmentation.get_pieces(), segmentation.get_uf(),
        segmentation.get_width(), segmentation.get_height());

    inventory_pieces.clear();
    inventory_page = 0;
    auto &pieces = segmentation.get_pieces();
    auto &uf = segmentation.get_uf();
    in_inventory.assign(pieces.size(), false);
    for (int i = 0; i < pieces.size(); ++i) {
      if (uf.find(i) == i && pieces[i].num_pixels > 0) {
        inventory_pieces.push_back(i);
        in_inventory[i] = true;
        generate_texture(i);
      }
    }

    camera_x = 50.0f;
    camera_y = 50.0f;
    camera_zoom = 1.0f;

    state = GameState::PLAYING;
    std::cout << "Succeeded to load image" << std::endl;
  } else {
    std::cerr << "Failed to load image: " << filepath << std::endl;
  }
}

void Game::open_file_dialog() {
  OPENFILENAMEA ofn;
  char szFile[260];
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "Images\0*.BMP;*.JPG;*.PNG\0All\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&ofn) == TRUE) {
    load_image(ofn.lpstrFile);
  }
}

void Game::get_inventory_slot_rect(int slot_index, SDL_Rect &rect) {
  int cols = INVENTORY_COL_NUM;
  int rows = INVENTORY_ROW_NUM;
  int col = slot_index % cols;
  int row = slot_index / cols;

  int margin = 20;
  int slot_w = (sidebar_width - margin * 3) / cols;
  int slot_h = (window_height - 150) / rows;

  rect.x = window_width - sidebar_width + margin + col * (slot_w + margin);
  rect.y = 80 + row * (slot_h + 10);
  rect.w = slot_w;
  rect.h = slot_h;
}

void Game::move_piece_to_board(int piece_id, int mouse_x, int mouse_y) {
  auto &pieces = segmentation.get_pieces();

  if (piece_textures.find(piece_id) == piece_textures.end())
    return;
  const auto &pt = piece_textures[piece_id];

  int center_x = pt.min_x + pt.w / 2;
  int center_y = pt.min_y + pt.h / 2;

  float world_x = (mouse_x - camera_x) / camera_zoom;
  float world_y = (mouse_y - camera_y) / camera_zoom;

  pieces[piece_id].offset_x = (int)world_x - center_x;
  pieces[piece_id].offset_y = (int)world_y - center_y;
}

bool Game::is_mouse_over_sidebar(int x) {
  return x >= window_width - sidebar_width;
}

void Game::handle_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      is_running = false;
    } else if (event.type == SDL_WINDOWEVENT) {
      if (event.window.event == SDL_WINDOWEVENT_RESIZED) {
        window_width = event.window.data1;
        window_height = event.window.data2;
      }
    } else if (event.type == SDL_DROPFILE) {
      char *dropped_filedir = event.drop.file;
      load_image(dropped_filedir);
      SDL_free(dropped_filedir);
    } else if (state == GameState::PLAYING) {
      bool in_sidebar = is_mouse_over_sidebar(event.button.x);

      if (event.type == SDL_MOUSEWHEEL) {
        int mx, my;
        SDL_GetMouseState(&mx, &my);
        if (!is_mouse_over_sidebar(mx)) {
          float old_zoom = camera_zoom;

          if (event.wheel.y > 0)
            camera_zoom *= 1.1f;
          else if (event.wheel.y < 0)
            camera_zoom /= 1.1f;

          if (camera_zoom != old_zoom) {
            float wx = (mx - camera_x) / old_zoom;
            float wy = (my - camera_y) / old_zoom;
            camera_x = mx - wx * camera_zoom;
            camera_y = my - wy * camera_zoom;
          }
        }
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        int hit_piece_id = -1;
        float hit_grab_x = 0;
        float hit_grab_y = 0;

        if (!in_sidebar) {
          float world_x = (event.button.x - camera_x) / camera_zoom;
          float world_y = (event.button.y - camera_y) / camera_zoom;
          auto &pieces = segmentation.get_pieces();
          auto &uf = segmentation.get_uf();

          for (int i = pieces.size() - 1; i >= 0; --i) {
            if (uf.find(i) == i) {
              if (in_inventory[i])
                continue;

              const auto &p = pieces[i];
              if (piece_textures.find(i) == piece_textures.end())
                continue;
              const auto &pt = piece_textures[i];

              // Quick bounding box check
              float margin = PIECE_GRAB_MARGIN / camera_zoom;
              float px = pt.min_x + p.offset_x;
              float py = pt.min_y + p.offset_y;
              if (world_x >= px - margin && world_x <= px + pt.w + margin &&
                  world_y >= py - margin && world_y <= py + pt.h + margin) {

                bool hit = false;
                for (const auto &pix : p.pixels) {
                  float px_pix = pix.first + p.offset_x;
                  float py_pix = pix.second + p.offset_y;
                  if (std::abs(px_pix - world_x) <= margin + 0.5f &&
                      std::abs(py_pix - world_y) <= margin + 0.5f) {
                    hit = true;
                    break;
                  }
                }
                if (hit) {
                  hit_piece_id = i;
                  hit_grab_x = p.offset_x - world_x;
                  hit_grab_y = p.offset_y - world_y;
                  break;
                }
              }
            }
          }
        }

        if (event.button.button == SDL_BUTTON_RIGHT) {
          if (!in_sidebar && hit_piece_id != -1) {
            inventory_pieces.push_back(hit_piece_id);
            in_inventory[hit_piece_id] = true;
          } else {
            is_panning = true;
            last_mouse_x = event.button.x;
            last_mouse_y = event.button.y;
          }
        } else if (event.button.button == SDL_BUTTON_LEFT) {
          if (in_sidebar) {
            // Load Image button
            if (event.button.y >= 15 && event.button.y <= 65 &&
                event.button.x >= window_width - sidebar_width + 20 &&
                event.button.x <= window_width - sidebar_width + 60) {
              open_file_dialog();
            }
            // Pagination
            if (event.button.y >= window_height - 50) {
              if (event.button.x < window_width - sidebar_width / 2) {
                if (inventory_page > 0)
                  inventory_page--;
              } else {
                int page_size = INVENTORY_COL_NUM * INVENTORY_ROW_NUM;
                int max_pages =
                    (inventory_pieces.size() + page_size - 1) / page_size;
                if (inventory_page < max_pages - 1)
                  inventory_page++;
              }
            }
            // Inventory slots
            int page_size = INVENTORY_COL_NUM * INVENTORY_ROW_NUM;
            int start_idx = inventory_page * page_size;
            for (int i = 0; i < page_size; ++i) {
              if (start_idx + i < inventory_pieces.size()) {
                SDL_Rect slot;
                get_inventory_slot_rect(i, slot);
                if (event.button.x >= slot.x &&
                    event.button.x <= slot.x + slot.w &&
                    event.button.y >= slot.y &&
                    event.button.y <= slot.y + slot.h) {

                  int piece_id = inventory_pieces[start_idx + i];
                  inventory_pieces.erase(inventory_pieces.begin() + start_idx +
                                         i);
                  in_inventory[piece_id] = false;
                  move_piece_to_board(piece_id, event.button.x, event.button.y);
                  selected_piece_id = piece_id;

                  float world_x = (event.button.x - camera_x) / camera_zoom;
                  float world_y = (event.button.y - camera_y) / camera_zoom;
                  grab_offset_x =
                      segmentation.get_pieces()[piece_id].offset_x - world_x;
                  grab_offset_y =
                      segmentation.get_pieces()[piece_id].offset_y - world_y;
                  break;
                }
              }
            }
          } else {
            if (hit_piece_id != -1) {
              selected_piece_id = hit_piece_id;
              grab_offset_x = hit_grab_x;
              grab_offset_y = hit_grab_y;
            } else {
              is_panning = true;
              last_mouse_x = event.button.x;
              last_mouse_y = event.button.y;
            }
          }
        }
      } else if (event.type == SDL_MOUSEMOTION) {
        if (is_panning) {
          camera_x += event.motion.x - last_mouse_x;
          camera_y += event.motion.y - last_mouse_y;
          last_mouse_x = event.motion.x;
          last_mouse_y = event.motion.y;
        } else if (selected_piece_id != -1) {
          float world_x = (event.motion.x - camera_x) / camera_zoom;
          float world_y = (event.motion.y - camera_y) / camera_zoom;
          segmentation.get_pieces()[selected_piece_id].offset_x =
              world_x + grab_offset_x;
          segmentation.get_pieces()[selected_piece_id].offset_y =
              world_y + grab_offset_y;
        }
      } else if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_RIGHT) {
          is_panning = false;
        } else if (event.button.button == SDL_BUTTON_LEFT) {
          if (selected_piece_id != -1) {
            if (is_mouse_over_sidebar(event.button.x)) {
              inventory_pieces.push_back(selected_piece_id);
              in_inventory[selected_piece_id] = true;
              selected_piece_id = -1;
            } else {
              if (board->snap_piece(selected_piece_id)) {
                clean_textures();
                int new_root = segmentation.get_uf().find(selected_piece_id);
                generate_texture(new_root);
                play_snap_sound();
              }
              selected_piece_id = -1;
              if (board->is_cleared()) {
                state = GameState::CLEARED;
              }
            }
          } else {
            is_panning = false;
          }
        }
      }
    } else if (state == GameState::MENU || state == GameState::CLEARED) {
      bool in_sidebar = is_mouse_over_sidebar(event.button.x);
      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        if (in_sidebar && event.button.y >= 15 && event.button.y <= 65 &&
            event.button.x >= window_width - sidebar_width + 20 &&
            event.button.x <= window_width - sidebar_width + 60) {
          open_file_dialog();
        } else if (state == GameState::CLEARED) {
          state = GameState::MENU;
          segmentation.cleanup();
          board.reset();
        }
      }
    }
  }
}

void Game::update() {
  if (state == GameState::PLAYING) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    float pan_speed = 30.0f;
    if (keys[SDL_SCANCODE_W])
      camera_y += pan_speed;
    if (keys[SDL_SCANCODE_S])
      camera_y -= pan_speed;
    if (keys[SDL_SCANCODE_A])
      camera_x += pan_speed;
    if (keys[SDL_SCANCODE_D])
      camera_x -= pan_speed;
  }
}

void Game::render_menu_buttons() {
  // Load Image Button
  SDL_Rect btn_rect = {window_width - sidebar_width + UI_OFFSET, UI_OFFSET, 50,
                       50};
  SDL_SetRenderDrawColor(renderer, 0xE0, 0xE0, 0xE0, 255);
  SDL_RenderFillRect(renderer, &btn_rect);
  // Raised border for button
  int btn_border = 3;
  for (int i = 0; i < btn_border; ++i) {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 255);
    SDL_RenderDrawLine(renderer, btn_rect.x + i, btn_rect.y + i,
                       btn_rect.x + btn_rect.w - 1 - i, btn_rect.y + i);
    SDL_RenderDrawLine(renderer, btn_rect.x + i, btn_rect.y + i, btn_rect.x + i,
                       btn_rect.y + btn_rect.h - 1 - i);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 255);
    SDL_RenderDrawLine(
        renderer, btn_rect.x + i, btn_rect.y + btn_rect.h - 1 - i,
        btn_rect.x + btn_rect.w - 1 - i, btn_rect.y + btn_rect.h - 1 - i);
    SDL_RenderDrawLine(renderer, btn_rect.x + btn_rect.w - 1 - i,
                       btn_rect.y + i, btn_rect.x + btn_rect.w - 1 - i,
                       btn_rect.y + btn_rect.h - 1 - i);
  }

  // Draw pixel art folder icon
  const char *icon_pixels[16] = {
      "                ", "  .....         ", " .DDDDD.        ",
      " .D...........  ", " .D.WWWWWWWW.D. ", " .D.WWWWWWWW.D. ",
      " .D.WWWWWWWW.D. ", " .............. ", " .YYYYYYYYYYYY. ",
      " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ",
      " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ",
      " .............  ",
  };

  int scale = 2;
  int offset_x = btn_rect.x + (btn_rect.w - 16 * scale) / 2;
  int offset_y = btn_rect.y + (btn_rect.h - 16 * scale) / 2;

  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      char c = icon_pixels[y][x];
      if (c == ' ')
        continue;

      if (c == '.')
        SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
      else if (c == 'D')
        SDL_SetRenderDrawColor(renderer, 220, 160, 40, 255); // Dark yellow
      else if (c == 'W')
        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255); // White paper
      else if (c == 'Y')
        SDL_SetRenderDrawColor(renderer, 255, 210, 80, 255); // Light yellow

      SDL_Rect p = {offset_x + x * scale, offset_y + y * scale, scale, scale};
      SDL_RenderFillRect(renderer, &p);
    }
  }
}

void Game::render_inventory() {
  SDL_Rect sidebar_rect = {window_width - sidebar_width + UI_OFFSET,
                           2 * UI_OFFSET + 50, sidebar_width - 2 * UI_OFFSET,
                           window_height - 3 * UI_OFFSET - 50};
  SDL_SetRenderDrawColor(renderer, 0xE0, 0xE0, 0xE0, 255);
  SDL_RenderFillRect(renderer, &sidebar_rect);

  // Raised border for sidebar
  int inv_border = 3;
  for (int i = 0; i < inv_border; ++i) {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 255);
    SDL_RenderDrawLine(renderer, sidebar_rect.x + i, sidebar_rect.y + i,
                       sidebar_rect.x + sidebar_rect.w - 1 - i,
                       sidebar_rect.y + i);
    SDL_RenderDrawLine(renderer, sidebar_rect.x + i, sidebar_rect.y + i,
                       sidebar_rect.x + i,
                       sidebar_rect.y + sidebar_rect.h - 1 - i);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 255);
    SDL_RenderDrawLine(renderer, sidebar_rect.x + i,
                       sidebar_rect.y + sidebar_rect.h - 1 - i,
                       sidebar_rect.x + sidebar_rect.w - 1 - i,
                       sidebar_rect.y + sidebar_rect.h - 1 - i);
    SDL_RenderDrawLine(renderer, sidebar_rect.x + sidebar_rect.w - 1 - i,
                       sidebar_rect.y + i,
                       sidebar_rect.x + sidebar_rect.w - 1 - i,
                       sidebar_rect.y + sidebar_rect.h - 1 - i);
  }

  SDL_Rect inv_band = {sidebar_rect.x + inv_border, sidebar_rect.y + inv_border,
                       sidebar_rect.w - 2 * inv_border, 24};
  SDL_SetRenderDrawColor(renderer, 0x20, 0x10, 0x60, 255);
  SDL_RenderFillRect(renderer, &inv_band);

  int text_w = 9 * 6 * 2; // "Inventory" is 9 chars
  draw_text("Inventory", sidebar_rect.x + (sidebar_rect.w - text_w) / 2,
            sidebar_rect.y + 8, 2, 255, 255, 255);

  if (state == GameState::PLAYING) {
    int page_size = INVENTORY_COL_NUM * INVENTORY_ROW_NUM;
    int start_idx = inventory_page * page_size;
    for (int i = 0; i < page_size; ++i) {
      if (start_idx + i < inventory_pieces.size()) {
        int piece_id = inventory_pieces[start_idx + i];

        SDL_Rect slot;
        get_inventory_slot_rect(i, slot);

        if (piece_textures.find(piece_id) != piece_textures.end()) {
          const auto &pt = piece_textures[piece_id];

          float scale = std::min((float)slot.w / (pt.w + 10),
                                 (float)slot.h / (pt.h + 10));
          if (scale > 1.0f)
            scale = 1.0f;

          float offset_x = slot.x + slot.w / 2.0f - (pt.w / 2.0f) * scale;
          float offset_y = slot.y + slot.h / 2.0f - (pt.h / 2.0f) * scale;

          SDL_FRect dst = {offset_x, offset_y, pt.w * scale, pt.h * scale};
          SDL_RenderCopyF(renderer, pt.texture, NULL, &dst);
        }
      }
    }

    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 255);
    int my = window_height - 25 - UI_OFFSET;
    int lx = window_width - sidebar_width / 2 - 40;
    SDL_RenderDrawLine(renderer, lx, my, lx + 15, my - 10);
    SDL_RenderDrawLine(renderer, lx, my, lx + 15, my + 10);

    int rx = window_width - sidebar_width / 2 + 40;
    SDL_RenderDrawLine(renderer, rx, my, rx - 15, my - 10);
    SDL_RenderDrawLine(renderer, rx, my, rx - 15, my + 10);

    int max_pages = (inventory_pieces.size() + page_size - 1) / page_size;
    if (max_pages == 0)
      max_pages = 1;
    char page_str[16];
    snprintf(page_str, sizeof(page_str), "%d/%d", inventory_page + 1,
             max_pages);
    int p_w = 0;
    for (int i = 0; page_str[i]; ++i)
      p_w += 6 * 2;
    draw_text(page_str, window_width - sidebar_width / 2 - p_w / 2, my - 7, 2);
  }
}

void Game::render_playground() {
  SDL_Rect playground_rect = {UI_OFFSET, UI_OFFSET,
                              window_width - sidebar_width - 2 * UI_OFFSET,
                              window_height - 2 * UI_OFFSET};
  SDL_SetRenderDrawColor(renderer, 0xE0, 0xE0, 0xE0, 255);
  SDL_RenderFillRect(renderer, &playground_rect);

  // Sunken border for playground
  int pg_border = 3;
  for (int i = 0; i < pg_border; ++i) {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 255);
    SDL_RenderDrawLine(renderer, playground_rect.x + i, playground_rect.y + i,
                       playground_rect.x + playground_rect.w - 1 - i,
                       playground_rect.y + i);
    SDL_RenderDrawLine(renderer, playground_rect.x + i, playground_rect.y + i,
                       playground_rect.x + i,
                       playground_rect.y + playground_rect.h - 1 - i);
    SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 255);
    SDL_RenderDrawLine(renderer, playground_rect.x + i,
                       playground_rect.y + playground_rect.h - 1 - i,
                       playground_rect.x + playground_rect.w - 1 - i,
                       playground_rect.y + playground_rect.h - 1 - i);
    SDL_RenderDrawLine(renderer, playground_rect.x + playground_rect.w - 1 - i,
                       playground_rect.y + i,
                       playground_rect.x + playground_rect.w - 1 - i,
                       playground_rect.y + playground_rect.h - 1 - i);
  }

  SDL_Rect pg_band = {playground_rect.x + pg_border,
                      playground_rect.y + pg_border,
                      playground_rect.w - 2 * pg_border, 24};
  SDL_SetRenderDrawColor(renderer, 0x20, 0x10, 0x60, 255);
  SDL_RenderFillRect(renderer, &pg_band);

  draw_text("Playground", playground_rect.x + 10, playground_rect.y + 8, 2, 255,
            255, 255);

  auto &pieces = segmentation.get_pieces();
  auto &uf = segmentation.get_uf();

  int dragged_root =
      (selected_piece_id != -1) ? uf.find(selected_piece_id) : -1;

  SDL_Rect clip_rect = {playground_rect.x + pg_border,
                        playground_rect.y + pg_border + 24,
                        playground_rect.w - 2 * pg_border,
                        playground_rect.h - 2 * pg_border - 24};
  SDL_RenderSetClipRect(renderer, &clip_rect);

  SDL_RenderSetScale(renderer, camera_zoom, camera_zoom);

  // Draw pieces on board
  for (int i = 0; i < pieces.size(); ++i) {
    if (uf.find(i) == i && i != dragged_root) {
      if (in_inventory[i])
        continue;

      if (piece_textures.find(i) != piece_textures.end()) {
        const auto &pt = piece_textures[i];
        const auto &p = pieces[i];

        SDL_FRect dst = {(float)pt.min_x + p.offset_x + camera_x / camera_zoom,
                         (float)pt.min_y + p.offset_y + camera_y / camera_zoom,
                         (float)pt.w, (float)pt.h};
        SDL_RenderCopyF(renderer, pt.texture, NULL, &dst);
      }
    }
  }

  if (dragged_root != -1) {
    if (piece_textures.find(dragged_root) != piece_textures.end()) {
      const auto &pt = piece_textures[dragged_root];
      const auto &p = pieces[dragged_root];

      SDL_FRect dst = {(float)pt.min_x + p.offset_x + camera_x / camera_zoom,
                       (float)pt.min_y + p.offset_y + camera_y / camera_zoom,
                       (float)pt.w, (float)pt.h};
      SDL_RenderCopyF(renderer, pt.texture, NULL, &dst);
    }
  }

  SDL_RenderSetScale(renderer, 1.0f, 1.0f);
  SDL_RenderSetClipRect(renderer, NULL);
}

void Game::render() {
  SDL_SetRenderDrawColor(renderer, 0x80, 0x80, 0x80, 255);
  SDL_RenderClear(renderer);

  render_playground();
  render_menu_buttons();
  render_inventory();

  SDL_RenderPresent(renderer);
}

void Game::run() {
  while (is_running) {
    handle_events();
    update();
    render();
    SDL_Delay(16);
  }
}

void Game::clean() {
  for (auto &pair : piece_textures) {
    if (pair.second.texture)
      SDL_DestroyTexture(pair.second.texture);
  }
  piece_textures.clear();

  segmentation.cleanup();
  if (audio_device != 0) {
    SDL_CloseAudioDevice(audio_device);
    audio_device = 0;
  }
  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }
  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }
  SDL_Quit();
}
