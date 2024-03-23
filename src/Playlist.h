#pragma once

#include "MediaItem.hpp"

#include <stdlib.h>

/// @brief Interface class representing a playlist
class Playlist {
public:
	/**
	 * @brief signature for the compare function
	 * @param a First element for the compare
	 * @param a Second element for the compare
	 * @return true if the expression a<b allpies (so if the first element is *less* than the second)
	 */
	using CompareFunc = std::function<bool(const MediaItem &a, const MediaItem &b)>;

	Playlist() = default;
	virtual ~Playlist() = default;

	// copy constructor / operators
	Playlist(const Playlist &) = default;
	Playlist &operator=(const Playlist &) = default;

	// move constructor / operators
	Playlist(Playlist &&) noexcept = default;
	Playlist &operator=(Playlist &&) noexcept = default;

	/**
	 * @brief get the status of the playlist
	 * @return true If the playlist has at least 1 playable entry
	 * @return false If the playlist is invalid
	 */
	virtual bool isValid() const = 0;

	/**
	 * @brief Allow explicit bool conversions, like when calling `if(playlist)`
	 * @see isValid()
	 * @return A bool conversion representing the status of the playlist
	 */
	explicit operator bool() const { return isValid(); }

	/**
	 * @brief Get the number of entries in the playlist
	 * @return size_t The number of MediaItem elemenets in the underlying container
	 */
	virtual size_t size() const = 0;

	/**
	 * @brief Get the element at index
	 * @param idx The queried index
	 * @return MediaItem the data at the index
	 */
	virtual MediaItem &at(size_t idx) = 0;

	/// @see MediaItem &at(size_t idx)
	const MediaItem &at(size_t idx) const { return at(idx); }

	/**
	 * @brief Add an item at the end of the container
	 * @param item The new item ot be added
	 * @return true when the operation succeeded
	 * @return false on error
	 */
	virtual bool addMediaItem(MediaItem &&item) = 0;

	/**
	 * @brief Add an item at the end of the container
	 * @param item The new item ot be added
	 * @param idx after which entry it'll be added
	 * @return true when the operation succeeded
	 * @return false on error
	 */
	virtual bool addMediaItem(MediaItem &&item, size_t idx) = 0;

	/**
	 * @brief Remove free space in underlying container
	 */
	virtual void compress() { }

	/**
	 * @brief Remove a single item from the container
	 * @param idx The idex to be removed
	 */
	virtual void removeMediaItem(size_t idx) = 0;

	/**
	 * @brief Remove a range of items from the underlying container
	 * @param startIdx The first element to be reomved
	 * @param endIdx The first element, which shall _not_ be removed
	 * @attention This function removes the range (first, last]
	 */
	virtual void removeMediaItem(size_t startIdx, size_t endIdx) {
		for (size_t i = startIdx; i < endIdx; i++) {
			removeMediaItem(i);
		}
	}

	/**
	 * @brief Sort the underlying container according to the supplied sort functions
	 * @param comp The compare function to use, defaults to strcmp between the two uri objects
	 */
	virtual void sort(CompareFunc comp = [](const MediaItem &a, const MediaItem &b) -> bool { return a.getUri() < b.getUri(); }) { }

	/**
	 * @brief Randomize the underlying entries
	 */
	virtual void shuffle() { }

	/**
	 * @brief Array opertor override for item access
	 * @param idx the queried index
	 * @return const MediaItem& Reference to the MediaItem at the index
	 */
	MediaItem &operator[](size_t idx) { return at(idx); };

	/// @see MediaItem &operator[](size_t idx)
	const MediaItem &operator[](size_t idx) const { return at(idx); };

	///@brief Iterator class to access playlist items
	class Iterator {
	public:
		// define what we can do
		using value_type = MediaItem;
		using iterator_category = std::random_access_iterator_tag; //< support random access iterators
		using difference_type = typename std::iterator<std::random_access_iterator_tag, value_type>::difference_type;
		using pointer = value_type *;
		using reference = value_type &;

		// Lifecycle
		Iterator() = default;
		Iterator(Playlist *playlist, size_t idx)
			: m_playlist(playlist)
			, m_idx(idx) { }

		// Operators: Access
		inline reference operator*() { return m_playlist->at(m_idx); }
		inline pointer operator->() { return &m_playlist->at(m_idx); }
		inline reference operator[](difference_type rhs) { return m_playlist->at(rhs); }

		// Operators: arithmetic
		// clang-format off
		inline Iterator& operator++() { ++m_idx; return *this; }
        inline Iterator& operator--() { --m_idx; return *this; }
        inline Iterator operator++(int) { Iterator tmp(*this); ++m_idx; return tmp; }
        inline Iterator operator--(int) { Iterator tmp(*this); --m_idx; return tmp; }
		inline difference_type operator-(const Iterator &rhs) const { return m_idx - rhs.m_idx; }
 		inline Iterator operator+(difference_type rhs) const { return Iterator(m_playlist, m_idx + rhs); }
		inline Iterator operator-(difference_type rhs) const { return Iterator(m_playlist, m_idx - rhs); }
		inline Iterator& operator+=(difference_type rhs) { m_idx += rhs; return *this; }
	    inline Iterator& operator-=(difference_type rhs) { m_idx -= rhs; return *this; }
		friend inline Iterator operator+(difference_type lhs, const Iterator &rhs) { return Iterator(rhs.m_playlist, rhs.m_idx + lhs); }
		friend inline Iterator operator-(difference_type lhs, const Iterator &rhs) { return Iterator(rhs.m_playlist, lhs - rhs.m_idx); }
		// clang-format on

		// Operators: comparison
		inline bool operator==(const Iterator &rhs) { return m_idx == rhs.m_idx; }
		inline bool operator!=(const Iterator &rhs) { return m_idx != rhs.m_idx; }
		inline bool operator>(const Iterator &rhs) { return m_idx > rhs.m_idx; }
		inline bool operator<(const Iterator &rhs) { return m_idx < rhs.m_idx; }
		inline bool operator>=(const Iterator &rhs) { return m_idx >= rhs.m_idx; }
		inline bool operator<=(const Iterator &rhs) { return m_idx <= rhs.m_idx; }

	protected:
		Playlist *m_playlist {nullptr};
		size_t m_idx {0};
	};

	Iterator begin() { return Iterator(this, 0); }
	Iterator end() { return Iterator(this, size()); }

	// convenience functions with iterators

	/**
	 * @brief Add an item to the container at the position marked by the iterator
	 * @see Playlist::addMediaItem(const MediaItem &&item, int idx)
	 */
	bool addMediaItem(MediaItem &&item, Iterator pos) { return addMediaItem(std::move(item), std::distance(begin(), pos)); }

	/**
	 * @brief Remove the item at the position marked by the iterator
	 * @see Playlist::removeMediaItem(int idx)
	 */
	void removeMediaItem(Iterator pos) { removeMediaItem(std::distance(begin(), pos)); }

	/**
	 * @brief Remove the range (first, last] defined by the iterators
	 * @see Playlist::removeMediaItemsize_t startIdx, size_t endIdx)
	 */
	void removeMediaItem(Iterator first, Iterator last) { removeMediaItem(std::distance(begin(), first), std::distance(begin(), last)); }
};
