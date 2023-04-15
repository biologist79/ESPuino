#pragma once

#include <stdint.h>
#include <WString.h>

#include "../Playlist.h"

class WebstreamPlaylist : public Playlist {
protected:
	pstring url;

public:
	WebstreamPlaylist(const char *_url) : url(_url) { }
	WebstreamPlaylist() : url(nullptr) { }
	virtual ~WebstreamPlaylist() override {
	};

	bool setUrl(const char *_url) {
		if(fileValid(_url)) {
			url = _url;
			return true;
		}
		return false;
	}

	virtual size_t size() const override { return (url.length()) ? 1 : 0; }
	virtual bool isValid() const override { return url.length(); }
	virtual const String getAbsolutePath(size_t idx = 0) const override { return String(url.c_str()); };
	virtual const String getFilename(size_t idx = 0) const override { return String(url.c_str()); };

};
