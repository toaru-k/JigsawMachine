#ifndef PUZZLE_CORE_H
#define PUZZLE_CORE_H

#include <tuple>
#include <utility>
#include <vector>

struct Piece {
  int id;
  int num_pixels;
  std::vector<std::pair<int, int>> pixels;
  std::vector<int> neighbors_id;
  int offset_x = 0;
  int offset_y = 0;
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

void pre_unite_pieces(UnionFind &uf, int idx_a, int idx_b);
void post_unite_pieces(std::vector<Piece> &pieces, UnionFind &uf);

#endif // PUZZLE_CORE_H
