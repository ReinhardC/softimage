#pragma once

#include "FileFormat.h"

#include <xsi_pluginregistrar.h>
#include <xsi_status.h>
#include <xsi_decl.h>

#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_status.h>
#include <xsi_string.h>
#include <xsi_command.h>
#include <xsi_model.h>
#include <xsi_parameter.h>
#include <xsi_x3dobject.h>
#include <xsi_selection.h>
#include <xsi_primitive.h>
#include <xsi_polygonmesh.h>
#include <xsi_geometryaccessor.h>
#include <xsi_floatarray.h>
#include <xsi_doublearray.h>
#include <xsi_customproperty.h>
#include <xsi_ppglayout.h>
#include <xsi_meshbuilder.h>
#include <xsi_math.h>
#include <xsi_comapihandler.h>
#include <xsi_kinematics.h>

#include <unordered_map>
#include <array>
#include <sstream>
#include <regex>

using namespace XSI;
using namespace std;

typedef struct _StlTri {
	float v[4][3];
	unsigned short attrCount;
} StlTri;

class CSTL : public CFileFormat {

	FILE* m_pFile;

public:
	string getFormatName() { return "STL"; }

	CStatus Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportBinary, bool bExportLocalCoords);
	CStatus Execute_Import(CRefArray& inObjects, string initFilePathName);

private:

	void parseAsciiFacet(StlTri& nextTriangle, FILE* file);
	void Output(FILE* file, string str);
};
