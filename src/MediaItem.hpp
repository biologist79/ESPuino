#pragma once

#include <Stream.h>
#include <WString.h>
#include <algorithm>
#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace media {

/// @brief Class for providing
class Artwork : public Stream {
public:
	Artwork() = default;
	virtual ~Artwork() = default;

	// copy operators
	Artwork(const Artwork &) = default;
	Artwork &operator=(const Artwork &) = default;

	// move operators
	Artwork(Artwork &&) noexcept = default;
	Artwork &operator=(Artwork &&) noexcept = default;

	String mimeType {String()};
};

class RamArtwork : public Artwork {
public:
	RamArtwork() = default;
	RamArtwork(std::vector<uint8_t> &data)
		: data(data) { }
	RamArtwork(std::vector<uint8_t> &&data)
		: data(std::move(data)) { }
	RamArtwork(const uint8_t *data, size_t len) { setContent(data, len); }
	virtual ~RamArtwork() override = default;

	// copy operators
	RamArtwork(const RamArtwork &) = default;
	RamArtwork &operator=(const RamArtwork &) = default;

	// move operators
	RamArtwork(RamArtwork &&) noexcept = default;
	RamArtwork &operator=(RamArtwork &&) noexcept = default;

	void setContent(const uint8_t *data, size_t len) {
		this->data.resize(len);
		std::copy(data, data + len, this->data.begin());
		readIt = this->data.cbegin();
	}

	void setContent(const std::vector<uint8_t> &data) {
		this->data = data;
	}

	void setContent(std::vector<uint8_t> &&data) {
		this->data = std::move(data);
	}

	// overrides for Stream

	/// @see https://www.arduino.cc/reference/en/language/functions/communication/stream/streamavailable/
	virtual int available() override {
		return std::distance(readIt, data.cend());
	}

	/// @see https://www.arduino.cc/reference/en/language/functions/communication/stream/streamread/
	virtual int read() override {
		int c = *readIt;
		readIt++;
		return c;
	}

	///@see https://www.arduino.cc/reference/en/language/functions/communication/stream/streampeek/
	virtual int peek() override {
		return *readIt;
	}

protected:
	std::vector<uint8_t> data {std::vector<uint8_t>()};
	std::vector<uint8_t>::const_iterator readIt {};
};

class Metadata {
public:
	Metadata() = default;
	virtual ~Metadata() = default;

	// copy operators for base class are deleted because std::unique_ptr
	Metadata(const Metadata &) = default;
	Metadata &operator=(const Metadata &) = default;

	// move operators
	Metadata(Metadata &&) noexcept = default;
	Metadata &operator=(Metadata &&) noexcept = default;

	enum MetaDataType {
		title, //< track title
		artist, //< track artist
		duration, //< track lenght in seconds
		albumTitle, //< album title
		albumArtist, //< album artist, use artist if not set
		displayTitle, //< overrides title if set
		trackNumber, //< track number _in the album_
		totalTrackCount, //< total number of track _in the album_
		chapter, //< the chapter number if this is a audiobook
		releaseYear, //< year of the track/album release
		artwork, //< album artwork
	};
	using MetaDataVariant = std::variant<uint32_t, String, std::shared_ptr<Artwork>>; //< variant ot hold all types of metadata

	/**
	 * @brief Returns if the field with the requested type is in the map
	 * @param type The field to find
	 * @return true when the field is in the map
	 * @return false when the field is not presen
	 */
	virtual bool containsField(MetaDataType type) const {
		return metadata.find(type) != metadata.cend();
	}

	/**
	 * @brief Get a field from the metadata
	 * @param type The requeszed field
	 * @return MetaDataVariant The vlaue of the field (or std::monostate if the field is not set)
	 */
	virtual std::optional<MetaDataVariant> getMetadataField(MetaDataType type) const {
		const auto it = metadata.find(type);
		if (it == metadata.end()) {
			return std::nullopt;
		}
		return it->second;
	}

	/**
	 * @brief Set a selected field of the metadata
	 * @param type The field to be modified
	 * @param value The value of the field
	 */
	virtual void setMetadataField(MetaDataType type, const MetaDataVariant &value) {
		metadata.insert_or_assign(type, value);
	}

	/**
	 * @see setMetadataField(MetaDataType type, const MetaDataVariant &value)
	 */
	virtual void setMetadataField(MetaDataType type, MetaDataVariant &&value) {
		metadata.insert_or_assign(type, value);
	}

