#include "OBJ.h"

#define log app.LogMessage

class ObjectDescription {
public:
	ObjectDescription(string name) { this->name = name; }
	string name;
	long nbPoints, pointOffset;
	unordered_map<long, long> ixPP_lookup;
	long ixPP_next = 0;
	CDoubleArray PP;
	CFloatArray Normals;
	CFloatArray RGBA;
	CFloatArray UVs;
	CFloatArray Mask;
	CLongArray PP_Ics;
	CLongArray Counts;
};

CStatus COBJ::Execute_Import(string initFilePathName, bool bImportUVs, bool bImportUserNormals, bool bImportMask, bool bImportPolypaint)
{
	CFileFormat::initStrings(initFilePathName);

	Application app;
	Model root = app.GetActiveSceneRoot();

	m_progress.PutVisible(true);
	m_progress.PutCaption("Importing OBJ Data");

	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	if ((m_file = fopen(m_filePathName.c_str(), "rb")) == NULL)
		return CStatus::Fail;

	int ch, number_of_lines = 0;
	while (EOF != (ch = getc(m_file)))
		if ('\n' == ch)
			++number_of_lines;

	m_progress.PutMaximum(number_of_lines);

	fseek(m_file, 0L, 0);

	char buf[32001];
	buf[32000] = '\0';

	CDoubleArray PP_inFile;
	CFloatArray UVs_inFile, MRGB_inFile, Normals_inFile;

	unordered_map<string, ObjectDescription*> object_map;
	ObjectDescription* pObj = NULL;

	bool bHasPolypaint = false, bHasNormals = false, bHasUVs = false;
	long impCurrentMode = 0;
	long modeCount2 = 0, modeCount3 = 0, modeCount4 = 0;
	long ixLineInFile = 0;
	do {
		if (!fgets(buf, 32000, m_file))
			break;

		string stdLine(buf);
		trim(stdLine);  
		std::vector<std::string> tokens = split(stdLine, ' ');

		size_t nbTokens = tokens.size();
		if(nbTokens == 0)
			continue;

		if (tokens[0] == "#MRGB") {
			bHasPolypaint = true;
			if (impCurrentMode != 1) {
				impCurrentMode = 1;
				if(pObj == NULL)
					m_progress.PutCaption("Importing OBJ (Vertex Colors and Weights)");
				else
					m_progress.PutCaption("Importing OBJ (Vertex Colors and Weights of '" + CString(pObj->name.c_str()) + "')");
			}

			for (size_t i = 0, i_max = tokens[1].size() / 8; i < i_max; i++)
			{
				size_t o = 8 * i;
				MRGB_inFile.Add(1.0f - (16.0f * hex2dec(tokens[1][o]) + hex2dec(tokens[1][o + 1])) / 255.0f);
				MRGB_inFile.Add((16.0f * hex2dec(tokens[1][o + 2]) + hex2dec(tokens[1][o + 3])) / 255.0f);
				MRGB_inFile.Add((16.0f * hex2dec(tokens[1][o + 4]) + hex2dec(tokens[1][o + 5])) / 255.0f);
				MRGB_inFile.Add((16.0f * hex2dec(tokens[1][o + 6]) + hex2dec(tokens[1][o + 7])) / 255.0f);
			}
		}
		else if (tokens[0] == "v") {
			if (impCurrentMode != 2) {				
				impCurrentMode = 2;
				modeCount2++;
				if (modeCount2 < 20) {
					if (pObj == NULL)
						m_progress.PutCaption("Importing OBJ (Vertex Positions)");
					else
						m_progress.PutCaption("Importing OBJ (Vertex Positions of '" + CString(pObj->name.c_str()) + "')");
				}
			}

			float x = (float)atof(tokens[1].c_str()); 
			
			PP_inFile.Add(x);
			PP_inFile.Add((float)atof(tokens[2].c_str()));
			PP_inFile.Add((float)atof(tokens[3].c_str()));

			if (x > lfMaxExtentX) // for auto scale
				lfMaxExtentX = x;

			if (x < lfMinExtentX) // for auto scale
				lfMinExtentX = x;

		}
		else if (tokens[0] == "vt") {
			bHasUVs = true;
			if (impCurrentMode != 3) {
				impCurrentMode = 3;
				modeCount3++;
				if (modeCount3 < 20) {
					if (pObj == NULL)
						m_progress.PutCaption("Importing OBJ (Texture Coordinates)");
					else
						m_progress.PutCaption("Importing OBJ (Texture Coordinates of '" + CString(pObj->name.c_str()) + "')");
				}
			}

			UVs_inFile.Add((float)atof(tokens[1].c_str()));
			UVs_inFile.Add((float)atof(tokens[2].c_str()));
			if(tokens.size() > 3)
				UVs_inFile.Add((float)atof(tokens[3].c_str()));
			else
				UVs_inFile.Add(0.0);
		}
		else if (tokens[0] == "vn") {
			bHasNormals = true;
			if (impCurrentMode != 4) {
				impCurrentMode = 4; 
				modeCount4++;
				if (modeCount4 < 20) {
					if (pObj == NULL)
						m_progress.PutCaption("Importing OBJ (Normals)");
					else
						m_progress.PutCaption("Importing OBJ (Normals of '" + CString(pObj->name.c_str()) + "')");
				}
			}
			
			Normals_inFile.Add((float)atof(tokens[1].c_str()));
			Normals_inFile.Add((float)atof(tokens[2].c_str()));
			Normals_inFile.Add((float)atof(tokens[3].c_str()));
		}
		else if (tokens[0] == "f") {
			if (impCurrentMode != 5) {
				impCurrentMode = 5;
				if (pObj == NULL)
					m_progress.PutCaption("Importing OBJ (Polygons)");
				else
					m_progress.PutCaption("Importing OBJ (Polygons of '" + CString(pObj->name.c_str()) + "')");
			}

			string name(m_fileName);
			if (pObj == NULL) {
				pObj = new ObjectDescription(name);
				object_map.insert({ name, pObj });
			}

			size_t nbPolypoints = nbTokens - 1;
			for (int i = 1; i <= nbPolypoints; i++) {

				std::vector<std::string> triple = split(tokens[i], '/'); // PointIndex/UVIndex/NormalIndex

				long ixPP_inFile = atol(triple[0].c_str()) - 1;
				long ixPP;
				if (pObj->ixPP_lookup.count(ixPP_inFile) == 0) {
					ixPP = pObj->ixPP_next;
					pObj->ixPP_lookup.insert({ ixPP_inFile, pObj->ixPP_next++ });
					pObj->PP.Add(PP_inFile[3 * ixPP_inFile]);
					pObj->PP.Add(PP_inFile[3 * ixPP_inFile + 1]);
					pObj->PP.Add(PP_inFile[3 * ixPP_inFile + 2]);
					if (bHasPolypaint) 
						pObj->Mask.Add(MRGB_inFile[4 * ixPP_inFile]);
				}
				else {					
					ixPP = pObj->ixPP_lookup[ixPP_inFile];
				}
								
				pObj->PP_Ics.Add(ixPP);

				if (bHasUVs && triple[1] != "") {
					int ixUV_inFile = atoi(triple[1].c_str()) - 1;
					pObj->UVs.Add(UVs_inFile[3 * ixUV_inFile]);
					pObj->UVs.Add(UVs_inFile[3 * ixUV_inFile + 1]);
					pObj->UVs.Add(UVs_inFile[3 * ixUV_inFile + 2]);
				}

				if (bHasNormals && triple[2] != "") {
					int ixNormal_inFile = atoi(triple[2].c_str()) - 1;
					pObj->Normals.Add(Normals_inFile[3 * ixNormal_inFile]);
					pObj->Normals.Add(Normals_inFile[3 * ixNormal_inFile + 1]);
					pObj->Normals.Add(Normals_inFile[3 * ixNormal_inFile + 2]);
				}

				if (bHasPolypaint) {
					pObj->RGBA.Add(MRGB_inFile[4 * ixPP_inFile + 1]);
					pObj->RGBA.Add(MRGB_inFile[4 * ixPP_inFile + 2]);
					pObj->RGBA.Add(MRGB_inFile[4 * ixPP_inFile + 3]);
					pObj->RGBA.Add(255.0);					
				}
			}
			pObj->Counts.Add((LONG)nbPolypoints);
		}
		else if (tokens[0] == "g") {
			string name(tokens[1]);
			if (object_map.count(name) == 0) {
				pObj = new ObjectDescription(name);
				object_map.insert({ name, pObj });
			}
			else {
				pObj = object_map[name];
			}
		}
		else if (tokens[0] == "usemtl") {
			// TODO new mtl assign4
		} 

		ixLineInFile++;
		if (ixLineInFile % 100000 == 0) { // increment progress bar
			m_progress.Increment(100000);
			if (m_progress.IsCancelPressed()) {
				for (auto p : object_map) delete(p.second);
				return CStatus::False;
			}
		}
	} while (!feof(m_file));

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale

	// auto scale meshes
	MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0);
	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);
	//m_progress.PutVisible(false);
	int ix = 0; 
	for (auto p : object_map) {

		ObjectDescription* pCurrentObj = p.second;
		
		m_progress.PutCaption("Importing OBJ (Building Mesh "+CString(++ix)+" of "+CString(object_map.size())+")");

		// use empty mesh to create the imported mesh
		X3DObject xobj;
		root.AddPrimitive("EmptyPolygonMesh", CString(pCurrentObj->name.c_str()), xobj); //  CString(m_fileName.c_str()), xobj);

		// get a mesh builder from the newly created geometry
		Primitive prim = xobj.GetActivePrimitive();
		PolygonMesh mesh = prim.GetGeometry();
		xobj.PutLocalScaling(vAutoScaling);

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();
		
		meshBuilder.AddVertices(pCurrentObj->PP.GetCount() / 3, pCurrentObj->PP.GetArray());
		meshBuilder.AddPolygons(pCurrentObj->Counts.GetCount(), pCurrentObj->Counts.GetArray(), pCurrentObj->PP_Ics.GetArray());
		
		// build mesh

		CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
		if (err != CStatus::OK)
			app.LogMessage("Error building the mesh: " + err.GetDescription());
	
		CClusterPropertyBuilder cpBuilder = mesh.GetClusterPropertyBuilder();
		
		if (pCurrentObj->UVs.GetCount() > 0) {
			ClusterProperty uv = cpBuilder.AddUV();
			uv.SetValues(pCurrentObj->UVs.GetArray(), pCurrentObj->UVs.GetCount() / 3);
		}

		if (bImportPolypaint && bHasPolypaint) {
			ClusterProperty rgb = cpBuilder.AddVertexColor(CString("PolyPaint"), CString("Vertex_Colors"));
			rgb.SetValues(pCurrentObj->RGBA.GetArray(), pCurrentObj->RGBA.GetCount() / 4);
		}
		
		if (bImportMask && bHasPolypaint) {
			ClusterProperty weights = cpBuilder.AddWeightMap(CString("Mask"), CString("WeightMapCls"));
			weights.SetValues(pCurrentObj->Mask.GetArray(), pCurrentObj->Mask.GetCount());
		}

		if (bImportUserNormals && bHasNormals) {
			ClusterProperty normals = cpBuilder.AddUserNormal();
			normals.SetValues(pCurrentObj->Normals.GetArray(), pCurrentObj->Normals.GetCount() / 3);
		}
	}
	
	for (auto p : object_map) delete(p.second);

	m_progress.PutVisible(false);
	
	fclose(m_file);

	return CStatus::OK;
}






























