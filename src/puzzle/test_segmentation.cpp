#define STB_IMAGE_IMPLEMENTATION
#define private public // privateメンバーへアクセスするためのハック
#include "PuzzleCore.cpp"
#include "Segmentation.cpp"
#include "PuzzleBoard.cpp"
#include <cassert>
#include <fstream>
#include <iostream>

void create_test_image(const char *filepath) {
  // 4x4の簡単なPPM画像（バイナリベース P6）を作成
  // 左半分が赤、右半分が青
  std::ofstream out(filepath, std::ios::binary);
  out << "P6\n4 4\n255\n";
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      if (x < 2) {
        out.put(255);
        out.put(0);
        out.put(0);
      } else {
        out.put(0);
        out.put(0);
        out.put(255);
      }
    }
  }
  out.close();
}

int main() {
  const char *test_file = "test_image.ppm";
  create_test_image(test_file);

  Segmentation seg;
  std::cout << "Running Segmentation::init()..." << std::endl;
  if (!seg.init(test_file)) {
    std::cerr << "Failed to init Segmentation\n";
    return 1;
  }

  // MIN_PIECE_SIZE = 100 なので、16ピクセルの画像は最終的に1ピースになるはず
  int num_roots = 0;
  int root_pixels = 0;
  for (int i = 0; i < seg.pieces.size(); i++) {
    if (seg.uf.find(i) == i && seg.pieces[i].num_pixels > 0) {
      num_roots++;
      root_pixels = seg.pieces[i].num_pixels;
    }
  }

  std::cout << "Number of initial segmented pieces: " << num_roots << std::endl;
  assert(num_roots == 1 && "Expected 1 piece due to MIN_PIECE_SIZE (100 > 16)");
  
  // テスト用に、わざと UnionFind を初期化し直して各ピクセルをバラバラのピースにする（MIN_PIECE_SIZE無視状態をシミュレート）
  std::cout << "Simulating game logic with 2 pieces..." << std::endl;
  Segmentation seg2;
  seg2.w = 4; seg2.h = 4;
  seg2.uf.init(16);
  seg2.pieces.resize(16);
  
  for (int y = 0; y < 4; y++) {
    for (int x = 0; x < 4; x++) {
      int idx = x + y * 4;
      seg2.pieces[idx].id = idx;
      seg2.pieces[idx].num_pixels = 1;
      seg2.pieces[idx].pixels.push_back(std::make_pair(x, y));
      seg2.pieces[idx].offset_x = 0;
      seg2.pieces[idx].offset_y = 0;
      if (x > 0) seg2.pieces[idx].neighbors_id.push_back(idx - 1);
      if (x + 1 < 4) seg2.pieces[idx].neighbors_id.push_back(idx + 1);
      if (y > 0) seg2.pieces[idx].neighbors_id.push_back(idx - 4);
      if (y + 1 < 4) seg2.pieces[idx].neighbors_id.push_back(idx + 4);
    }
  }

  // 0~7 (左半分) を結合, 8~15 (右半分) を結合
  for(int i=0; i<7; i++) unite_pieces(seg2.pieces, seg2.uf, i, i+1);
  for(int i=8; i<15; i++) unite_pieces(seg2.pieces, seg2.uf, i, i+1);
  
  PuzzleBoard board(seg2.pieces, seg2.uf, 4, 4);
  
  int root_left = seg2.uf.find(0);
  int root_right = seg2.uf.find(8);
  
  // バラバラに配置
  board.move_piece(root_left, -50, 0);
  board.move_piece(root_right, 50, 0);
  
  assert(!board.is_cleared());
  
  // 右のピースを左のピースに近づける
  board.move_piece(root_right, -90, 0); // distance is now 10, offset_y is 0. 10^2 = 100 < 200 (UNITE_DISTANCE)
  
  bool snapped = board.snap_piece(root_right);
  assert(snapped);
  assert(board.is_cleared());
  std::cout << "Game logic test passed (pieces snapped & cleared)!\n";

  seg.cleanup();
  seg2.cleanup();

  std::remove(test_file);
  return 0;
}
