// tmock.h - Tmock C++17 Mock Framework (header-only)
//
// v1.1.0 - EXPECT_CALL auto type deduction
//
// 用法：
//   MOCK_METHOD(Ret, Name, (T0, T1, ...))   // 括号包裹类型
//   EXPECT_CALL(mock, Name, matchers...).Return(val).Times(n)
//
// Matchers: _  An<T>()  Eq(v)  Matcher(fn)  HasPrefix(s)  Not(m)

#ifndef TMOCK_H_
#define TMOCK_H_

#include <any>
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
//  Matchers
// ============================================================

// 通配符
struct _TmockWildcard {
    template <typename T> bool matches(const T&) const { return true; }
};
inline constexpr _TmockWildcard _{};

// 任意 T
template <typename T> struct _TmockAny {
    template <typename U = T> bool matches(U&&) const { return true; }
};
template <typename T> _TmockAny<T> An() { return {}; }

// Eq
template <typename T> struct _TmockEq {
    T value;
    explicit _TmockEq(const T& v) : value(v) {}
    template <typename U> bool matches(U&& u) const { return value == u; }
};
template <typename T> _TmockEq<T> Eq(const T& v) { return _TmockEq<T>(v); }

// Matcher(fn)
template <typename T, typename Pred> struct _TmockPred {
    Pred pred;
    _TmockPred(Pred p) : pred(std::move(p)) {}
    template <typename U> bool matches(U&& u) const { return pred(u); }
};
template <typename T, typename Pred>
_TmockPred<T, Pred> Matcher(Pred p) { return _TmockPred<T, Pred>(std::move(p)); }

// HasPrefix
template <typename T = std::string> struct _TmockHasPrefix {
    std::string prefix;
    _TmockHasPrefix(std::string p) : prefix(std::move(p)) {}
    template <typename U> bool matches(U&& u) const {
        return u.size() >= prefix.size() &&
               u.compare(0, prefix.size(), prefix) == 0;
    }
};
template <typename T = std::string>
_TmockHasPrefix<T> HasPrefix(std::string p) { return _TmockHasPrefix<T>(std::move(p)); }

// Not
template <typename M> struct _TmockNot {
    M m;
    _TmockNot(M mm) : m(std::move(mm)) {}
    template <typename U> bool matches(U&& u) const { return !m.matches(u); }
};
template <typename M> _TmockNot<M> Not(M m) { return _TmockNot<M>(std::move(m)); }

// Matcher 检测：直接检测是否有 matches(U&&) 方法
template <typename T, typename A, typename = void>
struct TmockHasMatches : std::false_type {};
template <typename T, typename A>
struct TmockHasMatches<T, A, std::void_t<decltype(
    std::declval<T>().matches(std::declval<A>()))>> : std::true_type {};
template <typename T> struct _TmockArg { using type = T; };

// 比较单个值
template <typename E, typename A>
bool tmock_cmp(const E& expected, const A& actual) {
    if constexpr (std::is_same_v<std::decay_t<E>, std::any>) {
        if (auto* p = std::any_cast<std::decay_t<A>>(&expected))
            return *p == actual;
        return false;
    } else if constexpr (TmockHasMatches<E, const A&>::value) {
        return expected.matches(actual);
    } else if constexpr (std::is_invocable_v<E, A>) {
        return expected(actual);
    } else {
        return expected == actual;
    }
}

// const char* → string 特化
inline bool tmock_cmp(const char* a, const std::string& b) {
    return b == a;
}
inline bool tmock_cmp(const std::string& a, const char* b) {
    return a == b;
}
inline bool tmock_cmp(char a, char b) { return a == b; }

// ============================================================
//  TmockExpectation
// ============================================================
template <typename Ret, typename... Args>
struct TmockExpectation {
    std::function<bool(const Args&...)> matcher;
    std::function<Ret(const Args&...)> action;
    int minTimes = 1, maxTimes = 1, actual = 0;

    bool matches(const Args&... args) const {
        return !matcher || matcher(args...);
    }
    bool isSatisfied() const {
        return actual >= minTimes && (maxTimes < 0 || actual <= maxTimes);
    }

