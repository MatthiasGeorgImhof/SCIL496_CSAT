#pragma once

#include <array>
#include <type_traits>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <algorithm>

/**
 * @brief A container that stores elements in a fixed-size array, 
 *        keeping track of which slots are in use with a bitset.
 *
 * @tparam CONTENT The type of the elements to store.
 * @tparam N The size of the fixed-size array (must be 8, 16, 32, or 64).
 */
template<typename CONTENT, uint8_t N>
class BoxSet {
private:
    using UType = typename std::conditional<N==8, uint8_t,
                    typename std::conditional<N==16, uint16_t,
                        typename std::conditional<N==32, uint32_t,
                           typename std::conditional<N==64, uint64_t, void>>>>::type;
    static_assert(std::is_same_v<UType, void> == false, "N must be 8, 16, 32, or 64");

    std::array<CONTENT, N> content;
    UType active = {};
public:
    /**
     * @brief Default constructor.
     *
     * Initializes an empty BoxSet.
     */
    BoxSet() = default;

    /**
     * @brief Constructor that initializes the BoxSet with an array.
     *
     * @param init_content The array to initialize the BoxSet with. All slots are considered to be active.
     */
    BoxSet(const std::array<CONTENT, N>& init_content) : content(init_content) {
        active = std::numeric_limits<UType>::max();
    }

    /**
     * @brief Checks if the BoxSet is empty.
     *
     * @return True if the BoxSet is empty, false otherwise.
     */
    bool is_empty() const { return (active == 0); }

    /**
     * @brief Checks if the BoxSet is full.
     *
     * @return True if the BoxSet is full, false otherwise.
     */
    bool is_full() const { return (active == std::numeric_limits<UType>::max()); }

    /**
     * @brief Returns the capacity of the BoxSet.
     *
     * @return The maximum number of elements that the BoxSet can hold.
     */
    uint8_t capacity() const { return N; }

     /**
     * @brief Gets the number of elements currently in the BoxSet.
     *
     * @return The number of elements in use in the BoxSet.
     */
    uint8_t size() const
    {
       uint8_t count = 0;
        UType pattern = 1;
        for(int i=0; i<N; ++i)
        {
            if (active & pattern) ++count;
            pattern <<= 1;
        }
        return count;
    }

    /**
     * @brief Checks if a given index is in use.
     *
     * @param index The index to check.
     * @return True if the index is in use, false otherwise.
     */
    bool is_used(uint8_t index) const
    {
         return (active & (UType(1) << index));
    }

    /**
    * @brief Adds an item to the BoxSet.
    *
    * @param item The item to add to the container.
    *
    *  The item will be added to the first available slot.
    */
    void add(const CONTENT& item)
    {
        for (uint8_t i = 0; i < N; ++i)
        {
            if (!is_used(i))
            {
                content[i] = item;
                set(i);
                return;
            }
        }
    }

    /**
     * @brief Removes an item at a given index.
     *
     * @param index The index to remove.
     * @return The item that was removed.
     */
    CONTENT remove(uint8_t index)
    {
        CONTENT tmp = content[index];
        reset(index);
        return tmp;
    }

    /**
     * @brief Clears the BoxSet, removing all elements.
     *
     *  Removes all elements from the BoxSet and sets its size to 0.
     */
    void clear() {
       active = 0;
    }

    /**
     * @brief Checks if the BoxSet contains a particular item with a custom comparator.
     *
     * @tparam Comparator The type of the custom comparator.
     * @param item The item to search for.
     * @param comp The custom comparator to use.
     * @return True if the BoxSet contains the item, false otherwise.
     */
    template<typename Comparator>
     bool contains(const CONTENT& item, Comparator comp) const {
        return std::find_if(begin(), end(), [&](const CONTENT& storedItem) { return comp(item, storedItem); }) != end();
    }

    /**
     * @brief Checks if the BoxSet contains a particular item using equality comparison.
     *
     * @param item The item to search for.
     * @return True if the BoxSet contains the item, false otherwise.
     */
    bool contains(const CONTENT& item) const {
        return contains(item, std::equal_to<CONTENT>());
    }
    
    /**
     * @brief Iterator class for BoxSet.
     *
     * Provides forward iteration over active items in the container.
     */
    class iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = CONTENT;
        using difference_type = std::ptrdiff_t;
        using pointer = CONTENT*;
        using reference = CONTENT&;

        iterator(BoxSet& boxSet, size_t index) : m_boxSet(&boxSet), m_index(index) {
            _findNextActive();
        }

        iterator& operator++() {
            m_index++;
            _findNextActive();
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const { return m_boxSet == other.m_boxSet && m_index == other.m_index; }
        bool operator!=(const iterator& other) const { return !(*this == other); }

        reference operator*() const { return m_boxSet->content[m_index]; }
        pointer operator->() const { return &(m_boxSet->content[m_index]); }


    private:
        BoxSet* m_boxSet;
        size_t m_index;

        void _findNextActive() {
            while(m_index < m_boxSet->content.size() && !m_boxSet->is_used(m_index)) {
              m_index++;
           }
        }
    };

    /**
     * @brief Returns an iterator to the beginning of the BoxSet.
     *
     * @return An iterator pointing to the first active element in the BoxSet.
     */
    iterator begin() { return iterator(*this, 0); }

    /**
     * @brief Returns an iterator to the end of the BoxSet.
     *
     * @return An iterator pointing one past the last active element in the BoxSet.
     */
    iterator end() { return iterator(*this, N); }

private:

    /**
     * @brief Sets the bit at the given index in the active bitset.
     * @param index The index of the bit to set.
    */
    void set(uint8_t index) {
        active |= (UType(1) << index);
    }
    /**
     * @brief Resets the bit at the given index in the active bitset.
     * @param index The index of the bit to reset.
    */
    void reset(uint8_t index) {
        active &= ~(UType(1) << index);
    }
};