#pragma once

template <typename T>
struct IteratorRange {
  using iterator = T;

  /* implicit */ IteratorRange(T begin, T end) : m_begin{begin}, m_end{end} {}

  T begin() { return m_begin; }
  T end() { return m_end; }

private:
  T m_begin, m_end;
};
