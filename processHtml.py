# -*- coding: utf-8 -*-

"""
Use this script for creating PROGMEM header files from html files.
"""

from pathlib import Path
import os
import mimetypes

Import("env")  # pylint: disable=undefined-variable

OUTPUT_DIR = (
    Path(env.subst("$BUILD_DIR")) / "generated"
)  # pylint: disable=undefined-variable
HTML_DIR = Path("dist").absolute()
WWW_FILES = [
    Path("management.html"),
    Path("accesspoint.html"),
]
BINARY_FILES =[
    Path("js/i18next.min.js.gz"),
    Path("js/i18nextHttpBackend.min.js.gz"),
    Path("js/loc_i18next.min.js.gz"),
    Path("locales/de.json.gz"),
    Path("locales/en.json.gz")
]


class HtmlHeaderProcessor:
    """Create c code PROGMEM header files from HTML files"""

    @staticmethod
    def _escape_html(data):
        """Escape HTML characters for usage in C"""
        data = data.replace("\n", "\\\n")
        data = data.replace('"', r"\"")
        data = data.replace(r"\d", r"\\d")
        data = data.replace(r"\.", r"\\.")
        data = data.replace(r"\^", r"\\^")
        data = data.replace("%;", "%%;")
        return data

    @classmethod
    def _process_header_file(cls, html_path, header_path):
        with html_path.open("r") as html_file:
            content = cls._escape_html(html_file.read())

        with header_path.open("w") as header_file:
            header_file.write(
                f"static const char {html_path.name.split('.')[0]}_HTML[] PROGMEM = \""
            )
            header_file.write(content)
            header_file.write('";')
        header_file.close()

    @classmethod
    def _process_binary_file(cls, binary_path, header_path, info):
        with binary_path.open("rb") as f:
            content = f.read()

        with header_path.open("a") as header_file:
            path = binary_path.relative_to(HTML_DIR).as_posix()
            path = path[:-len(".gz")]
            varName = binary_path.name.split('.')[0]
            header_file.write(
                f"static const uint8_t {varName}_BIN[] PROGMEM = {{"
            )
            size = 0
            for d in content:
                header_file.write("0x{:02X},".format(d))
                if not (size % 20):
                    header_file.write("\n    ")
                size = size + 1
            header_file.write("\n};\n\n")
            info["size"] = size
            info["variable"] = f"{varName}_BIN"
            return info

    @classmethod
    def process(cls):
        print("GENERATING HTML HEADER FILES")
        for html_file in WWW_FILES:
            header_file = f"HTML{html_file.stem}.h"
            print(f"  {HTML_DIR / html_file} -> {OUTPUT_DIR / header_file}")
            cls._process_header_file(HTML_DIR / html_file, OUTPUT_DIR / header_file)
        binary_header = OUTPUT_DIR / "HTMLbinary.h"
        if binary_header.exists():
            os.remove(binary_header)

        files = []
        for binary_file in BINARY_FILES:
            fp = HTML_DIR / binary_file
            print(f"  {fp} -> {binary_header}")
            # we always deal with compressed files here
            uri = fp.relative_to(HTML_DIR).as_posix()
            uri = uri[:-len(".gz")]
            info = dict()
            info["uri"] = "/" + uri
            info["mimeType"] = mimetypes.types_map[Path(uri).suffix]
            info = cls._process_binary_file(HTML_DIR / binary_file, binary_header, info)
            files.append(info)

        with binary_header.open("a") as f:
            f.write("""typedef std::function<void(const String& uri, const String& contentType, const uint8_t * content, size_t len)> RouteRegistrationHandler;

class WWWData {
    public:
        static void registerRoutes(RouteRegistrationHandler handler) {
""")
            for info in files:
                f.write(f'            handler("{info["uri"]}", "{info["mimeType"]}", {info["variable"]}, {info["size"]});\n')
            f.write("        }\n};\n")


HtmlHeaderProcessor().process()
