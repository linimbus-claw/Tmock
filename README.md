# Tmock v0.8.0

Header-only C++17 Mock Framework —— 轻量、易用、单文件。

## 特性

- ✅ **Header-only**，只需 `#include "tmock/tmock.h"`
- ✅ **继承式打桩**，符合 C++ OOP 习惯
- ✅ **同步 / 异步接口** 统一支持（`std::function` 回调）
- ✅ **参数匹配**：精确值 / 自定义 lambda matcher
- ✅ **调用次数校验**：`times(n)` / `times(lo,hi)` / `atLeast(n)`
- ✅ **调用时序校验**：`TMOCK_IN_SEQUENCE` + `TMOCK_EXPECT_ORDER_BEFORE`
- ✅ **多线程安全**：`TmockCore` 内部用 `std::mutex` 保护
- ✅ **析构时自动 verify**，未满足的期望自动报错

## 快速开始

### 1. 定义接口

```cpp
// userService.h
#include <string>

class UserService {
public:
    virtual ~UserService() = default;
    virtual bool login(const std::string& user, int pwd) = 0;
    virtual int  getScore(const std::string& user) = 0;
};
```

### 2. 用宏生成 Mock

```cpp
// mock_user_service.h
#include "tmock/tmock.h"
#include "userService.h"

class MockUserService : public UserService {
public:
    // 同步方法：2 个参数
    MOCK_METHOD2(bool, login, const std::string&, int);
    // 同步方法：1 个参数
    MOCK_METHOD1(int, getScore, const std::string&);
};
```

### 3. 在测试中打桩

```cpp
#include "mock_user_service.h"
#include <cassert>

void test_login_success() {
    MockUserService mock;

    // 设置预期：login("alice", 1234) 返回 true，只调用 1 次
    mock.core_login.expect("alice", 1234)
        .willReturn(true)
        .times(1);

    // 调用被测代码
    bool result = mock.login("alice", 1234);
    assert(result == true);
}  // 离开作用域时自动 verify
```

### 4. 异步接口

```cpp
class ProfileService {
public:
    virtual void getProfile(const std::string& user,
                            std::function<void(const std::string&, int)> cb) = 0;
};

class MockProfileService : public ProfileService {
public:
    MOCK_METHOD_VOID2(getProfile,
                         const std::string&,
                         std::function<void(const std::string&, int)>);
};

void test_async() {
    MockProfileService mock;

    mock.core_getProfile.addExpectation()
        .withArgs("alice",
                    std::function<void(const std::string&, int)>{})
        .willInvoke([](const std::string& user,
                      std::function<void(const std::string&, int)> cb) {
            // 立即回调
            cb("Alice", 100);
        })
        .times(1);

    mock.getProfile("alice", [](const std::string& n, int s) {
        assert(n == "Alice");
        assert(s == 100);
    });
}
```

### 5. 调用时序校验

```cpp
void test_order() {
    TMOCK_IN_SEQUENCE;  // 开启时序记录

    MockDataService mock;
    mock.core_send.addExpectation().willInvoke([](const std::string& d) {
        tmock_record_call("send");
    });
    mock.core_ack.addExpectation().willInvoke([](int id) {
        tmock_record_call("ack");
        return true;
    });

    mock.send("hello");
    mock.ack(1);

    // 校验顺序
    TMOCK_EXPECT_ORDER_BEFORE("send", "ack");
}
```

## 宏参考

| 宏 | 说明 |
|---|---|
| `MOCK_METHOD0(Ret, Name)` | 0 参数，返回 `Ret` |
| `MOCK_METHOD1(Ret, Name, T0)` | 1 参数 |
| `MOCK_METHOD2(Ret, Name, T0, T1)` | 2 参数 |
| `MOCK_METHOD3(Ret, Name, T0, T1, T2)` | 3 参数 |
| `MOCK_METHOD4(Ret, Name, T0, T1, T2, T3)` | 4 参数 |
| `MOCK_METHOD5(Ret, Name, T0, T1, T2, T3, T4)` | 5 参数 |
| `MOCK_METHOD_VOID0(Name)` | `void` 返回，0 参数 |
| `MOCK_METHOD_VOID1(Name, T0)` | `void` 返回，1 参数 |
| ... | ... |

## Fluent API（`TmockExpectation`）

```cpp
auto& e = mock.core_xxx.addExpectation();
e.withArgs(expected1, expected2, ...)  // 参数精确匹配
 .with([](args...) { return ...; })  // 自定义 matcher
 .willReturn(value)                 // 返回值
 .willInvoke([](args...) { ... })    // 自定义逻辑
 .times(1)                          // 精确次数
 .times(1, 3)                      // 范围次数
 .atLeast(2);                       // 最少次数
```

## 构建测试

```bash
mkdir build && cd build
cmake ..
make
ctest
```

## 依赖

- C++17 编译器（GCC 7+, Clang 5+, MSVC 2017+）
- CMake 3.10+
- 无第三方库依赖

## License

MIT
