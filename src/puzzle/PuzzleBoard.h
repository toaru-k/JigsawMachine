#ifndef PUZZLE_BOARD_H
#define PUZZLE_BOARD_H

#include "PuzzleCore.h"
#include <vector>

#define UNITE_DISTANCE 200

class PuzzleBoard {
public:
  PuzzleBoard(std::vector<Piece>& pieces, UnionFind& uf, int width, int height);

  // Scatter pieces randomly around the screen
  void scatter_pieces(int screen_width, int screen_height);

  // Move a specific piece (and its connected components)
  void move_piece(int idx, int dx, int dy);

  // Check if a piece can snap to its neighbors. Returns true if snapped.
  bool snap_piece(int idx, const std::vector<bool>& in_inventory);

  // Returns true if all pieces have been connected into a single group
  bool is_cleared() const;

private:
  std::vector<Piece>& pieces;
  UnionFind& uf;
  int board_w, board_h;

  std::vector<int> near_piece(int idx, const std::vector<bool>& in_inventory);
};

#endif // PUZZLE_BOARD_H
