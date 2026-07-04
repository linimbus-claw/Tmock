// tmock.h - Tmock C++17 Mock Framework (header-only)
// v0.8.0 - working withArgs with std::function args
//
// Macro API:
//   MOCK_METHOD0(Ret, Name)                         // 0 arg
//   MOCK_METHOD1(Ret, Name, T0)                    // 1 arg
//   MOCK_METHOD2(Ret, Name, T0, T1)               // 2 args
//   MOCK_METHOD3(Ret, Name, T0, T1, T2)          // 3 args
//   MOCK_METHOD4(Ret, Name, T0, T1, T2, T3)     // 4 args
//   MOCK_METHOD5(Ret, Name, T0, T1, T2, T3, T4) // 5 args
//   MOCK_METHOD_VOID0(Name)
//   MOCK_METHOD_VOID1(Name, T0)
//   ...
//
// Usage:
//   class MockFoo : public Foo {
//   public:
//     MOCK_METHOD2(bool, login, const std::string&, int);
//   };

#ifndef TMOCK_H_
#define TMOCK_H_

#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// ============================================================
//  withArgs 辅助：检测类型是否有 operator==
// ============================================================
template <typename T, typename = void>
struct TmockIsComparable : std::false_type {};

template <typename T>
struct TmockIsComparable<T, std::void_t<decltype(
    std::declval<T>() == std::declval<T>())>> : std::true_type {};

// 比较两个值（不可比较的类型跳过）
template <typename T>
bool tmock_compare_one(const T& a, const T& b) {
    if constexpr (TmockIsComparable<T>::value) {
        return a == b;
    } else {
        return true;  // 跳过
    }
}

// 用 tuple + index_sequence 逐个比较
template <typename... Args, size_t... Is>
bool tmock_compare_tuples_impl(const std::tuple<Args...>& expected,
                                const std::tuple<Args...>& actual,
                                std::index_sequence<Is...>) {
    bool ok = true;
    ((ok = ok && tmock_compare_one(std::get<Is>(expected),
                                   std::get<Is>(actual))),
     ...);
    return ok;
}

template <typename... Args>
bool tmock_compare_tuples(const std::tuple<Args...>& expected,
                            const std::tuple<Args...>& actual) {
    return tmock_compare_tuples_impl(
        expected, actual, std::make_index_sequence<sizeof...(Args)>{});
}

// ============================================================
//  TmockExpectation
// ============================================================
template <typename Ret, typename... Args>
struct TmockExpectation {
  using MatcherFn = std::function<bool(const Args&...)>;
  using ActionFn  = std::function<Ret(const Args&...)>;

  MatcherFn matcher;
  ActionFn  action;
  int       minTimes = 1;
  int       maxTimes = 1;
  int       actual   = 0;

  bool matches(const Args&... args) const {
    if (!matcher) return true;
    return matcher(args...);
  }
  bool isSaturated() const { return maxTimes >= 0 && actual >= maxTimes; }
  bool isSatisfied() const {
    return actual >= minTimes && (maxTimes < 0 || actual <= maxTimes);
  }

  // withArgs：按值匹配（自动跳过不可比较类型如 std::function）
  TmockExpectation& withArgs(const Args&... expected) {
    matcher = [=](const Args&... actual) {
      return tmock_compare_tuples(std::make_tuple(expected...),
                                  std::make_tuple(actual...));
    };
    return *this;
  }

  TmockExpectation& with(MatcherFn m) {
    matcher = std::move(m);
    return *this;
  }

  TmockExpectation& willReturn(Ret val) {
    action = [val](const Args&...) { return val; };
    return *this;
  }

  template <typename Fn>
  TmockExpectation& willInvoke(Fn&& fn) {
    action = std::forward<Fn>(fn);
    return *this;
  }

  TmockExpectation& times(int n) {
    minTimes = n; maxTimes = n; return *this;
  }
  TmockExpectation& times(int lo, int hi) {
    minTimes = lo; maxTimes = hi; return *this;
  }
  TmockExpectation& atLeast(int n) {
    minTimes = n; maxTimes = -1; return *this;
  }
};

// void 特化
template <typename... Args>
struct TmockExpectation<void, Args...> {
  using MatcherFn = std::function<bool(const Args&...)>;
  using ActionFn  = std::function<void(const Args&...)>;

  MatcherFn matcher;
  ActionFn  action;
  int       minTimes = 1;
  int       maxTimes = 1;
  int       actual   = 0;

  bool matches(const Args&... args) const {
    if (!matcher) return true;
    return matcher(args...);
  }
  bool isSaturated() const { return maxTimes >= 0 && actual >= maxTimes; }
  bool isSatisfied() const {
    return actual >= minTimes && (maxTimes < 0 || actual <= maxTimes);
  }

