#include "Segmentation.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define MIN_PIECE_SIZE 200
#define DATA_MASK 0xf0

bool Segmentation::init(const char *filepath, int max_dimension, bool skip_process) {
  original_data = stbi_load(filepath, &orig_w, &orig_h, &n, 3);
  if (!original_data) {

    return false;
  }

  int max_dim = std::max(orig_w, orig_h);
  float scale = std::min(1.0f, (float)max_dimension / max_dim);

  w = orig_w;
  h = orig_h;

  if (scale < 1.0f) {
    w = (int)(orig_w * scale);
    h = (int)(orig_h * scale);
    data = (unsigned char *)malloc(w * h * 3);
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int src_x = (int)(x / scale);
        int src_y = (int)(y / scale);
        if (src_x >= orig_w)
          src_x = orig_w - 1;
        if (src_y >= orig_h)
          src_y = orig_h - 1;

        int src_idx = (src_y * orig_w + src_x) * 3;
        int dst_idx = (y * w + x) * 3;
        data[dst_idx + 0] = original_data[src_idx + 0];
        data[dst_idx + 1] = original_data[src_idx + 1];
        data[dst_idx + 2] = original_data[src_idx + 2];
      }
    }
  } else {
    data = (unsigned char *)malloc(w * h * 3);
    memcpy(data, original_data, w * h * 3);
  }


  uf.init(w * h);
  pieces.resize(w * h);

  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int idx = x + y * w;

      pieces[idx].id = idx;
      pieces[idx].num_pixels = 1;
      pieces[idx].pixels.push_back(idx);
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


  if (!skip_process) {
    process();
  }
  return true;
}

std::tuple<int, int, int> Segmentation::get_pixel_color(int idx) {
  int r = data[idx * 3 + 0] & DATA_MASK;
  int g = data[idx * 3 + 1] & DATA_MASK;
  int b = data[idx * 3 + 2] & DATA_MASK;
  return std::make_tuple(r, g, b);
}

void Segmentation::cleanup() {
  if (original_data) {
    stbi_image_free(original_data);
    original_data = nullptr;
  }
  if (data) {
    free(data);
    data = nullptr;
  }
  pieces.clear();
  pieces.shrink_to_fit();
}

void Segmentation::process() {
  /* 同色との結合 (Union-Findツリー上の結合のみ) */
  for (int x = 0; x < w; x++) {
    for (int y = 0; y < h; y++) {
      int idx = x + y * w;
      auto color = pieces[idx].color;
      if (x + 1 < w) {
        if (color == pieces[idx + 1].color) {
          pre_unite_pieces(uf, idx, idx + 1);
        }
      }
      if (y + 1 < h) {
        if (color == pieces[idx + w].color) {
          pre_unite_pieces(uf, idx, idx + w);
        }
      }
    }
  }

  // ルートにデータを集約し、隣接情報を整理する
  post_unite_pieces(pieces, uf);



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
        pre_unite_pieces(uf, i, best_neighbor_id);
        changed = true;
      } else {
        small_piece_idx[k] = small_piece_idx.back();
        small_piece_idx.pop_back();
        continue;
      }
      k++;
    }

    post_unite_pieces(pieces, uf);
  }

  int piece_cout = 0;
  for (int i = 0; i < pieces.size(); i++) {
    if (uf.find(i) == i) {
      piece_cout++;
    }
  }


}
