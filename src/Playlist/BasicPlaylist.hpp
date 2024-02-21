#pragma once

#include <Arduino.h>

#include "../Playlist.h"

#include <FS.h>
#include <algorithm>
#include <random>
#include <vector>

class BasicPlaylist : public Playlist {
public:
	BasicPlaylist() = default;
	BasicPlaylist(size_t initialCap) {
		container.reserve(initialCap);
	}
	virtual ~BasicPlaylist() = default;

	// copy operators
	BasicPlaylist(const BasicPlaylist &) = default;
	BasicPlaylist &operator=(const BasicPlaylist &) = default;

	// move operators
	BasicPlaylist(BasicPlaylist &&) noexcept = default;
	BasicPlaylist &operator=(BasicPlaylist &&) noexcept = default;

	// implementation fo the virtual funtions

	/// @see Playlist::isValid()
	virtual bool isValid() const { return !container.empty(); }

	/// @see Playlist::size()
	virtual size_t size() const { return container.size(); }

	/// @see Playlist::at(size_t idx)
	virtual MediaItem &at(size_t idx) { return container.at(idx); }

	/// @see Playlist::addMediaItem(const MediaItem &&item)
	virtual bool addMediaItem(MediaItem &&item) { return addMediaItem(std::move(item), container.size()); }

	/// @see Playlist::addMediaItem(const MediaItem &&item, int idx)
	virtual bool addMediaItem(MediaItem &&item, size_t idx) {
		if (!item.isValid()) {
			return false;
		}
		container.emplace(container.begin() + idx, std::move(item));
		return true;
	}

	/// @see Playlist::compress()
	virtual void compress() override { container.shrink_to_fit(); }

	/**
	 * @brief Remove a single item from the container
	 * @param idx The idex to be removed
	 */
	virtual void removeMediaItem(size_t idx) override {
		container.erase(container.begin() + idx);
	}

	/// @see Playlist::sort(CompareFunc comp)
	virtual void sort(CompareFunc comp) {
		if (container.size() < 2) {
			// we can not sort less than two items
			return;
		}
		std::sort(container.begin(), container.end(), comp);
	}

	/// @see Playlist::shuffle()
	virtual void shuffle() {
		if (container.size() < 2) {
			// we can not shuffle less than two items
			return;
		}
		auto rng = std::default_random_engine {};
		std::shuffle(container.begin(), container.end(), rng);
	}

protected:
	std::vector<MediaItem> container;
};
