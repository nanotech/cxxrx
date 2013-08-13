#pragma once
#include <utility>
#include <type_traits>

template <typename T> class Maybe;

template <typename T, typename std::enable_if<
  !std::is_reference<T>::value>::type* = nullptr> // rvalues only
static Maybe<T> Just(T &&x) { return Maybe<T>(std::forward<T>(x)); }

template <typename T>
static Maybe<T> Just(const T &x) { return Maybe<T>(x); }

template <typename T>
static Maybe<T> Nothing() { return Maybe<T>(); }

#define JUST_ELSE_NOTHING(P, X) \
  ((P) ? (X) : Nothing<decltype((X).x)>())

template <typename T>
class Maybe {
public:
  T x;

  Maybe() : hasValue(false) {}
  explicit Maybe(T &&xa) : x(std::move(xa)), hasValue(true) {}
  explicit Maybe(const T &xa) : x(xa), hasValue(true) {}
  bool isJust() const { return hasValue; }
  bool isNothing() const { return !hasValue; }

  const T orDefault(T y) const { return hasValue ? x : y; }
  T& orDefault(T&& y) { return hasValue ? x : y; }
  T* orNull() { return hasValue ? &x : nullptr; }

  template <typename ...Args>
  T* orConstructDefault(Args... args) {
    return hasValue ? &x : new T(args...);
  }

  template <typename F, typename G>
  auto cond(F f, G g) const -> decltype(f(x)) {
    return hasValue ? f(x) : g();
  }

  template <typename F>
  auto bind(F f) const -> decltype(f(x)) {
    return JUST_ELSE_NOTHING(hasValue, f(x));
  }

  template <typename F>
  auto map(F f) const -> Maybe<decltype(f(x))> {
    return JUST_ELSE_NOTHING(hasValue, Just(f(x)));
  }

private:
  bool hasValue;
};

template <typename M, typename K>
static auto findMaybe(const M &m, const K &k)
  -> decltype(Just(m.find(k)->second))
{
  auto it = m.find(k);
  return JUST_ELSE_NOTHING(it != m.end(), Just(it->second));
}
