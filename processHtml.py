# -*- coding: utf-8 -*-
from pathlib import Path
import os
import shutil
import mimetypes
import gzip
import json
import re

# Use PlatformIO's build directory to keep the project clean
# this matches your version 1 logic exactly
try:
    from SCons.Script import Import
    Import("env")
    OUTPUT_DIR = Path(env.subst("$BUILD_DIR")) / "generated"
    IS_PIO = True
except Exception:
    OUTPUT_DIR = Path("./generated")
    IS_PIO = False

# Ensure the directory exists so the script doesn't crash
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

# Ensure HTML_DIR is relative to the project root
ROOT_DIR = Path(env.subst("$PROJECT_DIR")) if IS_PIO else Path.cwd()
HTML_DIR = ROOT_DIR / "html"

BINARY_FILES = [
    Path("management.html"),
    Path("accesspoint.html"),
    Path("logo.svg"),
    Path("js/i18next.min.js"),
    Path("js/i18nextHttpBackend.min.js"),
    Path("REST_API.yaml"),
    Path("swagger.html"),
    Path("js/swaggerInitializer.js"),
    Path("js/loc_i18next.min.js"),
    Path("locales/de.json"),
    Path("locales/en.json"),
    Path("locales/fr.json")
]

class HtmlHeaderProcessor:
    @staticmethod
    def _safe_minify(content, suffix):
        lines = []
        for line in content.splitlines():
            stripped = line.strip()
            if stripped:
                if suffix in [".html", ".htm"]:
                    stripped = re.sub(r'>\s+<', '><', stripped)
                lines.append(stripped)
        return "\n".join(lines)

    @classmethod
    def _process_binary_file(cls, binary_path, header_path, info):
        if binary_path.suffix == ".json":
            with binary_path.open(mode="r", encoding="utf-8") as f:
                content = json.dumps(json.load(f), separators=(',', ':'))
        elif binary_path.suffix in [".htm", ".html", ".js", ".css"]:
            with open(binary_path, 'r', encoding="utf-8") as f:
                content = f.read()
                if ".min" not in str(binary_path):
                    content = cls._safe_minify(content, binary_path.suffix)
        else:
            with binary_path.open(mode="r", encoding="utf-8") as f:
                content = f.read()

        stinfo = os.stat(binary_path)
        data = gzip.compress(content.encode(), mtime=stinfo.st_mtime)

        with header_path.open(mode="a", encoding="utf-8") as header_file:
            varName = re.sub(r'[^a-zA-Z0-9]', '_', binary_path.name.split('.')[0])
            header_file.write(f"static const uint8_t {varName}_BIN[] = {{\n    ")
 
            size = 0
            for d in data:
                header_file.write("0x{:02X},".format(d))
                size += 1
                if not (size % 20):
                    header_file.write("\n    ")
            header_file.write("\n};\n\n")

            info["size"] = size
            info["variable"] = f"{varName}_BIN"
            return info

    def process(self):
        print(f"--- GENERATING HTML BINARY HEADERS -> {OUTPUT_DIR} ---")
        binary_header = OUTPUT_DIR / "HTMLbinary.h"

        yaml_src = ROOT_DIR / "REST-API.yaml"
        if yaml_src.exists():
            shutil.copy2(yaml_src, HTML_DIR / "REST_API.yaml")

        if binary_header.exists():
            os.remove(binary_header)

        file_list = []
        for binary_file in BINARY_FILES:
            file_path = HTML_DIR / binary_file
            if not file_path.exists():
                print(f"  Warning: {file_path} not found.")
                continue

            print(f"  Encoding: {binary_file.as_posix()}")
            info = {
                "uri": "/" + binary_file.as_posix(),
                "mimeType": mimetypes.types_map.get(file_path.suffix, "application/octet-stream")
            }
            if file_path.suffix in [".yaml", ".yml"]:
                info["mimeType"] = "application/yaml"

            processed_info = self._process_binary_file(file_path, binary_header, info)
            file_list.append(processed_info)

        with binary_header.open(mode="a", encoding="utf-8") as f:
            f.write("#pragma once\n")
            f.write("#include <Arduino.h>\n")
            f.write("#include <functional>\n\n")
            f.write("using RouteRegistrationHandler = std::function<void(const String& uri, const String& contentType, const uint8_t * content, size_t len)>;\n\n")
            f.write("class WWWData {\n    public:\n        static void registerRoutes(RouteRegistrationHandler handler) {\n")
            for item in file_list:
                f.write(f'            handler("{item["uri"]}", "{item["mimeType"]}", {item["variable"]}, {item["size"]});\n')
            f.write("        }\n};\n")

HtmlHeaderProcessor().process()