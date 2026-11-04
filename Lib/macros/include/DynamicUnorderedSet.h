#pragma once
#include<functional>
#include<unordered_set>
#include<MemAllocator.h>
template<typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using DynamicUnorderedSet = std::unordered_set<Key, Hash, KeyEqual, Mem::Allocator<Key>>;