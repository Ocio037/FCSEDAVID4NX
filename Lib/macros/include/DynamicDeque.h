#pragma once
#include<deque>
#include<MemAllocator.h>
template<typename T>
using DynamicDeque = std::deque<T, Mem::Allocator<T>>;