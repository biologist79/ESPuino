#!/usr/bin/python

content = ''

with open('html/website.html', 'r') as r:
    data = r.read().replace('\n', '\\\n')
    #content += '"'
    #data = r.read().replace('\n', '"\n')
    data = data.replace('\"', '\\"')
    content += data

with open('src/websiteMgmt.h', 'w') as w:
    w.write("static const char mgtWebsite[] PROGMEM = \"")
    w.write(content)
    w.write("\";")

r.close()
w.close()