#include "mesh.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

scene::Mesh scene::LoadCube() {
  std::vector<DirectX::XMFLOAT3A> vertices = {
      {1.000000f, -1.000000f, -1.000000f}, {1.000000f, -1.000000f, 1.000000f},
      {-1.000000f, -1.000000f, 1.000000f}, {-1.000000f, -1.000000f, -1.000000f},
      {1.000000f, 1.000000f, -0.999999f},  {0.999999f, 1.000000f, 1.000001f},
      {-1.000000f, 1.000000f, 1.000000f},  {-1.000000f, 1.000000f, -1.000000f}};

  std::vector<DirectX::XMINT3> indices = {
      {2 - 1, 3 - 1, 4 - 1}, {8 - 1, 7 - 1, 6 - 1}, {5 - 1, 6 - 1, 2 - 1},
      {6 - 1, 7 - 1, 3 - 1}, {3 - 1, 7 - 1, 8 - 1}, {1 - 1, 4 - 1, 8 - 1},
      {1 - 1, 2 - 1, 4 - 1}, {5 - 1, 8 - 1, 6 - 1}, {1 - 1, 5 - 1, 2 - 1},
      {2 - 1, 6 - 1, 3 - 1}, {4 - 1, 3 - 1, 8 - 1}, {5 - 1, 1 - 1, 8 - 1}};

  return {vertices, indices};
}

scene::Mesh scene::LoadOctahedron() {
  std::vector<DirectX::XMFLOAT3A> vertices = {
      {1.000000f, 0.000000f, 0.000000f},  {-1.000000f, 0.000000f, 0.000000f},
      {0.000000f, 0.000000f, -1.000000f}, {0.000000f, 0.000000f, 1.000000f},
      {0.000000f, 1.000000f, 0.000000f},  {0.000000f, -1.000000f, 0.000000f}};

  std::vector<DirectX::XMINT3> indices = {
      {5 - 1, 1 - 1, 3 - 1}, {5 - 1, 3 - 1, 2 - 1}, {5 - 1, 2 - 1, 4 - 1},
      {5 - 1, 4 - 1, 1 - 1}, {6 - 1, 3 - 1, 1 - 1}, {6 - 1, 2 - 1, 3 - 1},
      {6 - 1, 4 - 1, 2 - 1}, {6 - 1, 1 - 1, 4 - 1}};

  return {vertices, indices};
}

scene::Mesh scene::LoadRectangle() {
  std::vector<DirectX::XMFLOAT3A> vertices = {{-0.5f, -0.5f, 0.0f},
                                              {0.5f, -0.5f, 0.0f},
                                              {0.5f, 0.5f, 0.0f},
                                              {-0.5f, 0.5f, 0.0f}};

  std::vector<DirectX::XMINT3> indices = {{0, 1, 2}, {0, 2, 3}};

  return {vertices, indices};
}
