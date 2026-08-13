#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include "PuzzleCore.h"
#include <vector>

class Segmentation {
public:
  bool init(const char *filepath);
  void cleanup();

  std::vector<Piece>& get_pieces() { return pieces; }
  UnionFind& get_uf() { return uf; }
  int get_width() const { return w; }
  int get_height() const { return h; }

private:
  int w, h, n;
  unsigned char *data;
  std::vector<Piece> pieces;
  UnionFind uf;

  std::tuple<int, int, int> get_pixel_color(int idx);
  void process();
};

void unite_pieces(std::vector<Piece>& pieces, UnionFind& uf, int idx_a, int idx_b);

#endif // SEGMENTATION_H
