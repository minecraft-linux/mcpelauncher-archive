#pragma once
#include <type_traits>

template<class T = void(*)(), bool = false> struct remove_all_extends;
template<class T, class... arg> struct remove_all_extends<T(*)(arg...), false> {
    using CType = T(arg...);
    using PointerType = T(*)(arg...);
    using ReferenceType = T(&)(arg...);
};
template<class T, class... arg> struct remove_all_extends<T(*)(arg...) noexcept,std::is_same<void (*)() noexcept, void (*)()>::value> : remove_all_extends<T(*)(arg...)> {};
template<class T, class... arg> struct remove_all_extends<T(&)(arg...)> : remove_all_extends<T(*)(arg...)> {};
template<class T, class... arg> struct remove_all_extends<T(&)(arg...) noexcept,std::is_same<void (&)() noexcept, void (&)()>::value> : remove_all_extends<T(*)(arg...)> {};
template<class T, class... arg> struct remove_all_extends<T(arg...)> : remove_all_extends<T(*)(arg...)> {};
template<class T, class... arg> struct remove_all_extends<T(arg...) noexcept,std::is_same<void() noexcept, void ()>::value> : remove_all_extends<T(*)(arg...)> {};