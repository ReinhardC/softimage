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
#include <xsi_material.h>
#include <xsi_shader.h>
#include <xsi_color4f.h>
#include <xsi_color.h>
#include <xsi_point.h>
#include <xsi_clusterpropertybuilder.h>

#include <unordered_map>
#include <utility>
#include <array>
#include <sstream>
#include <regex>

using namespace XSI;
using namespace std;

typedef struct _ObjVtx {
	float x, y, z;	
} ObjVtx;

class COBJ : public CFileFormat
{
	FILE* m_file;
	FILE* m_matfile;
	unordered_map<tr1::array<float, 3>, int, f3hash> m_hashmap_uv, m_hashmap_normals;
	size_t m_vertices_base_ix_group = 0, m_uvs_base_ix_group = 0, m_normals_base_ix_group = 0;

	void Output(FILE* file, string str);
	ULONG hex2dec(char hex);

public:
	string getFormatName() { return "OBJ"; };
	CStatus Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportPolypaint, bool bSeparateFiles, bool bWriteMTLFile, bool bExportLocalCoords);
	CStatus Execute_Import(string initFilePathName, bool bImportUVs, bool bImportUserNormals, bool bImportMask, bool bImportPolypaint);
};
