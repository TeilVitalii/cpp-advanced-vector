#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>


template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity)
    {}

    RawMemory(const RawMemory& other) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;
    
    RawMemory(RawMemory&& other) noexcept
        : buffer_(std::exchange(other.buffer_, nullptr))
        , capacity_(std::exchange(other.capacity_, 0))
    {}
    
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }

        return *this;
    }
    
    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const noexcept {
        return capacity_;
    }

private:
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new (n * sizeof(T))) : nullptr; 
    }

    static void Deallocate(T* buf) noexcept {
        operator delete (buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};


template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_ + 0;
    }

    iterator end() noexcept {
        return data_ + size_;
    }

    const_iterator begin() const noexcept {
        return data_ + 0;
    }
    const_iterator end() const noexcept {
        return data_ + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_ + 0;
    }
    const_iterator cend() const noexcept {
        return data_ + size_;
    }


    Vector() = default;
        
    explicit Vector(size_t size)
    : data_(size)
    , size_(size)
    {   
        std::uninitialized_value_construct_n(data_.GetAddress(), size);    
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress()); 
    }
    
    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0))
    {}
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {

            if (rhs.size_ > data_.Capacity()) {

                Vector rhs_copy(rhs);
                Swap(rhs_copy);

            } else {
                
                std::size_t min = std::min(size_ , rhs.size_);
                std::copy_n(rhs.data_.GetAddress(), min, data_.GetAddress());

                if (size_ >= rhs.size_) {
                    std::destroy_n(data_ + rhs.size_, size_ - rhs.size_);
                
                } else {
                    std::uninitialized_copy_n(rhs.data_ + size_, rhs.size_ - size_, data_ + size_);
                }
            }

            size_ = rhs.size_;
        }

        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = std::exchange(rhs.size_, 0);
        }

        return *this;
    }

    void Swap(Vector& other) noexcept {
        if (this != &other) {
            data_.Swap(other.data_);
            std::swap(size_, other.size_);
        }
    }
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    T& operator[](size_t index) noexcept {
        return data_[index];
    }
    
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size <= size_) {
            std::destroy_n(data_ + new_size, size_ - new_size);
        } else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }

        size_ = new_size;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == Capacity()) {

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data + size_) T(std::forward<Args>(args)...);
        

            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

        } else {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }

        return data_[size_++];
    }
    
    void PushBack(const T& value) {
        EmplaceBack(value);
    }
    
    void PushBack(T&& value) {
        EmplaceBack(std::move(value));
    }  

    void PopBack() {
        assert(size_);
        std::destroy_at(data_ + size_ - 1);
        --size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= cbegin() && pos <= cend());    
        
        std::size_t offset = pos - cbegin();
 
        if (size_ == Capacity()) {
            
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            new (new_data.GetAddress() + offset) T(std::forward<Args>(args)...);
 
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), offset, new_data.GetAddress());
                std::uninitialized_move_n(data_.GetAddress() + offset, size_ - offset, 
                                          new_data.GetAddress() + offset + 1);
            
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), offset, new_data.GetAddress());
                std::uninitialized_copy_n(data_.GetAddress() + offset, size_ - offset, 
                                          new_data.GetAddress() + offset + 1);
            }
 
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
            
        } else {
            
            try {
                
                if (pos != cend()) {
                    
                    T temp(std::forward<Args>(args)...);                   
                    new (end()) T(std::forward<T>(data_[size_ - 1]));
                    
                    std::move_backward(begin() + offset, end() - 1, end());                 
                    *(begin() + offset) = std::forward<T>(temp);
                    
                } else {
                    new (end()) T(std::forward<Args>(args)...);
                }
                
            } catch (...) {
                operator delete (end());
                throw;
            }
        }
        
        ++size_;

        return begin() + offset;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    iterator Erase(const_iterator pos) {
        assert(pos >= cbegin() && pos <= cend());

        std::size_t offset = pos - cbegin();

        std::move(begin() + offset + 1, end(), begin() + offset);
        std::destroy_at(cend() - 1);
        --size_;

        return begin() + offset;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};