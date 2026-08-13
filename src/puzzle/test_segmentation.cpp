#define STB_IMAGE_IMPLEMENTATION
#define private public // privateメンバーへアクセスするためのハック
#include "Segmentation.cpp"
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

  Puzzle p;
  std::cout << "Running Puzzle::init()..." << std::endl;
  if (!p.init(test_file)) {
    std::cerr << "Failed to init Puzzle\n";
    return 1;
  }

  // 期待される結果:
  // Segmentation.cpp では MIN_PIECE_SIZE が 100 に設定されています。
  // 今回作成したテスト画像は 4x4 = 16 ピクセルしかありません。
  // そのため、赤（8px）と青（8px）に一旦分かれた後、さらに両者が結合されて
  // 最終的に 16 ピクセルの1つのピースになるはずです。

  int num_roots = 0;
  int root_pixels = 0;
  for (int i = 0; i < p.pieces.size(); i++) {
    if (p.uf.find(i) == i) {
      num_roots++;
      root_pixels = p.pieces[i].num_pixels;
    }
  }

  std::cout << "Number of final pieces: " << num_roots << std::endl;
  std::cout << "Pixels in the root piece: " << root_pixels << std::endl;

  assert(num_roots == 1 && "Expected all pixels to merge into 1 piece due to "
                           "MIN_PIECE_SIZE (100 > 16)");
  assert(root_pixels == 16 && "Expected 16 pixels in the root piece");

  std::cout << "All assertions passed! Test successful.\n";

  p.cleanup();

  // clean up 後のメモリ状態確認（piecesが空になっているか）
  assert(p.pieces.capacity() == 0 &&
         "Expected pieces vector to be fully cleared and shrunk");

  std::remove(test_file);
  return 0;
}
