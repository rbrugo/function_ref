///
// function_ref - A low-overhead non-owning function
// Written in 2017 by Simon Brand (@TartanLlama)
//
// To the extent possible under law, the author(s) have dedicated all
// copyright and related and neighboring rights to this software to the
// public domain worldwide. This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication
// along with this software. If not, see
// <http://creativecommons.org/publicdomain/zero/1.0/>.
///

#ifndef TL_FUNCTION_REF_HPP
#define TL_FUNCTION_REF_HPP

#define TL_FUNCTION_REF_VERSION_MAJOR 0
#define TL_FUNCTION_REF_VERSION_MINOR 1

#include <functional>
#include <utility>

namespace tl {
namespace detail {
#ifndef TL_DECL_MUTEX
// C++14-style aliases for brevity
template <class T> using remove_const_t = typename std::remove_const<T>::type;
template <class T>
using remove_reference_t = typename std::remove_reference<T>::type;
template <class T> using decay_t = typename std::decay<T>::type;
template <bool E, class T = void>
using enable_if_t = typename std::enable_if<E, T>::type;
template <bool B, class T, class F>
using conditional_t = typename std::conditional<B, T, F>::type;

// std::invoke from C++17
// https://stackoverflow.com/questions/38288042/c11-14-invoke-workaround
template <typename Fn, typename... Args,
          typename = enable_if_t<std::is_member_pointer<decay_t<Fn>>{}>,
          int = 0>
constexpr auto invoke(Fn &&f, Args &&... args) noexcept(
    noexcept(std::mem_fn(f)(std::forward<Args>(args)...)))
    -> decltype(std::mem_fn(f)(std::forward<Args>(args)...)) {
  return std::mem_fn(f)(std::forward<Args>(args)...);
}

template <typename Fn, typename... Args,
          typename = enable_if_t<!std::is_member_pointer<decay_t<Fn>>{}>>
constexpr auto invoke(Fn &&f, Args &&... args) noexcept(
    noexcept(std::forward<Fn>(f)(std::forward<Args>(args)...)))
    -> decltype(std::forward<Fn>(f)(std::forward<Args>(args)...)) {
  return std::forward<Fn>(f)(std::forward<Args>(args)...);
}

// std::invoke_result from C++17
template <class F, class, class... Us> struct invoke_result_impl;

template <class F, class... Us>
struct invoke_result_impl<
    F, decltype(invoke(std::declval<F>(), std::declval<Us>()...), void()),
    Us...> {
  using type = decltype(invoke(std::declval<F>(), std::declval<Us>()...));
};

template <class F, class... Us>
using invoke_result = invoke_result_impl<F, void, Us...>;

template <class F, class... Us>
using invoke_result_t = typename invoke_result<F, Us...>::type;
#endif

template <bool, class R, class F, class... Args>
struct is_invocable_r_impl : std::false_type {};

template <class R, class F, class... Args>
struct is_invocable_r_impl<true, R, F, Args...> : std::true_type {};

template <class R, class F, class... Args>
using is_invocable_r =
    is_invocable_r_impl<std::is_same<invoke_result_t<F, Args...>, R>::value, R,
                        F, Args...>;

} // namespace detail

template <class F> class function_ref;

template <class R, class... Args> class function_ref<R(Args...)> {
public:
  constexpr function_ref() noexcept = default;
  constexpr function_ref(std::nullptr_t) noexcept : function_ref() {}
  constexpr function_ref(const function_ref &) noexcept = default;

  template <typename F,
            detail::enable_if_t<
                !std::is_same<detail::decay_t<F>, function_ref>::value &&
                detail::is_invocable_r<R, F &&, Args...>::value> * = nullptr>
  constexpr function_ref(F &&f) noexcept
      : obj_(reinterpret_cast<void *>(std::addressof(f))) {
    callback_ = [](void *obj, Args... args) {
      return detail::invoke(
          std::forward<F>(
              *reinterpret_cast<typename std::add_pointer<F>::type>(obj)),
          std::forward<Args>(args)...);
    };
  }

  constexpr function_ref &operator=(const function_ref &rhs) noexcept {
    obj_ = rhs.obj_;
    callback_ = rhs.callback_;
  }
  constexpr function_ref &operator=(std::nullptr_t) noexcept {
    obj_ = nullptr;
    callback_ = nullptr;
  }

  template <typename F,
            detail::enable_if_t<detail::is_invocable_r<R, F &&, Args...>::value>
                * = nullptr>
  constexpr function_ref &operator=(F &&f) noexcept {
    obj_ = std::addressof(f);
    callback_ = [](void *obj, Args... args) {
      return detail::invoke(std::forward<F>(*reinterpret_cast<F *>(obj)),
                            std::forward<Args>(args)...);
    };
  }

  constexpr void swap(function_ref &rhs) noexcept {
    std::swap(obj_, rhs.obj_);
    std::swap(callback_, rhs.callback_);
  }

  constexpr explicit operator bool() const noexcept { return obj_ != nullptr; }

  R operator()(Args... args) const {
    return callback_(obj_, std::forward<Args>(args)...);
  }

private:
  void *obj_ = nullptr;
  R (*callback_)(void *, Args...) = nullptr;
};

template <typename R, typename... Args>
constexpr void swap(function_ref<R(Args...)> &lhs,
                    function_ref<R(Args...)> &rhs) noexcept {
  lhs.swap(rhs);
}

template <typename R, typename... Args>
constexpr bool operator==(const function_ref<R(Args...)> &f,
                          std::nullptr_t) noexcept {
  return f == nullptr;
}

template <typename R, typename... Args>
constexpr bool operator==(std::nullptr_t,
                          const function_ref<R(Args...)> &f) noexcept {
  return f == nullptr;
}

template <typename R, typename... Args>
constexpr bool operator!=(const function_ref<R(Args...)> &f,
                          std::nullptr_t) noexcept {
  return f != nullptr;
}

template <typename R, typename... Args>
constexpr bool operator!=(std::nullptr_t,
                          const function_ref<R(Args...)> &f) noexcept {
  return f != nullptr;
}

#if __cplusplus >= 201703L
template <typename R, typename... Args>
function_ref(R (*)(Args...))->function_ref<R(Args...)>;

// TODO, will require some kind of callable traits
// template <typename F>
// function_ref(F) -> function_ref</* deduced if possible */>;
#endif
} // namespace tl

#endif