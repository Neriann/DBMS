#pragma once

#if DBMS_INDEX_TREE == DBMS_TREE_B
#include <b_tree.h>
template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey> >
using IndexTree = B_tree<tkey, tvalue, compare>;
#elif DBMS_INDEX_TREE == DBMS_TREE_BP
#include <b_plus_tree.h>
template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey> >
using IndexTree = BP_tree<tkey, tvalue, compare>;
#elif DBMS_INDEX_TREE == DBMS_TREE_BS
#include <b_star_tree.h>
template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey> >
using IndexTree = BS_tree<tkey, tvalue, compare>;
#else
#include <b_star_plus_tree.h>
template<typename tkey, typename tvalue, comparator<tkey> compare = std::less<tkey> >
using IndexTree = BSP_tree<tkey, tvalue, compare>;
#endif
