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
    Path("js/i18next.min.js"),
    Path("js/i18nextHttpBackend.min.js"),
    Path("REST_API.yaml"),
    Path("swagger.html"),
    Path("js/swaggerInitializer.js"),
    Path("js/loc_i18next.min.js"),
    Path("locales/de.json"),
    Path("locales/en.json"),
    Path("locales/fr.json"),
    # Vendored frontend libraries for the management interface (previously loaded from CDN).
    # Self-hosted so the management interface no longer depends on internet access.
    Path("vendor/bootstrap/bootstrap.min.css"),
    Path("vendor/bootstrap/bootstrap.bundle.min.js"),
    Path("vendor/jquery/jquery.min.js"),
    Path("vendor/jqueryui/jquery-ui.min.js"),
    Path("vendor/jstree/jstree.min.js"),
    Path("vendor/jstree/light/style.min.css"),
    Path("vendor/jstree/light/32px.png"),
    Path("vendor/jstree/light/40px.png"),
    Path("vendor/jstree/light/throbber.gif"),
    Path("vendor/jstree/dark/style.min.css"),
    Path("vendor/jstree/dark/32px.png"),
    Path("vendor/jstree/dark/40px.png"),
    Path("vendor/jstree/dark/throbber.gif"),
    Path("vendor/fontawesome/css/all.min.css"),
    Path("vendor/fontawesome/webfonts/fa-solid-900.woff2"),
    Path("vendor/fontawesome/webfonts/fa-regular-400.woff2"),
    Path("vendor/fontawesome/webfonts/fa-brands-400.woff2"),
    Path("vendor/bootstrap-slider/bootstrap-slider.min.css"),
    Path("vendor/bootstrap-slider/bootstrap-slider.min.js"),
    Path("vendor/i18next/i18next.min.js"),
    Path("vendor/i18next/i18nextHttpBackend.min.js"),
    Path("vendor/i18next/loc-i18next.min.js"),
    Path("vendor/natsort/natcompare.js"),
    # Default branding (logo/favicon), previously served via an external redirect to espuino.de - embedded
    # so the management interface works fully offline too (e.g. AP-mode, no internet available).
    Path("vendor/branding/logo.webp"),
    Path("vendor/branding/favicon.ico"),
]

# mimetypes.types_map doesn't reliably know these on every Python version.
EXTRA_MIME_TYPES = {
    ".woff2": "font/woff2",
    ".woff": "font/woff",
    ".ttf": "font/ttf",
    ".png": "image/png",
    ".gif": "image/gif",
    ".webp": "image/webp",
    ".ico": "image/x-icon",
}

# Suffixes that are text and safe to decode/minify as UTF-8; everything else (fonts, images, ...)
# is treated as opaque binary data and passed through unmodified.
TEXT_SUFFIXES = {".htm", ".html", ".js", ".css", ".yaml", ".yml"}

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
            raw = content.encode()
        elif binary_path.suffix in [".htm", ".html", ".js", ".css"]:
            with open(binary_path, 'r', encoding="utf-8") as f:
                content = f.read()
                if ".min" not in str(binary_path):
                    content = cls._safe_minify(content, binary_path.suffix)
            raw = content.encode()
        elif binary_path.suffix in TEXT_SUFFIXES:
            with binary_path.open(mode="r", encoding="utf-8") as f:
                raw = f.read().encode()
        else:
            # Opaque binary asset (webfont, image, ...): pass through unmodified, no text decoding.
            with binary_path.open(mode="rb") as f:
                raw = f.read()

        stinfo = os.stat(binary_path)
        data = gzip.compress(raw, mtime=stinfo.st_mtime)

        with header_path.open(mode="a", encoding="utf-8") as header_file:
            # Derive a unique, valid C identifier from the full path relative to HTML_DIR, extension
            # included (not just the filename stem) so files sharing a basename in different directories
            # (e.g. "32px.png" for both the light and dark jstree theme) don't collide, and so files that
            # only differ by extension (e.g. "bootstrap-slider.min.css" vs "...min.js") stay distinct.
            # Names never start with a digit (invalid C identifier).
            rel_full = binary_path.relative_to(HTML_DIR).as_posix()
            varName = re.sub(r'[^a-zA-Z0-9]', '_', rel_full)
            if varName[0].isdigit():
                varName = "_" + varName
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
                "mimeType": EXTRA_MIME_TYPES.get(
                    file_path.suffix,
                    mimetypes.types_map.get(file_path.suffix, "application/octet-stream")
                )
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