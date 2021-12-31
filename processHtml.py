# -*- coding: utf-8 -*-
#!/usr/bin/python
###
# Use this script for creating PROGMEM header files from html files.
# needs pip install requests
##
# html file base names
import requests
import argparse

def str2bool(v):
    if isinstance(v, bool):
        return v
    if v.lower() in ('yes', 'true', 't', 'y', '1'):
        return True
    elif v.lower() in ('no', 'false', 'f', 'n', '0'):
        return False
    else:
        raise argparse.ArgumentTypeError('Boolean value expected.')

HTML_FILES = ["management_DE","management_EN", "accesspoint_DE", "accesspoint_EN"]

class htmlHeaderProcessor(object):

    """
    Returns a minified HTML string, uses html-minifier.com api.
    """
    def minifyHTML(self, filename):
        with open('html/' + filename + '.html', 'r') as r:
            data = r.read()
            return requests.post('https://html-minifier.com/raw', data=dict(input=data)).text.encode('utf8')

    def escape_html(self, data):
        data = data.replace('\n', '\\\n')
        data = data.replace('\"', '\\"')
        data = data.replace('\\d', '\\\d')
        data = data.replace('\\.', '\\\.')
        data = data.replace('\\^', '\\\\^')
        data = data.replace('%;', '%%;')
        return data

    def html_to_c_header(self, filename):
        content = ""
        with open('html/' + filename + '.html', 'r') as r:
            data = r.read()
            content += self.escape_html(data)
        return content


    def write_header_file(self, filename, content):
        with open('src/HTML' + filename + '.h', 'w') as w:
            varname = filename.split('_')[0]
            w.write("static const char " + varname + "_HTML[] PROGMEM = \"")
            w.write(content)
            w.write("\";")

    def main(self):
        parser = argparse.ArgumentParser(description='Create c code PROGMEM header files from HTML files.')
        parser.add_argument("--minify", type=str2bool, nargs='?',
                            const=True, default=False,
                            help="Minify HTML Code")
        args = parser.parse_args()

        for file in HTML_FILES:
            if args.minify:

                self.header_file_content = self.minifyHTML(file)
                self.header_file_content = self.escape_html(self.header_file_content)
            else:
                self.header_file_content = self.html_to_c_header(file)
            self.write_header_file(file, self.header_file_content)

if __name__ == '__main__':
    htmlHeaderProcessor().main()