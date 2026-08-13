#include "PuzzleCore.h"
#include <algorithm>

UnionFind::UnionFind() {}

void UnionFind::init(int size) {
  parents.resize(size);
  for (int i = 0; i < size; i++)
    parents[i] = i;
}

int UnionFind::find(int x) {
  if (parents[x] == x)
    return x;
  return parents[x] = find(parents[x]);
}

bool UnionFind::same(int x, int y) { return find(x) == find(y); }

void UnionFind::unite(int x, int y) {
  int root_x = find(x);
  int root_y = find(y);
  if (root_x != root_y) {
    parents[root_y] = root_x;
  }
}

void unite_pieces(std::vector<Piece>& pieces, UnionFind& uf, int idx_a, int idx_b) {
  int root_a = uf.find(idx_a);
  int root_b = uf.find(idx_b);

  if (root_a == root_b)
    return;

  uf.unite(root_a, root_b);
  int new_root = uf.find(root_a);
  int child = (new_root == root_a) ? root_b : root_a;

  Piece &parent_piece = pieces[new_root];
  Piece &child_piece = pieces[child];

  parent_piece.num_pixels += child_piece.num_pixels;
  parent_piece.pixels.insert(parent_piece.pixels.end(),
                             child_piece.pixels.begin(),
                             child_piece.pixels.end());

  child_piece.pixels.clear();
  child_piece.pixels.shrink_to_fit();

  std::vector<int> &nid = parent_piece.neighbors_id;
  nid.insert(nid.end(), child_piece.neighbors_id.begin(),
             child_piece.neighbors_id.end());

  child_piece.neighbors_id.clear();
  child_piece.neighbors_id.shrink_to_fit();

  for (int &nid_i : nid) {
    nid_i = uf.find(nid_i);
  }

  std::sort(nid.begin(), nid.end());
  nid.erase(std::unique(nid.begin(), nid.end()), nid.end());

  auto it = std::find(nid.begin(), nid.end(), new_root);
  if (it != nid.end()) {
    nid.erase(it);
  }
}
