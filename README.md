# Softimage XSI♰ plugin projects

please report any issues!
 
github repository so far has only 1 project in it:

* *"Additional File Formats"*
  this project was started because of factory OBJ export performance issues with large meshes

features
-------------------------
* STL export 
* STL import
* OBJ import (vertex color/UV/weightmap support)
* OBJ export (vertex color/UV/weightmap support)
* experimental PLY import (vertex color/UV support)

installation instructions:
---------------------
* drag and drop xsiaddon files from project release section into application
* when reinstalling, uninstall first using the instructions below

uninstallation instructions:
---------------------
* use File > addons > uninstall
* delete the custom preference "Preferences > Custom > File Format Options" (right click > delete)
* delete the custom preference preset file "C:\Users\\[your_username]\Autodesk\Softimage_2015_R2-SP2\Data\Preferences\File Format Options.Preset" 

build instructions:
-------------------------
* set XSISDK_ROOT environment variable to path of your XSISDK directory
* open .sln file in VS2017 community edition or compatible
* set build configuration to x64 (release or debug)
* build solution

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
* Version 4.0 - mesh replace feature for stl/obj (select mesh before import)
* Version 3.0 - bugfixes + added material support for obj 
* Version 2.3 - some speed improvements reading obj
* Version 2.0 - improved obj export speed, fixes progress bar
* Version 2.2 - more fixes reading obj, changed preferences (might have to delete File Format Preferences.prefs)
* Version 2.1 - fixes 1 off error reading obj




