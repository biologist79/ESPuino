# -*- coding: utf-8 -*-

"""
Use this script for creating binary header files from html files.
"""

from pathlib import Path
import os
import shutil
import mimetypes
import gzip
Import("env")  # pylint: disable=undefined-variable

try:
    from flask_minify.parsers import Parser
except ImportError:
  print("Trying to Install required module: flask_minify\nIf this failes, please execute \"pip install flask_minify\" manually.")
  env.Execute("$PYTHONEXE -m pip install flask_minify")

from flask_minify.parsers import Parser
import json

try:
    import minify_html
except ImportError:
  print("Trying to Install required module: minify_html\nIf this failes, please execute \"pip install minify_html\" manually.")
  env.Execute("$PYTHONEXE -m pip install minify_html")
import minify_html

OUTPUT_DIR = (
    Path(env.subst("$BUILD_DIR")) / "generated"
)  # pylint: disable=undefined-variable
HTML_DIR = Path("html").absolute()
# List of files, which will only be minifed but not compressed (f.e. html files with templates)
WWW_FILES = []
# list of all files, which shall be compressed before embedding
# files with ".json" ending will be minifed before compression, ".js" and ".yaml" will not be changed!
BINARY_FILES =[
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
    Path("locales/fr.json")
]


class HtmlHeaderProcessor:
    """Create c code binary header files from HTML files"""

    @staticmethod
    def _escape_html(data):
        """Escape HTML characters for usage in C"""
        data = data.replace("\n", "\\n")
        data = data.replace('"', r"\"")
        data = data.replace(r"\d", r"\\d")
        data = data.replace(r"\.", r"\\.")
        data = data.replace(r"\^", r"\\^")
        data = data.replace("%;", "%%;")
        return data

    @classmethod
    def _process_header_file(cls, html_path, header_path):
        parser = Parser({}, True)
        parser.update_runtime_options(True, True, True)
        with html_path.open(mode="r", encoding="utf-8") as html_file:
            content = html_file.read()
            content = parser.minify(content, "html")    # minify content as html
            content = cls._escape_html(content)

        with header_path.open(mode="w", encoding="utf-8") as header_file:
            header_file.write(
                f"static const char {html_path.name.split('.')[0]}_HTML[] = \""
            )
            header_file.write(content)
            header_file.write('";\n')
        header_file.close()

    @classmethod
    def _process_binary_file(cls, binary_path, header_path, info):
        # minify json files explicitly
        if binary_path.suffix == ".json":
            with binary_path.open(mode="r", encoding="utf-8") as f:
                jsonObj = json.load(f)
                content = json.dumps(jsonObj, separators=(',', ':'))
        elif binary_path.suffix in [".htm", ".html", ".js", ".css"]:
            with open(binary_path, 'r') as f:
                content = f.read()
                # Minify the HTML, JS and CSS code
                content = minify_html.minify(content, minify_js=True, minify_css=True, remove_processing_instructions=True)
                # Write it to a file, just for debugging
                with open(str(binary_path).replace(binary_path.suffix, ".min" + binary_path.suffix), "w", encoding="utf-8") as f:
                    for line in content:
                        f.write(str(line))
        elif binary_path.suffix in [".yml", ".yaml"]:
            with open(binary_path, 'r') as f:
                content = f.read()
        # use everything else as is
        else:
            with binary_path.open(mode="r", encoding="utf-8") as f:
                content = f.read()

        # get status of the file (copy modified time to gzip header)
        stinfo = os.stat(binary_path)

        # compress content
        data = gzip.compress(content.encode(), mtime=stinfo.st_mtime)

        with header_path.open(mode="a", encoding="utf-8") as header_file:
            varName = binary_path.name.split('.')[0]
            header_file.write(
                f"static const uint8_t {varName}_BIN[] = {{\n    "
            )
            size = 0
            for d in data:
                # write out the compressed byte stream as a hex array and create a newline after every 20th entry
                header_file.write("0x{:02X},".format(d))
                size = size + 1
                if not (size % 20):
                    header_file.write("\n    ")
            header_file.write("\n};\n\n")
            # populate dict with our information
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

        shutil.copy2('REST-API.yaml', 'html/REST_API.yaml')  # Copy and rename the file with keeping the mtime

        if binary_header.exists():
            os.remove(binary_header)    # remove file if it exists, since we are appending to it

        fileList = []   # dict holding the array of metadata for all processed binary files
        for binary_file in BINARY_FILES:
            filePath = HTML_DIR / binary_file
            print(f"  {filePath} -> {binary_header}")

            info = dict()   # the dict entry for this file
            info["uri"] = "/" + filePath.relative_to(HTML_DIR).as_posix()
            if filePath.suffix == ".yaml":
                info["mimeType"] = "application/yaml" # mimetypes does not yet know yaml, see https://datatracker.ietf.org/doc/rfc9512/
            else:
                info["mimeType"] = mimetypes.types_map[filePath.suffix]
            info = cls._process_binary_file(HTML_DIR / binary_file, binary_header, info)
            fileList.append(info)

        with binary_header.open(mode="a", encoding="utf-8") as f:
            f.write("""using RouteRegistrationHandler = std::function<void(const String& uri, const String& contentType, const uint8_t * content, size_t len)>;

class WWWData {
    public:
        static void registerRoutes(RouteRegistrationHandler handler) {
""")
            for info in fileList:
                # write the fileList entries to the binary file. These will be the paramenter with which the handler is called to register the endpoint on the webserver
                f.write(f'            handler("{info["uri"]}", "{info["mimeType"]}", {info["variable"]}, {info["size"]});\n')
            f.write("        }\n};\n")


HtmlHeaderProcessor().process()
