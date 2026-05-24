#pragma once
#include <functional>

#include <b_star_plus_tree.hpp>

template<typename TKey, typename TValue, comparator<TKey> Compare = std::less<TKey> >
using IndexTree = BSP_tree<TKey, TValue, Compare>;
