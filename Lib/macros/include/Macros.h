#pragma once
template <bool Mov = false>
class NonCopy {
protected:
    NonCopy() = default;
    ~NonCopy() = default;
    NonCopy(const NonCopy&) = delete;
    NonCopy& operator=(const NonCopy&) = delete;
    NonCopy(NonCopy&&) noexcept = default;
    NonCopy& operator=(NonCopy&&) noexcept = default;
    NonCopy(NonCopy&&) requires (!Mov) = delete;
    NonCopy& operator=(NonCopy&&) requires (!Mov) = delete;
};