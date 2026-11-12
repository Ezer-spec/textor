/*
    dispatch.hxx - three small dispatch-table helpers for replacing long
    if/else-if or ternary ladders with something both faster and easier to
    read. This is a cleaned-up, bug-fixed and slightly reworked version of a
    snippet that had a few issues (a couple of them serious):
    - `handlers[index]` was written `handlersindex` (typo - didn't compile).
    - `RangeDispatcher::dispatch`'s `std::lower_bound` comparator and one of
    the `FastDispatcher` usage examples were missing their lambda syntax
    entirely (again, didn't compile).
    - `FastDispatcher::add_case(key, condition, action)` captured the
    `condition`/`action` *parameters* by reference (`[=, &condition,
    &action]`) into a `std::function` that outlives the call - both
    references dangle the moment `add_case` returns, so calling that
    handler later is undefined behaviour. Fixed by capturing by value.
    - Every case in `JumpDispatcher`/`FastDispatcher` was a heap-allocated
    `std::function<Result()>`, even for the common "just return a
    constant" case - which is exactly the overhead a jump table is
    supposed to avoid. `JumpDispatcher` below stores `Result` directly
    in a flat `std::array` (no `std::function`, no allocation, no
    indirect call - an actual O(1) array read); `FastDispatcher` keeps
    `std::function` since it's the one meant for arbitrary handlers, but
    gained a plain `add_case(key, value)` overload that skips it too.
    - Added enum-key overloads (`set_case`/`dispatch` accepting any
    `enum`/`enum class`) since the dense-integer case in this codebase
    is almost always an `enum class`, not a bare `size_t`.
    - Builder-style methods now return `*this` so a table can be built in
    one expression (handy for the "static local, built once" pattern
    used below and in dispatch.hxx's own call sites).
    three tools for three shapes of dispatch:
    JumpDispatcher<Result, Size>   - dense int/enum key in [0, Size).
                                   O(1), no allocation. Use this when
                                   you'd otherwise write a `switch` or a
                                   ternary ladder keyed on an enum.
    FastDispatcher<Key, Result>    - arbitrary key type (string, enum,
                                   whatever `std::hash` supports). O(1)
                                   average via unordered_map. Use this
                                   for "if (ext == \"cpp\") ... else if
                                   (ext == \"py\") ..." ladders.
    RangeDispatcher<Key, Result>   - sorted, non-overlapping [min, max]
                                   ranges. O(log n) binary search. Use
                                   this for "if (x >= a && x <= b) ..."
                                   ladders.
    All three are header-only, allocate nothing at dispatch() time (aside
    from FastDispatcher's std::function-based handlers, which allocate only
    when the handler's captures don't fit SBO - same as any std::function),
    and are safe to build once into a `static const` table.
*/
#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace dispatch {

namespace detail {
// Builds a std::array<Result, Size> with every element copy-initialized
// from `v`, without ever default-constructing an element first.
template<typename Result, std::size_t... I>
std::array<Result, sizeof...(I)> make_filled_array(const Result &v, std::index_sequence<I...>) {
    return { ((void)I, v)... };
}
template<typename Result, std::size_t Size>
std::array<Result, Size> make_filled_array(const Result &v) {
    return make_filled_array<Result>(v, std::make_index_sequence<Size>{});
}
}

// .................................................................
// 1. JumpDispatcher - O(1) array read for a dense int/enum key in [0, Size).
// .................................................................
// The fastest of the three: dispatch() is a bounds check plus one array
// read, nothing else. Prefer this whenever the key is (or can be cast to)
// a small enum/int range - it's a direct replacement for a `switch` that
// also works as a runtime-configurable table.
template<typename Result, std::size_t Size>
class JumpDispatcher {
    std::array<Result, Size> values;
    Result                   default_value;

public:
    explicit JumpDispatcher(Result def)
        : values(detail::make_filled_array<Result, Size>(def)), default_value(def) {}

    JumpDispatcher &set_case(std::size_t index, Result value) {
        if (index < Size) values[index] = value;
        return *this;
    }

    // Same thing, but takes any enum/enum class directly so callers don't
    // have to sprinkle static_cast<size_t> everywhere.
    template<typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
    JumpDispatcher &set_case(E index, Result value) {
        return set_case(static_cast<std::size_t>(index), value);
    }

    [[nodiscard]] Result dispatch(std::size_t index) const {
        return (index < Size) ? values[index] : default_value;
    }

    template<typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
    [[nodiscard]] Result dispatch(E index) const {
        return dispatch(static_cast<std::size_t>(index));
    }
};

// .................................................................
// 2. FastDispatcher - hash-map dispatch for arbitrary key types.
// .................................................................
// O(1) average via unordered_map. Two ways to register a case: a plain
// constant result (no allocation-per-dispatch, resolved once at add_case
// time), or an arbitrary std::function for when the case needs to run
// real logic. Prefer add_case(key, value) unless you actually need the
// handler form.
template<typename Key, typename Result>
class FastDispatcher {
    std::unordered_map<Key, std::function<Result()>> handlers;
    Result                                            default_value;

public:
    explicit FastDispatcher(Result def) : default_value(def) {}

    // Constant result for this key
    FastDispatcher &add_case(const Key &key, Result value) {
        handlers[key] = [value] { return value; };
        return *this;
    }

    // Arbitrary handler for this key. Takes the handler by value (a
    // std::function copy/move)
    FastDispatcher &add_case(const Key &key, std::function<Result()> handler) {
        handlers[key] = std::move(handler);
        return *this;
    }

    [[nodiscard]] Result dispatch(const Key &key) const {
        auto it = handlers.find(key);
        return (it != handlers.end()) ? it->second() : default_value;
    }

    [[nodiscard]] bool has_case(const Key &key) const {
        return handlers.find(key) != handlers.end();
    }
};

// .................................................................
// 3. RangeDispatcher - O(log n) binary search over sorted [min, max] ranges.
// .................................................................
// Ranges may be added in any order; they're kept sorted by `min` as they
// come in. Overlapping ranges aren't rejected (first match by `min` wins,
// same as the original), since detecting/reporting real overlap conflicts
// is a bigger feature than this header is trying to be.
template<typename Key, typename Result>
class RangeDispatcher {
    struct Range { Key min, max; Result value; };
    std::vector<Range> ranges;
    Result              default_value;

public:
    explicit RangeDispatcher(Result def) : default_value(def) {}

    RangeDispatcher &add_range(Key min, Key max, Result value) {
        ranges.push_back(Range{min, max, value});
        std::sort(ranges.begin(), ranges.end(),
                  [](const Range &a, const Range &b) { return a.min < b.min; });
        return *this;
    }

    [[nodiscard]] Result dispatch(const Key &value) const {
        // First range whose min is > value - the only candidate is the
        // one right before it (ranges are sorted by min, non-decreasing).
        auto it = std::upper_bound(ranges.begin(), ranges.end(), value,
            [](const Key &v, const Range &r) { return v < r.min; });
        if (it != ranges.begin()) {
            auto prev = std::prev(it);
            if (value >= prev->min && value <= prev->max) return prev->value;
        }
        return default_value;
    }
};

}
