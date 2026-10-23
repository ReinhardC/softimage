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

// one "property" line of the PLY header
struct plyprop {
	string name;
	string type;      // scalar type, or the entry type of a list property
	string countType; // list properties only: type of the entry count
	bool isList;
};

// one "element" line of the PLY header together with its properties
struct elementinfo {
	string name;
	long count;
	vector<plyprop> props;
};

class CPLY : public CFileFormat {

	FILE* m_pFile;

	bool m_bBinary, m_bBigEndian;
	vector<string> m_tokens; // ASCII format: tokens of the data line being read
	size_t m_tokIx;          // ASCII format: next token to consume

	// everything read from the file before the mesh is built
	struct PlyData {
		vector<double> positions;    // x y z per vertex
		vector<float> rgb;           // r g b per vertex (0..1), empty if the file has no vertex colors
		vector<long> counts;         // points per polygon
		vector<long> indices;        // point indices of all polygons
		vector<float> rgbaPerSample; // vertex colors per polygon corner
		vector<float> uvwPerSample;  // u v w per polygon corner
		vector<float> multiTexUVs;   // u v per multi_texture_vertex of texture 0
		double minX, maxX;           // for auto scale
	};

	bool read_header(vector<elementinfo>& out_elinfo);
	bool readScalar(const string& type, double& out);
	bool readElement(const elementinfo& el, vector<double>& scalars, vector<vector<double> >& lists);
	CStatus readBody(const vector<elementinfo>& header, PlyData& data);

public:
	string getFormatName() { return "PLY"; }

	CStatus Execute_Import(string initFilePathName, bool bImportUVs, bool bImportVertexColors);
};