	/**
	 * @brief Compare two Metatdata object
	 * @param a The first object
	 * @param b The second object
	 * @return true if all fields match
	 */
	friend bool operator==(const Metadata &a, const Metadata &b) {
		return a.metadata == b.metadata;
	}

protected:
	std::map<MetaDataType, MetaDataVariant> metadata;
};

} // namespace media

/// @brief Object representing a single entry in the playlist
struct MediaItem {
	MediaItem() = default;
	MediaItem(const String &path) {
		if (fileValid(path)) {
			parseMetaData(path);
			this->uri = path;
		}
	}
	MediaItem(String &&path) {
		if (fileValid(path)) {
			parseMetaData(path);
			this->uri = std::move(path);
		}
	}
	virtual ~MediaItem() noexcept = default;

	//  copy constructor / operator
	MediaItem(const MediaItem &) = default;
	MediaItem &operator=(const MediaItem &) = default;

	//  move constructor / operator
	MediaItem(MediaItem &&rhs) noexcept = default;
	MediaItem &operator=(MediaItem &&rhs) noexcept = default;

	/**
	 * @brief Get the playable path to the MediaItem
	 * @return String a playlable path
	 */
	virtual String getUri() const {
		return uri;
	}

	virtual const media::Metadata &getMetadata() const {
		return metadata;
	}

	/// @brief Compare operator overload for MediaItem, returns true if lhs==rhs
	friend bool operator==(MediaItem const &lhs, MediaItem const &rhs) {
		return lhs.uri == rhs.uri && lhs.metadata == rhs.metadata;
	}

	/**
	 * @brief Returns the status of the MediaItem
	 * @return true when the uri is filles out
	 * @return false when uri is empty
	 */
	bool isValid() const {
		return !uri.isEmpty();
	}

	/// @brief bool operator override
	explicit operator bool() const { return isValid(); }

protected:
	String uri {String()}; //< playable uri of the entry
	media::Metadata metadata {media::Metadata()}; //< optional metadata for the entry

	/**
	 * @brief Check the path against a fixed set of urles to determine if this is (propably) a playlable media files
	 * @param path The uri to be checked
	 * @return true if all rules pass
	 * @return false if a single rule fails
	 * @todo This function could be improved to check against the content instead of the file name
	 */
	static bool fileValid(const String &path) {
		// clang-format off
		// all supported extension
		constexpr std::array audioFileSufix = {
			"mp3",
			"aac",
			"m4a",
			"wav",
			"flac",
			"ogg",
			"oga",
			"opus",
			// playlists
			"m3u",
			"m3u8",
			"pls",
			"asx"
		};
		// clang-format on
		constexpr size_t maxExtLen = strlen(*std::max_element(audioFileSufix.begin(), audioFileSufix.end(), [](const char *a, const char *b) {
			return strlen(a) < strlen(b);
		}));

		if (!path || path.isEmpty()) {
			// invalid entry
			// log_n("Empty path");
			return false;
		}

		// check for streams
		if (path.startsWith("http://") || path.startsWith("https")) {
			return true;
		}

		// check for files which start with "/."
		int lastSlashPos = path.lastIndexOf('/');
		if (path[lastSlashPos + 1] == '.') {
			// this is a hidden file
			// log_n("File is hidden: %s", path.c_str());
			return false;
		}

		// extract the file extension
		int extStartPos = path.lastIndexOf('.');
		if (extStartPos == -1) {
			// no extension found
			// log_n( "File has no extension: %s", path.c_str());
			return false;
		}
		extStartPos++;
		if ((path.length() - extStartPos) > maxExtLen) {
			// the extension is too long
			// log_n("File not supported (extension to long): %s", path.c_str());
			return false;
		}
		String extension = path.substring(extStartPos);
		extension.toLowerCase();

		// check extension against all supported values
		for (const auto &e : audioFileSufix) {
			if (extension.equals(e)) {
				// hit we found the extension
				return true;
			}
		}
		// miss, we did not find the extension
		// log_n( "File not supported: %s", path.c_str());
		return false;
	}

	/**
	 * @brief Parse meta data from a Stream
	 * @attention not yet implemented
	 */
	void parseMetaData(Stream &s) {
	}

	/**
	 * @brief Parse meta data from an uri
	 * @attention not yet implemented
	 */
	void parseMetaData(const String &path) {
	}
};
