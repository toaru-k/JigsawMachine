#include "Game.h"
#include "../graphics/Font.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
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
      grab_offset_y(0.0f), sidebar_width(200), inventory_page(0),
      clear_anim_start_time(0), clear_click_x(0), clear_click_y(0),
      original_texture(nullptr), retro_texture(nullptr),
      clear_retro_surf(nullptr), clear_sound_played(false) {}

Game::~Game() { clean(); }

bool Game::init(const char *title, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {

    return false;
  }

  window =
      SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
  if (!window) {

    return false;
  }

  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {

    return false;
  }

  SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

  window_width = width;
  window_height = height;
  is_running = true;



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

void Game::play_clear_sound() {
  if (audio_device == 0)
    return;

  const int sample_rate = 44100;
  const int duration_ms = 800; // 800ms clear jingle
  const int num_samples = (sample_rate * duration_ms) / 1000;
  std::vector<int16_t> buffer(num_samples);

  // Arpeggio frequencies: C5, E5, G5, C6
  float freqs[] = {523.25f, 659.25f, 783.99f, 1046.50f};
  int num_notes = 4;
  int samples_per_note = num_samples / num_notes;

  float phase = 0.0f;
  for (int i = 0; i < num_samples; ++i) {
    int note_idx = i / samples_per_note;
    float freq = freqs[note_idx];

    // Envelope per note
    int note_sample = i % samples_per_note;
    float volume = 1.0f - (float)note_sample / samples_per_note;
    if (note_idx == num_notes - 1) {
      volume = 1.0f - (float)note_sample / (samples_per_note *
                                            2.0f); // Last note rings out longer
      if (volume < 0.0f)
        volume = 0.0f;
    }

    // Square wave + slight duty cycle variation
    float wave = (sin(phase) > 0.5f) ? 1.0f : -1.0f;

    buffer[i] = (int16_t)(16000.0f * volume * wave);
    phase += 2.0f * M_PI * freq / sample_rate;
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
    int px = pix % segmentation.w;
    int py = pix / segmentation.w;
    if (px < min_x)
      min_x = px;
    if (px > max_x)
      max_x = px;
    if (py < min_y)
      min_y = py;
    if (py > max_y)
      max_y = py;
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
    int px = pix % segmentation.w;
    int py = pix / segmentation.w;
    int lx = px - min_x;
    int ly = py - min_y;
    in_piece[ly * w + lx] = true;
  }

  for (const auto &pix : p.pixels) {
    int px = pix % segmentation.w;
    int py = pix / segmentation.w;
    int lx = px - min_x;
    int ly = py - min_y;

    bool top = ly == 0 || !in_piece[(ly - 1) * w + lx];
    bool left = lx == 0 || !in_piece[ly * w + (lx - 1)];
    bool bottom = ly == h - 1 || !in_piece[(ly + 1) * w + lx];
    bool right = lx == w - 1 || !in_piece[ly * w + (lx + 1)];

    int orig_idx = pix;
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
  pending_filepath = filepath;
  state = GameState::DIFFICULTY_SELECT;
}

void Game::start_puzzle(int max_dimension) {
  for (auto &pair : piece_textures) {
    if (pair.second.texture)
      SDL_DestroyTexture(pair.second.texture);
  }
  piece_textures.clear();

  if (original_texture) {
    SDL_DestroyTexture(original_texture);
    original_texture = nullptr;
  }
  if (retro_texture) {
    SDL_DestroyTexture(retro_texture);
    retro_texture = nullptr;
  }
  if (clear_retro_surf) {
    SDL_FreeSurface(clear_retro_surf);
    clear_retro_surf = nullptr;
  }
  segmentation.cleanup();


  if (segmentation.init(pending_filepath.c_str(), max_dimension)) {
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

  } else {

    state = GameState::MENU;
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

void Game::save_game_dialog() {
  if (state != GameState::PLAYING && state != GameState::CLEARED)
    return;
  OPENFILENAMEA ofn;
  char szFile[260];
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "Save Files (*.sav)\0*.sav\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrDefExt = "sav";
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;

  if (GetSaveFileNameA(&ofn) == TRUE) {
    if (save_game(ofn.lpstrFile)) {

    } else {

    }
  }
}

void Game::load_game_dialog() {
  OPENFILENAMEA ofn;
  char szFile[260];
  ZeroMemory(&ofn, sizeof(ofn));
  ofn.lStructSize = sizeof(ofn);
  ofn.hwndOwner = NULL;
  ofn.lpstrFile = szFile;
  ofn.lpstrFile[0] = '\0';
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFilter = "Save Files (*.sav)\0*.sav\0All Files (*.*)\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.lpstrFileTitle = NULL;
  ofn.nMaxFileTitle = 0;
  ofn.lpstrInitialDir = NULL;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

  if (GetOpenFileNameA(&ofn) == TRUE) {
    if (load_game(ofn.lpstrFile)) {

    } else {

    }
  }
}

bool Game::save_game(const std::string &filepath) {
  std::FILE *f = std::fopen(filepath.c_str(), "wb");
  if (!f)
    return false;

  std::fwrite("JGSM", 1, 4, f);

  int w = segmentation.get_width();
  int h = segmentation.get_height();
  std::fwrite(&w, sizeof(int), 1, f);
  std::fwrite(&h, sizeof(int), 1, f);

  std::FILE *img_f = std::fopen(pending_filepath.c_str(), "rb");
  if (!img_f) {
    std::fclose(f);
    return false;
  }
  std::fseek(img_f, 0, SEEK_END);
  size_t img_size = std::ftell(img_f);
  std::fseek(img_f, 0, SEEK_SET);

  std::fwrite(&img_size, sizeof(size_t), 1, f);
  std::vector<char> img_buf(img_size);
  std::fread(img_buf.data(), 1, img_size, img_f);
  std::fwrite(img_buf.data(), 1, img_size, f);
  std::fclose(img_f);

  auto &uf = segmentation.get_uf();
  for (int i = 0; i < w * h; ++i) {
    int parent = uf.find(i);
    std::fwrite(&parent, sizeof(int), 1, f);
  }

  auto &pieces = segmentation.get_pieces();
  int num_roots = 0;
  for (int i = 0; i < pieces.size(); ++i) {
    if (uf.find(i) == i && pieces[i].num_pixels > 0)
      num_roots++;
  }
  std::fwrite(&num_roots, sizeof(int), 1, f);

  for (int i = 0; i < pieces.size(); ++i) {
    if (uf.find(i) == i && pieces[i].num_pixels > 0) {
      std::fwrite(&i, sizeof(int), 1, f);
      std::fwrite(&pieces[i].offset_x, sizeof(int), 1, f);
      std::fwrite(&pieces[i].offset_y, sizeof(int), 1, f);
      bool inv = false;
      if (i < in_inventory.size() && in_inventory[i])
        inv = true;
      std::fwrite(&inv, sizeof(bool), 1, f);
    }
  }

  std::fclose(f);
  return true;
}

bool Game::load_game(const std::string &filepath) {
  std::FILE *f = std::fopen(filepath.c_str(), "rb");
  if (!f)
    return false;

  char magic[5] = {0};
  std::fread(magic, 1, 4, f);
  if (std::string(magic) != "JGSM") {
    std::fclose(f);
    return false;
  }

  int w, h;
  std::fread(&w, sizeof(int), 1, f);
  std::fread(&h, sizeof(int), 1, f);

  size_t img_size;
  std::fread(&img_size, sizeof(size_t), 1, f);
  std::vector<char> img_buf(img_size);
  std::fread(img_buf.data(), 1, img_size, f);

  std::string temp_img_path = "temp_loaded_image.dat";
  std::FILE *tmp_f = std::fopen(temp_img_path.c_str(), "wb");
  if (tmp_f) {
    std::fwrite(img_buf.data(), 1, img_size, tmp_f);
    std::fclose(tmp_f);
  }

  pending_filepath = temp_img_path;

  for (auto &pair : piece_textures) {
    if (pair.second.texture)
      SDL_DestroyTexture(pair.second.texture);
  }
  piece_textures.clear();

  if (original_texture) {
    SDL_DestroyTexture(original_texture);
    original_texture = nullptr;
  }
  if (retro_texture) {
    SDL_DestroyTexture(retro_texture);
    retro_texture = nullptr;
  }
  if (clear_retro_surf) {
    SDL_FreeSurface(clear_retro_surf);
    clear_retro_surf = nullptr;
  }
  segmentation.cleanup();

  int max_dim = std::max(w, h);
  if (!segmentation.init(pending_filepath.c_str(), max_dim, true)) {
    std::fclose(f);
    return false;
  }

  auto &uf = segmentation.get_uf();
  auto &parents = uf.get_parents();
  for (int i = 0; i < w * h; ++i) {
    int parent;
    std::fread(&parent, sizeof(int), 1, f);
    parents[i] = parent;
  }

  auto &pieces = segmentation.get_pieces();
  post_unite_pieces(pieces, uf);

  int num_roots;
  std::fread(&num_roots, sizeof(int), 1, f);

  in_inventory.assign(pieces.size(), false);
  inventory_pieces.clear();
  inventory_page = 0;

  for (int i = 0; i < num_roots; ++i) {
    int id;
    std::fread(&id, sizeof(int), 1, f);
    std::fread(&pieces[id].offset_x, sizeof(int), 1, f);
    std::fread(&pieces[id].offset_y, sizeof(int), 1, f);
    bool inv;
    std::fread(&inv, sizeof(bool), 1, f);
    if (inv) {
      in_inventory[id] = true;
      inventory_pieces.push_back(id);
    }
    generate_texture(id);
  }

  board = std::make_unique<PuzzleBoard>(
      segmentation.get_pieces(), segmentation.get_uf(),
      segmentation.get_width(), segmentation.get_height());

  camera_x = 50.0f;
  camera_y = 50.0f;
  camera_zoom = 1.0f;
  state = GameState::PLAYING;

  std::fclose(f);
  return true;
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
    } else if (state == GameState::PLAYING || state == GameState::CLEARED) {
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

          if (state == GameState::PLAYING) {
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
                    float px_pix = (pix % segmentation.w) + p.offset_x;
                    float py_pix = (pix / segmentation.w) + p.offset_y;
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
            // Menu buttons
            if (event.button.y >= UI_OFFSET &&
                event.button.y <= UI_OFFSET + 50) {
              int btn_w = (sidebar_width - 4 * UI_OFFSET) / 3;
              int bx1 = window_width - sidebar_width + UI_OFFSET;
              int bx2 = bx1 + btn_w + UI_OFFSET;
              int bx3 = bx2 + btn_w + UI_OFFSET;

              if (event.button.x >= bx1 && event.button.x < bx1 + btn_w) {
                open_file_dialog();
              } else if (event.button.x >= bx2 &&
                         event.button.x < bx2 + btn_w) {
                save_game_dialog();
              } else if (event.button.x >= bx3 &&
                         event.button.x <= bx3 + btn_w) {
                load_game_dialog();
              }
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
            if (hit_piece_id != -1 && state == GameState::PLAYING) {
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
                clear_anim_start_time = SDL_GetTicks();
                clear_sound_played = false;
                clear_anim_prev_radius = 0;

                // Get the position of the click in the puzzle image space
                auto &pieces = segmentation.get_pieces();
                int root = segmentation.get_uf().find(
                    0); // Everything is one piece now

                float world_x = (event.button.x - camera_x) / camera_zoom;
                float world_y = (event.button.y - camera_y) / camera_zoom;

                clear_click_x = (int)world_x - pieces[root].offset_x;
                clear_click_y = (int)world_y - pieces[root].offset_y;

                // If the user clicked outside the image somehow, clamp it
                if (clear_click_x < 0)
                  clear_click_x = 0;
                if (clear_click_x >= segmentation.get_width())
                  clear_click_x = segmentation.get_width() - 1;
                if (clear_click_y < 0)
                  clear_click_y = 0;
                if (clear_click_y >= segmentation.get_height())
                  clear_click_y = segmentation.get_height() - 1;

                // We will generate the original and retro textures here
                SDL_Surface *orig_surf = SDL_CreateRGBSurfaceWithFormat(
                    0, segmentation.orig_w, segmentation.orig_h, 32,
                    SDL_PIXELFORMAT_RGBA32);
                if (clear_retro_surf)
                  SDL_FreeSurface(clear_retro_surf);
                clear_retro_surf = SDL_CreateRGBSurfaceWithFormat(
                    0, segmentation.get_width(), segmentation.get_height(), 32,
                    SDL_PIXELFORMAT_RGBA32);

                SDL_LockSurface(orig_surf);
                SDL_LockSurface(clear_retro_surf);
                Uint32 *orig_pixels = (Uint32 *)orig_surf->pixels;
                Uint32 *retro_pixels = (Uint32 *)clear_retro_surf->pixels;
                int orig_pitch = orig_surf->pitch / 4;
                int retro_pitch = clear_retro_surf->pitch / 4;

                for (int y = 0; y < segmentation.orig_h; ++y) {
                  for (int x = 0; x < segmentation.orig_w; ++x) {
                    int idx = (y * segmentation.orig_w + x) * 3;
                    unsigned char r = segmentation.original_data[idx + 0];
                    unsigned char g = segmentation.original_data[idx + 1];
                    unsigned char b = segmentation.original_data[idx + 2];
                    orig_pixels[y * orig_pitch + x] =
                        SDL_MapRGBA(orig_surf->format, r, g, b, 255);
                  }
                }

                for (int y = 0; y < segmentation.get_height(); ++y) {
                  for (int x = 0; x < segmentation.get_width(); ++x) {
                    int idx = (y * segmentation.get_width() + x) * 3;
                    unsigned char r = segmentation.data[idx + 0];
                    unsigned char g = segmentation.data[idx + 1];
                    unsigned char b = segmentation.data[idx + 2];

                    // Retro is masked
                    unsigned char rr = r & 0xf0;
                    unsigned char rg = g & 0xf0;
                    unsigned char rb = b & 0xf0;
                    retro_pixels[y * retro_pitch + x] =
                        SDL_MapRGBA(clear_retro_surf->format, rr, rg, rb, 255);
                  }
                }
                SDL_UnlockSurface(clear_retro_surf);
                SDL_UnlockSurface(orig_surf);

                if (original_texture)
                  SDL_DestroyTexture(original_texture);
                original_texture =
                    SDL_CreateTextureFromSurface(renderer, orig_surf);
                SDL_SetTextureBlendMode(original_texture, SDL_BLENDMODE_BLEND);

                if (retro_texture)
                  SDL_DestroyTexture(retro_texture);
                retro_texture = SDL_CreateTexture(
                    renderer, SDL_PIXELFORMAT_RGBA32,
                    SDL_TEXTUREACCESS_STREAMING, segmentation.get_width(),
                    segmentation.get_height());
                SDL_SetTextureBlendMode(retro_texture, SDL_BLENDMODE_BLEND);

                SDL_UpdateTexture(retro_texture, NULL, clear_retro_surf->pixels,
                                  clear_retro_surf->pitch);

                SDL_FreeSurface(orig_surf);

                particles.clear();
              }
            }
          } else {
            is_panning = false;
          }
        }
      }
    } else if (state == GameState::MENU) {
      bool in_sidebar = is_mouse_over_sidebar(event.button.x);
      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        if (in_sidebar) {
          if (event.button.y >= UI_OFFSET && event.button.y <= UI_OFFSET + 50) {
            int btn_w = (sidebar_width - 4 * UI_OFFSET) / 3;
            int bx1 = window_width - sidebar_width + UI_OFFSET;
            int bx2 = bx1 + btn_w + UI_OFFSET;
            int bx3 = bx2 + btn_w + UI_OFFSET;

            if (event.button.x >= bx1 && event.button.x < bx1 + btn_w) {
              open_file_dialog();
            } else if (event.button.x >= bx2 && event.button.x < bx2 + btn_w) {
              // save_game_dialog() is ignored in MENU state
            } else if (event.button.x >= bx3 && event.button.x <= bx3 + btn_w) {
              load_game_dialog();
            }
          }
        }
      }
    } else if (state == GameState::DIFFICULTY_SELECT) {
      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        int x = event.button.x;
        int y = event.button.y;
        SDL_Rect modal = {window_width / 2 - 150, window_height / 2 - 120, 300,
                          240};

        SDL_Rect btn1 = {modal.x + 50, modal.y + 60, 200, 30};
        SDL_Rect btn2 = {modal.x + 50, modal.y + 100, 200, 30};
        SDL_Rect btn3 = {modal.x + 50, modal.y + 140, 200, 30};
        SDL_Rect btn4 = {modal.x + 50, modal.y + 180, 200, 30};

        if (x >= btn1.x && x <= btn1.x + btn1.w && y >= btn1.y &&
            y <= btn1.y + btn1.h) {
          start_puzzle(100); // Easy
        } else if (x >= btn2.x && x <= btn2.x + btn2.w && y >= btn2.y &&
                   y <= btn2.y + btn2.h) {
          start_puzzle(250); // Normal
        } else if (x >= btn3.x && x <= btn3.x + btn3.w && y >= btn3.y &&
                   y <= btn3.y + btn3.h) {
          start_puzzle(400); // Hard
        } else if (x >= btn4.x && x <= btn4.x + btn4.w && y >= btn4.y &&
                   y <= btn4.y + btn4.h) {
          start_puzzle(4000); // Very Hard
        }
      }
    }
  }
}

