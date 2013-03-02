#pragma once

template <typename T> class Maybe;

template <typename T>
static Maybe<T> Just(const T &x) { return Maybe<T>(x); }

template <typename T>
static Maybe<T> Nothing() { return Maybe<T>(); }

template <typename T>
class Maybe {
public:
  T x;

  Maybe() : hasValue(false) {}
  explicit Maybe(const T &xa) : x(xa), hasValue(true) {}
  bool isJust() const { return hasValue; }
  bool isNothing() const { return !hasValue; }

  T orDefault(T y) const {
    return hasValue ? x : y;
  }

  template <typename F, typename G>
  auto cond(F f, G g) const -> decltype(f(x)) {
    return hasValue ? f(x) : g();
  }

  template <typename F>
  auto bind(F f) const -> decltype(f(x)) {
    return hasValue ? f(x) : Nothing<decltype(f(x).x)>();
  }

  template <typename F>
  auto map(F f) const -> Maybe<decltype(f(x))> {
    return hasValue ? Just(f(x)) : Nothing<decltype(f(x))>();
  }

private:
  bool hasValue;
};
