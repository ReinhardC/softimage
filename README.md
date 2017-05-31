# Softimage XSI♰ plugin projects

please report an issue if you find one!
 
github repository so far has only 1 project in it:

* *"Additional File Formats"*

features
-------------------------
* STL export 
* STL import
* OBJ import (with vertex color/UV/weightmap support)
* OBJ export (higher speed/with vertex color/UV/weightmap support)
* experimental PLY import (vertex color/UV support)

installation instructions:
---------------------
* drag and drop xsiaddon files from project release section into application

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