  TmockExpectation& withArgs(const Args&... expected) {
    matcher = [=](const Args&... actual) {
      return tmock_compare_tuples(std::make_tuple(expected...),
                                  std::make_tuple(actual...));
    };
    return *this;
  }

  TmockExpectation& with(MatcherFn m) {
    matcher = std::move(m);
    return *this;
  }

  template <typename Fn>
  TmockExpectation& willInvoke(Fn&& fn) {
    action = std::forward<Fn>(fn);
    return *this;
  }

  TmockExpectation& times(int n) {
    minTimes = n; maxTimes = n; return *this;
  }
  TmockExpectation& times(int lo, int hi) {
    minTimes = lo; maxTimes = hi; return *this;
  }
  TmockExpectation& atLeast(int n) {
    minTimes = n; maxTimes = -1; return *this;
  }
};

// ============================================================
//  TmockCore
// ============================================================
template <typename Ret, typename... Args>
class TmockCore {
public:
  using Expect = TmockExpectation<Ret, Args...>;

  ~TmockCore() { verify(); }

  Expect& addExpectation() {
    std::lock_guard<std::mutex> lock(mu_);
    expects_.emplace_back();
    return expects_.back();
  }

  Expect& expect(const Args&... args) {
    auto& e = addExpectation();
    e.withArgs(args...);
    return e;
  }

  Ret call(const Args&... args) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : expects_) {
      if (e.matches(args...) && !e.isSaturated()) {
        e.actual++;
        if (e.action) return e.action(args...);
        break;
      }
    }
    if constexpr (std::is_default_constructible_v<Ret> && !std::is_void_v<Ret>) {
      return Ret{};
    } else if constexpr (std::is_void_v<Ret>) {
      return;
    } else {
      throw std::runtime_error("Tmock: no matching expectation");
    }
  }

  void verify() const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& e : expects_) {
      if (!e.isSatisfied()) {
        std::ostringstream os;
        os << "[Tmock] VERIFY FAILED:"
           << "  expected [" << e.minTimes << ", "
           << (e.maxTimes < 0 ? "inf" : std::to_string(e.maxTimes))
           << "]  actual=" << e.actual;
        std::cerr << os.str() << std::endl;
      }
    }
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mu_);
    expects_.clear();
  }

private:
  mutable std::mutex mu_;
  std::vector<Expect> expects_;
};

template <typename... Args>
class TmockCore<void, Args...> {
public:
  using Expect = TmockExpectation<void, Args...>;

  ~TmockCore() { verify(); }

  Expect& addExpectation() {
    std::lock_guard<std::mutex> lock(mu_);
    expects_.emplace_back();
    return expects_.back();
  }

  Expect& expect(const Args&... args) {
    auto& e = addExpectation();
    e.withArgs(args...);
    return e;
  }

  void call(const Args&... args) {
    std::lock_guard<std::mutex> lock(mu_);
    for (auto& e : expects_) {
      if (e.matches(args...) && !e.isSaturated()) {
        e.actual++;
        if (e.action) e.action(args...);
        return;
      }
    }
  }

  void verify() const {
    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& e : expects_) {
      if (!e.isSatisfied()) {
        std::cerr << "[Tmock] VERIFY FAILED" << std::endl;
      }
    }
  }

  void reset() {
    std::lock_guard<std::mutex> lock(mu_);
    expects_.clear();
  }

private:
  mutable std::mutex mu_;
  std::vector<Expect> expects_;
};

// ============================================================
//  MOCK_METHOD0 .. MOCK_METHOD5 宏
// ============================================================

#define MOCK_METHOD0(Ret, Name)                               \
  public:                                                          \
    TmockCore<Ret> core_##Name;                               \
    Ret Name() override {                                       \
      return core_##Name.call();                               \
    }

#define MOCK_METHOD1(Ret, Name, T0)                         \
  public:                                                          \
    TmockCore<Ret, T0> core_##Name;                        \
    Ret Name(T0 tmock_a0) override {                        \
      return core_##Name.call(tmock_a0);                   \
    }

#define MOCK_METHOD2(Ret, Name, T0, T1)                   \
  public:                                                          \
    TmockCore<Ret, T0, T1> core_##Name;                  \
    Ret Name(T0 tmock_a0, T1 tmock_a1) override {        \
      return core_##Name.call(tmock_a0, tmock_a1);       \
    }

#define MOCK_METHOD3(Ret, Name, T0, T1, T2)             \
  public:                                                          \
    TmockCore<Ret, T0, T1, T2> core_##Name;            \
    Ret Name(T0 tmock_a0, T1 tmock_a1, T2 tmock_a2)    \
        override {                                              \
      return core_##Name.call(tmock_a0, tmock_a1, tmock_a2); \
    }

