# Builds release/XsiMcp.xsiaddon from this repository. Run inside Softimage, from the
# Script Editor (Python): execfile(r'<repo>\XsiMcp\build_addon.py')
# PackageAddon packages an add-on folder with its layout. It wants the folder inside an
# Addons folder, so the two files are staged in a temporary one; then the package's
# item descriptions (full local paths) are replaced by file names and its metadata set.
import os
import re
import shutil
import sys
import tempfile

# execfile sets no __file__; the code object knows which file it came from.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(sys._getframe().f_code.co_filename)))
SOURCE = os.path.join(REPO, 'Workgroup', 'Addons', 'XsiMcp')
FILES = (r'Application\Plugins\XsiMcp.py', r'Application\xsi_mcp\xsi_mcp_server.py')
OUT = os.path.join(REPO, 'release', 'XsiMcp.xsiaddon')
METADATA = (('Title', 'XsiMcp 1.0'), ('AuthorName', 'ReinhardC'),
            ('URL', 'https://github.com/ReinhardC/softimage'),
            ('Summary', 'An MCP endpoint in Softimage for AI assistants such as Claude Code'))


def build():
    base = tempfile.mkdtemp(prefix='xsimcp-')
    try:
        stage = os.path.join(base, 'Addons', 'XsiMcp')
        for rel in FILES:
            target = os.path.join(stage, rel)
            os.makedirs(os.path.dirname(target))
            shutil.copy2(os.path.join(SOURCE, rel), target)
        if not os.path.isdir(os.path.dirname(OUT)):
            os.makedirs(os.path.dirname(OUT))
        if os.path.exists(OUT):
            os.remove(OUT)
        message = Application.PackageAddon(stage, OUT, True)
        if not os.path.isfile(OUT):
            raise RuntimeError('PackageAddon wrote nothing: %s' % message)
    finally:
        shutil.rmtree(base, True)

    with open(OUT, 'rb') as f:
        xml = f.read()
    xml = re.sub(r'(<AddonItem filename="([^"]+)"[^>]*?) description="[^"]*"', r'\1 description="\2"', xml)
    for tag, value in METADATA:
        xml = xml.replace('<%s></%s>' % (tag, tag), '<%s>%s</%s>' % (tag, value, tag), 1)
    items = re.findall(r'<AddonItem filename="([^"]+)" destination="([^"]+)"', xml)
    if sorted(items) != sorted((os.path.basename(r), '\\' + os.path.dirname(r)) for r in FILES):
        raise RuntimeError('unexpected package items: %s' % items)
    if base.split(os.sep)[-1] in xml or os.path.expanduser('~') in xml:
        raise RuntimeError('a local path is left in the package')
    with open(OUT, 'wb') as f:
        f.write(xml)
    return OUT, items


print 'built %s with %s' % build()
