#include "PLY.h"

#include <map>
#include <vector>
#include <utility>
#include <cstring>

using namespace std;

// size in bytes of a PLY scalar type (0: unknown type)
static int plyTypeSize(const string& t)
{
	if (t == "char" || t == "uchar" || t == "int8" || t == "uint8" || t == "uchar8") return 1;
	if (t == "short" || t == "ushort" || t == "int16" || t == "uint16") return 2;
	if (t == "int" || t == "uint" || t == "int32" || t == "uint32" || t == "float" || t == "float32") return 4;
	if (t == "double" || t == "float64" || t == "double64" || t == "int64" || t == "uint64") return 8;
	return 0;
}

// true for the 8-bit integer types, which hold colors in the 0..255 range
static bool plyIsByteType(const string& t)
{
	return t == "char" || t == "uchar" || t == "int8" || t == "uint8" || t == "uchar8";
}

// reads one line of any length into out; returns false when the file ends before anything was read
static bool readLine(FILE* file, string& out)
{
	out.clear();
	char buf[1024];
	while (fgets(buf, sizeof(buf), file)) {
		out += buf;
		if (out[out.size() - 1] == '\n')
			break;
	}
	return !out.empty();
}

// splits on any whitespace (space, tab, CR, LF)
static vector<string> splitWhitespace(const string& line)
{
	vector<string> tokens;
	istringstream ss(line);
	string tok;
	while (ss >> tok)
		tokens.push_back(tok);
	return tokens;
}

// index of the property called name (or altName); -1 if the element has no such property
static int findProp(const elementinfo& el, const char* name, const char* altName = NULL)
{
	for (size_t p = 0; p < el.props.size(); p++)
		if (el.props[p].name == name || (altName && el.props[p].name == altName))
			return (int)p;
	return -1;
}

bool CPLY::read_header(vector<elementinfo>& out_elinfo)
{
	Application app;

	m_bBinary = false;
	m_bBigEndian = false;

	string line;
	bool bEndHeader = false;
	while (!bEndHeader && readLine(m_pFile, line))
	{
		vector<string> tokens = splitWhitespace(line);
		if (tokens.empty())
			continue;

		if (tokens[0] == "end_header")
			bEndHeader = true;
		else if (tokens[0] == "format" && tokens.size() >= 2) {
			m_bBinary = (tokens[1] != "ascii");
			m_bBigEndian = (tokens[1] == "binary_big_endian");
		}
		else if (tokens[0] == "element" && tokens.size() >= 3) {
			elementinfo el;
			el.name = tokens[1];
			el.count = atol(tokens[2].c_str());
			out_elinfo.push_back(el);
		}
		else if (tokens[0] == "property") {
			string strLine(line);
			trim(strLine);

			if (out_elinfo.empty()) {
				app.LogMessage("PLY header: property before any element ('" + CString(strLine.c_str()) + "').", siErrorMsg);
				return false;
			}

			plyprop prop;
			if (tokens.size() >= 5 && tokens[1] == "list") {
				prop.isList = true;
				prop.countType = tokens[2];
				prop.type = tokens[3];
				prop.name = tokens[4];
			}
			else if (tokens.size() >= 3 && tokens[1] != "list") {
				prop.isList = false;
				prop.type = tokens[1];
				prop.name = tokens[2];
			}
			else {
				app.LogMessage("PLY header: malformed property line '" + CString(strLine.c_str()) + "'.", siErrorMsg);
				return false;
			}

			if (plyTypeSize(prop.type) == 0 || (prop.isList && plyTypeSize(prop.countType) == 0)) {
				app.LogMessage("PLY header: unsupported property type in '" + CString(strLine.c_str()) + "'.", siErrorMsg);
				return false;
			}

			out_elinfo.back().props.push_back(prop);
		}
		// "ply", "comment", "obj_info" lines are ignored
	}

	if (!bEndHeader) {
		app.LogMessage(L"PLY header: end_header not found.", siErrorMsg);
		return false;
	}

	return true;
}