void Game::update() {
  if (state == GameState::PLAYING || state == GameState::CLEARED) {
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

  if (state == GameState::CLEARED) {
    if (!clear_sound_played) {
      play_clear_sound();
      clear_sound_played = true;
    }

    for (auto &p : particles) {
      p.x += p.vx;
      p.y += p.vy;
      p.vx += 0.05f; // wind
      p.vy += 0.02f; // gravity slightly less than wind
      p.life--;
    }
    particles.erase(
        std::remove_if(particles.begin(), particles.end(),
                       [](const Particle &p) { return p.life <= 0; }),
        particles.end());

    Uint32 current_time = SDL_GetTicks();
    float progress =
        (current_time - clear_anim_start_time) / 2500.0f; // 2.5 seconds
    if (progress > 1.0f)
      progress = 1.0f;

    int max_radius =
        std::max(segmentation.get_width(), segmentation.get_height()) * 1.5f;
    int current_radius = (int)(max_radius * progress);

    if (retro_texture && clear_retro_surf &&
        current_radius > clear_anim_prev_radius) {
      SDL_LockSurface(clear_retro_surf);
      Uint32 *format_pixels = (Uint32 *)clear_retro_surf->pixels;
      int w = segmentation.get_width();
      int h = segmentation.get_height();
      int pitch = clear_retro_surf->pitch / 4;

      int min_x = std::max(0, clear_click_x - current_radius);
      int max_x = std::min(w - 1, clear_click_x + current_radius);
      int min_y = std::max(0, clear_click_y - current_radius);
      int max_y = std::min(h - 1, clear_click_y + current_radius);

      SDL_PixelFormat *fmt = clear_retro_surf->format;

      for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
          int dx = x - clear_click_x;
          int dy = y - clear_click_y;
          int dist_sq = dx * dx + dy * dy;

          // If pixel is in the newly expanded ring
          if (dist_sq <= current_radius * current_radius &&
              dist_sq > clear_anim_prev_radius * clear_anim_prev_radius) {
            Uint32 current_color = format_pixels[y * pitch + x];
            if (current_color != 0) {
              Uint8 r, g, b, a;
              SDL_GetRGBA(current_color, fmt, &r, &g, &b, &a);
              format_pixels[y * pitch + x] = 0; // Make transparent

              // Spawn particle
              if ((std::rand() % 100) < 5) {
                Particle p;
                p.x = x;
                p.y = y;
                p.vx = (std::rand() % 100) / 50.0f - 1.0f + 3.0f; // drift right
                p.vy = (std::rand() % 100) / 50.0f - 2.0f - 2.0f; // drift up
                p.r = r;
                p.g = g;
                p.b = b;
                p.a = 255;
                p.max_life = p.life = 60 + std::rand() % 60;
                p.size = 2.0f + (std::rand() % 4);
                particles.push_back(p);
              }
            }
          }
        }
      }
      SDL_UnlockSurface(clear_retro_surf);

      // Update texture from surface
      SDL_Rect update_rect = {min_x, min_y, max_x - min_x + 1,
                              max_y - min_y + 1};
      // Convert CPU rect to memory offset for SDL_UpdateTexture
      Uint32 *offset_pixels = format_pixels + (min_y * pitch + min_x);
      SDL_UpdateTexture(retro_texture, &update_rect, offset_pixels,
                        clear_retro_surf->pitch);

      clear_anim_prev_radius = current_radius;
    }
  }
}

