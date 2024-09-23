#pragma once

#include <stdlib.h>
#include <vector>

using Playlist = std::vector<char *>;

// Release previously allocated memory
inline void freePlaylist(Playlist *(&playlist)) {
	if (playlist == nullptr) {
		return;
	}
	for (auto e : *playlist) {
		free(e);
	}
	delete playlist;
	playlist = nullptr;
}
