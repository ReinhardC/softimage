#include "OBJ.h"

#define log app.LogMessage

class MeshData {
public:
	MeshData(string name) { this->name = name; }

	string name;

	CLongArray Counts; // polygons

	CDoubleArray PointPosition; // positions
	CLongArray IcsPointPosition; // points
	long ixPointPosition_next = 0;
	unordered_map<long, long> ixPointPosition_lookup;
	
	unordered_map<string, CLongArray> IcsMaterialClusters;

	bool bHasNormals = false;
	CFloatArray Normals;

	bool bHasUVs = false;
	CFloatArray UVs;

	bool bHasPolypaint = false;
	CFloatArray RGBA;
	CFloatArray Mask; 
};

CStatus COBJ::Execute_Import(string initFilePathName, bool bImportUVs, bool bImportUserNormals, bool bImportMask, bool bImportPolypaint)
{
	Application app;
	
	CValue prefs_CreateObjectsTag;
	app.GetPreferences().GetPreferenceValue(L"File Format Options.OBJ_CreateObjectsTag", prefs_CreateObjectsTag);
	CValue prefs_CreateClustersTag;
	app.GetPreferences().GetPreferenceValue(L"File Format Options.OBJ_CreateClustersTag", prefs_CreateClustersTag);

	string new_object_tag = "g"; // use auto object
	string new_cluster_tag = "usemtl";

	switch ((int)prefs_CreateObjectsTag) {
		case 0: new_object_tag = "g"; break;
		case 1: new_object_tag = "o"; break;
		case 2: new_object_tag = "usemtl"; break;
		case 3: new_object_tag = "_____"; break;
	}

	switch ((int)prefs_CreateClustersTag) {
		case 0: new_cluster_tag = "g"; break;
		case 1: new_cluster_tag = "o"; break;
		case 2: new_cluster_tag = "usemtl"; break;
		case 3: new_cluster_tag = "_____"; break;
	}

	Model root = app.GetActiveSceneRoot();

	MeshData* pCurrentMesh = NULL;
	CLongArray* pCurrentMatCluster = NULL;
	string strCurrentMatName;

	unordered_map<string, MeshData*> mesh_map;

	CFileFormat::initStrings(initFilePathName);

	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	if ((m_file = fopen(m_filePathName.c_str(), "rb")) == NULL)
		return CStatus::Fail;

	// collect stats
	fseek(m_file, 0L, SEEK_END);
	m_progress.PutCaption("Importing OBJ (Scanning File for Objects)");
	m_progress.PutMaximum(ftell(m_file) / 10000);
	m_progress.PutVisible(true);
	rewind(m_file);

	char c, strLine[80];
	long pos_in_file = 0;
	int pos_in_line = 0;
	int line_in_file = 0;	
	int max_line_length = -1;

	while (EOF != (c = getc(m_file))) {

		if (pos_in_line < 79)
			strLine[pos_in_line] = c;

		if (pos_in_file % 10000 == 0)
			m_progress.Increment(1);

		if (pos_in_line >= 2 && ((c == '\n' && pos_in_line < 78) || pos_in_line == 78))
		{
			// early opt outs (speed opt)
			char c1 = strLine[0], c2 = strLine[1];
			if (!(c1 == 'f' || // ignore faces
				(c1 == 'v' && c2 == ' ' && pCurrentMesh) || // ignore vertices if we have a mesh
				(c1 == 'v' && c2 == 'n' && pCurrentMesh && pCurrentMesh->bHasNormals) || // ignore normals if we already found normals for the current mesh
				(c1 == 'v' && c2 == 't' && pCurrentMesh && pCurrentMesh->bHasUVs) // ignore uvs if we already found uvs for the current mesh
				))
			{
				strLine[pos_in_line + 1] = '\0';
				string stdLine(strLine);
				trim(stdLine);
				std::vector<std::string> tokens = split(stdLine, ' ');

				size_t nbTokens = tokens.size();
				string& firstToken = tokens[0];
				if (nbTokens == 2 && firstToken == new_object_tag) {
					string name(tokens[1]);
					if (mesh_map.count(name) == 0) {
						pCurrentMesh = new MeshData(name);
						mesh_map.insert({ name, pCurrentMesh });
					}
					else {
						pCurrentMesh = mesh_map[name];
					}
				}
				else if (nbTokens != 0 && (firstToken == "v" || firstToken == "vt" || firstToken == "vn")) {
					if (!pCurrentMesh) {
						string name(m_fileName);
						pCurrentMesh = new MeshData(name);
						mesh_map.insert({ name, pCurrentMesh });
					}
					if (firstToken == "vt")
						pCurrentMesh->bHasUVs = true;
					else if (firstToken == "vn")
						pCurrentMesh->bHasNormals = true;								
				}
				else if (nbTokens != 0 && firstToken == "#MRGB")
					pCurrentMesh->bHasPolypaint = true;
			}
		}

		if (c == '\n') 
		{
			if (pos_in_line > max_line_length) 
				max_line_length = pos_in_line;

			pos_in_line = 0;
			++line_in_file;
		}
		else
			pos_in_line++;

		pos_in_file++;
	}
	//stats done.

	pCurrentMesh = NULL;

	m_progress.PutValue(0);
	m_progress.PutMaximum(line_in_file / 1000);

	fseek(m_file, 0L, 0);

	char* pbuf = (char*)malloc(max_line_length);
	*(pbuf + max_line_length - 1) = '\0';

	CDoubleArray PP_inFile;
	CFloatArray UVs_inFile, MRGB_inFile, Normals_inFile;

	long current_action_id = 0; // inactve
	long nbModeSwitches2 = 0, nbModeSwitches3 = 0, nbModeSwitches4 = 0, nbModeSwitches5 = 0, nbModeSwitches6 = 0;
	line_in_file = 0;
	do {
		if (!fgets(pbuf, max_line_length, m_file))
			break;

		string stdLine(pbuf);
		trim(stdLine);
		std::vector<std::string> tokens = split(stdLine, ' ');

		size_t nbTokens = tokens.size();
		if (nbTokens < 2) {
			line_in_file++;
			continue;
		}

		string& firstToken = tokens[0];
		string& secondToken = tokens[1];
		if (firstToken == new_object_tag)
		{
			string name(secondToken);
			if (mesh_map.count(name) == 0) {
				app.LogMessage("Error: Object '" + CString(secondToken.c_str()) + "' found during Scan not found anymore", siErrorMsg);
				return CStatus::Fail;
			}

			pCurrentMesh = mesh_map[name];
		}
		else if (firstToken == new_cluster_tag || firstToken == "v" || firstToken == "vt" || firstToken == "vn") {
			if (!pCurrentMesh) {
				string name(m_fileName);
				if (mesh_map.count(name) == 0) {
					app.LogMessage("Error: Object '" + CString(secondToken.c_str()) + "' found during Scan not found anymore", siErrorMsg);
					return CStatus::Fail;
				}
				pCurrentMesh = mesh_map[name];
			}
			
			if (firstToken == new_cluster_tag) { 
				if (strCurrentMatName != secondToken) {
					strCurrentMatName = secondToken;
					if (pCurrentMesh->IcsMaterialClusters.count(strCurrentMatName) == 0) {
						pCurrentMesh->IcsMaterialClusters.insert({ strCurrentMatName, CLongArray() });
					}
					pCurrentMatCluster = &pCurrentMesh->IcsMaterialClusters.at(strCurrentMatName);
				}
			}
			else if (firstToken == "v") {
				assert(nbTokens == 4);
				if (current_action_id != 2) {
					current_action_id = 2;
					nbModeSwitches2++;
					if (nbModeSwitches2 < 20)
						m_progress.PutCaption("Importing OBJ (Vertex Positions of '" + CString(pCurrentMesh->name.c_str()) + "')");
				}

				float x = (float)atof(secondToken.c_str());

				PP_inFile.Add(x);
				PP_inFile.Add((float)atof(tokens[2].c_str()));
				PP_inFile.Add((float)atof(tokens[3].c_str()));

				if (x > lfMaxExtentX) // for auto scale
					lfMaxExtentX = x;

				if (x < lfMinExtentX) // for auto scale
					lfMinExtentX = x;

			}
			else if (firstToken == "vt") {
				assert(nbTokens >= 3 && nbTokens <= 4);
				assert(pCurrentMesh->bHasUVs == true);
				if (current_action_id != 3) {
					current_action_id = 3;
					nbModeSwitches3++;
					if (nbModeSwitches3 < 20)
						m_progress.PutCaption("Importing OBJ (Texture Coordinates of '" + CString(pCurrentMesh->name.c_str()) + "')");
				}

				UVs_inFile.Add((float)atof(secondToken.c_str()));
				UVs_inFile.Add((float)atof(tokens[2].c_str()));
				if (tokens.size() > 3)
					UVs_inFile.Add((float)atof(tokens[3].c_str()));
				else
					UVs_inFile.Add(0.0);
			}
			else if (firstToken == "vn") {
				assert(nbTokens == 4);
				assert(pCurrentMesh->bHasNormals == true);
				if (current_action_id != 4) {
					current_action_id = 4;
					nbModeSwitches4++;
					if (nbModeSwitches4 < 20)
						m_progress.PutCaption("Importing OBJ (Normals of '" + CString(pCurrentMesh->name.c_str()) + "')");
				}

				Normals_inFile.Add((float)atof(secondToken.c_str()));
				Normals_inFile.Add((float)atof(tokens[2].c_str()));
				Normals_inFile.Add((float)atof(tokens[3].c_str()));
			}
		}
		else if (firstToken == "f")
		{
			assert(nbTokens > 3);
			if (current_action_id != 5) {
				current_action_id = 5;
				nbModeSwitches5++;
				if (nbModeSwitches5 < 20)										
					m_progress.PutCaption("Importing OBJ (Polygons of '" + CString(pCurrentMesh->name.c_str()) + "')");
			}

			if (pCurrentMatCluster)
				pCurrentMatCluster->Add(pCurrentMesh->Counts.GetCount()); // add current polygon index to material cluster

			size_t nbPolypoints = nbTokens - 1;
			for (int i = 1; i <= nbPolypoints; i++) {

				std::vector<std::string> triple = split(tokens[i], '/'); 

				// ** Triple format: **********
				// **
				// ** PointIndex / UVIndex / NormalIndex

				size_t nbTripleEls = triple.size();				

				long ixPointPosition_inFile = atol(triple[0].c_str()) - 1;
				long ixPointPosition;
				if (pCurrentMesh->ixPointPosition_lookup.count(ixPointPosition_inFile) == 0) {
					ixPointPosition = pCurrentMesh->ixPointPosition_next;
					pCurrentMesh->ixPointPosition_lookup.insert({ ixPointPosition_inFile, pCurrentMesh->ixPointPosition_next++ });
					pCurrentMesh->PointPosition.Add(PP_inFile[3 * ixPointPosition_inFile]);
					pCurrentMesh->PointPosition.Add(PP_inFile[3 * ixPointPosition_inFile + 1]);
					pCurrentMesh->PointPosition.Add(PP_inFile[3 * ixPointPosition_inFile + 2]);
					if (pCurrentMesh->bHasPolypaint)
						pCurrentMesh->Mask.Add(MRGB_inFile[4 * ixPointPosition_inFile]);
				}
				else 
					ixPointPosition = pCurrentMesh->ixPointPosition_lookup[ixPointPosition_inFile];
	
				pCurrentMesh->IcsPointPosition.Add(ixPointPosition);

				if (pCurrentMesh->bHasUVs) {
					if (nbTripleEls > 1 && triple[1] != "") {
						int ixUV_inFile = atoi(triple[1].c_str()) - 1;
						pCurrentMesh->UVs.Add(UVs_inFile[3 * ixUV_inFile]);
						pCurrentMesh->UVs.Add(UVs_inFile[3 * ixUV_inFile + 1]);
						pCurrentMesh->UVs.Add(UVs_inFile[3 * ixUV_inFile + 2]);
					}
					else {						
						double u = ((double)rand() / (RAND_MAX)) / 10000; //bug workaround: uvs on 0 can lead to crashes in subdivision.dll for some topologies...
						double v = ((double)rand() / (RAND_MAX)) / 10000;
						pCurrentMesh->UVs.Add(u);
						pCurrentMesh->UVs.Add(v);
						pCurrentMesh->UVs.Add(0.0f);
					}
				}

				if (pCurrentMesh->bHasNormals) {
					if (nbTripleEls > 2 && triple[2] != "") {
						int ixNormal_inFile = atoi(triple[2].c_str()) - 1;
						pCurrentMesh->Normals.Add(Normals_inFile[3 * ixNormal_inFile]);
						pCurrentMesh->Normals.Add(Normals_inFile[3 * ixNormal_inFile + 1]);
						pCurrentMesh->Normals.Add(Normals_inFile[3 * ixNormal_inFile + 2]);
					}
					else {
						pCurrentMesh->Normals.Add(0.0); //TODO: to be be replaced by actual normal later on
						pCurrentMesh->Normals.Add(0.0);
						pCurrentMesh->Normals.Add(0.0);						
					}
				}

				if (pCurrentMesh->bHasPolypaint) {
					pCurrentMesh->RGBA.Add(MRGB_inFile[4 * ixPointPosition_inFile + 1]);
					pCurrentMesh->RGBA.Add(MRGB_inFile[4 * ixPointPosition_inFile + 2]);
					pCurrentMesh->RGBA.Add(MRGB_inFile[4 * ixPointPosition_inFile + 3]);
					pCurrentMesh->RGBA.Add(255.0);
				}
			}
			pCurrentMesh->Counts.Add((LONG)nbPolypoints);
		}
		else if (firstToken == "#MRGB")
		{
			assert(pCurrentMesh->bHasPolypaint == true);
			if (current_action_id != 6) {
				current_action_id = 6;
				nbModeSwitches6++;
				if (nbModeSwitches6 < 20)
					m_progress.PutCaption("Importing OBJ (Vertex Colors and Weights of '" + CString(pCurrentMesh->name.c_str()) + "')");
			}

			for (size_t i = 0, i_max = secondToken.size() / 8; i < i_max; i++)
			{
				size_t o = 8 * i;
				MRGB_inFile.Add(1.0f - (16.0f * hex2dec(secondToken[o]) + hex2dec(secondToken[o + 1])) / 255.0f);
				MRGB_inFile.Add((16.0f * hex2dec(secondToken[o + 2]) + hex2dec(secondToken[o + 3])) / 255.0f);
				MRGB_inFile.Add((16.0f * hex2dec(secondToken[o + 4]) + hex2dec(secondToken[o + 5])) / 255.0f);
				MRGB_inFile.Add((16.0f * hex2dec(secondToken[o + 6]) + hex2dec(secondToken[o + 7])) / 255.0f);
			}
		}

		line_in_file++;
		if (line_in_file % 1000 == 0) { // increment progress bar
			m_progress.Increment(1);
			if (m_progress.IsCancelPressed()) {
				for (auto p : mesh_map) delete(p.second);
				return CStatus::False;
			}
		}
	} while (!feof(m_file));

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale
											 
	MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0); // auto scale meshes
	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);

	int ix = 0;
	for (auto p : mesh_map) {

		pCurrentMesh = p.second;

		m_progress.PutCaption("Importing OBJ (Building Mesh " + CString(++ix) + " of " + CString(mesh_map.size()) + ")");

		if (pCurrentMesh->Counts.GetCount() == 0)
			continue;

		// use empty mesh to create the imported mesh
		X3DObject xobj;
		root.AddPrimitive("EmptyPolygonMesh", CString(pCurrentMesh->name.c_str()), xobj); //  CString(m_fileName.c_str()), xobj);
					
		Primitive prim = xobj.GetActivePrimitive();
		PolygonMesh mesh = prim.GetGeometry();
		xobj.PutLocalScaling(vAutoScaling);

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

		meshBuilder.AddVertices(pCurrentMesh->PointPosition.GetCount() / 3, pCurrentMesh->PointPosition.GetArray());
		meshBuilder.AddPolygons(pCurrentMesh->Counts.GetCount(), pCurrentMesh->Counts.GetArray(), pCurrentMesh->IcsPointPosition.GetArray());

		// build mesh

		CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
		if (err != CStatus::OK)
			app.LogMessage("Error building the mesh: " + err.GetDescription());

		for (auto currentMaterial : pCurrentMesh->IcsMaterialClusters) {
			Cluster cls;
			mesh.AddCluster(siPolygonCluster, CString(currentMaterial.first.c_str()), currentMaterial.second, cls);
		}

		CClusterPropertyBuilder cpBuilder = mesh.GetClusterPropertyBuilder();

		if (pCurrentMesh->bHasUVs && pCurrentMesh->UVs.GetCount() > 0) {
			ClusterProperty uv = cpBuilder.AddUV();
			uv.SetValues(pCurrentMesh->UVs.GetArray(), pCurrentMesh->UVs.GetCount() / 3);
		}

		if (bImportPolypaint && pCurrentMesh->bHasPolypaint) {
			ClusterProperty rgb = cpBuilder.AddVertexColor(CString("PolyPaint"), CString("Vertex_Colors"));
			rgb.SetValues(pCurrentMesh->RGBA.GetArray(), pCurrentMesh->RGBA.GetCount() / 4);
		}

		if (bImportMask && pCurrentMesh->bHasPolypaint) {
			ClusterProperty weights = cpBuilder.AddWeightMap(CString("Mask"), CString("WeightMapCls"));
			weights.SetValues(pCurrentMesh->Mask.GetArray(), pCurrentMesh->Mask.GetCount());
		}

		if (bImportUserNormals && pCurrentMesh->bHasNormals) {
			ClusterProperty normals = cpBuilder.AddUserNormal();
			normals.SetValues(pCurrentMesh->Normals.GetArray(), pCurrentMesh->Normals.GetCount() / 3);
		}
	}

	for (auto p : mesh_map) delete(p.second);

	m_progress.PutVisible(false);

	fclose(m_file);

	return CStatus::OK;
}

CStatus COBJ::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportVertexColors, bool bSeparateFiles, bool bWriteMTLFile, bool bExportLocalCoords)
{
	XSI::Application app;

	initStrings(initFilePathName);

	if ((m_file = fopen(m_filePathName.c_str(), "wb")) == NULL)
		return CStatus::Fail;

	if (bWriteMTLFile)
		if ((m_matfile = fopen((m_filePath + m_fileName + string(".mtl")).c_str(), "wb")) == NULL)
			return CStatus::Fail;

	// output header
	Output(m_file, "# Custom Wavefront OBJ Exporter\r\n");

	if (bWriteMTLFile)
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

			if (foundProperty.IsValid()) {

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

			// if(bWriteMTLFile) - commented out because usemtl might be used for other purposes
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

			if (bWriteMTLFile)
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

	if (bWriteMTLFile)
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


