#!/usr/bin/python

content = ''
content2 = ''
contentEN = ''
content2EN = ''

with open('html/website.html', 'r') as r:
    data = r.read().replace('\n', '\\\n')
    data = data.replace('\"', '\\"')
    data = data.replace('\\d', '\\\d')
    data = data.replace('\\.', '\\\.')
    data = data.replace('\\^', '\\\\^')
    content += data

with open('src/websiteMgmt.h', 'w') as w:
    w.write("static const char mgtWebsite[] PROGMEM = \"")
    w.write(content)
    w.write("\";")

with open('html/website_EN.html', 'r') as ren:
    data = ren.read().replace('\n', '\\\n')
    data = data.replace('\"', '\\"')
    data = data.replace('\\d', '\\\d')
    data = data.replace('\\.', '\\\.')
    data = data.replace('\\^', '\\\\^')
    contentEN += data

with open('src/websiteMgmt_EN.h', 'w') as wen:
    wen.write("static const char mgtWebsite[] PROGMEM = \"")
    wen.write(contentEN)
    wen.write("\";")

with open('html/websiteBasic.html', 'r') as r2:
    data = r2.read().replace('\n', '\\\n')
    data = data.replace('\"', '\\"')
    content2 += data

with open('src/websiteBasic.h', 'w') as w2:
    w2.write("static const char basicWebsite[] PROGMEM = \"")
    w2.write(content2)
    w2.write("\";")

with open('html/websiteBasic_EN.html', 'r') as r2en:
    data = r2en.read().replace('\n', '\\\n')
    data = data.replace('\"', '\\"')
    content2EN += data

with open('src/websiteBasic_EN.h', 'w') as w2en:
    w2en.write("static const char basicWebsite[] PROGMEM = \"")
    w2en.write(content2EN)
    w2en.write("\";")

r.close()
w.close()
r2.close()
w2.close()