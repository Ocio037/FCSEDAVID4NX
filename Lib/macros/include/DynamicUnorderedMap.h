#pragma once
#include<functional>
#include<unordered_map>
#include<MemAllocator.h>
template<typename Key, typename Value, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using DynamicUnorderedMap = std::unordered_map<Key, Value, Hash, KeyEqual, Mem::Allocator<std::pair<const Key, Value>>>;