#ifndef SEGMENTATION_H
#define SEGMENTATION_H

#include <vector>
#include <tuple>
#include <utility>

struct Piece {
  int id;
  int num_pixels;
  std::vector<std::pair<int, int>> pixels;
  std::vector<int> neighbors_id;
  std::tuple<int, int, int> transition;
  std::tuple<int, int, int> color;
};

class UnionFind {
public:
  UnionFind();
  void init(int size);
  int find(int x);
  bool same(int x, int y);
  void unite(int x, int y);

private:
  std::vector<int> parents;
};

class Puzzle {
public:
  bool init(const char *filepath);
  std::tuple<int, int, int> get_pixel_color(int idx);
  std::vector<int> near_piece(int idx);
  void cleanup();

private:
  int w, h, n;
  unsigned char *data;
  std::vector<Piece> pieces;
  UnionFind uf;

  void unite_pieces(int idx_a, int idx_b);
  void process();
};

#endif // SEGMENTATION_H
