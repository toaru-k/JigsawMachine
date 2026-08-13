#include "PuzzleBoard.h"
#include <cstdlib>

PuzzleBoard::PuzzleBoard(std::vector<Piece>& pieces, UnionFind& uf, int width, int height)
  : pieces(pieces), uf(uf), board_w(width), board_h(height) {}

void PuzzleBoard::scatter_pieces(int screen_width, int screen_height) {
  for (int i = 0; i < pieces.size(); ++i) {
    if (uf.find(i) == i && pieces[i].num_pixels > 0) {
      int rand_x = (std::rand() % screen_width) - (board_w / 2);
      int rand_y = (std::rand() % screen_height) - (board_h / 2);
      pieces[i].offset_x = rand_x;
      pieces[i].offset_y = rand_y;
    }
  }
}

void PuzzleBoard::move_piece(int idx, int dx, int dy) {
  int root = uf.find(idx);
  pieces[root].offset_x += dx;
  pieces[root].offset_y += dy;
}

std::vector<int> PuzzleBoard::near_piece(int idx) {
  int root = uf.find(idx);
  Piece &p = pieces[root];
  
  std::vector<int> nids;
  for (int nid : p.neighbors_id) {
    int nroot = uf.find(nid);
    if (nroot == root) continue;
    
    Piece &np = pieces[nroot];
    long distance = 
        (p.offset_x - np.offset_x) * (p.offset_x - np.offset_x) +
        (p.offset_y - np.offset_y) * (p.offset_y - np.offset_y);

    if (distance < UNITE_DISTANCE) {
      nids.push_back(nroot);
    }
  }

  return nids;
}

bool PuzzleBoard::snap_piece(int idx) {
  int root = uf.find(idx);
  std::vector<int> nearby = near_piece(root);
  
  bool snapped = false;
  for (int nid : nearby) {
    int nroot = uf.find(nid);
    if (nroot == root) continue;

    // Align the piece to the snapped neighbor's offset
    pieces[root].offset_x = pieces[nroot].offset_x;
    pieces[root].offset_y = pieces[nroot].offset_y;

    unite_pieces(pieces, uf, root, nroot);
    
    // Update root because unite_pieces changed the structure
    root = uf.find(root); 
    snapped = true;
  }
  
  return snapped;
}

bool PuzzleBoard::is_cleared() const {
  int num_roots = 0;
  for (int i = 0; i < pieces.size(); ++i) {
    if (pieces[i].num_pixels > 0 && uf.find(i) == i) {
      num_roots++;
      if (num_roots > 1) return false;
    }
  }
  return num_roots == 1;
}
