#pragma once
#include <algorithm>
#include <array>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>

#include <assert.h>

/*
The MIT License (MIT)

Copyright (c) 2020 Alex Anderson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

namespace inline_vector {
    namespace details {
        template <typename _Ty> constexpr void destroy_at(_Ty *const ptr) {
            ptr->~_Ty();
        }
        template <typename It1> constexpr void destroy(It1 first, It1 last) {
            for (; first != last; ++first)
                ::inline_vector::details::destroy_at(::std::addressof(*first));
        }

        template <typename It1, typename It2> constexpr It1 uninitialized_copy(It1 I, It1 E, It2 Dest) {
            return ::std::uninitialized_copy(I, E, Dest);
        }
        template <typename It1, typename It2> constexpr It1 uninitialized_copy_n(It1 I, size_t C, It2 Dest) {
            return ::std::uninitialized_copy_n(I, C, Dest);
        }
        template <typename It1, typename It2> constexpr It1 uninitialized_move(It1 I, It1 E, It2 Dest) {
            return ::std::uninitialized_copy(::std::make_move_iterator(I), ::std::make_move_iterator(E),
                                             Dest);
        }
        template <typename It1, typename It2> constexpr It1 uninitialized_move_n(It1 I, size_t C, It2 Dest) {
            return ::std::uninitialized_copy_n(::std::make_move_iterator(I), C, Dest);
        }
        template <typename It1, typename Val1> constexpr void uninitialized_fill(It1 I, It1 E, Val1 Dest) {
            ::std::uninitialized_fill(I, E, Dest);
        }
        template <typename It1, typename Val1> constexpr void uninitialized_fill_n(It1 I, size_t C, Val1 V) {
            ::std::uninitialized_fill_n(I, C, V);
        }

        enum class error_handling : uint8_t { _noop, _saturate, _exception, _error_code };
        constexpr const error_handling error_handler = error_handling::_noop;
    }; // namespace details

    template <typename T, bool destruct_on_exit = false> struct inline_vector {
        using element_type           = T;
        using value_type             = typename ::std::remove_cv<T>::type;
        using const_reference        = const value_type &;
        using size_type              = ::std::size_t;
        using difference_type        = ::std::ptrdiff_t;
        using pointer                = element_type *;
        using const_pointer          = const element_type *;
        using reference              = element_type &;
        using iterator               = pointer;
        using const_iterator         = const_pointer;
        using reverse_iterator       = ::std::reverse_iterator<iterator>;
        using const_reverse_iterator = ::std::reverse_iterator<const_iterator>;

        pointer _data = {}; // start of constructed range
        pointer _end  = {}; // end of constructed range
        pointer _cap  = {}; // end of entire range
      private:
        template <typename RetType>
        __forceinline RetType return_error(RetType ret, [[maybe_unused]] const char *err_msg) noexcept(::inline_vector::details::error_handler !=
                       ::inline_vector::details::error_handling::_exception) {
            if constexpr (::inline_vector::details::error_handler ==
                          ::inline_vector::details::error_handling::_noop) {
                return ret;
            } else if (::inline_vector::details::error_handler ==
                       ::inline_vector::details::error_handling::_saturate) {
                return ret;
            } else if (::inline_vector::details::error_handler ==
                       ::inline_vector::details::error_handling::_exception) {
#if __clang__
                throw std::bad_alloc();
#else
                throw std::bad_alloc(err_msg);
#endif

                return ret;
            } else if (::inline_vector::details::error_handler ==
                       ::inline_vector::details::error_handling::_error_code) {
                if constexpr (std::is_same<RetType, iterator>::value) {
                    return ++ret;
                } else if (std::is_same<RetType, bool>::value) {
                    return false;
                }
            } else {
                return ret;
            }
        };

        template <class It1> constexpr iterator append_range(It1 first, It1 last) {
            // insert input range [first, last) at _Where
            iterator ret_it = end();
            if (first == last) {
                return ret_it;
            }

            if constexpr (::std::is_same<::std::random_access_iterator_tag,
                                         ::std::iterator_traits<It1>::iterator_category>::value) {
                size_type insert_count = last - first;
                if (insert_count > (capacity() - size())) { // error? or noop
                    if constexpr (::inline_vector::details::error_handler !=
                                  ::inline_vector::details::error_handling::_noop) {
                        return ret_it = return_error(ret_it, "inline_vector cannot allocate space to insert");
                    }
                }
                // already safe from check above*
                ::inline_vector::details::uninitialized_copy_n(first, insert_count, end());
                _end += insert_count;
            } else {
                // bounds check each emplace_back, saturating
                for (; first != last && size() < capacity(); ++first) {
                    unchecked_emplace_back(*first);
                }
                if constexpr (::inline_vector::details::error_handler !=
                              ::inline_vector::details::error_handling::_noop) {
                    if (first != last) {
                        return ret_it = return_error(ret_it, "inline_vector cannot allocate space to insert");
                    }
                }
            }

            return ret_it;
        }

      public:

        constexpr inline_vector() = default;
        constexpr inline_vector(iterator data, iterator end = {}, iterator cap = {}) noexcept
            : _data(data), _end(end), _cap(cap){};

        // DANGER DANGER...HIGH VOLTAGE
        constexpr inline_vector(const inline_vector &other) {
            if (!other.empty())
                this->operator=(other);
        };

        constexpr inline_vector(inline_vector &&other) noexcept {
            if (!other.empty())
                this->operator=(::std::move(other));
        };

        // front
        [[nodiscard]] constexpr reference front() {
            assert(!empty());
            return _data[0];
        };
        [[nodiscard]] constexpr const_reference front() const {
            assert(!empty());
            return _data[0];
        };
        // back's
        [[nodiscard]] constexpr reference back() {
            assert(!empty());
            return *(_end - 1);
        };
        [[nodiscard]] constexpr const_reference back() const {
            assert(!empty());
            return *(_end - 1);
        };
        // data's
        [[nodiscard]] constexpr T *data() noexcept {
            return _data;
        };
        [[nodiscard]] constexpr const T *data() const noexcept {
            return _data;
        };
        // begin's
        [[nodiscard]] constexpr iterator begin() noexcept {
            return _data;
        };
        [[nodiscard]] constexpr const_iterator begin() const noexcept {
            return _data;
        };
        [[nodiscard]] constexpr const_iterator cbegin() const noexcept {
            return _data;
        };
        // rbegin's
        [[nodiscard]] constexpr reverse_iterator rbegin() noexcept {
            return reverse_iterator(end());
        };
        [[nodiscard]] constexpr const_reverse_iterator rbegin() const noexcept {
            return const_reverse_iterator(end());
        };
        [[nodiscard]] constexpr const_reverse_iterator crbegin() const noexcept {
            return const_reverse_iterator(end());
        };
        // end's
        [[nodiscard]] constexpr iterator end() noexcept {
            return _end;
        };
        [[nodiscard]] constexpr const_iterator end() const noexcept {
            return _end;
        };
        [[nodiscard]] constexpr const_iterator cend() const noexcept {
            return _end;
        };
        // rend's
        [[nodiscard]] constexpr reverse_iterator rend() noexcept {
            return reverse_iterator(begin());
        };
        [[nodiscard]] constexpr const_reverse_iterator rend() const noexcept {
            return const_reverse_iterator(begin());
        };
        [[nodiscard]] constexpr const_reverse_iterator crend() const noexcept {
            return const_reverse_iterator(begin());
        };
        // empty's
        [[nodiscard]] constexpr bool empty() const noexcept {
            return size() == 0;
        };
        // full (non standard)
        [[nodiscard]] constexpr bool full() const noexcept {
            return size() >= capacity();
        };

        // size
        constexpr size_type size() const noexcept {
            return _end - _data;
        };
        // capacity
        constexpr size_type capacity() const noexcept {
            return _cap - _data;
        };
        // max_size (constant)
        constexpr size_type max_size() const noexcept {
            return (std::numeric_limits<size_type>::max)() / sizeof(value_type);
        };

        // assign's
        constexpr void assign(size_type count, const T &value) {
            if (count <= capacity()) [[likely]] {
                clear();
                ::inline_vector::details::uninitialized_fill_n(begin(), count, value);
                _end = count;
            } else {
                if constexpr (::inline_vector::details::error_handler !=
                              ::inline_vector::details::error_handling::_noop) {
                    return_error(false, "inline_vector cannot allocate space to insert");
                }
            }
        };
        template <class It1> constexpr void assign(It1 first, It1 last) {
            if constexpr (::std::is_same<::std::random_access_iterator_tag,
                                         ::std::iterator_traits<It1>::iterator_category>::value) {
                size_type insert_count = last - first;
                if ((size() + insert_count) <= capacity()) {
                    clear();
                    ::inline_vector::details::uninitialized_copy_n(first, insert_count, end());
                    _end = insert_count;
                } else {
                    if constexpr (::inline_vector::details::error_handler !=
                                  ::inline_vector::details::error_handling::_noop) {
                        return_error(false, "inline_vector cannot allocate space to insert");
                    }
                }
            } else {
                clear();
                for (; first != last && size() < capacity(); ++first) {
                    unchecked_emplace_back(*first);
                }
                if constexpr (::inline_vector::details::error_handler !=
                              ::inline_vector::details::error_handling::_noop) {
                    if (first != last) {
                        return_error(false, "inline_vector cannot allocate space to insert");
                    }
                }
            }
        };
        constexpr void assign(::std::initializer_list<T> ilist) {
            assign(ilist.begin(), ilist.end());
        };

        // operator ='s
        constexpr inline_vector &operator=(const inline_vector &other) {
            if (this == &other)
                return *this;
            size_t rhs_size = other.size();
            size_t lhs_size = size();
            size_t lhs_cap  = capacity();
            // the other vector is smaller than us
            if (lhs_size >= rhs_size) {
                iterator new_end;
                if (rhs_size)
                    new_end = ::std::copy(other.begin(), other.begin() + rhs_size, begin());
                else // no copy, setup to clear
                    new_end = begin();
                // destroy excess elements
                ::inline_vector::details::destroy(new_end, end());
                _end  = _begin + rhs_size;
                return *this;
            }
            // the other vector fits within our capacity
            if (rhs_size <= lhs_cap) {
                // copy to initialized memory
                ::std::copy(other.begin(), other.begin() + lhs_size, begin());
                // copy to uninitialized memory
                ::inline_vector::details::uninitialized_copy(other.begin() + lhs_size,
                                                            other.begin() + rhs_size, begin() + lhs_size);
                _end = _begin + rhs_size;
                return *this;
            }
            
            // problem
            if constexpr (::inline_vector::details::error_handler !=
                          ::inline_vector::details::error_handling::_noop) {
                return_error(false, "inline_vector cannot allocate space to insert");
            }

            return *this;
        };
        constexpr inline_vector &operator=(inline_vector &&other) noexcept {
            size_t rhs_size = other.size();
            size_t lhs_size = size();
            size_t lhs_cap  = capacity();

            if (this == &other) {
                // do nothing
            } else if (lhs_size >= rhs_size) {
                // assign
                iterator new_end;
                if (rhs_size)
                    new_end = ::std::move(other.begin(), other.begin() + rhs_size, begin());
                else // no copy, setup to clear
                    new_end = begin();
                // destroy excess elements
                ::inline_vector::details::destroy(new_end, end());
                _end  = _data + rhs_size;
                // clear other
                other.clear();
            } else if (rhs_size <= lhs_cap) {
                // copy to initialized memory
                ::std::move(other.begin(), other.begin() + lhs_size, begin());
                // copy to uninitialized memory
                ::inline_vector::details::uninitialized_move(other.begin() + lhs_size,
                                                            other.begin() + rhs_size, begin() + lhs_size);
                _end = _data + rhs_size;
                other.clear();
            }

            // problem
            if constexpr (::inline_vector::details::error_handler !=
                          ::inline_vector::details::error_handling::_noop) {
                return_error(false, "inline_vector cannot allocate space to insert");
            }

            return *this;
        };
        constexpr inline_vector &operator=(::std::initializer_list<T> ilist) {
            assign(ilist);
            return *this;
        };

        // append's (non-standard)
        void append(size_type count, const T &value) {
            size_t space_remaining = capacity() - size();
            if (count && count <= space_remaining) [[likely]] {
                ::inline_vector::details::uninitialized_fill_n(end(), count, value);
                _end += count;
            } else {
                if constexpr (::inline_vector::details::error_handler !=
                              ::inline_vector::details::error_handling::_noop) {
                    return_error(false, "inline_vector cannot allocate space to insert");
                }
            }
        }

        template <typename It1> void append(It1 first, It1 last) {
            append_range(first, last);
        }


        // emplace_back's
        template <class... Args>
        constexpr reference
        emplace_back(Args &&...args) noexcept(::inline_vector::details::error_handler !=
                                              ::inline_vector::details::error_handling::_exception) {
            iterator it = end();
            if (size() < capacity()) [[likely]] {
                ::new ((void *)it) T(::std::forward<Args>(args)...);
                _end += 1;
            } else { // error?
                if constexpr (::inline_vector::details::error_handler ==
                              ::inline_vector::details::error_handling::_exception) {
#if __clang__
                    throw std::bad_alloc();
#else
                    throw std::bad_alloc("inline_vector cannot allocate to insert elements");
#endif
                }
            }
            return *it;
        };

        // emplace's
        template <class... Args> constexpr iterator emplace(const_iterator pos, Args &&...args) {
            size_type insert_idx = pos - cbegin();
            iterator  ret_it     = begin() + insert_idx;
            if (pos == cend()) { // special case for empty vector
                if (!full()) [[likely]] {
                    ::new ((void *)ret_it) T(::std::forward<Args>(args)...);
                    _end += 1;
                    return ret_it;
                } else { // error?
                    return ret_it = return_error(ret_it, "inline_vector cannot allocate to insert elements");
                }
            }

            assert(pos >= cbegin() && "insertion iterator is out of bounds.");
            assert(pos <= cend() && "inserting past the end of the inline_vector.");
            // if full we back out
            if (full()) {
                return ret_it = return_error(ret_it, "inline_vector cannot allocate to insert elements");
            }
            T tmp = T(::std::forward<Args>(args)...);
            // placement new back item, eg... ...insert here, a, b, c, end -> ...insert
            // here, a, a, b, c, (end)
            ::new ((void *)end()) T(::std::move(back()));
            // first last, destination
            ::std::move_backward(begin() + insert_idx, end() - 1, end());
            // up the size
            _end += 1;
            // insert the value
            *ret_it = ::std::move(tmp);

            return ret_it;
        };

        // insert's
        // be sure to verify range
        constexpr iterator insert(const_iterator pos, const T &value) {
            return emplace(pos, value);
        };
        constexpr iterator insert(const_iterator pos, T &&value) {
            return emplace(pos, ::std::move(value));
        };

        // push_back's
        constexpr void push_back(const T &value) noexcept(noexcept(emplace_back)) {
            emplace_back(::std::forward<const T &>(value));
        }
        constexpr void push_back(T &&value) noexcept(noexcept(emplace_back)) {
            emplace_back(::std::forward<T &&>(value));
        };
        template <class... Args> constexpr reference unchecked_emplace_back(Args &&...args) {
            iterator it = end();
            ::new ((void *)it) T(::std::forward<Args>(args)...);
            _end += 1;
            return *it;
        };
        // shove_back's (unchecked_push_back)
        constexpr void shove_back(const T &value) {
            unchecked_emplace_back(value);
        }
        constexpr void shove_back(T &&value) {
            unchecked_emplace_back(value);
        }

        // pop_back's
        constexpr void pop_back() {
            if (size()) [[likely]] {
                if constexpr (::std::is_trivially_destructible<element_type>::value) {
                    _end -= 1;
                } else {
                    _end -= 1;
                    end()->~T(); // destroy the tailing value
                }
            } else { // error?
                if constexpr (::inline_vector::details::error_handler ==
                              ::inline_vector::details::error_handling::_exception) {
                    throw std::domain_error("inline_vector cannot pop_back when empty");
                }
            }
        };

        // erase's
        constexpr iterator
        erase(const_iterator pos) noexcept(::std::is_nothrow_move_assignable_v<value_type>) {
            size_type erase_idx = pos - cbegin();

            assert(pos >= cbegin() && pos <= cend() &&
                   "erase iterator is out of bounds of the inline_vector");
            // move on top
            iterator first = begin() + erase_idx + 1;
            iterator last  = end();
            iterator dest  = begin() + erase_idx;
            for (; first != last; ++dest, (void)++first) {
                *dest = ::std::move(*first);
            }
            ::inline_vector::details::destroy_at(end() - 1);
            _end -= 1;
            return begin() + erase_idx;
        }

        constexpr void swap(inline_vector &other) noexcept {
            if (this == &other)
                return;

            pointer tmp_data = _data;
            pointer tmp_end  = _end;
            pointer tmp_cap  = _cap;

            _data = other._data;
            _end  = other._end;
            _cap  = other._cap;

            other._data = tmp_data;
            other._end  = tmp_end;
            other._cap  = tmp_cap;
        }

        //[]'s
        [[nodiscard]] constexpr reference operator[](size_type pos) {
            assert(pos < size());
            return _data[pos];
        };
        [[nodiscard]] constexpr const_reference operator[](size_type pos) const {
            assert(pos < size());
            return _data[pos];
        };

        // DANGER 
        [[nodiscard]] size_t unchecked_reserve(size_type n) {
            size_t c = _cap - _data;
            size_t m = std::max(c, n);
            _cap     = _data + m;
            return m;
        }

        constexpr void clear() noexcept {
            if constexpr (!::std::is_trivially_destructible<element_type>::value) {
                ::inline_vector::details::destroy(begin(), end());
            }
            _end = _data;
        };
    };
} // namespace inline_vector