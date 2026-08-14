#define SDL_MAIN_HANDLED
#include "core/Game.h"
#include <iostream>

const int WINDOW_WIDTH = 800;
const int WINDOW_HEIGHT = 600;

int main(int argc, char *argv[]) {
  Game game;

  if (!game.init("JigsawMachine.exe", WINDOW_WIDTH, WINDOW_HEIGHT)) {
    std::cerr << "Failed to initialize game!" << std::endl;
    return 1;
  }

  if (argc > 1) {
    game.load_image(argv[1]);
  }

  game.run();

  return 0;
}
