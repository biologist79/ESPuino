#pragma once

#include <vector>
#include <stdlib.h>

using Playlist = std::vector<char*>;

// Release previously allocated memory
inline void freePlaylist(Playlist *playlist) {
	for (auto e : *playlist) {
		free(e);
	}
	delete playlist;
}