    TmockExpectation& with(std::function<bool(const Args&...)> m) {
        matcher = std::move(m); return *this;
    }
    TmockExpectation& willReturn(Ret val) {
        action = [val](const Args&...) { return val; }; return *this;
    }
    template <typename F>
    TmockExpectation& willInvoke(F&& fn) {
        action = std::forward<F>(fn); return *this;
    }
    TmockExpectation& times(int n) { minTimes = n; maxTimes = n; return *this; }
    TmockExpectation& times(int lo, int hi) { minTimes = lo; maxTimes = hi; return *this; }
    TmockExpectation& atLeast(int n) { minTimes = n; maxTimes = -1; return *this; }
};

template <typename... Args>
struct TmockExpectation<void, Args...> {
    std::function<bool(const Args&...)> matcher;
    std::function<void(const Args&...)> action;
    int minTimes = 1, maxTimes = 1, actual = 0;

    bool matches(const Args&... args) const {
        return !matcher || matcher(args...);
    }
    bool isSatisfied() const {
        return actual >= minTimes && (maxTimes < 0 || actual <= maxTimes);
    }

    TmockExpectation& with(std::function<bool(const Args&...)> m) {
        matcher = std::move(m); return *this;
    }
    template <typename F>
    TmockExpectation& willInvoke(F&& fn) {
        action = std::forward<F>(fn); return *this;
    }
    TmockExpectation& times(int n) { minTimes = n; maxTimes = n; return *this; }
    TmockExpectation& times(int lo, int hi) { minTimes = lo; maxTimes = hi; return *this; }
    TmockExpectation& atLeast(int n) { minTimes = n; maxTimes = -1; return *this; }
};

// ============================================================
//  TmockCore
// ============================================================
template <typename Ret, typename... Args>
class TmockCore {
public:
    using Expect = TmockExpectation<Ret, Args...>;
    Expect& addExpectation() {
        std::lock_guard<std::mutex> lock(mu_);
        expects_.emplace_back(); return expects_.back();
    }

    Ret call(const Args&... args) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : expects_) {
            if (e.matches(args...) && e.actual < e.maxTimes) {
                e.actual++;
                if (e.action) return e.action(args...);
                break;
            }
        }
        if constexpr (std::is_void_v<Ret>) return;
        else if constexpr (std::is_default_constructible_v<Ret>) return Ret{};
        else throw std::runtime_error("Tmock: no matching expectation");
    }

    void verify() const {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& e : expects_) {
            if (!e.isSatisfied()) {
                std::cerr << "[Tmock] Unsatisfied: expected ["
                          << e.minTimes << ", "
                          << (e.maxTimes < 0 ? "inf" : std::to_string(e.maxTimes))
                          << "] actual=" << e.actual << std::endl;
            }
        }
    }
    bool allSatisfied() const {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& e : expects_) if (!e.isSatisfied()) return false;
        return true;
    }
    void reset() { std::lock_guard<std::mutex> lock(mu_); expects_.clear(); }

private:
    mutable std::mutex mu_;
    std::vector<Expect> expects_;
};

template <typename... Args>
class TmockCore<void, Args...> {
public:
    using Expect = TmockExpectation<void, Args...>;
    Expect& addExpectation() {
        std::lock_guard<std::mutex> lock(mu_);
        expects_.emplace_back(); return expects_.back();
    }
    void call(const Args&... args) {
        std::lock_guard<std::mutex> lock(mu_);
        for (auto& e : expects_) {
            if (e.matches(args...) && e.actual < e.maxTimes) {
                e.actual++;
                if (e.action) e.action(args...);
                return;
            }
        }
    }
    void verify() const {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& e : expects_) {
            if (!e.isSatisfied()) std::cerr << "[Tmock] Unsatisfied expectation" << std::endl;
        }
    }
    bool allSatisfied() const {
        std::lock_guard<std::mutex> lock(mu_);
        for (const auto& e : expects_) if (!e.isSatisfied()) return false;
        return true;
    }
    void reset() { std::lock_guard<std::mutex> lock(mu_); expects_.clear(); }

private:
    mutable std::mutex mu_;
    std::vector<Expect> expects_;
};

