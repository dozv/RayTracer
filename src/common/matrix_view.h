#pragma once

#include <algorithm>
#include <cassert>
#include <ranges>
#include <span>

template <typename T>
class MatrixView {
 public:
  inline MatrixView(std::span<T> elements, size_t rows, size_t columns)
      : elements_(elements), rows_(rows), columns_(columns) {
    assert(rows * columns == elements.size());
  }

  inline const T& At(size_t row, size_t col) const {
    assert(row < rows_ && col < columns_);
    return elements_[Offset(row, col)];
  }

  inline T& At(size_t row, size_t column) {
    assert(row < rows_ && column < columns_);
    return elements_[Offset(row, column)];
  }

  inline MatrixView Submatrix(size_t start_row, size_t start_column,
                              size_t num_rows, size_t num_columns) const {
    assert(start_row + num_rows <= rows_ &&
           start_column + num_columns <= columns_);
    auto sub_elements = elements_.subspan(
        start_row * num_columns + start_column, num_columns * num_rows);
    return MatrixView(sub_elements, num_rows, num_columns);
  }

  inline size_t Rows() const { return rows_; }

  inline size_t Columns() const { return columns_; }

  inline void Clear(T value = {}) { std::ranges::fill(elements_, value); }

  inline auto Elements() { return elements_; }

 private:
  std::span<T> elements_;
  size_t rows_;
  size_t columns_;

  inline size_t Offset(size_t row, size_t column) const {
    return columns_ * row + column;
  }
};
