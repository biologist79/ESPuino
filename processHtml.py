#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Use this script for creating PROGMEM header files from html files.
"""
from pathlib import Path

SRC_DIR = Path("src")
HTML_DIR = Path("html")
HTML_FILES = [
    Path("management_DE.html"),
    Path("management_EN.html"),
    Path("accesspoint_DE.html"),
    Path("accesspoint_EN.html"),
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
                f"static const char {html_path.name.split('_')[0]}_HTML[] PROGMEM = \""
            )
            header_file.write(content)
            header_file.write('";')

    @classmethod
    def process(cls):
        print("GENERATING HTML HEADER FILES")
        for html_file in HTML_FILES:
            print(f"  {HTML_DIR / html_file}")
            cls._process_header_file(
                HTML_DIR / html_file, SRC_DIR / f"HTML{html_file.stem}.h"
            )


if __name__ in ["__main__", "SCons.Script"]:
    HtmlHeaderProcessor().process()