// ============================================================
// ============================================================
//  EXPECT_CALL
//
// 设计：matchers 存入 std::tuple<Ms...>，index_sequence 逐个比较
// 用法：EXPECT_CALL(mock, login, "alice", _).Return(true)
// Ret 和 Args 由 TmockCore<Ret,Args...> 自动推导，无需手动指定
//
// 链式方法：.Return(val) .Invoke(fn) .Times(n) .AtLeast(n) .with(fn)
// ============================================================

template <typename Ret, typename... Args>
class _TmockCall {
    TmockCore<Ret, Args...>* core_;

    // 逐个比较：matchers[i] vs actuals[i]
    // 用 auto 捕获器 lambda + index_sequence，确保 (index, actual) 两个参数
    // 展开：ok=(ok&&check(0,a₀)), ok=(ok&&check(1,a₁)), ... 短路求值
    template <typename... Ms, size_t... Is>
    static bool match_tuple_impl(const std::tuple<Ms...>& ms,
                                  const Args&... actual,
                                  std::index_sequence<Is...>) {
        bool ok = true;
        ((ok = (ok && ([ms](auto I, const auto& a) -> bool {
                        return tmock_cmp(std::get<I>(ms), a);
                    }(std::integral_constant<size_t, Is>{}, actual)))), ...);
        return ok;
    }

    template <typename... Ms>
    static bool match_tuple(const std::tuple<Ms...>& ms, const Args&... actual) {
        if constexpr (sizeof...(Ms) == 0) return true;
        return match_tuple_impl(ms, actual..., std::make_index_sequence<sizeof...(Ms)>{});
    }

    std::function<bool(const Args&...)> matcher_fn_;

public:
    template <typename... Ms>
    _TmockCall(TmockCore<Ret, Args...>* c, std::tuple<Ms...> matchers)
        : core_(c) {
        if constexpr (sizeof...(Ms) == 0) {
            matcher_fn_ = [](const Args&...) -> bool { return true; };
        } else {
            auto ms_copy = matchers;
            matcher_fn_ = [ms_copy](const Args&... actual) -> bool {
                return match_tuple(ms_copy, actual...);
            };
        }
    }

    // 复用同一个 Expectation：matcher_fn_ 来自构造函数的 matchers，
    // with() 覆盖为自定义 matcher，后续 Return/Invoke/Times 共用同一个 Expectation
    TmockExpectation<Ret, Args...>* lastExpectation_ = nullptr;

    template <typename Fn>
    _TmockCall& with(Fn fn) {
        if (!lastExpectation_) lastExpectation_ = &core_->addExpectation();
        // 合并：stored matchers OR custom matcher
        auto orig = matcher_fn_;
        lastExpectation_->with([orig=std::move(orig), fn=std::move(fn)](const Args&... a) {
            return orig(a...) || fn(a...);
        });
        return *this;
    }

    template <typename V>
    _TmockCall& Return(V val) {
        if (!lastExpectation_) lastExpectation_ = &core_->addExpectation();
        lastExpectation_->with(matcher_fn_);
        lastExpectation_->willReturn(val);
        return *this;
    }

    template <typename Fn>
    _TmockCall& Invoke(Fn fn) {
        if (!lastExpectation_) lastExpectation_ = &core_->addExpectation();
        lastExpectation_->with(matcher_fn_);
        lastExpectation_->willInvoke(fn);
        return *this;
    }

    _TmockCall& Times(int n) {
        if (!lastExpectation_) lastExpectation_ = &core_->addExpectation();
        lastExpectation_->with(matcher_fn_);
        lastExpectation_->times(n);
        return *this;
    }

    _TmockCall& Times(int lo, int hi) {
        if (!lastExpectation_) lastExpectation_ = &core_->addExpectation();
        lastExpectation_->with(matcher_fn_);
        lastExpectation_->times(lo, hi);
        return *this;
    }

    _TmockCall& AtLeast(int n) {
        if (!lastExpectation_) lastExpectation_ = &core_->addExpectation();
        lastExpectation_->with(matcher_fn_);
        lastExpectation_->atLeast(n);
        return *this;
    }

    void verify() const { core_->verify(); }
    bool allSatisfied() const { return core_->allSatisfied(); }
};

