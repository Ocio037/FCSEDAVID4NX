#pragma once
#include<MemAllocator.h>
#include<string>
template<typename Character, typename Traits = std::char_traits<Character>>
using DynamicString = std::basic_string<Character, Traits, Mem::Allocator<Character>>;
using String = DynamicString<char>;