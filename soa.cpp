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
    /*!
     * Helper struct to map template argument index to the corresponding type.
     */
    template <std::size_t i, typename T, typename... Ts>
    struct IndexToType : IndexToType<i-1, Ts...>
    {
        static_assert(i < sizeof...(Ts) + 1, "index out of bounds");
    };

    template <typename T, typename... Ts>
    struct IndexToType<0, T, Ts...> { T value; };

    /*!
     * The value type of the N'th template argument.
     */
    template <std::size_t i>
    using value_type = decltype(IndexToType<i, Types...>::value);

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
                    defaultInitializeElements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types)),
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
                    copyElements<Types>(
                        other.array_ptrs_[type_index],
                        other.array_ptrs_[type_index] + other.size_ * sizeof(Types),
                        this->array_ptrs_[type_index]),
                    ++type_index
                ),
                ...
            );
        }
        size_ = other.size_;
    }

    SOAVector(SOAVector && other):
        array_ptrs_(other.array_ptrs_),
        size_(other.size_),
        capacity_(other.capacity_)
    {
        std::fill(other.array_ptrs_.begin(), other.array_ptrs_.end(), nullptr);
        other.size_ = 0;
        other.capacity_ = 0;
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
                    deleteElements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
        delete[] array_ptrs_[0];
    }

    SOAVector<Types...> & operator=(SOAVector<Types...> const & other)
    {
        if (&other == this)
        {
            // Avoid self assignment.
            return *this;
        }

        this->clear();
        this->reserve(other.size());

        if (other.size() > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    copyElements<Types>(
                        other.array_ptrs_[type_index],
                        other.array_ptrs_[type_index] + other.size_ * sizeof(Types),
                        this->array_ptrs_[type_index]),
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
        if (&other == this)
        {
            // Avoid self assignment.
            return *this;
        }

        this->clear();

        if (capacity_ > 0)
        {
            delete[] array_ptrs_[0];
        }

        array_ptrs_ = other.array_ptrs_;
        size_ = other.size_;
        capacity_ = other.capacity_;

        std::fill(other.array_ptrs_.begin(), other.array_ptrs_.end(), nullptr);
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
    value_type<TypeIndex> * data() noexcept
    {
        return reinterpret_cast<value_type<TypeIndex> *>(array_ptrs_[TypeIndex]);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> const * data() const noexcept
    {
        return reinterpret_cast<value_type<TypeIndex> const *>(array_ptrs_[TypeIndex]);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> & get(size_type index) noexcept
    {
        assert(index < size_);
        return *(this->data<TypeIndex>() + index);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> const & get(size_type index) const noexcept
    {
        assert(index < size_);
        return *(this->data<TypeIndex>() + index);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> & front() noexcept
    {
        return this->get<TypeIndex>(0);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> const & front() const noexcept
    {
        return this->get<TypeIndex>(0);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> & back() noexcept
    {
        return this->get<TypeIndex>(this->size_ - 1);
    }

    template<size_type TypeIndex>
    value_type<TypeIndex> const & back() const noexcept
    {
        return this->get<TypeIndex>(this->size_ - 1);
    }


    template<size_type TypeIndex>
    std::span<value_type<TypeIndex> const> span() const noexcept
    {
        return std::span<value_type<TypeIndex> const>(this->data<TypeIndex>(), this->size_);
    }

    template<size_type TypeIndex>
    std::span<value_type<TypeIndex>> span() noexcept
    {
        return std::span<value_type<TypeIndex>>(this->data<TypeIndex>(), this->size_);
    }

    void clear()
    {
        if (size_ > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    deleteElements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types)),
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
                createElement(array_ptrs_[type_index] + size_ * sizeof(Types), std::forward<decltype(args)>(args)),
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
                deleteElement<Types>(array_ptrs_[type_index] + (size_ - 1) * sizeof(Types)),
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
    /*!
     * Create a range of default initialized elements in existing memory allocation.
     *
     * @param first The pointer to where the first element should be created.
     * @param last The pointer to where one past the last last element should
     *      be created.
     */
    template<typename T>
    static void defaultInitializeElements(char * const first, char * const last)
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
        reinterpret_cast<T *>(ptr)->~T();
    }

    template<typename T>
    static void deleteElements(char * const first, char * const last)
    {
        for (char * it = first; it != last; it += sizeof(T))
        {
            reinterpret_cast<T *>(it)->~T();
        }
    }

    template<typename T>
    static void copyElements(char const * const first, char const * const last, char * dst_first)
    {
        for (char const * it = first; it != last; it += sizeof(T), dst_first += sizeof(T))
        {
            T const * const original_obj_ptr = reinterpret_cast<T const *>(it);
            // Create a new object in destination as a copy of original.
            new(dst_first) T(*original_obj_ptr);
        }
    }

    template<typename T>
    static void moveElements(char * const first, char * const last, char * dst_first)
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
        char * const new_data_ptr = new (std::align_val_t(alignof(value_type<0>))) char[total_num_bytes];

        std::array<char *, sizeof...(Types)> new_array_ptrs{};
        for (std::size_t array_index = 0; array_index < new_array_ptrs.size(); ++array_index)
        {
            new_array_ptrs[array_index] = new_data_ptr + new_offsets[array_index];
        }

        if (size_ > 0)
        {
            // Move existing objects to the new memory allocation.
            std::size_t type_index = 0;
            (
                (
                    moveElements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types),
                        new_array_ptrs[type_index]),
                    ++type_index
                ),
                ...
            );
        }

        if (capacity_ > 0)
        {
            delete[] array_ptrs_[0];
        }
        array_ptrs_ = std::move(new_array_ptrs);
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
    std::array<char *, sizeof...(Types)> array_ptrs_;
    size_type size_;
    size_type capacity_;
    static constexpr float growth_factor = 1.5;
};

int main()
{
    using VecType = SOAVector<int16_t, std::string, double>;
    VecType vec;
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

    VecType copy(vec);
    VecType new_vec(std::move(vec));

    VecType vec2;
    vec.push_back(5, "five", 5.67);
    new_vec = vec2;

    vec2 = std::move(vec);
}
