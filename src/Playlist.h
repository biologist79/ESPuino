#pragma once

#include <WString.h>
#include <string.h>
#include "cpp.h"

#if MEM_DEBUG == 1
	#warning Memory access guards are enabled. Disable MEM_DEBUG for production builds
#endif

using sortFunc = int(*)(const void*,const void*);

class Playlist {
public:
	Playlist() { }
	virtual ~Playlist() { }

	virtual size_t size() const = 0;

	virtual bool isValid() const = 0;

	virtual const String getAbsolutePath(size_t idx) const = 0;

	virtual const String getFilename(size_t idx) const = 0;

	static int alphabeticSort(const void *x, const void *y) {
		const char *a = static_cast<const char*>(x);
		const char *b = static_cast<const char*>(y);

		return strcmp(a, b);
	}

	virtual void sort(sortFunc func = alphabeticSort) { }

	virtual void randomize() { }


protected:

	template <typename T>
	class PsramAllocator {
	public:
		typedef T value_type;
		typedef value_type* pointer;
		typedef const value_type* const_pointer;
		typedef value_type& reference;
		typedef const value_type& const_reference;
		typedef std::size_t size_type;
		typedef std::ptrdiff_t difference_type;

	public:
		template<typename U>
		struct rebind {
			typedef PsramAllocator<U> other;
		};

	public:
		inline explicit PsramAllocator() {}
		inline ~PsramAllocator() {}
		inline PsramAllocator(PsramAllocator const&) {}
		template<typename U>
		inline explicit PsramAllocator(PsramAllocator<U> const&) {}

		// address
		inline pointer address(reference r) { return &r; }

		inline const_pointer address(const_reference r) { return &r; }

		// memory allocation
		inline pointer allocate(size_type cnt, typename std::allocator<void>::const_pointer = 0) {
			T *ptr = nullptr;
			if(psramFound()) {
				ptr = (T*)ps_malloc(cnt * sizeof(T));
			} else {
				ptr = (T*)malloc(cnt * sizeof(T));
			}
			return ptr;
		}

		inline void deallocate(pointer p, size_type cnt) {
			free(p);
		}

		//   size
		inline size_type max_size() const {
			return std::numeric_limits<size_type>::max() / sizeof(T);
		}

		// construction/destruction
		inline void construct(pointer p, const T& t) {
			new(p) T(t);
		}

		inline void destroy(pointer p) {
			p->~T();
		}

		inline bool operator==(PsramAllocator const& a) { return this == &a; }
		inline bool operator!=(PsramAllocator const& a) { return !operator==(a); }
	};

	using pstring = std::basic_string<char, std::char_traits<char>, PsramAllocator<char>>;

	virtual void destroy() { }

	static constexpr auto audioFileSufix = std::to_array<const char*>({
		".mp3",
		".aac",
		".m3u",
		".m4a",
		".wav",
		".flac",
		".aac"
	});

	// Check if file-type is correct
	bool fileValid(const String _fileItem) {
		if(!_fileItem)
			return false;

		// check for http address 
		if(_fileItem.startsWith("http://") || _fileItem.startsWith("https://")) {
			return true;
		}

		// Ignore hidden files starting with a '.'
		//    lastIndex is -1 if '/' is not found --> first index will be 0
		int fileNameIndex = _fileItem.lastIndexOf('/') + 1;	
		if(_fileItem[fileNameIndex] == '.') {
			return false;
		}

		for(const auto e:audioFileSufix) {
			if(_fileItem.endsWith(e)) {
				return true;
			}
		}
		return false;
	}

};
