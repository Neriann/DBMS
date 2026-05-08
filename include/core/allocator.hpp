#pragma once

#if DBMS_ALLOCATOR == DBMS_ALLOC_GLOBAL_HEAP
#include <allocator_global_heap.h>
using Allocator = allocator_global_heap;
#elif DBMS_ALLOCATOR == DBMS_ALLOC_BOUNDARY_TAGS
#include <allocator_boundary_tags.h>
using Allocator = allocator_boundary_tags;
#elif DBMS_ALLOCATOR == DBMS_ALLOC_BUDDIES
#include <allocator_buddies_system.h>
using Allocator = allocator_buddies_system;
#elif DBMS_ALLOCATOR == DBMS_ALLOC_SORTED_LIST
#include <allocator_sorted_list.h>
using Allocator = allocator_sorted_list;
#else
#include <allocator_red_black_tree.h>
using Allocator = allocator_red_black_tree;
#endif
