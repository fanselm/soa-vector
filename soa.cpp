#include <array>
#include <cassert>
#include <cstddef>
#include <new>
#include <span>

#include <iostream>
#include <string>

/*!
 * Implementation of dynamic "Struct Of Arrays" vector with a single memory allocation.
 */
template <typename... Types>
class SOAVector
{
    /*!
     * Helper struct to map template argument index to the corresponding type.
     */
    template <std::size_t i, typename T, typename... Ts>
    struct IndexToType : IndexToType<i-1, Ts...>
    {
        static_assert(i < sizeof...(Ts) + 1, "Index out of bounds");
    };

    template <typename T, typename... Ts>
    struct IndexToType<0, T, Ts...> { T value; };
public:
    /*!
     * The value type of the N'th template argument.
     */
    template <std::size_t i>
    using value_type = decltype(IndexToType<i, Types...>::value);

    using size_type = std::size_t;

    /*!
     * Default constructor.
     *
     * Creates an empty SOA vector.
     */
    SOAVector() = default;

    /*!
     * Create a SOA vector with a given size, default initializing all elements.
     *
     * @param size The number of elements.
     */
    SOAVector(size_type size)
    {
        this->reserve(size);
        size_ = size;

        if (size_ > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    create_default_elements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
    }

    /*!
     * Copy constructor.
     */
    SOAVector(SOAVector const & other)
    {
        this->reserve(other.size());

        if (other.size() > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    copy_elements<Types>(
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

    /*!
     * Move constructor.
     */
    SOAVector(SOAVector && other):
        array_ptrs_(other.array_ptrs_),
        size_(other.size_),
        capacity_(other.capacity_)
    {
        std::fill(other.array_ptrs_.begin(), other.array_ptrs_.end(), nullptr);
        other.size_ = 0;
        other.capacity_ = 0;
    }

    /*!
     * Destructor.
     */
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
                    delete_elements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
        delete[] array_ptrs_[0];
    }

    /*!
     * Copy assignment operator.
     */
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
                    copy_elements<Types>(
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

    /*!
     * Move assignment operator.
     */
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

        return *this;
    }

    /*!
     * Get whether the vector is empty.
     *
     * @return True if the vector is empty, otherwise false.
     */
    bool empty() const noexcept
    {
        return this->size_ > 0;
    }

    /*!
     * Get the size, i.e. number of elements in the vector.
     *
     * @return The number of elements.
     */
    size_type size() const noexcept
    {
        return this->size_;
    }

    /*!
     * Get the capacity, i.e. the number of elements the current memory
     * allocation can fit.
     *
     * @return The capacity.
     */
    size_type capacity() const noexcept
    {
        return capacity_;
    }

    /*!
     * Get the pointer to the first element of an array.
     *
     * @tparam TypeIndex The index of the array to get the pointer of.
     * @return The pointer to the first element in the array.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> * data() noexcept
    {
        return reinterpret_cast<value_type<TypeIndex> *>(array_ptrs_[TypeIndex]);
    }

    /*!
     * Get the pointer to the first element of an array.
     *
     * @tparam TypeIndex The index of the array to get the pointer of.
     * @return The pointer to the first element in the array.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> const * data() const noexcept
    {
        return reinterpret_cast<value_type<TypeIndex> const *>(array_ptrs_[TypeIndex]);
    }

    /*!
     * Get a reference to the element of a certain array at a given index.
     *
     * @tparam TypeIndex The index of the array.
     * @param index The index.
     * @return A reference to the element at the position.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> & get(size_type index) noexcept
    {
        assert(index < size_);
        return *(this->data<TypeIndex>() + index);
    }

    /*!
     * Get a reference to the element of a certain array at a given index.
     *
     * @tparam TypeIndex The index of the array.
     * @param index The index.
     * @return A reference to the element at the position.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> const & get(size_type index) const noexcept
    {
        assert(index < size_);
        return *(this->data<TypeIndex>() + index);
    }

    /*!
     * Get a reference to the first element of a certain array.
     *
     * @tparam TypeIndex The index of the array.
     * @return A reference to the first element in the array.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> & front() noexcept
    {
        return this->get<TypeIndex>(0);
    }

    /*!
     * Get a reference to the first element of a certain array.
     *
     * @tparam TypeIndex The index of the array.
     * @return A reference to the first element in the array.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> const & front() const noexcept
    {
        return this->get<TypeIndex>(0);
    }

    /*!
     * Get a reference to the last element of a certain array.
     *
     * @tparam TypeIndex The index of the array.
     * @return A reference to the last element in the array.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> & back() noexcept
    {
        return this->get<TypeIndex>(this->size_ - 1);
    }

    /*!
     * Get a reference to the last element of a certain array.
     *
     * @tparam TypeIndex The index of the array.
     * @return A reference to the last element in the array.
     */
    template<size_type TypeIndex>
    value_type<TypeIndex> const & back() const noexcept
    {
        return this->get<TypeIndex>(this->size_ - 1);
    }

    /*!
     * Get a span over the elements in a certain array.
     *
     * @tparam TypeIndex The index of the array.
     * @return A span over the elements in the array.
     */
    template<size_type TypeIndex>
    std::span<value_type<TypeIndex> const> span() const noexcept
    {
        return std::span<value_type<TypeIndex> const>(this->data<TypeIndex>(), this->size_);
    }

    /*!
     * Get a span over the elements in a certain array.
     *
     * @tparam TypeIndex The index of the array.
     * @return A span over the elements in the array.
     */
    template<size_type TypeIndex>
    std::span<value_type<TypeIndex>> span() noexcept
    {
        return std::span<value_type<TypeIndex>>(this->data<TypeIndex>(), this->size_);
    }

    /*!
     * Clears the contents, deleting all elements of all arrays.
     *
     * Calling 'clear()' does not change the memory allocation and change the
     * capacity.
     */
    void clear()
    {
        if (size_ > 0)
        {
            std::size_t type_index = 0;
            (
                (
                    delete_elements<Types>(
                        array_ptrs_[type_index],
                        array_ptrs_[type_index] + size_ * sizeof(Types)),
                    ++type_index
                ),
                ...
            );
        }
        size_ = 0;
    }

    /*!
     * Adds a set of elements at the end of each array.
     *
     * @param args The elements to add.
     */
    void push_back(Types&&... args)
    {
        // Grow the capacity if we have to.
        if (size_ + 1 > capacity_)
        {
            this->reserve(this->size_ * growth_factor + 1);
        }

        std::size_t type_index = 0;
        (
            (
                create_element(array_ptrs_[type_index] + size_ * sizeof(Types), std::forward<decltype(args)>(args)),
                ++type_index
            ),
            ...
        );
        ++size_;
    }

    /*!
     * Remove the last element of each array.
     */
    void pop_back()
    {
        assert(size_ > 0);

        std::size_t type_index = 0;
        (
            (
                delete_element<Types>(array_ptrs_[type_index] + (size_ - 1) * sizeof(Types)),
                ++type_index
            ),
            ...
        );
        --size_;
    }

    /*!
     * Reserve storage.
     *
     * @param new_capacity The new capacity of the arrays. Will only
     *      reallocate if 'new_capacity' is larger than the current capacity.
     */
    void reserve(size_type new_capacity)
    {
        if (new_capacity > capacity_)
        {
            reallocate(new_capacity);
        }
    }

    /*!
     * Reduce the memory allocation to fit only the current size.
     */
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
    static void create_default_elements(char * const first, char * const last)
    {
        for (char * it = first; it != last; it += sizeof(T))
        {
            // Create a new default object in destination
            new(it) T();
        }
    }

    /*!
     * Create an element with a specific value in existing memory allocation.
     *
     * @param ptr The pointer to where the element should be created.
     * @param value The value of the element.
     */
    template<typename T>
    static void create_element(char * const ptr, T&& value)
    {
        new(ptr) T(std::forward<T>(value));
    }

    /*!
     * Delete an element at a specific location in existing memory allocation.
     *
     * @param ptr The pointer to the element.
     */
    template<typename T>
    static void delete_element(char * const ptr)
    {
        reinterpret_cast<T *>(ptr)->~T();
    }

    /*!
     * Call destructor on a range of elements.
     *
     * @param first The pointer to the first element to delete.
     * @param last The pointer to one past the last element to delete.
     */
    template<typename T>
    static void delete_elements(char * const first, char * const last)
    {
        for (char * it = first; it != last; it += sizeof(T))
        {
            reinterpret_cast<T *>(it)->~T();
        }
    }

    /*!
     * Copy a range of elements to a new location in an existing memory allocation.
     *
     * @param first A pointer to the first element to copy.
     * @param last A pointer to one past the last element to copy.
     * @param dst_first A pointer to the first element in the new location.
     */
    template<typename T>
    static void copy_elements(char const * const first, char const * const last, char * dst_first)
    {
        for (char const * it = first; it != last; it += sizeof(T), dst_first += sizeof(T))
        {
            T const * const original_obj_ptr = reinterpret_cast<T const *>(it);
            // Create a new object in destination as a copy of original.
            new(dst_first) T(*original_obj_ptr);
        }
    }

    /*!
     * Move a range of elements to a new location in an existing memory allocation.
     *
     * @param first A pointer to the first element to move.
     * @param last A pointer to one past the last element to move.
     * @param dst_first A pointer to the first element in the new location.
     */
    template<typename T>
    static void move_elements(char * const first, char * const last, char * dst_first)
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

    /*!
     * Reallocate the data to a new memory allocation of a specific capacity.
     *
     * This will allocate a new chunk of memory, move the elements there,
     * destruct the original elements and free the original memory allocation.
     *
     * @param new_capacity The capacity of the new memory allocation.
     */
    void reallocate(size_type new_capacity)
    {
        assert(new_capacity >= size_);

        // Get the byte offsets for each array as well as the total number of bytes needed.
        auto const [new_offsets, total_num_bytes] = calculate_array_offsets_and_allocation_size(new_capacity);

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
                    move_elements<Types>(
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

    /*!
     * Calculate the pointer offsets of each array and the total required memory
     * allocation size for a specific size of the vector.
     *
     * @param element_count The number of elements to get the size and
     *      offsets for.
     * @return An array with the pointer offsets of each array and the
     *      required memory allocation size.
     */
    constexpr std::pair<std::array<ptrdiff_t, sizeof...(Types)>, size_t>
    calculate_array_offsets_and_allocation_size(size_type element_count)
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
