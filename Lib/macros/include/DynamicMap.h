#pragma once
#include<functional>
#include<map>
#include<MemAllocator.h>
template<typename Key, typename Value, typename Compare = std::less<Key>>
using DynamicMap = std::map<Key, Value, Compare, Mem::Allocator<std::pair<const Key, Value>>>;