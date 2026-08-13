#include "Segmentation.h"
#include <algorithm>
#include <iostream>
#include <stb_image.h>

#define MIN_PIECE_SIZE 100
#define DATA_MASK 0xf8
#define UNITE_DISTANCE 200

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

bool Puzzle::init(const char *filepath) {
  data = stbi_load(filepath, &w, &h, &n, 3);
  if (!data) {
    std::cerr << "Failed to load image" << std::endl;
    return false;
  }

  uf.init(w * h);
  pieces.resize(w * h);

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = x + y * w;

      pieces[idx].id = idx;
      pieces[idx].num_pixels = 1;
      pieces[idx].pixels.push_back(std::make_pair(x, y));
      pieces[idx].color = get_pixel_color(idx);

      if (x > 0)
        pieces[idx].neighbors_id.push_back(idx - 1);
      if (x + 1 < w)
        pieces[idx].neighbors_id.push_back(idx + 1);
      if (y > 0)
        pieces[idx].neighbors_id.push_back(idx - w);
      if (y + 1 < h)
        pieces[idx].neighbors_id.push_back(idx + w);
    }
  }

  process();
  return true;
}

std::tuple<int, int, int> Puzzle::get_pixel_color(int idx) {
  int r = data[idx * 3 + 0] & DATA_MASK;
  int g = data[idx * 3 + 1] & DATA_MASK;
  int b = data[idx * 3 + 2] & DATA_MASK;
  return std::make_tuple(r, g, b);
}

std::vector<int> Puzzle::near_piece(int idx) {
  Piece &p = pieces[idx];
  auto [px, py, pd] = p.transition;

  std::vector<int> nids;
  for (int nid : p.neighbors_id) {
    int nroot = uf.find(nid);
    auto [nx, ny, nd] = pieces[nroot].transition;

    long distance =
        (px - nx) * (px - nx) + (py - ny) * (py - ny) + (pd - nd) * (pd - nd);

    if (distance < UNITE_DISTANCE) {
      nids.push_back(nid);
    }
  }

  return nids;
}

void Puzzle::cleanup() {
  stbi_image_free(data);
  pieces.clear();
  pieces.shrink_to_fit();
}

void Puzzle::unite_pieces(int idx_a, int idx_b) {
  int root_a = uf.find(idx_a);
  int root_b = uf.find(idx_b);

  if (root_a == root_b)
    return;

  uf.unite(root_a, root_b);

  Piece &parent_piece = pieces[root_a];
  Piece &child_piece = pieces[root_b];

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

  auto it = std::find(nid.begin(), nid.end(), root_a);
  if (it != nid.end()) {
    nid.erase(it);
  }
}

void Puzzle::process() {
  /* 同色との結合 */
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      int idx = x + y * w;
      auto color = pieces[idx].color;
      if (x + 1 < w) {
        if (color == pieces[idx + 1].color) {
          unite_pieces(idx, idx + 1);
        }
      }
      if (y + 1 < h) {
        if (color == pieces[idx + w].color) {
          unite_pieces(idx, idx + w);
        }
      }
    }
  }

  /* 面積が最小のピースを探し、そのピースの最も色が近い隣接ピースと結合する */
  std::vector<int> small_piece_idx;
  for (int i = 0; i < pieces.size(); i++) {
    if (uf.find(i) == i && pieces[i].num_pixels < MIN_PIECE_SIZE) {
      small_piece_idx.push_back(i);
    }
  }

  bool changed = true;
  while (changed && !small_piece_idx.empty()) {
    changed = false;
    for (int k = 0; k < small_piece_idx.size();) {
      int i = small_piece_idx[k];
      if (uf.find(i) != i || pieces[i].num_pixels >= MIN_PIECE_SIZE) {
        small_piece_idx[k] = small_piece_idx.back();
        small_piece_idx.pop_back();
        continue;
      }

      int r, g, b;
      std::tie(r, g, b) = pieces[i].color;

      int best_neighbor_id = -1;
      long min_diff = 1000000; // max possible difference is 255*255*3 = 195075

      for (int nid : pieces[i].neighbors_id) {
        int nroot = uf.find(nid);
        if (nroot == i)
          continue;

        int nr, ng, nb;
        std::tie(nr, ng, nb) = pieces[nroot].color;

        long diff =
            (r - nr) * (r - nr) + (g - ng) * (g - ng) + (b - nb) * (b - nb);

        if (diff < min_diff) {
          min_diff = diff;
          best_neighbor_id = nroot;
        }
      }

      if (best_neighbor_id != -1) {
        unite_pieces(i, best_neighbor_id);
        changed = true;
      } else {
        small_piece_idx[k] = small_piece_idx.back();
        small_piece_idx.pop_back();
        continue;
      }
      k++;
    }
  }
}
