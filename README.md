# softimage
Softimage XSI♰ plugin projects

Repository so far has only 1 project in it:

"Additional File Formats" features
-------------------------
* STL export 
* STL import
* OBJ import (with vertex color/UV/weightmap support)
* OBJ export (higher speed/with vertex color/UV/weightmap support)
* PLY import (vertex color/UV support) - experimental version

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
* first make sure all xsiaddons related to the project are uninstalled
* in project x64 debug configuration settings, debugging section, change command to your xsi.exe [c:\program files\.....\Application\bin\xsi.exe]
* build the x64 debug configuration
* connect XSI to repository Workgroup
* set a test breakpoint, then press green triangle button to connect to XSI
* after executing the command in XSI, the breakpoint should be hit 