// reads one scalar of the given type: binary from the file, ASCII from the tokens of the current line
bool CPLY::readScalar(const string& type, double& out)
{
	if (m_bBinary) {
		int size = plyTypeSize(type);
		unsigned char buf[8];
		if (size == 0 || fread(buf, 1, size, m_pFile) != (size_t)size)
			return false;
		if (m_bBigEndian)
			reverse(buf, buf + size);

		if (type == "char" || type == "int8") { signed char v; memcpy(&v, buf, 1); out = v; }
		else if (type == "uchar" || type == "uint8" || type == "uchar8") { unsigned char v; memcpy(&v, buf, 1); out = v; }
		else if (type == "short" || type == "int16") { short v; memcpy(&v, buf, 2); out = v; }
		else if (type == "ushort" || type == "uint16") { unsigned short v; memcpy(&v, buf, 2); out = v; }
		else if (type == "int" || type == "int32") { int v; memcpy(&v, buf, 4); out = v; }
		else if (type == "uint" || type == "uint32") { unsigned int v; memcpy(&v, buf, 4); out = v; }
		else if (type == "float" || type == "float32") { float v; memcpy(&v, buf, 4); out = v; }
		else if (type == "double" || type == "float64" || type == "double64") { double v; memcpy(&v, buf, 8); out = v; }
		else if (type == "int64") { long long v; memcpy(&v, buf, 8); out = (double)v; }
		else if (type == "uint64") { unsigned long long v; memcpy(&v, buf, 8); out = (double)v; }
		else return false;

		return true;
	}
	else {
		if (m_tokIx >= m_tokens.size())
			return false;
		out = atof(m_tokens[m_tokIx++].c_str());
		return true;
	}
}

// reads one item of an element: scalars[p] / lists[p] receive the value(s) of property p
bool CPLY::readElement(const elementinfo& el, vector<double>& scalars, vector<vector<double> >& lists)
{
	if (!m_bBinary) {
		// one data line per item; blank lines are skipped
		string line;
		m_tokens.clear();
		while (m_tokens.empty()) {
			if (!readLine(m_pFile, line))
				return false;
			m_tokens = splitWhitespace(line);
		}
		m_tokIx = 0;
	}

	for (size_t p = 0; p < el.props.size(); p++) {
		const plyprop& prop = el.props[p];
		if (prop.isList) {
			double n;
			if (!readScalar(prop.countType, n) || n < 0)
				return false;
			lists[p].resize((size_t)n);
			for (size_t k = 0; k < (size_t)n; k++)
				if (!readScalar(prop.type, lists[p][k]))
					return false;
		}
		else if (!readScalar(prop.type, scalars[p]))
			return false;
	}

	return true;
}

