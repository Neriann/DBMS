#pragma once
#include <functional>

#if DBMS_INDEX_TREE == DBMS_TREE_B
#include <b_tree.h>
template<typename TKey, typename TValue, comparator<TKey> Compare = std::less<TKey> >
using IndexTree = B_tree<TKey, TValue, Compare>;
#elif DBMS_INDEX_TREE == DBMS_TREE_BP
#include <b_plus_tree.h>
template<typename TKey, typename TValue, comparator<TKey> Compare = std::less<TKey> >
using IndexTree = BP_tree<TKey, TValue, Compare>;
#elif DBMS_INDEX_TREE == DBMS_TREE_BS
#include <b_star_tree.h>
template<typename TKey, typename TValue, comparator<TKey> Compare = std::less<TKey> >
using IndexTree = BS_tree<TKey, TValue, Compare>;
#else
#include <b_star_plus_tree.h>
template<typename TKey, typename TValue, comparator<TKey> Compare = std::less<TKey> >
using IndexTree = BSP_tree<TKey, TValue, Compare>;
#endif
