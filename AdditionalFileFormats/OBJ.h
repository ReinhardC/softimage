#pragma once

#include "FileFormat.h"
 
#include "ObjPreferences.h"

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
#include <xsi_vector3.h>
#include <xsi_comapihandler.h>
#include <xsi_kinematics.h>
#include <xsi_material.h>
#include <xsi_shader.h>
#include <xsi_color4f.h>
#include <xsi_color.h>
#include <xsi_point.h>
#include <xsi_polygonface.h>
#include <xsi_preferences.h>
#include <xsi_clusterpropertybuilder.h>

#include <string>
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

class MeshData {
public:
	MeshData(string name) { this->name = name; }

	string name;

	bool bIsDefaultMesh = true; // has not been initialized by an "o" tag yet

	long ixPolygon = 0;
	vector<long> PolygonPointCounts;
	vector<double> PointPositions;
	vector<long> PointIndices;
	long next_meshIx = 0;
	unordered_map<long, long> meshIxFromFileIx_lookup;
	unordered_map<long, long> fileIxFromMeshIx_lookup;
	unordered_map<string, CLongArray> IcsMaterialClusters;

	bool bHasNormals = false;
	vector <float> Normals;

	bool bHasUVs = false;
	vector<float> UVs;

	vector<float> RGBA;
	vector<float> Mask;
};

class COBJ : public CFileFormat
{
	FILE* m_pFile;
	FILE* m_matfile;

	// polypaint is per file (all vertices in a file), uv and normals are per object
	bool bFileHasUVs = false;
	bool bFileHasNormals = false;
	bool bFileHasPolypaint = false;

	vector<double> PP_inFile;
	vector<float> RGB_inFile;
	vector<float> Mask_inFile;

	MATH::CVector3 vAutoScaling; // auto scale meshes

public:	
	ObjPreferences Prefs;

	unordered_map<tr1::array<float, 3>, int, f3hash> m_hashmap_uv, m_hashmap_normals;
	size_t m_vertices_base_ix_group = 0, m_uvs_base_ix_group = 0, m_normals_base_ix_group = 0;

	void Output(FILE* file, string str);
	ULONG hex2dec(char hex);

public:
	string getFormatName() { return "OBJ"; };
	CStatus Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportMask, bool bExportPolypaint, bool bSeparateFiles, bool bWriteMTLFile, bool bExportLocalCoords);
	CStatus Execute_Import(CRefArray& inObjects, string filePathNam);
	CStatus Import(string filePathNam, 
		unordered_map<string, MeshData*>& mesh_map, 
		unordered_map<string, string>& material_map,
		string& mat_file);
};