CStatus CPLY::readBody(const vector<elementinfo>& header, PlyData& data)
{
	Application app;

	data.minX = DBL_MAX;
	data.maxX = -DBL_MAX;

	long totalItems = 0;
	for (size_t e = 0; e < header.size(); e++)
		totalItems += header[e].count;
	m_progress.PutMaximum(totalItems);
	m_progress.PutValue(0);

	for (size_t iElem = 0; iElem < header.size(); iElem++)
	{
		const elementinfo& el = header[iElem];

		vector<double> scalars(el.props.size(), 0.0);
		vector<vector<double> > lists(el.props.size());

		m_progress.PutCaption("Importing PLY (" + CString(el.name.c_str()) + ")");

		// property lookups, resolved once per element
		int ixX = findProp(el, "x"), ixY = findProp(el, "y"), ixZ = findProp(el, "z");
		int ixR = findProp(el, "r", "red"), ixG = findProp(el, "g", "green"), ixB = findProp(el, "b", "blue");
		int ixVertexIndices = findProp(el, "vertex_indices", "vertex_index");
		int ixTexcoord = findProp(el, "texcoord");
		int ixU = findProp(el, "u"), ixV = findProp(el, "v"), ixTx = findProp(el, "tx");
		int ixTexVertexIndices = findProp(el, "texture_vertex_indices");

		bool bHasColors = ixR >= 0 && ixG >= 0 && ixB >= 0;
		bool bColorsAreBytes = bHasColors && plyIsByteType(el.props[ixR].type);

		if (el.name == "vertex" && (ixX < 0 || ixY < 0 || ixZ < 0)) {
			app.LogMessage(L"PLY: the vertex element has no x, y, z properties.", siErrorMsg);
			return CStatus::Fail;
		}
		if (el.name == "face" && (ixVertexIndices < 0 || !el.props[ixVertexIndices].isList)) {
			app.LogMessage(L"PLY: the face element has no vertex_indices list.", siErrorMsg);
			return CStatus::Fail;
		}

		for (long iData = 0; iData < el.count; iData++) {

			if (!readElement(el, scalars, lists)) {
				app.LogMessage("PLY: unexpected end of file or malformed data in element '" + CString(el.name.c_str()) + "', item " + CString(iData + 1) + " of " + CString(el.count) + ".", siErrorMsg);
				return CStatus::Fail;
			}

			if (el.name == "vertex") {
				double x = scalars[ixX];
				data.positions.push_back(x);
				data.positions.push_back(scalars[ixY]);
				data.positions.push_back(scalars[ixZ]);

				if (x > data.maxX) data.maxX = x;
				if (x < data.minX) data.minX = x;

				if (bHasColors) {
					double scale = bColorsAreBytes ? 1.0 / 255.0 : 1.0;
					data.rgb.push_back((float)(scalars[ixR] * scale));
					data.rgb.push_back((float)(scalars[ixG] * scale));
					data.rgb.push_back((float)(scalars[ixB] * scale));
				}
			}
			else if (el.name == "face") {
				const vector<double>& vertexIndices = lists[ixVertexIndices];
				long nbVertices = (long)(data.positions.size() / 3);
				long nbCorners = (long)vertexIndices.size();
				if (nbCorners < 3)
					continue; // degenerate face

				data.counts.push_back(nbCorners);
				for (long k = 0; k < nbCorners; k++) {
					long index = (long)vertexIndices[k];
					if (index < 0 || index >= nbVertices) {
						app.LogMessage("PLY: face " + CString(iData + 1) + " refers to vertex " + CString(index) + " but the file only has " + CString(nbVertices) + " vertices.", siErrorMsg);
						return CStatus::Fail;
					}
					data.indices.push_back(index);

					if (!data.rgb.empty()) {
						data.rgbaPerSample.push_back(data.rgb[3 * index]);
						data.rgbaPerSample.push_back(data.rgb[3 * index + 1]);
						data.rgbaPerSample.push_back(data.rgb[3 * index + 2]);
						data.rgbaPerSample.push_back(1.0f);
					}
				}

				// per-corner uvs as written by e.g. Blender and MeshLab: "property list uchar float texcoord"
				if (ixTexcoord >= 0 && el.props[ixTexcoord].isList) {
					const vector<double>& texcoord = lists[ixTexcoord];
					bool bComplete = texcoord.size() == 2 * (size_t)nbCorners;
					for (long k = 0; k < nbCorners; k++) {
						data.uvwPerSample.push_back(bComplete ? (float)texcoord[2 * k] : 0.0f);
						data.uvwPerSample.push_back(bComplete ? (float)texcoord[2 * k + 1] : 0.0f);
						data.uvwPerSample.push_back(0.0f);
					}
				}
			}
			else if (el.name == "multi_texture_vertex") {
				// only texture #0 is used
				if (ixU >= 0 && ixV >= 0 && ixTx >= 0 && (long)scalars[ixTx] == 0) {
					data.multiTexUVs.push_back((float)scalars[ixU]);
					data.multiTexUVs.push_back(1.0f - (float)scalars[ixV]);
				}
			}
			else if (el.name == "multi_texture_face") {
				if (ixTexVertexIndices >= 0 && el.props[ixTexVertexIndices].isList && ixTx >= 0 && (long)scalars[ixTx] == 0) {
					const vector<double>& texIndices = lists[ixTexVertexIndices];
					long nbTexVertices = (long)(data.multiTexUVs.size() / 2);
					for (size_t k = 0; k < texIndices.size(); k++) {
						long index = (long)texIndices[k];
						if (index < 0 || index >= nbTexVertices) {
							app.LogMessage("PLY: multi_texture_face " + CString(iData + 1) + " refers to texture vertex " + CString(index) + " but the file only has " + CString(nbTexVertices) + " texture vertices of texture 0.", siErrorMsg);
							return CStatus::Fail;
						}
						data.uvwPerSample.push_back(data.multiTexUVs[2 * index]);
						data.uvwPerSample.push_back(data.multiTexUVs[2 * index + 1]);
						data.uvwPerSample.push_back(0.0f);
					}
				}
			}
			// items of other elements (edge, material, ...) are read and ignored

			if ((iData + 1) % 10000 == 0) {
				m_progress.Increment(10000);
				if (m_progress.IsCancelPressed())
					return CStatus::False;
			}
		}
	}

	return CStatus::OK;
}

