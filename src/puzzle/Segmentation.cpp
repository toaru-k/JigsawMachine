#include "Segmentation.h"
#include <iostream>
#include <stb_image.h>

#define MIN_PIECE_SIZE 100
#define DATA_MASK 0xf8

bool Segmentation::init(const char *filepath) {
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
      // Initialize offset explicitly to 0 (though Piece struct does it too)
      pieces[idx].offset_x = 0;
      pieces[idx].offset_y = 0;

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

std::tuple<int, int, int> Segmentation::get_pixel_color(int idx) {
  int r = data[idx * 3 + 0] & DATA_MASK;
  int g = data[idx * 3 + 1] & DATA_MASK;
  int b = data[idx * 3 + 2] & DATA_MASK;
  return std::make_tuple(r, g, b);
}

void Segmentation::cleanup() {
  stbi_image_free(data);
  pieces.clear();
  pieces.shrink_to_fit();
}

void Segmentation::process() {
  /* 同色との結合 */
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      int idx = x + y * w;
      auto color = pieces[idx].color;
      if (x + 1 < w) {
        if (color == pieces[idx + 1].color) {
          unite_pieces(pieces, uf, idx, idx + 1);
        }
      }
      if (y + 1 < h) {
        if (color == pieces[idx + w].color) {
          unite_pieces(pieces, uf, idx, idx + w);
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
      long min_diff = 1000000;

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
        unite_pieces(pieces, uf, i, best_neighbor_id);
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