#define MOCK_METHOD4(Ret, Name, T0, T1, T2, T3)       \
  public:                                                          \
    TmockCore<Ret, T0, T1, T2, T3> core_##Name;      \
    Ret Name(T0 tmock_a0, T1 tmock_a1, T2 tmock_a2,    \
             T3 tmock_a3) override {                        \
      return core_##Name.call(tmock_a0, tmock_a1,         \
                              tmock_a2, tmock_a3);       \
    }

#define MOCK_METHOD5(Ret, Name, T0, T1, T2, T3, T4) \
  public:                                                          \
    TmockCore<Ret, T0, T1, T2, T3, T4> core_##Name; \
    Ret Name(T0 tmock_a0, T1 tmock_a1, T2 tmock_a2,    \
             T3 tmock_a3, T4 tmock_a4) override {         \
      return core_##Name.call(tmock_a0, tmock_a1,         \
                              tmock_a2, tmock_a3, tmock_a4); \
    }

// ---- void 版本 ----
#define MOCK_METHOD_VOID0(Name)                              \
  public:                                                          \
    TmockCore<void> core_##Name;                             \
    void Name() override {                                          \
      core_##Name.call();                                         \
    }

#define MOCK_METHOD_VOID1(Name, T0)                         \
  public:                                                          \
    TmockCore<void, T0> core_##Name;                      \
    void Name(T0 tmock_a0) override {                      \
      core_##Name.call(tmock_a0);                          \
    }

#define MOCK_METHOD_VOID2(Name, T0, T1)                   \
  public:                                                          \
    TmockCore<void, T0, T1> core_##Name;                \
    void Name(T0 tmock_a0, T1 tmock_a1) override {     \
      core_##Name.call(tmock_a0, tmock_a1);             \
    }

#define MOCK_METHOD_VOID3(Name, T0, T1, T2)           \
  public:                                                          \
    TmockCore<void, T0, T1, T2> core_##Name;        \
    void Name(T0 tmock_a0, T1 tmock_a1, T2 tmock_a2) \
        override {                                              \
      core_##Name.call(tmock_a0, tmock_a1, tmock_a2);  \
    }

#define MOCK_METHOD_VOID4(Name, T0, T1, T2, T3)     \
  public:                                                          \
    TmockCore<void, T0, T1, T2, T3> core_##Name;  \
    void Name(T0 tmock_a0, T1 tmock_a1, T2 tmock_a2, \
              T3 tmock_a3) override {                     \
      core_##Name.call(tmock_a0, tmock_a1,              \
                        tmock_a2, tmock_a3);            \
    }

#define MOCK_METHOD_VOID5(Name, T0, T1, T2, T3, T4) \
  public:                                                          \
    TmockCore<void, T0, T1, T2, T3, T4> core_##Name; \
    void Name(T0 tmock_a0, T1 tmock_a1, T2 tmock_a2,    \
              T3 tmock_a3, T4 tmock_a4) override {         \
      core_##Name.call(tmock_a0, tmock_a1,              \
                        tmock_a2, tmock_a3, tmock_a4); \
    }

// ============================================================
//  Sequence（时序校验）
// ============================================================
struct TmockSequence {
  std::vector<std::string> calls;
  std::mutex mu;
  bool enabled = false;

  static TmockSequence& global() {
    static TmockSequence seq;
    return seq;
  }
};

struct TmockScopedInSequence {
  TmockScopedInSequence() { TmockSequence::global().enabled = true; }
  ~TmockScopedInSequence() { TmockSequence::global().enabled = false; }
};

#define TMOCK_IN_SEQUENCE                                       \
  TmockScopedInSequence _tmock_seq_obj_

inline void tmock_record_call(const std::string& label) {
  auto& seq = TmockSequence::global();
  if (!seq.enabled) return;
  std::lock_guard<std::mutex> lock(seq.mu);
  seq.calls.push_back(label);
}

inline bool tmock_verify_order(const std::string& label1,
                                const std::string& label2) {
  auto& seq = TmockSequence::global();
  std::lock_guard<std::mutex> lock(seq.mu);
  size_t p1 = seq.calls.size(), p2 = seq.calls.size();
  for (size_t i = 0; i < seq.calls.size(); i++) {
    if (seq.calls[i] == label1) p1 = i;
    if (seq.calls[i] == label2) p2 = i;
  }
  return p1 < p2 && p1 < seq.calls.size() && p2 < seq.calls.size();
}

#define TMOCK_EXPECT_ORDER_BEFORE(label1, label2)              \
  do {                                                          \
    if (!tmock_verify_order(label1, label2)) {                 \
      std::cerr << "[Tmock] ORDER FAILED: " << label1          \
                << " should be before " << label2 << std::endl; \
    }                                                           \
  } while (0)

#endif  // TMOCK_H_
