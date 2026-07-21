#pragma once

#include <FS.h>

class SanitizedFS : public fs::FS {
public:
	// Extract the implementation pointer from the existing FS object
	// We use a pointer-based approach to bypass the protected access if needed,
	// but usually, assigning the internal _impl works.
	SanitizedFS(fs::FS &realFS)
		: fs::FS(realFS) { }

	String name(fs::File file) {
		String name = file.name();
		return reparse(name);
	}

	String nextFileName(fs::File &file, bool *isDir = nullptr) {
		String name = file.getNextFileName(isDir);
		return reparse(name);
	}

	String path(fs::File file) {
		String path = file.path();
		return reparse(path);
	}

	String rawPath(const char *path) {
		return sanitize(path);
	}

	// --- Overridden/Shadowed Methods ---

	fs::File open(const char *path, const char *mode = "r", const bool create = false) {
		return fs::FS::open(sanitize(path).c_str(), mode, create);
	}

	fs::File open(const String &path, const char *mode = "r", const bool create = false) {
		return fs::FS::open(sanitize(path), mode, create);
	}

	bool exists(const char *path) {
		return fs::FS::exists(sanitize(path).c_str());
	}
	bool exists(const String &path) {
		return fs::FS::exists(sanitize(path));
	}

	bool existsRaw(const char *path) {
		return fs::FS::exists(path);
	}

	bool rename(const char *pathFrom, const char *pathTo) {
		return fs::FS::rename(sanitize(pathFrom).c_str(), sanitize(pathTo).c_str());
	}
	bool rename(const String &pathFrom, const String &pathTo) {
		return fs::FS::rename(sanitize(pathFrom), sanitize(pathTo));
	}

	bool remove(const char *path) {
		return fs::FS::remove(sanitize(path).c_str());
	}

	bool remove(const String &path) {
		return fs::FS::remove(sanitize(path));
	}

	bool remove(const fs::File &file) {
		return fs::FS::remove(file.path());
	}

	bool mkdir(const char *path) {
		return fs::FS::mkdir(sanitize(path).c_str());
	}

	bool mkdir(const String &path) {
		return fs::FS::mkdir(sanitize(path));
	}

	bool rmdir(const char *path) {
		return fs::FS::rmdir(sanitize(path).c_str());
	}
	bool rmdir(const String &path) {
		return fs::FS::rmdir(sanitize(path));
	}
	bool rmdir(const fs::File &dir) {
		return fs::FS::rmdir(dir.path());
	}

	const char *mountpoint() {
		return fs::FS::mountpoint();
	}

private:
	static const char *getIllegalChars() {
		return ":*?\"<>|%";
	}

	// Helper to convert nibble to hex character
	char to_hex(unsigned char v) {
		return v < 10 ? '0' + v : 'A' + (v - 10);
	}

	// Helper to convert hex character to nibble
	int from_hex(char c) {
		if (c >= '0' && c <= '9') {
			return c - '0';
		}
		if (c >= 'A' && c <= 'F') {
			return c - 'A' + 10;
		}
		if (c >= 'a' && c <= 'f') {
			return c - 'a' + 10;
		}
		return -1;
	}

	String sanitize(const String &input) {
		String output = "";
		// Pre-allocate memory to prevent heap fragmentation
		output.reserve(input.length());

		for (size_t i = 0; i < input.length(); i++) {
			unsigned char c = input[i];
			// Check for illegal chars
			if (strchr(getIllegalChars(), (int) c) != nullptr) {
				output += '%';
				output += to_hex(c >> 4);
				output += to_hex(c & 0x0F);
			} else {
				output += (char) c;
			}
		}
		return output;
	}

	String reparse(const String &input) {
		String output = "";
		output.reserve(input.length());

		for (size_t i = 0; i < input.length(); i++) {
			if (input[i] == '%' && i + 2 < input.length()) {
				int high = from_hex(input[i + 1]);
				int low = from_hex(input[i + 2]);

				if (high != -1 && low != -1) {
					char c = (char) ((high << 4) | low);
					if (strchr(getIllegalChars(), (int) c) != nullptr) {
						output += c;
						i += 2; // Skip the two hex characters
						continue;
					}
				}
			}
			output += input[i];
		}
		return output;
	}
};
