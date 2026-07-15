"""Read-only access to the media library under MEDIA_DIR.

MediaHub never copies or uploads audio files — the admin mounts their own
existing library into the container (see docker-compose.yml) and only
*points* cards at files/folders already there (concept: the hub is the
source of truth for playMode and file list, not for the bytes themselves).
This module resolves browser-supplied relative paths safely against the
library root and lists/hashes files on demand.
"""

import hashlib
import os

# Extensions the ESPuino firmware actually recognizes as playable audio
# (see `fileValid()` / `audioFileSufix` in src/SdCard.cpp). Playlist formats
# (.m3u, .pls, ...) are deliberately excluded — MediaHub doesn't support
# LOCAL_M3U (see concept §7.2); selecting a whole folder already covers the
# "multiple tracks" case.
AUDIO_EXTENSIONS = {".mp3", ".aac", ".m4a", ".wav", ".flac", ".ogg", ".oga", ".opus"}


def is_audio_file(name):
    return os.path.splitext(name)[1].lower() in AUDIO_EXTENSIONS


def resolve(media_root, relpath):
    """Resolves a browser-supplied relative path against media_root.

    Returns the absolute path, or None if it would escape media_root
    (traversal attempt) or media_root itself doesn't exist.
    """
    media_root = os.path.realpath(media_root)
    relpath = (relpath or "").strip().strip("/")
    candidate = os.path.realpath(os.path.join(media_root, relpath))
    if candidate != media_root and not candidate.startswith(media_root + os.sep):
        return None
    return candidate


def list_directory(media_root, relpath):
    """Lists one directory level: subfolders and audio files, dirs first,
    both natural-sorted case-insensitively. Returns None if relpath doesn't
    resolve to a directory inside media_root.
    """
    abs_path = resolve(media_root, relpath)
    if abs_path is None or not os.path.isdir(abs_path):
        return None

    dirs, files = [], []
    try:
        with os.scandir(abs_path) as entries:
            for entry in entries:
                if entry.name.startswith("."):
                    continue
                try:
                    if entry.is_dir(follow_symlinks=False):
                        dirs.append(entry.name)
                    elif entry.is_file(follow_symlinks=False) and is_audio_file(entry.name):
                        files.append(entry.name)
                except OSError:
                    # Individual entry unreadable (permissions, broken symlink, ...) — skip it,
                    # don't fail the whole listing over one bad entry.
                    continue
    except OSError:
        # Directory itself not readable/traversable by the container's user.
        return None

    dirs.sort(key=str.casefold)
    files.sort(key=str.casefold)
    return {"dirs": dirs, "files": files}


def list_audio_files_in_directory(media_root, relpath):
    """Relative paths (media_root-rooted) of all direct audio file children
    of relpath, sorted. Used for the "use this whole folder" selection.
    """
    listing = list_directory(media_root, relpath)
    if listing is None:
        return []
    relpath = (relpath or "").strip().strip("/")
    return [f"{relpath}/{name}" if relpath else name for name in listing["files"]]


def stat_and_hash(media_root, relpath):
    """Validates relpath is a real, readable audio file inside media_root and
    returns {"path", "size", "sha256"} (path normalized with forward
    slashes), or None if it doesn't qualify — including when the container's
    user lacks read permission on the file (directory listing and file
    reading are separate Unix permissions; a file can be visible in the
    browser yet unreadable, e.g. mode 600 owned by someone else).
    """
    abs_path = resolve(media_root, relpath)
    if abs_path is None or not os.path.isfile(abs_path) or not is_audio_file(abs_path):
        return None

    hasher = hashlib.sha256()
    size = 0
    try:
        with open(abs_path, "rb") as f:
            while True:
                chunk = f.read(1024 * 1024)
                if not chunk:
                    break
                hasher.update(chunk)
                size += len(chunk)
    except OSError:
        return None

    normalized = os.path.relpath(abs_path, os.path.realpath(media_root)).replace(os.sep, "/")
    return {"path": normalized, "size": size, "sha256": hasher.hexdigest()}