void Game::render_menu_buttons() {
  int btn_w = (sidebar_width - 4 * UI_OFFSET) / 3;
  int bx[3] = {window_width - sidebar_width + UI_OFFSET,
               window_width - sidebar_width + 2 * UI_OFFSET + btn_w,
               window_width - sidebar_width + 3 * UI_OFFSET + 2 * btn_w};

  for (int b = 0; b < 3; ++b) {
    SDL_Rect btn_rect = {bx[b], UI_OFFSET, btn_w, 50};
    SDL_SetRenderDrawColor(renderer, 0xE0, 0xE0, 0xE0, 255);
    SDL_RenderFillRect(renderer, &btn_rect);
    int btn_border = 3;
    for (int i = 0; i < btn_border; ++i) {
      SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 255);
      SDL_RenderDrawLine(renderer, btn_rect.x + i, btn_rect.y + i,
                         btn_rect.x + btn_rect.w - 1 - i, btn_rect.y + i);
      SDL_RenderDrawLine(renderer, btn_rect.x + i, btn_rect.y + i,
                         btn_rect.x + i, btn_rect.y + btn_rect.h - 1 - i);
      SDL_SetRenderDrawColor(renderer, 0x40, 0x40, 0x40, 255);
      SDL_RenderDrawLine(
          renderer, btn_rect.x + i, btn_rect.y + btn_rect.h - 1 - i,
          btn_rect.x + btn_rect.w - 1 - i, btn_rect.y + btn_rect.h - 1 - i);
      SDL_RenderDrawLine(renderer, btn_rect.x + btn_rect.w - 1 - i,
                         btn_rect.y + i, btn_rect.x + btn_rect.w - 1 - i,
                         btn_rect.y + btn_rect.h - 1 - i);
    }
  }

  int scale = 2;

  auto draw_icon = [&](const char *icon[16], int offset_x, int offset_y) {
    for (int y = 0; y < 16; ++y) {
      for (int x = 0; x < 16; ++x) {
        char c = icon[y][x];
        if (c == ' ')
          continue;
        if (c == '.')
          SDL_SetRenderDrawColor(renderer, 40, 40, 40, 255);
        else if (c == 'D')
          SDL_SetRenderDrawColor(renderer, 220, 160, 40, 255);
        else if (c == 'W')
          SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        else if (c == 'Y')
          SDL_SetRenderDrawColor(renderer, 255, 210, 80, 255);
        else if (c == 'B')
          SDL_SetRenderDrawColor(renderer, 100, 150, 255, 255);
        else if (c == 'G')
          SDL_SetRenderDrawColor(renderer, 80, 200, 80, 255);
        else if (c == 'S')
          SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);

        SDL_Rect p = {offset_x + x * scale, offset_y + y * scale, scale, scale};
        SDL_RenderFillRect(renderer, &p);
      }
    }
  };

  const char *load_img_pixels[16] = {
      "                ", "  .....         ", " .DDDDD.        ",
      " .D............ ", " .D.BBBGSSBB.D. ", " .D.BBGGGSBB.D. ",
      " .D.BGGGGGBB.D. ", " .............. ", " .YYYYYYYYYYYY. ",
      " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ",
      " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ",
      " .............  ",
  };
  draw_icon(load_img_pixels, bx[0] + (btn_w - 16 * scale) / 2,
            UI_OFFSET + (50 - 16 * scale) / 2);

  const char *save_pixels[16] = {
      "                ", "  .....         ", " .DDDDD.        ",
      " .D...........  ", " .D.WWWWWWWW.D. ", " .D.WWWWWWWW.D. ",
      " .D.WWWBBWWW.D. ", " .....BBBB..... ", " .YYYBBBBBBYYY. ",
      " .YYYYYBBYYYYY. ", " .YYYYYBBYYYYY. ", " .YYYYYBBYYYYY. ",
      " .YYYYYBBYYYYY. ", " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ",
      " .............  ",
  };
  draw_icon(save_pixels, bx[1] + (btn_w - 16 * scale) / 2,
            UI_OFFSET + (50 - 16 * scale) / 2);

  const char *load_game_pixels[16] = {
      "                ", "  .....         ", " .DDDDD.        ",
      " .D...........  ", " .D.WWWWWWWW.D. ", " .D.WWWWWWWW.D. ",
      " .D.WWWBBWWW.D. ", " ......BB...... ", " .YYYYYBBYYYYY. ",
      " .YYYYYBBYYYYY. ", " .YYYBBBBBBYYY. ", " .YYYYBBBBYYYY. ",
      " .YYYYYBBYYYYY. ", " .YYYYYYYYYYYY. ", " .YYYYYYYYYYYY. ",
      " .............  ",
  };
  draw_icon(load_game_pixels, bx[2] + (btn_w - 16 * scale) / 2,
            UI_OFFSET + (50 - 16 * scale) / 2);
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

  if (state == GameState::CLEARED) {
    int root = uf.find(0);
    const auto &p = pieces[root];

    SDL_FRect dst = {(float)p.offset_x + camera_x / camera_zoom,
                     (float)p.offset_y + camera_y / camera_zoom,
                     (float)segmentation.get_width(),
                     (float)segmentation.get_height()};

    if (original_texture) {
      SDL_RenderCopyF(renderer, original_texture, NULL, &dst);
    }
    if (retro_texture) {
      SDL_RenderCopyF(renderer, retro_texture, NULL, &dst);
    }

    // Render particles
    for (const auto &part : particles) {
      Uint8 alpha = part.a * part.life / part.max_life;
      SDL_SetRenderDrawColor(renderer, part.r, part.g, part.b, alpha);
      SDL_FRect pdst = {(float)p.offset_x + part.x + camera_x / camera_zoom,
                        (float)p.offset_y + part.y + camera_y / camera_zoom,
                        part.size, part.size};
      SDL_RenderFillRectF(renderer, &pdst);
    }
  } else {
    // Draw pieces on board
    for (int i = 0; i < pieces.size(); ++i) {
      if (uf.find(i) == i && i != dragged_root) {
        if (in_inventory[i])
          continue;

        if (piece_textures.find(i) != piece_textures.end()) {
          const auto &pt = piece_textures[i];
          const auto &p = pieces[i];

          SDL_FRect dst = {
              (float)pt.min_x + p.offset_x + camera_x / camera_zoom,
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

  if (state == GameState::DIFFICULTY_SELECT) {
    SDL_Rect modal = {window_width / 2 - 150, window_height / 2 - 120, 300,
                      240};
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_RenderFillRect(renderer, &modal);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderDrawRect(renderer, &modal);

    draw_text("Select Difficulty", modal.x + 10, modal.y + 20, 2, 255, 255,
              255);

    SDL_Rect btn1 = {modal.x + 50, modal.y + 60, 200, 30};
    SDL_SetRenderDrawColor(renderer, 40, 140, 40, 255);
    SDL_RenderFillRect(renderer, &btn1);
    draw_text("Easy", btn1.x + 75, btn1.y + 8, 2, 255, 255, 255);

    SDL_Rect btn2 = {modal.x + 50, modal.y + 100, 200, 30};
    SDL_SetRenderDrawColor(renderer, 180, 140, 40, 255);
    SDL_RenderFillRect(renderer, &btn2);
    draw_text("Normal", btn2.x + 65, btn2.y + 8, 2, 255, 255, 255);

    SDL_Rect btn3 = {modal.x + 50, modal.y + 140, 200, 30};
    SDL_SetRenderDrawColor(renderer, 180, 40, 40, 255);
    SDL_RenderFillRect(renderer, &btn3);
    draw_text("Hard", btn3.x + 75, btn3.y + 8, 2, 255, 255, 255);

    SDL_Rect btn4 = {modal.x + 50, modal.y + 180, 200, 30};
    SDL_SetRenderDrawColor(renderer, 100, 0, 0, 255);
    SDL_RenderFillRect(renderer, &btn4);
    draw_text("Very Hard", btn4.x + 45, btn4.y + 8, 2, 255, 255, 255);
  }

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

  if (original_texture) {
    SDL_DestroyTexture(original_texture);
    original_texture = nullptr;
  }
  if (retro_texture) {
    SDL_DestroyTexture(retro_texture);
    retro_texture = nullptr;
  }
  if (clear_retro_surf) {
    SDL_FreeSurface(clear_retro_surf);
    clear_retro_surf = nullptr;
  }

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