CStatus COBJ::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportVertexColors, bool bSeparateFiles, bool bExportLocalCoords)
{
	XSI::Application app;

	initStrings(initFilePathName);

	if ((m_file = fopen(m_filePathName.c_str(), "wb")) == NULL)
		return CStatus::Fail;

	if ((m_matfile = fopen((m_filePath + m_fileName + string(".mtl")).c_str(), "wb")) == NULL)
		return CStatus::Fail;

	// output header
	Output(m_file, "# Custom Wavefront OBJ Exporter\r\n");
	Output(m_file, "\r\nmtllib " + m_fileName + ".mtl\r\n");
	Output(m_file, "\r\no " + m_fileName + "\r\n");

	long fails = 0;	
	
	m_progress.PutVisible(true);
	m_progress.PutMaximum(3 * inObjects.GetCount());

	for (long iObject = 0, iObject_max = inObjects.GetCount(); iObject < iObject_max; iObject++) {

		X3DObject xobj(inObjects[iObject]);
		if (!xobj.IsValid()) {
			fails++; 
			continue;
		}

		// Get a geometry accessor from the selected object	
		XSI::Primitive prim = xobj.GetActivePrimitive();
		XSI::PolygonMesh mesh = prim.GetGeometry();
		if (!mesh.IsValid()) return CStatus::False;

		m_progress.PutCaption("Exporting OBJ (" + xobj.GetName() + ")");
		m_progress.Increment();

		long tic = 0;

		XSI::CGeometryAccessor ga = mesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, 0);

		CDoubleArray VP;
		CLongArray LA_Nodes, LA_Indices, LA_Counts, LA_Materials;

		ga.GetVertexPositions(VP);
		ga.GetNodeIndices(LA_Nodes);
		ga.GetVertexIndices(LA_Indices);
		ga.GetPolygonVerticesCount(LA_Counts);
		ga.GetPolygonMaterialIndices(LA_Materials);

		// ************************************************
		// * Export Vertex Positions

		Output(m_file, "\r\ng " + string(xobj.GetName().GetAsciiString()) + "\r\n");

		m_progress.PutCaption("Exporting OBJ (Vertex Positions of '" + xobj.GetName() + "')");
		m_progress.Increment();

		Output(m_file, "\r\n# vertex positions\r\n");

		MATH::CTransformation xfo = xobj.GetKinematics().GetGlobal().GetTransform();

		for (long iVertexPosition = 0, i_max = VP.GetCount() / 3; iVertexPosition < i_max; iVertexPosition++) {

			MATH::CVector3 v(VP[3 * iVertexPosition + 0], VP[3 * iVertexPosition + 1], VP[3 * iVertexPosition + 2]);

			if (!bExportLocalCoords)
				v = MATH::MapObjectPositionToWorldSpace(xfo, v);


			Output(m_file, "v " + to_string(v.GetX()) + " " + to_string(v.GetY()) + " " + to_string(v.GetZ()) + "\r\n");
		}
		Output(m_file, "# end vertex positions (" + to_string(VP.GetCount() / 3) + ")\r\n");

		tr1::array<float, 3> triple;

		// ************************************************************************************************
		// * Export Mask and Vertex Colors in ZBrush Polypaint format
		// * can be read by 3dcoat, sculptgl, xnormal, meshmixer, but NOT by ZBrush!

		if (bExportVertexColors) {

			ClusterProperty foundProperty(CFileFormat::getVertexColorProperty(xobj));

			if(foundProperty.IsValid()) {
			
				m_progress.PutCaption("Exporting OBJ (Vertex Colors of '" + xobj.GetName() + "')");

				char hex[] = "0123456789abcdef";

				Output(m_file, "\r\n# The following MRGB block contains ZBrush Vertex Color (Polypaint) and masking output as 4 hexadecimal values per vertex. The vertex color format is MMRRGGBB with up to 64 entries per MRGB line.\r\n");
			
				CFloatArray FA_RGB_ordered;
				CLongArray LA_RGBCounts_ordered;
				CFloatArray FA_RGBA;
				CBitArray flags_RGBA;

				foundProperty.GetValues(FA_RGBA, flags_RGBA);

				FA_RGB_ordered.Resize(3 * mesh.GetPoints().GetCount());
				for (long i = 0, i_max = FA_RGB_ordered.GetCount(); i < i_max; i++)
					FA_RGB_ordered[i] = 0.0;

				LA_RGBCounts_ordered.Resize(mesh.GetPoints().GetCount());
				for (long i = 0, i_max = LA_RGBCounts_ordered.GetCount(); i < i_max; i++)
					LA_RGBCounts_ordered[i] = 0;

				// reorder sample property to vertices (average)
				for (long iRGBA = 0, iRGBA_max = FA_RGBA.GetCount() / 4; iRGBA < iRGBA_max; iRGBA++) {

					LA_RGBCounts_ordered[LA_Indices[iRGBA]] += 1;
					FA_RGB_ordered[3 * LA_Indices[iRGBA] + 0] += FA_RGBA[4 * iRGBA + 0];
					FA_RGB_ordered[3 * LA_Indices[iRGBA] + 1] += FA_RGBA[4 * iRGBA + 1];
					FA_RGB_ordered[3 * LA_Indices[iRGBA] + 2] += FA_RGBA[4 * iRGBA + 2];
				}

				string strLine = "#MRGB ";
				int nColors = 0;
				for (int i = 0, i_max = FA_RGB_ordered.GetCount() / 3; i < i_max; i++) {
					strLine += "ff";
					int r = (int)(255.0 * (FA_RGB_ordered[3 * i] / LA_RGBCounts_ordered[i]));
					strLine += hex[(r >> 4) & 0xf];
					strLine += hex[r & 0xf];
					int g = (int)(255.0 * (FA_RGB_ordered[3 * i + 1] / LA_RGBCounts_ordered[i]));
					strLine += hex[(g >> 4) & 0xf];
					strLine += hex[g & 0xf];
					int b = (int)(255.0 * (FA_RGB_ordered[3 * i + 2] / LA_RGBCounts_ordered[i]));
					strLine += hex[(b >> 4) & 0xf];
					strLine += hex[b & 0xf];
					if (nColors++ == 63) {
						Output(m_file, strLine + "\r\n");
						strLine = "#MRGB ";
						nColors = 0;
					}
				}

				if (nColors != 0)
					Output(m_file, strLine + "\r\n");

				Output(m_file, "# End of MRGB block \r\n");
			}
		}

		// ************************
		// * Export UVs

		ClusterProperty UVCP(ga.GetUVs()[0]);
		CFloatArray FA_UVs;
		CBitArray flags_UVs;
		m_hashmap_uv.clear();

		bool bExportUVs = UVCP.IsValid();
		if (bExportUVs) {

			UVCP.GetValues(FA_UVs, flags_UVs);

			m_progress.PutCaption("Exporting OBJ (Texture Positions of '" + xobj.GetName() + "')");

			Output(m_file, "\r\n# texture positions\r\n");

			long ix = 0;
			for (long iUVCoord = 0, l_max = FA_UVs.GetCount() / 3; iUVCoord < l_max; iUVCoord++) {

				float u = FA_UVs[3 * iUVCoord + 0];
				float v = FA_UVs[3 * iUVCoord + 1];
				float w = FA_UVs[3 * iUVCoord + 2];
				triple = { u, v, w };
				if (m_hashmap_uv.count(triple) == 0) {

					m_hashmap_uv.insert({ triple, ix++ });
					//Output(m_file, "vt " + to_string(u) + " " + to_string(v) + " " + to_string(w) + "\r\n");
					Output(m_file, "vt " + to_string(u) + " " + to_string(v) + "\r\n");
				}
			}

			Output(m_file, "# end texture positions (" + to_string(m_hashmap_uv.size()) + ", down from " + to_string(FA_UVs.GetCount() / 3) + " with duplicates)\r\n");
		}

		// ************************
		// * Export User Normals

		ClusterProperty UNCP(ga.GetUserNormals()[0]);
		CFloatArray FA_UserNormals;
		CBitArray flags_UserNormals;
		m_hashmap_normals.clear();

		bool bExportUserNormals = UNCP.IsValid();
		if (bExportUserNormals)
		{
			ga.GetNodeNormals(FA_UserNormals);

			m_progress.PutCaption("Exporting OBJ (Normals of '" + xobj.GetName() + "')");

			Output(m_file, "\r\n# normals\r\n");

			long ix = 0;
			for (long iUserNormal = 0, l_max = FA_UserNormals.GetCount() / 3; iUserNormal < l_max; iUserNormal++) {
				float nx = FA_UserNormals[3 * iUserNormal + 0];
				float ny = FA_UserNormals[3 * iUserNormal + 1];
				float nz = FA_UserNormals[3 * iUserNormal + 2];
				triple = { nx, ny, nz };
				if (m_hashmap_normals.count(triple) == 0) {

					m_hashmap_normals.insert({ triple, ix++ });
					Output(m_file, string("vn ") + to_string(nx) + " " + to_string(ny) + " " + to_string(nz) + "\r\n");
				}
			}

			Output(m_file, string("# end normals (") + to_string(m_hashmap_normals.size()) + ", down from " + to_string(FA_UserNormals.GetCount() / 3) + " with duplicates)\r\n");
		}

		CRefArray Materials = ga.GetMaterials();

		Output(m_file, string("\r\n# ") + to_string(LA_Counts.GetCount()) + " polygons using " + to_string(Materials.GetCount()) + " Material(s)\r\n");

		// ************************
		// * Export Polygons

		m_progress.PutCaption("Exporting OBJ (Polygons of '" + xobj.GetName() + "')");
		m_progress.Increment();

		// loop materials
		for (long iMaterial = 0, i_max = Materials.GetCount(); iMaterial < i_max; iMaterial++) {

			Material material(Materials[iMaterial]);

			Output(m_file, string("usemtl ") + string(material.GetName().GetAsciiString()) + "\r\n");

			string strKa = "Ka 0.0 0.0 0.0\r\n";
			string strKd = "Kd 0.0 0.0 0.0\r\n";
			string strKs = "Ks 0.0 0.0 0.0\r\n";
			string strKsss, strNs, strIllum;

			CRefArray shader_refs = material.GetAllShaders();
			for (long iShader = 0, s_max = shader_refs.GetCount(); iShader < s_max; iShader++) {
				Shader shader(shader_refs[iShader]);
				CParameterRefArray param_refs = shader.GetParameters();
				for (long iParam = 0, t_max = param_refs.GetCount(); iParam < t_max; iParam++) {
					Parameter param(param_refs[iParam]);
					CValue value = param.GetValue();
					CValue::DataType m_t = value.m_t;

					if (m_t == CValue::DataType::siColor4f && (param.GetName() == "Kd_color" || param.GetName() == "diffuse")) {
						MATH::CColor4f v = value;
						strKd = "Kd " + to_string(v.GetR()) + " " + to_string(v.GetG()) + " " + to_string(v.GetB()) + "\r\n";
					}
					else if (m_t == CValue::DataType::siColor4f && (param.GetName() == "Ks_color" || param.GetName() == "specular")) {
						MATH::CColor4f v = value;
						strKs = "Ks " + to_string(v.GetR()) + " " + to_string(v.GetG()) + " " + to_string(v.GetB()) + "\r\n";
					}
					else if (m_t == CValue::DataType::siColor4f && (param.GetName() == "Ka_color" || param.GetName() == "ambient")) {
						MATH::CColor4f v = value;
						strKa = "Ka " + to_string(v.GetR()) + " " + to_string(v.GetG()) + " " + to_string(v.GetB()) + "\r\n";
					}
					else if (m_t == CValue::DataType::siColor4f && (param.GetName() == "Ksss_color" || param.GetName() == "subsurface")) {
						MATH::CColor4f v = value;
						strKsss = "Ksss " + to_string(v.GetR()) + " " + to_string(v.GetG()) + " " + to_string(v.GetB()) + "\r\n";
					}
					else if (m_t == CValue::DataType::siFloat && param.GetName() == "shiny") {
						float v = value;
						strNs = "Ns " + to_string(v) + "\r\n";
					}
				}
			}

			Output(m_matfile, string("newmtl ") + material.GetName().GetAsciiString() + "\r\n" + strKa + strKd + strKs + strNs + strIllum + "\r\n");

			long ix = 0;

			// loop counts for each polygons / material for each polygon
			for (long iPolycount = 0, j_max = LA_Counts.GetCount(); iPolycount < j_max; iPolycount++) {

				long polyVertexCount = LA_Counts[iPolycount];

				if (LA_Materials[iPolycount] == iMaterial) {

					string strTmp = "f ";

					// loop single polygon
					for (long iPolyVertexcount = 0, k_max = polyVertexCount; iPolyVertexcount < k_max; iPolyVertexcount++) {

						// vertex index
						strTmp.append(to_string(LA_Indices[ix + iPolyVertexcount] + 1 + m_vertices_base_ix_group));

						if (bExportUVs) {
							float u = FA_UVs[3 * LA_Nodes[ix + iPolyVertexcount] + 0];
							float v = FA_UVs[3 * LA_Nodes[ix + iPolyVertexcount] + 1];
							float w = FA_UVs[3 * LA_Nodes[ix + iPolyVertexcount] + 2];
							triple = { u, v, w };
							strTmp.append("/" + to_string(m_hashmap_uv[triple] + 1 + m_uvs_base_ix_group));
						}
						else
							strTmp += "/";

						if (bExportUserNormals) {
							float nx = FA_UserNormals[3 * LA_Nodes[ix + iPolyVertexcount] + 0];
							float ny = FA_UserNormals[3 * LA_Nodes[ix + iPolyVertexcount] + 1];
							float nz = FA_UserNormals[3 * LA_Nodes[ix + iPolyVertexcount] + 2];
							triple = { nx, ny, nz };
							strTmp.append("/" + to_string(m_hashmap_normals[triple] + 1 + m_normals_base_ix_group));
						}

						strTmp += " ";
					} // for each polygon point

					strTmp += "\r\n";

					Output(m_file, strTmp);
				}

				ix += polyVertexCount;
			} // for each polygon
		} // for each material4

		m_vertices_base_ix_group += VP.GetCount() / 3;
		m_uvs_base_ix_group += m_hashmap_uv.size();
		m_normals_base_ix_group += m_hashmap_normals.size();	
	}

	fclose(m_file);
	fclose(m_matfile);

	if (fails == 0)
		return CStatus::OK;
	else {
		app.LogMessage("OBJ File output for " + CString(fails) + " objects failed.", siErrorMsg);
		return CStatus::Fail;
	}
}

void COBJ::Output(FILE* file, string str) {
	fprintf_s(file, str.c_str());
}

ULONG COBJ::hex2dec(char f)
{
	if (f >= '0' && f <= '9')
		return f - '0';
	else if (f >= 'a' && f <= 'z')
		return f - 'a' + 10;
	else if (f >= 'A' && f <= 'Z') 
		return f - 'A' + 10;
	else
		return 0;
}


