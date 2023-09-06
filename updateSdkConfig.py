Import("env")
from pathlib import Path
import json

default_file = "sdkconfig.defaults"
build_file = ".pio/lastBuild.json"

# Get last modified timestamp of default file
default_path = Path(default_file)
if default_path.is_file():
    default_time = default_path.stat().st_mtime
else:
    default_time = 0

# Check if default file has been modified since last build
build_path = Path(build_file)
if build_path.is_file():
    with open(build_file, "r") as f:
        build_data = json.load(f)
        last_timestamp = build_data.get("last_timestamp", 0)
else:
    last_timestamp = 0
    build_data = {}  # default value for build_data

if default_time > last_timestamp:
    # Delete all sdkconfig files except for sdkconfig.default
    for file_path in Path(".").glob("sdkconfig.*"):
        if file_path.name != default_file:
            file_path.unlink()

    # Write last known timestamp to lastBuild.json
    build_data["last_timestamp"] = default_time  # update last_timestamp
    with open(build_file, "w") as f:
        json.dump(build_data, f)