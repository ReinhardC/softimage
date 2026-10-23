# Builds release/AdditionalFileFormats.xsiaddon from this repository. Run inside Softimage,
# from the Script Editor (Python): execfile(r'<repo>\AdditionalFileFormats\build_addon.py')
# The DLL is taken from the repository Workgroup folder, where the x64 Release build's
# post-build step copies it, so build the solution first.
# PackageAddon packages an add-on folder with its layout. It wants the folder inside an
# Addons folder, so the files are staged in a temporary one; then the package's item
# descriptions (full local paths) are replaced by file names and its metadata set.
import os
import re
import shutil
import sys
import tempfile

# execfile sets no __file__; the code object knows which file it came from.
REPO = os.path.dirname(os.path.dirname(os.path.abspath(sys._getframe().f_code.co_filename)))
SOURCE = os.path.join(REPO, 'Workgroup', 'Addons', 'AdditionalFileFormats')
FILES = (r'Application\Plugins\AdditionalFileFormats.64.dll', r'Application\Plugins\AdditionalFileFormats.js')
OUT = os.path.join(REPO, 'release', 'AdditionalFileFormats.xsiaddon')
METADATA = (('Title', 'Additional File Formats 4.7'), ('AuthorName', 'ReinhardC'),
            ('URL', 'https://github.com/ReinhardC/softimage'),
            ('Summary', 'OBJ, STL and PLY import/export for Softimage (ZBrush polypaint and mask, MTL materials, mesh replace)'))


def build():
    for rel in FILES:
        if not os.path.isfile(os.path.join(SOURCE, rel)):
            raise RuntimeError('missing %s (build the x64 Release configuration first)' % os.path.join(SOURCE, rel))

    base = tempfile.mkdtemp(prefix='aff-')
    try:
        stage = os.path.join(base, 'Addons', 'AdditionalFileFormats')
        for rel in FILES:
            target = os.path.join(stage, rel)
            if not os.path.isdir(os.path.dirname(target)):
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
