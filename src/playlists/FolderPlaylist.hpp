#pragma once

#include "../Playlist.h"
#include "Common.h"

#include <FS.h>
#include <WString.h>
#include <random>
#include <stdint.h>
#include <string>
#include <vector>

class FolderPlaylist : public Playlist {
protected:
	pstring base;
	std::vector<pstring, PsramAllocator<pstring>> files;
	char divider;

public:
	FolderPlaylist(size_t _capacity = 64, char _divider = '/')
		: base(pstring())
		, files(std::vector<pstring, PsramAllocator<pstring>>(_capacity))
		, divider(_divider) { }
	FolderPlaylist(File &folder, size_t _capacity = 64, char _divider = '/')
		: FolderPlaylist(_capacity, divider) {
		createFromFolder(folder);
	}

	virtual ~FolderPlaylist() {
		destroy();
	}

	bool createFromFolder(File &folder) {
		// This is not a folder, so bail out
		if (!folder || !folder.isDirectory()) {
			return false;
		}

		// clean up any previously used memory
		clear();

		// since we are enumerating, we don't have to think about absolute files with different bases
		base = getPath(folder);

		// reserve a sane amout of memory
		files.reserve(64);

		// enumerate all files in the folder
		while (true) {
			bool isDir;
			String path;
			if constexpr (fileNameSupport) {
				path = folder.getNextFileName(&isDir);
			} else {
				File f = folder.openNextFile();
				if (!f) {
					path = "";
				} else {
					path = getPath(f);
					isDir = f.isDirectory();
				}
			}
			if (path.isEmpty()) {
				break;
			}
			if (isDir) {
				continue;
			}

			if (fileValid(path)) {
				// push this file into the array
				bool success = push_back(path);
				if (!success) {
					return false;
				}
			}
		}
		// resize memory to fit our count
		files.shrink_to_fit();

		return true;
	}

	void setBase(const char *_base) {
		base = _base;
	}

	void setBase(const String _base) {
		base = _base.c_str();
	}

	const char *getBase() const {
		return base.c_str();
	}

	bool isRelative() const {
		return base.length();
	}

	bool push_back(const char *path) {
		log_n("path: %s", path);
		if (!fileValid(path)) {
			return false;
		}

		// here we check if we have to cut up the path (currently it's only a crude check for absolute paths)
		if (isRelative() && path[0] == '/') {
			// we are in relative mode and got an absolute path, check if the path begins with our base
			// Also check if the path is so short, that there is no space for a filename in it
			if ((strncmp(path, base.c_str(), base.length()) != 0) || (strlen(path) < (base.length() + strlen("/.abc")))) {
				// we refuse files other than our base
				return false;
			}
			path = path + base.length(); // modify pointer to the end of the path
		}

		files.push_back(path);
		return true;
	}

	bool push_back(const String path) {
		return push_back(path.c_str());
	}

	void compress() {
		files.shrink_to_fit();
	}

	void clear() {
		destroy();
		init();
	}

	void setDivider(char _divider) { divider = _divider; }
	bool getDivider() const { return divider; }

	virtual size_t size() const override { return files.size(); };

	virtual bool isValid() const override { return files.size(); }

	virtual const String getAbsolutePath(size_t idx) const override {
		if (isRelative()) {
			// we are in relative mode
			return String(base.c_str()) + divider + files[idx].c_str();
		}
		return String(files[idx].c_str());
	};

	virtual const String getFilename(size_t idx) const override {
		if (isRelative()) {
			return String(files[idx].c_str());
		}
		pstring path = files[idx];
		return String(path.substr(path.find_last_of("/") + 1).c_str());
	};

	virtual void sort(sortFunc func = alphabeticSort) override {
		std::sort(files.begin(), files.end());
	}

	virtual void randomize() override {
		if (files.size() < 2) {
			// we can not randomize less than 2 entries
			return;
		}

		// randomize using the "normal" random engine and shuffle
		std::default_random_engine rnd(millis());
		std::shuffle(files.begin(), files.end(), rnd);
	}

	class Iterator {
	public:
		using iterator_category = std::forward_iterator_tag; // could be increased to random_access_iterator_tag
		using difference_type = std::ptrdiff_t;
		using value_type = pstring;
		using pointer = value_type *;
		using reference = value_type &;

		class ArrowHelper {
			value_type value;

		public:
			ArrowHelper(value_type _str)
				: value(_str) { }
			pointer operator->() {
				return &value;
			}
		};

		__attribute__((always_inline)) inline const ArrowHelper operator->() const {
			return ArrowHelper(operator*());
		}

		__attribute__((always_inline)) inline const value_type operator*() const {
			return m_folder->base + m_folder->divider + *m_ptr;
		}

		// Constructor
		__attribute__((always_inline)) inline Iterator(FolderPlaylist *folder, pointer ptr)
			: m_folder(folder)
			, m_ptr(ptr) { }

		// Copy Constructor & assignment
		__attribute__((always_inline)) inline Iterator(const Iterator &rhs)
			: m_folder(rhs.m_folder)
			, m_ptr(rhs.m_ptr) { }
		__attribute__((always_inline)) inline Iterator &operator=(const Iterator &rhs) = default;

		// Pointer increment
		__attribute__((always_inline)) inline Iterator &operator++() {
			m_ptr++;
			return *this;
		}
		__attribute__((always_inline)) inline Iterator operator++(int) {
			Iterator tmp(*this);
			m_ptr++;
			return tmp;
		}

		// boolean operators
		__attribute__((always_inline)) inline operator bool() const {
			return (m_ptr);
		}
		__attribute__((always_inline)) inline bool operator==(const Iterator &rhs) {
			return (m_ptr == rhs.m_ptr) && (m_folder == rhs.m_folder);
		}
		__attribute__((always_inline)) inline bool operator!=(const Iterator &rhs) {
			return !operator==(rhs);
		}

	protected:
		const FolderPlaylist *m_folder;
		pointer m_ptr;
	};

	Iterator cbegin() { return Iterator(this, files.data()); }
	Iterator cend() { return Iterator(this, (files.data() + files.size())); }

protected:
	static constexpr bool fileNameSupport = (ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(2, 0, 8));

	virtual void destroy() override {
		files.clear();
		base.clear();
	}

	void init() {
		divider = '/';
	}
};
