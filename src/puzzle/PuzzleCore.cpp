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

void pre_unite_pieces(UnionFind &uf, int idx_a, int idx_b) {
  int root_a = uf.find(idx_a);
  int root_b = uf.find(idx_b);

  if (root_a != root_b)
    uf.unite(root_a, root_b);
}

void post_unite_pieces(std::vector<Piece> &pieces, UnionFind &uf) {
  int n = pieces.size();

  // ルートごとの子のリストを単方向リストで構築 (メモリ確保を最小限にするため)
  std::vector<int> head(n, -1);
  std::vector<int> next(n, -1);
  for (int i = 0; i < n; i++) {
    int root = uf.find(i);
    if (root != i) {
      next[i] = head[root];
      head[root] = i;
    }
  }

  for (int root = 0; root < n; root++) {
    if (uf.find(root) != root)
      continue; // ルートでない場合はスキップ

    Piece &parent_piece = pieces[root];

    if (head[root] != -1) { // 子がいる場合のみ統合処理を行う
      // 子の総ピクセル数と隣接ノード数を計算して一括確保
      int extra_pixels = 0;
      int extra_neighbors = 0;
      for (int child = head[root]; child != -1; child = next[child]) {
        extra_pixels += pieces[child].num_pixels;
        extra_neighbors += pieces[child].neighbors_id.size();
      }

      parent_piece.pixels.reserve(parent_piece.pixels.size() + extra_pixels);
      parent_piece.neighbors_id.reserve(parent_piece.neighbors_id.size() +
                                        extra_neighbors);

      for (int child = head[root]; child != -1; child = next[child]) {
        Piece &child_piece = pieces[child];

        parent_piece.num_pixels += child_piece.num_pixels;
        parent_piece.pixels.insert(parent_piece.pixels.end(),
                                   child_piece.pixels.begin(),
                                   child_piece.pixels.end());

        parent_piece.neighbors_id.insert(parent_piece.neighbors_id.end(),
                                         child_piece.neighbors_id.begin(),
                                         child_piece.neighbors_id.end());

        // 子のメモリを解放
        child_piece.pixels.clear();
        child_piece.pixels.shrink_to_fit();
        child_piece.neighbors_id.clear();
        child_piece.neighbors_id.shrink_to_fit();
        child_piece.num_pixels = 0;
      }
    }

    // すべてのルートで neighbors_id の整理を実行する
    std::vector<int> &nid = parent_piece.neighbors_id;
    for (int &nid_i : nid) {
      nid_i = uf.find(nid_i);
    }
    std::sort(nid.begin(), nid.end());
    nid.erase(std::unique(nid.begin(), nid.end()), nid.end());
    auto it = std::find(nid.begin(), nid.end(), root);
    if (it != nid.end()) {
      nid.erase(it);
    }
  }
}