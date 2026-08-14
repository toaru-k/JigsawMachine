#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include "PuzzleCore.h"
#include <vector>

class Segmentation {
public:
  int w = 0, h = 0, n = 0;
  unsigned char *data = nullptr;

  bool init(const char *filepath);
  void cleanup();

  std::vector<Piece> &get_pieces() { return pieces; }
  UnionFind &get_uf() { return uf; }
  int get_width() const { return w; }
  int get_height() const { return h; }

private:
  std::vector<Piece> pieces;
  UnionFind uf;

  std::tuple<int, int, int> get_pixel_color(int idx);
  void process();
};

#endif // SEGMENTATION_H
