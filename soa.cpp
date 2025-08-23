#include <array>
#include <cassert>
#include <cstddef>
#include <span>
#include <new>

#include <string>
#include <iostream>

template <typename... Types>
class SOAVector
{
public:
    template <std::size_t i, typename T, typename... Ts>
    struct IndexToType : IndexToType<i-1, Ts...>
    {
        static_assert(i < sizeof...(Ts) + 1, "index out of bounds");
    };

    template <typename T, typename... Ts>
    struct IndexToType<0, T, Ts...> { T value; };

    using size_type = std::size_t;

    SOAVector() = default;

    SOAVector(size_type size)
    {
        this->reserve(size);
        size_ = size;

        if (size_ > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    defaultInitializeData<Types>(
                        data_ + data_offsets_[type_index],
                        data_ + data_offsets_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
    }

    SOAVector(SOAVector const & other)
    {
        this->reserve(other.size());

        if (other.size() > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    copyData<Types>(
                        other.data_ + other.data_offsets_[type_index],
                        other.data_ + other.data_offsets_[type_index] + other.size_ * sizeof(Types),
                        this->data_ + this->data_offsets_[type_index]),
                    ++type_index
                ),
                ...
            );
        }
        size_ = other.size_;
    }

    SOAVector(SOAVector && other):
        data_(other.data_),
        data_offsets_(other.data_offsets_),
        capacity_(other.capacity_),
        size_(other.size_)
    {
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    ~SOAVector()
    {
        if (capacity_ == 0)
        {
            return;
        }
        if (size_ > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    deleteData<Types>(
                        data_ + data_offsets_[type_index],
                        data_ + data_offsets_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
        delete[] data_;
    }

    SOAVector<Types...> & operator=(SOAVector<Types...> const & other)
    {
        this->clear();

        this->reserve(other.size());

        if (other.size() > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    copyData<Types>(
                        other.data_ + other.data_offsets_[type_index],
                        other.data_ + other.data_offsets_[type_index] + other.size_ * sizeof(Types),
                        this->data_ + this->data_offsets_[type_index]),
                    ++type_index
                ),
                ...
            );
        }
        size_ = other.size_;

        return *this;
    }

    SOAVector<Types...> & operator=(SOAVector<Types...> && other)
    {
        this->clear();

        if (capacity_ > 0)
        {
            delete[] data_;
        }

        data_ = other.data_;
        data_offsets_ = other.data_offsets_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
    }

    bool empty() const noexcept
    {
        return this->size_ > 0;
    }

    size_type size() const noexcept
    {
        return this->size_;
    }

    size_type capacity() const noexcept
    {
        return capacity_;
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) * data() noexcept
    {
        using ReturnType = decltype(IndexToType<TypeIndex, Types...>::value);
        char * obj_ptr = data_ + data_offsets_[TypeIndex];
        return reinterpret_cast<ReturnType *>(obj_ptr);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) const * data() const noexcept
    {
        using ReturnType = decltype(IndexToType<TypeIndex, Types...>::value);
        char const * obj_ptr = data_ + data_offsets_[TypeIndex];
        return reinterpret_cast<ReturnType const *>(obj_ptr);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) & get(size_type index) noexcept
    {
        assert(index < size_);
        return *(this->data<TypeIndex>() + index);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) const & get(size_type index) const noexcept
    {
        assert(index < size_);
        return *(this->data<TypeIndex>() + index);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) & front() noexcept
    {
        return this->get<TypeIndex>(0);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) const & front() const noexcept
    {
        return this->get<TypeIndex>(0);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) & back() noexcept
    {
        return this->get<TypeIndex>(this->size_ - 1);
    }

    template<size_type TypeIndex>
    decltype(IndexToType<TypeIndex, Types...>::value) const & back() const noexcept
    {
        return this->get<TypeIndex>(this->size_ - 1);
    }


    template<size_type TypeIndex>
    std::span<decltype(IndexToType<TypeIndex, Types...>::value) const> span() const noexcept
    {
        using ReturnType = decltype(IndexToType<TypeIndex, Types...>::value);
        return std::span<ReturnType const>(this->data<TypeIndex>(), this->size_);
    }

    template<size_type TypeIndex>
    std::span<decltype(IndexToType<TypeIndex, Types...>::value)> span() noexcept
    {
        using ReturnType = decltype(IndexToType<TypeIndex, Types...>::value);
        return std::span<ReturnType>(this->data<TypeIndex>(), this->size_);
    }

    void clear()
    {
        if (size_ > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    deleteData<Types>(
                        data_ + data_offsets_[type_index],
                        data_ + data_offsets_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
        size_ = 0;
    }

    void push_back(Types&&... args)
    {
        if (size_ + 1 > capacity_)
        {
            this->reserve(this->size_ * growth_factor + 1);
        }

        std::size_t type_index = 0;
        (
            (
                createElement(data_ + data_offsets_[type_index] + size_ * sizeof(Types), std::forward<decltype(args)>(args)),
                ++type_index
            ),
            ...
        );
        ++size_;
    }

    void pop_back()
    {
        assert(size_ > 0);

        std::size_t type_index = 0;
        (
            (
                deleteElement<Types>(data_ + data_offsets_[type_index] + (size_ - 1) * sizeof(Types)),
                ++type_index
            ),
            ...
        );
        --size_;
    }

    void reserve(size_type new_capacity)
    {
        if (new_capacity > capacity_)
        {
            reallocate(new_capacity);
        }
    }

    void shrink_to_fit()
    {
        if (capacity_ > size_)
        {
            reallocate(size_);
        }
    }

private:
    template<typename T>
    static void defaultInitializeData(char * const first, char * const last)
    {
        for (char * it = first; it != last; it += sizeof(T))
        {
            // Create a new default object in destination
            new(it) T();
        }
    }

    template<typename T>
    static void createElement(char * const ptr, T&& value)
    {
        new(ptr) T(std::forward<T>(value));
    }

    template<typename T>
    static void deleteElement(char * const ptr)
    {
        T * const original_obj_ptr = reinterpret_cast<T *>(ptr);
        original_obj_ptr->~T();
    }

    template<typename T>
    static void deleteData(char * const first, char * const last)
    {
        for (char * it = first; it != last; it += sizeof(T))
        {
            T * const original_obj_ptr = reinterpret_cast<T *>(it);
            // Delete the previous object
            original_obj_ptr->~T();
        }
    }

    template<typename T>
    static void copyData(char const * const first, char const * const last, char * dst_first)
    {
        for (char const * it = first; it != last; it += sizeof(T), dst_first += sizeof(T))
        {
            T const * const original_obj_ptr = reinterpret_cast<T *>(it);
            // Create a new object in destination as a copy of original.
            new(dst_first) T(*original_obj_ptr);
        }
    }

    template<typename T>
    static void moveData(char * const first, char * const last, char * dst_first)
    {
        for (char * it = first; it != last; it += sizeof(T), dst_first += sizeof(T))
        {
            T * const original_obj_ptr = reinterpret_cast<T *>(it);
            // Create a new object in destination, possibly by invoking move constructor.
            new(dst_first) T(std::move(*original_obj_ptr));
            // Delete the previous object
            original_obj_ptr->~T();
        }
    }

    void reallocate(size_type new_capacity)
    {
        assert(new_capacity >= size_);

        // Get the byte offsets for each array as well as the total number of bytes needed.
        auto const [new_offsets, total_num_bytes] = getArrayOffsetsAndAllocationSize(new_capacity);

        // Allocate memory aligned to the first type.
        using FirstType = decltype(IndexToType<0, Types...>::value);
        char * const new_data = new (std::align_val_t(alignof(FirstType))) char[total_num_bytes];

        if (size_ > 0)
        {
            // Move existing objects to the new memory allocation.
            std::size_t type_index = 0;
            (
                (
                    moveData<Types>(
                        data_ + data_offsets_[type_index],
                        data_ + data_offsets_[type_index] + size_ * sizeof(Types),
                        new_data + new_offsets[type_index]),
                    ++type_index
                ),
                ...
            );
        }

        if (capacity_ > 0)
        {
            delete[] data_;
        }
        data_ = new_data;
        data_offsets_ = std::move(new_offsets);
        capacity_ = new_capacity;
    }

    constexpr std::pair<std::array<ptrdiff_t, sizeof...(Types)>, size_t>
    getArrayOffsetsAndAllocationSize(size_type element_count)
    {
        // Get the byte sizes and alignments of the types.
        const auto sizes = std::array{(sizeof(Types) * element_count)...};
        const auto alignments = std::array{(alignof(Types))...};

        // Calculate the offsets of each array from the 
        std::array<ptrdiff_t, sizeof...(Types)> offsets{};
        ptrdiff_t p = alignments[0];
        for (size_type i = 1; i < sizeof...(Types); ++i)
        {
            p = (p + alignments[i] - 1) / alignments[i] * alignments[i];
            offsets[i] = p;
            p += sizes[i];
        }

        return {offsets, p};
    }

    // Member variables:
    char * data_ = nullptr;
    std::array<ptrdiff_t, sizeof...(Types)> data_offsets_;
    size_type size_ = 0;
    size_type capacity_ = 0;
    static constexpr float growth_factor = 1.5;
};

int main()
{
    SOAVector<int16_t, std::string, double> vec;
    vec.push_back(0, "zero", 1.23);
    vec.push_back(1, "one", 2.34);
    vec.push_back(2, "two", 3.45);
    vec.pop_back();
    //vec.push_back(4, "four", 4.56);
    std::cout << "vec.size() = " << vec.size() << "\n";
    for (std::size_t i = 0; i < vec.size(); ++i)
    {
        std::cout << "i=" << i << ": " << vec.get<0>(i) << ", " << vec.get<1>(i) << ", " << vec.get<2>(i) << "\n";
    }
}