CStatus CPLY::Execute_Import(string initFilePathName, bool bImportUVs, bool bImportVertexColors)
{
	CFileFormat::initStrings(initFilePathName);

	Application app;
	Model root = app.GetActiveSceneRoot();

	m_progress.PutVisible(true);
	m_progress.PutCaption("Importing PLY Data");

	if ((m_pFile = fopen(m_filePathName.c_str(), "rb")) == NULL) {
		app.LogMessage("Could not open '" + CString(m_filePathName.c_str()) + "'.", siErrorMsg);
		m_progress.PutVisible(false);
		return CStatus::Fail;
	}

	vector<elementinfo> header;
	PlyData data;
	CStatus status = read_header(header) ? readBody(header, data) : CStatus::Fail;

	fclose(m_pFile);
	m_pFile = NULL;

	if (status != CStatus::OK) {
		m_progress.PutVisible(false);
		return status;
	}

	if (data.positions.empty() || data.counts.empty()) {
		app.LogMessage(L"PLY: the file contains no vertices or no faces.", siErrorMsg);
		m_progress.PutVisible(false);
		return CStatus::Fail;
	}

	m_progress.PutCaption("Importing PLY (Building Mesh)");

	// use empty mesh to create the imported mesh
	X3DObject xobj;
	root.AddPrimitive("EmptyPolygonMesh", CString(m_fileName.c_str()), xobj);

	// get a mesh builder from the newly created geometry
	Primitive prim = xobj.GetActivePrimitive();
	PolygonMesh mesh = prim.GetGeometry();
	if (!mesh.IsValid()) {
		m_progress.PutVisible(false);
		return CStatus::False;
	}

	// auto scale mesh (1, 0.1 or 0.01) from its extent in x
	double lfExtentX = data.maxX - data.minX;
	MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0);
	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);

	xobj.PutLocalScaling(vAutoScaling);

	CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

	meshBuilder.AddVertices((LONG)(data.positions.size() / 3), data.positions.data());
	meshBuilder.AddPolygons((LONG)data.counts.size(), data.counts.data(), data.indices.data());

	// build mesh
	CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
	if (err != CStatus::OK)
		app.LogMessage("Error building the mesh: " + err.GetDescription());

	CClusterPropertyBuilder cpBuilder = mesh.GetClusterPropertyBuilder();

	if (bImportVertexColors && !data.rgbaPerSample.empty()) {
		ClusterProperty rgb = cpBuilder.AddVertexColor(CString("PolyPaint"), CString("Vertex_Colors"));
		rgb.SetValues(data.rgbaPerSample.data(), (LONG)(data.rgbaPerSample.size() / 4));
	}

	if (bImportUVs && !data.uvwPerSample.empty()) {
		ClusterProperty uv = cpBuilder.AddUV();
		uv.SetValues(data.uvwPerSample.data(), (LONG)(data.uvwPerSample.size() / 3));
	}

	m_progress.PutVisible(false);

	return CStatus::OK;
}


