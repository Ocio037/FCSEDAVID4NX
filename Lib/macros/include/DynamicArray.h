#pragma once
#include<MemAllocator.h>
#include<vector>
template<typename T>
using DynamicArray = std::vector<T, Mem::Allocator<T>>;