template <typename Ret, typename... Args, typename... Ms>
_TmockCall<Ret, Args...>
make_tmock_call(TmockCore<Ret, Args...>& c, std::tuple<Ms...> matchers) {
    return _TmockCall<Ret, Args...>(&c, std::move(matchers));
}

// EXPECT_CALL: mock, methodName, matchers...
// Ret 和 Args 由 TmockCore<Ret,Args...> 自动推导
#define EXPECT_CALL(mock_obj, methodName, ...) \
    make_tmock_call((mock_obj.core_##methodName), std::make_tuple(__VA_ARGS__))

#define TMOCK_UNPACK(...) __VA_ARGS__

// ============================================================
// ============================================================
//  MOCK_METHOD 宏（括号包裹类型，gMock 风格）
// 用法：MOCK_METHOD(Ret, Name, (T0, T1, ...))
// 支持 0-8 参数
// ============================================================

#define TMOCK_NARG_(...) TMOCK_ARG_N(__VA_ARGS__)
#define TMOCK_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,N,...) N
#define TMOCK_RSEQ_N() 8,7,6,5,4,3,2,1,0
#define TMOCK_NARG(...) TMOCK_NARG_(__VA_ARGS__,TMOCK_RSEQ_N())

#define TMOCK_IMPL0(Ret,Name) TmockCore<Ret> core_##Name; Ret Name() override { return core_##Name.call(); }
#define TMOCK_IMPL1(Ret,Name,T0) TmockCore<Ret,T0> core_##Name; Ret Name(T0 t0) override { return core_##Name.call(t0); }
#define TMOCK_IMPL2(Ret,Name,T0,T1) TmockCore<Ret,T0,T1> core_##Name; Ret Name(T0 t0,T1 t1) override { return core_##Name.call(t0,t1); }
#define TMOCK_IMPL3(Ret,Name,T0,T1,T2) TmockCore<Ret,T0,T1,T2> core_##Name; Ret Name(T0 t0,T1 t1,T2 t2) override { return core_##Name.call(t0,t1,t2); }
#define TMOCK_IMPL4(Ret,Name,T0,T1,T2,T3) TmockCore<Ret,T0,T1,T2,T3> core_##Name; Ret Name(T0 t0,T1 t1,T2 t2,T3 t3) override { return core_##Name.call(t0,t1,t2,t3); }
#define TMOCK_IMPL5(Ret,Name,T0,T1,T2,T3,T4) TmockCore<Ret,T0,T1,T2,T3,T4> core_##Name; Ret Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4) override { return core_##Name.call(t0,t1,t2,t3,t4); }
#define TMOCK_IMPL6(Ret,Name,T0,T1,T2,T3,T4,T5) TmockCore<Ret,T0,T1,T2,T3,T4,T5> core_##Name; Ret Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4,T5 t5) override { return core_##Name.call(t0,t1,t2,t3,t4,t5); }
#define TMOCK_IMPL7(Ret,Name,T0,T1,T2,T3,T4,T5,T6) TmockCore<Ret,T0,T1,T2,T3,T4,T5,T6> core_##Name; Ret Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6) override { return core_##Name.call(t0,t1,t2,t3,t4,t5,t6); }
#define TMOCK_IMPL8(Ret,Name,T0,T1,T2,T3,T4,T5,T6,T7) TmockCore<Ret,T0,T1,T2,T3,T4,T5,T6,T7> core_##Name; Ret Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6,T7 t7) override { return core_##Name.call(t0,t1,t2,t3,t4,t5,t6,t7); }

#define TMOCK_DISPATCH_(Ret,Name,N,...) TMOCK_IMPL##N(Ret,Name,__VA_ARGS__)
#define TMOCK_DISPATCH__(Ret,Name,N,...) TMOCK_DISPATCH_(Ret,Name,N,__VA_ARGS__)
#define MOCK_METHOD(Ret,Name,types) TMOCK_DISPATCH__(Ret,Name,TMOCK_NARG types,TMOCK_UNPACK types)

// void 特化版本
#define TMOCK_VIMPL0(Name) TmockCore<void> core_##Name; void Name() override { core_##Name.call(); }
#define TMOCK_VIMPL1(Name,T0) TmockCore<void,T0> core_##Name; void Name(T0 t0) override { core_##Name.call(t0); }
#define TMOCK_VIMPL2(Name,T0,T1) TmockCore<void,T0,T1> core_##Name; void Name(T0 t0,T1 t1) override { core_##Name.call(t0,t1); }
#define TMOCK_VIMPL3(Name,T0,T1,T2) TmockCore<void,T0,T1,T2> core_##Name; void Name(T0 t0,T1 t1,T2 t2) override { core_##Name.call(t0,t1,t2); }
#define TMOCK_VIMPL4(Name,T0,T1,T2,T3) TmockCore<void,T0,T1,T2,T3> core_##Name; void Name(T0 t0,T1 t1,T2 t2,T3 t3) override { core_##Name.call(t0,t1,t2,t3); }
#define TMOCK_VIMPL5(Name,T0,T1,T2,T3,T4) TmockCore<void,T0,T1,T2,T3,T4> core_##Name; void Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4) override { core_##Name.call(t0,t1,t2,t3,t4); }
#define TMOCK_VIMPL6(Name,T0,T1,T2,T3,T4,T5) TmockCore<void,T0,T1,T2,T3,T4,T5> core_##Name; void Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4,T5 t5) override { core_##Name.call(t0,t1,t2,t3,t4,t5); }
#define TMOCK_VIMPL7(Name,T0,T1,T2,T3,T4,T5,T6) TmockCore<void,T0,T1,T2,T3,T4,T5,T6> core_##Name; void Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6) override { core_##Name.call(t0,t1,t2,t3,t4,t5,t6); }
#define TMOCK_VIMPL8(Name,T0,T1,T2,T3,T4,T5,T6,T7) TmockCore<void,T0,T1,T2,T3,T4,T5,T6,T7> core_##Name; void Name(T0 t0,T1 t1,T2 t2,T3 t3,T4 t4,T5 t5,T6 t6,T7 t7) override { core_##Name.call(t0,t1,t2,t3,t4,t5,t6,t7); }

#define TMOCK_VDISPATCH_(Name,N,...) TMOCK_VIMPL##N(Name,__VA_ARGS__)
#define TMOCK_VDISPATCH__(Name,N,...) TMOCK_VDISPATCH_(Name,N,__VA_ARGS__)
#define MOCK_METHOD_VOID(Name,types) TMOCK_VDISPATCH__(Name,TMOCK_NARG types,TMOCK_UNPACK types)

// ============================================================
//  Verify
// ============================================================
struct MockViolation { std::string msg; };

template <typename Ret, typename... Args>
void Verify(const TmockCore<Ret, Args...>& c) {
    const_cast<TmockCore<Ret, Args...>&>(c).verify();
}

// ============================================================
//  Sequence
// ============================================================
struct TmockSequence {
    std::vector<std::string> calls;
    std::mutex mu;
    bool enabled = false;
    static TmockSequence& global() { static TmockSequence s; return s; }
};

struct TmockScopedInSequence {
    TmockScopedInSequence() { TmockSequence::global().enabled = true; }
    ~TmockScopedInSequence() { TmockSequence::global().enabled = false; }
};

#define TMOCK_IN_SEQUENCE TmockScopedInSequence _tmock_seq_obj_

inline void tmock_record_call(const std::string& label) {
    auto& seq = TmockSequence::global();
    if (!seq.enabled) return;
    std::lock_guard<std::mutex> lock(seq.mu);
    seq.calls.push_back(label);
}

inline bool tmock_verify_order(const std::string& a, const std::string& b) {
    auto& seq = TmockSequence::global();
    std::lock_guard<std::mutex> lock(seq.mu);
    size_t pa = seq.calls.size(), pb = seq.calls.size();
    for (size_t i = 0; i < seq.calls.size(); i++) {
        if (seq.calls[i] == a) pa = i;
        if (seq.calls[i] == b) pb = i;
    }
    return pa < seq.calls.size() && pb < seq.calls.size() && pa < pb;
}

#define TMOCK_EXPECT_ORDER_BEFORE(a, b) \
    do { if (!tmock_verify_order(a,b)) \
        std::cerr << "[Tmock] ORDER FAILED: " << a << " should be before " << b << std::endl; \
    } while(0)

#endif  // TMOCK_H_
