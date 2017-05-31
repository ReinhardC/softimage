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
#include <xsi_clusterpropertybuilder.h>

#include <unordered_map>
#include <array>
#include <sstream>
#include <regex>

using namespace XSI;
using namespace std;

typedef struct _elementinfo {
	string name;
	long size, count;	
	vector<string> types;
	vector<string> list_element_type;
	vector<string> names;
} elementinfo;

class CPLY : public CFileFormat {

	FILE* m_file;

	bool read_header(FILE* file, vector<elementinfo*>&, bool&);
	void Output(FILE* file, string str);

public:
	string getFormatName() { return "PLY"; }

	CStatus Execute_Import(string initFilePathName, bool, bool, bool, bool);
	//CStatus Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportPolypaint, bool bSeparateFiles, bool bExportLocalCoords, bool bBinary);

};
