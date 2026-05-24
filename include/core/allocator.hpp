#pragma once

#include <memory_resource>

using Allocator = std::pmr::unsynchronized_pool_resource;
