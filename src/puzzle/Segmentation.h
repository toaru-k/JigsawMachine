#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include "PuzzleCore.h"
#include <vector>

class Segmentation {
public:
  int w = 0, h = 0, n = 0;
  unsigned char *data = nullptr;

  int orig_w, orig_h;
  unsigned char *original_data;

  bool init(const char *filepath, int max_dimension = 400, bool skip_process = false);
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
