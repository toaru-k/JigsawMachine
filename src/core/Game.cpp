#include "Game.h"
#include <iostream>

Game::Game()
    : is_running(false), window(nullptr), renderer(nullptr),
      state(GameState::MENU), selected_piece_id(-1), window_width(0),
      window_height(0) {}

Game::~Game() { clean(); }

bool Game::init(const char *title, int width, int height) {
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
    std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError()
              << std::endl;
    return false;
  }

  window =
      SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                       width, height, SDL_WINDOW_SHOWN);
  if (!window) {
    std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    return false;
  }

  // To support drag and drop
  SDL_EventState(SDL_DROPFILE, SDL_ENABLE);

  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError()
              << std::endl;
    return false;
  }

  window_width = width;
  window_height = height;
  is_running = true;

  std::cout << "Drag and drop image to play" << std::endl;

  return true;
}

void Game::load_image(const std::string &filepath) {
  segmentation.cleanup();
  std::cout << "Loading image: " << filepath << std::endl;
  if (segmentation.init(filepath.c_str())) {
    board = std::make_unique<PuzzleBoard>(
        segmentation.get_pieces(), segmentation.get_uf(),
        segmentation.get_width(), segmentation.get_height());
    board->scatter_pieces(window_width, window_height);
    state = GameState::PLAYING;
    std::cout << "Succeeded to load image: " << filepath << std::endl;
  } else {
    std::cerr << "Failed to load image: " << filepath << std::endl;
  }
}

void Game::handle_events() {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    if (event.type == SDL_QUIT) {
      is_running = false;
    } else if (event.type == SDL_DROPFILE) {
      char *dropped_filedir = event.drop.file;
      load_image(dropped_filedir);
      SDL_free(dropped_filedir);
    } else if (state == GameState::PLAYING) {
      if (event.type == SDL_MOUSEBUTTONDOWN &&
          event.button.button == SDL_BUTTON_LEFT) {
        int mouse_x = event.button.x;
        int mouse_y = event.button.y;
        // Find clicked piece
        auto &pieces = segmentation.get_pieces();
        auto &uf = segmentation.get_uf();

        selected_piece_id = -1;
        // Iterate in reverse for naive top-most selection if they overlap
        for (int i = pieces.size() - 1; i >= 0; --i) {
          if (uf.find(i) == i) { // only check roots
            const auto &p = pieces[i];
            bool hit = false;
            for (const auto &pix : p.pixels) {
              if (pix.first + p.offset_x == mouse_x &&
                  pix.second + p.offset_y == mouse_y) {
                hit = true;
                break;
              }
            }
            if (hit) {
              selected_piece_id = i;
              last_mouse_x = mouse_x;
              last_mouse_y = mouse_y;
              break;
            }
          }
        }
      } else if (event.type == SDL_MOUSEMOTION) {
        if (selected_piece_id != -1) {
          int dx = event.motion.x - last_mouse_x;
          int dy = event.motion.y - last_mouse_y;
          board->move_piece(selected_piece_id, dx, dy);
          last_mouse_x = event.motion.x;
          last_mouse_y = event.motion.y;
        }
      } else if (event.type == SDL_MOUSEBUTTONUP &&
                 event.button.button == SDL_BUTTON_LEFT) {
        if (selected_piece_id != -1) {
          board->snap_piece(selected_piece_id);
          selected_piece_id = -1;
          if (board->is_cleared()) {
            state = GameState::CLEARED;
          }
        }
      }
    } else if (state == GameState::CLEARED) {
      if (event.type == SDL_MOUSEBUTTONDOWN) {
        // reset to menu on click
        state = GameState::MENU;
        segmentation.cleanup();
        board.reset();
      }
    }
  }
}

void Game::update() {
  // Game logic update
}

void Game::render() {
  // Clear screen to retro black
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  if (state == GameState::MENU) {
    // Draw drop zone (simple dashed or solid rectangle)
    SDL_SetRenderDrawColor(renderer, 100, 100, 100, 255);
    SDL_Rect drop_rect = {window_width / 4, window_height / 4, window_width / 2,
                          window_height / 2};
    SDL_RenderDrawRect(renderer, &drop_rect);
    // Draw inner box
    SDL_Rect drop_rect2 = {window_width / 4 + 4, window_height / 4 + 4,
                           window_width / 2 - 8, window_height / 2 - 8};
    SDL_RenderDrawRect(renderer, &drop_rect2);
  } else if (state == GameState::PLAYING || state == GameState::CLEARED) {
    auto &pieces = segmentation.get_pieces();
    auto &uf = segmentation.get_uf();

    int dragged_root =
        (selected_piece_id != -1) ? uf.find(selected_piece_id) : -1;

    // 選択されていないピースを先に描画
    for (int i = 0; i < pieces.size(); ++i) {
      if (uf.find(i) == i && i != dragged_root) {
        const auto &p = pieces[i];

        for (auto &pix : p.pixels) {
          int idx = pix.first + pix.second * segmentation.w;
          unsigned char r = segmentation.data[idx * 3 + 0];
          unsigned char g = segmentation.data[idx * 3 + 1];
          unsigned char b = segmentation.data[idx * 3 + 2];
          SDL_SetRenderDrawColor(renderer, r, g, b, 255);
          SDL_RenderDrawPoint(renderer, pix.first + p.offset_x,
                              pix.second + p.offset_y);
        }
      }
    }

    // 選択されているピースを最前面（最後）に描画
    if (dragged_root != -1) {
      const auto &p = pieces[dragged_root];
      auto [r, g, b] = p.color;
      SDL_SetRenderDrawColor(renderer, r, g, b, 255);
      for (const auto &pix : p.pixels) {
        SDL_RenderDrawPoint(renderer, pix.first + p.offset_x,
                            pix.second + p.offset_y);
      }
    }
  }

  SDL_RenderPresent(renderer);
}

void Game::run() {
  while (is_running) {
    handle_events();
    update();
    render();
    SDL_Delay(16); // ~60fps
  }
}

void Game::clean() {
  segmentation.cleanup();
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
