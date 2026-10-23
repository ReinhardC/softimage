# Softimage XSI♰ plugin projects

please report any issues!
 
github repository has 2 projects in it:

* *"Additional File Formats"* (below)
  this project was started because of factory OBJ export performance issues with large meshes
* *"XsiMcp"*: an MCP endpoint in Softimage, so an AI assistant such as Claude Code can
  run Python in Softimage and capture its window and viewports; see [XsiMcp/README.md](XsiMcp/README.md)

Additional File Formats features
-------------------------
* STL export (binary or ASCII, world or local coordinates, several objects into one file)
* STL import
* OBJ import (groups/objects, material clusters, MTL materials with textures, UVs, user normals, ZBrush polypaint and mask)
* OBJ export (UVs, user normals, ZBrush polypaint and mask, MTL file, optionally one file per object)
* PLY import (ASCII, binary little and big endian, vertex colors, per-face or multi-texture UVs)
* import replaces the mesh of the selected object (keeps its transform and properties), or creates new objects when nothing is selected
* large meshes are scaled down automatically on import (0.1 / 0.01 when wider than 55 / 150 units); switchable per format in the preferences
* MTL materials are created for the renderer of the current pass (mental ray and Arnold are implemented, see `ParseShaderTree_*` and `ApplyShaderTree_*` in the JScript plugin to add another one)

commands (for scripting)
-------------------------
* `ImportOBJ(ObjectToReplace, FileName, ImportUVs, ImportUserNormals, ImportZBrushMask, ImportZBrushPolypaint, CreateObjectsTag, CreateClustersTag, AutoScale)`
* `ExportOBJ(Objects, FileName, ExportMask, ExportColorAtVertices, ExportToSeparateFiles, WriteMTLFile, UseLocalCoords)`
* `ImportSTL(ObjectToReplace, FileName, AutoScale)`
* `ExportSTL(Objects, FileName, ExportBinaryFormat, UseLocalCoords)`
* `ImportPLY(FileName, ImportUVs, ImportVertexColors, AutoScale)`

an empty object collection means the current selection: deselect all before importing to create new objects.

installation instructions:
---------------------
* drag and drop xsiaddon files from project release section into application
* when reinstalling, uninstall first using the instructions below

uninstallation instructions:
---------------------
* use File > addons > uninstall
* delete the custom preference "Preferences > Custom > File Format Options" (right click > delete)
* delete the custom preference preset file "C:\Users\\[your_username]\Autodesk\Softimage_2015_R2-SP2\Data\Preferences\File Format Options.Preset" 

the preferences (File > Import/Export > File Format Options) hold the default and last used folders, the export options and the OBJ import options.
When upgrading from a version before 4.6 delete the preset file named above once, so that the new options get their defaults.

build instructions:
-------------------------
* set XSISDK_ROOT environment variable to path of your XSISDK directory
* open .sln file in Visual Studio 2019 or newer (the project uses the v142 toolset, pick another one in the project settings if needed)
* set build configuration to x64 (release or debug)
* build solution (the post-build step copies the DLL into the repository Workgroup folder)

debug instructions:
-------------------------
* *make sure all xsiaddons related to this project are uninstalled*
* if necessary, in the project *x64 debug configuration* settings, debugging section, change command to your xsi.exe [c:\program files\.....\Application\bin\xsi.exe]
* build the x64 debug configuration
* connect XSI to repository Workgroup
* set a test breakpoint, then press green triangle button (debug...) to connect to XSI
* after executing the command in XSI, your test breakpoint should be hit 

release history:
-------------------------
* Version 4.6 - large bugfix round:
  * OBJ import: `v//vn` faces, negative indices, tabs, `#comments` without a space and short lines are read correctly; material clusters follow `usemtl` across groups; objects are named after their group and created in file order; MTL materials are applied (never worked before) and texture options such as `-s` are skipped; point positions stay double; cancelling no longer crashes
  * OBJ export: `%` in names no longer corrupts the file, mask and color flags work separately, one file per object option
  * STL: ASCII export had no newline after `endfacet` and swapped axes compared to binary; binary export wrote uninitialised normals; the last triangle of an ASCII file without trailing newline was dropped
  * PLY: rewritten reader handles any property layout, all scalar types, big endian and Blender/MeshLab face texcoords
  * files are closed on every error path (they stayed locked until Softimage was closed); auto scaling works for new objects; auto scale preference; unsupported renderers fall back to default materials; Arnold roughness is converted to/from the MTL `Ns` exponent
* Version 4.5 - keep original point order for 1 mesh
* Version 4.0 - mesh replace feature for stl/obj (select mesh before import)
* Version 3.0 - bugfixes + added material support for obj 
* Version 2.3 - some speed improvements reading obj
* Version 2.0 - improved obj export speed, fixes progress bar
* Version 2.2 - more fixes reading obj, changed preferences (might have to delete File Format Preferences.prefs)
* Version 2.1 - fixes 1 off error reading obj




