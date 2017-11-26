#include "OBJ.h"

#define log app.LogMessage

class MeshData {
public:
	MeshData(string name) { this->name = name; }

	string name;

	bool bIsDefaultMesh = true; // has not been initialized by an "o" tag yet

	long ixPolygon = 0;
	vector<long> PolygonPointCounts;
	vector<double> PointPositions;
	vector<long> PointIndices;
	long ixPointPosition_next = 0;
	unordered_map<long, long> ixPointPosition_lookup;
	unordered_map<string, CLongArray> IcsMaterialClusters;

	bool bHasNormals = false;
	vector <float> Normals;

	bool bHasUVs = false;
	vector<float> UVs;

	vector<float> RGBA;
	vector<float> Mask; 
};

CStatus COBJ::Execute_Import(string initFilePathName, bool OBJ_ImportUVs, bool OBJ_ImportUserNormals, bool OBJ_ImportMask, bool OBJ_ImportPolypaint, int OBJ_CreateObjectsTag, int OBJ_CreateClustersTag)
{
	Application app;

	char* new_object_tag;
	char* new_cluster_tag;

	switch ((int)OBJ_CreateObjectsTag) {
		case 1: new_object_tag = "o"; break;
		case 2: new_object_tag = "usemtl"; break;
		case 3: new_object_tag = "_____"; break;
		default: new_object_tag = "g"; break;
	}

	switch ((int)OBJ_CreateClustersTag) {
		case 0: new_cluster_tag = "g"; break;
		case 1: new_cluster_tag = "o"; break;		
		case 3: new_cluster_tag = "_____"; break;
		default: new_cluster_tag = "usemtl"; break;
	}

	Model root = app.GetActiveSceneRoot();	

	CLongArray* pCurrentMatCluster = NULL;
	string strCurrentMatName;

	bool bFileHasUVs = false;
	bool bFileHasNormals = false;
	bool bFileHasPolypaint = false; // polypaint is per file (all vertices in a file), uv and normals are per object

	unordered_map<string, MeshData*> mesh_map;

	CFileFormat::initStrings(initFilePathName);
	
	MeshData* pCurrentMesh = new MeshData(m_fileName);

	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	if ((m_file = fopen(m_filePathName.c_str(), "rb")) == NULL)
		return CStatus::Fail;

	fseek(m_file, 0L, SEEK_END);
	long file_size_bytes = ftell(m_file);
	rewind(m_file);

	m_progress.PutValue(0);
	m_progress.PutMaximum(file_size_bytes / 10000);
	m_progress.PutVisible(true);

	int max_line_length = 4096;
	int increment_bytes = 2048;

	char* pbuf = (char*)malloc(max_line_length);
	*(pbuf + max_line_length - 1) = '\0';

	vector<double> PP_inFile;
	vector<float> UVs_inFile, MRGB_inFile, Normals_inFile;

	long current_action_id = 0; // inactve
	long nbModeSwitches2 = 0, nbModeSwitches3 = 0, nbModeSwitches4 = 0, nbModeSwitches5 = 0, nbModeSwitches6 = 0;
	int pos_in_file_bytes = 0, last_pos_in_file_bytes, actual_bytes_read;
	int line_in_file = 0;
	do {
		if (!fgets(pbuf, max_line_length, m_file))
			break;
		
		last_pos_in_file_bytes = pos_in_file_bytes;
		pos_in_file_bytes = ftell(m_file);
		actual_bytes_read = pos_in_file_bytes - last_pos_in_file_bytes + 1;

		// realloc if line is too short
		while (actual_bytes_read == max_line_length && *(pbuf + max_line_length - 2) != '\n') {
			max_line_length += increment_bytes;
			pbuf = (char*)realloc(pbuf, max_line_length);

			if (!fgets(pbuf + max_line_length - increment_bytes - 1, increment_bytes + 1, m_file))
				break;

			pos_in_file_bytes = ftell(m_file);
			actual_bytes_read = pos_in_file_bytes - last_pos_in_file_bytes + 1;
		}

		vector<char*>& tokens = quickSplit(pbuf, ' '); // speed opt std::string => char*

		size_t nbTokens = tokens.size();
		if (nbTokens < 2) {
			line_in_file++;
			continue;
		}

		char* firstToken = tokens[0];
		char* secondToken = tokens[1];

		if (!strcmp(firstToken, "f"))
		{
			if (current_action_id != 5) {
				current_action_id = 5;
				nbModeSwitches5++;
				if (nbModeSwitches5 < 20)
					m_progress.PutCaption("Importing OBJ (Polygons of " + CString(pCurrentMesh->name.c_str()) + ")");
			}

			if (!mesh_map.size())
				mesh_map.insert({ pCurrentMesh->name, pCurrentMesh });

			if (pCurrentMatCluster)
				pCurrentMatCluster->Add(pCurrentMesh->ixPolygon++); // add current polygon index to material cluster

			size_t nbPolypoints = nbTokens - 1;
			for (int i = 1; i <= nbPolypoints; i++) { 

				vector<char*>& qTriple = quickSplit(tokens[i], '/'); // speed opt std::string => char*

				// ** Triple format: ********** 
				// **
				// ** PointIndex / UVIndex / NormalIndex

				size_t nbTripleEls = qTriple.size();				

				long ixPointPosition_inFile = atol(qTriple[0]) - 1;
				long ixPointPosition;
				if (pCurrentMesh->ixPointPosition_lookup.count(ixPointPosition_inFile) == 0) {
					ixPointPosition = pCurrentMesh->ixPointPosition_next;
					pCurrentMesh->ixPointPosition_lookup.insert({ ixPointPosition_inFile, pCurrentMesh->ixPointPosition_next++ });
					pCurrentMesh->PointPositions.push_back(PP_inFile[3 * ixPointPosition_inFile]);
					pCurrentMesh->PointPositions.push_back(PP_inFile[3 * ixPointPosition_inFile + 1]);
					pCurrentMesh->PointPositions.push_back(PP_inFile[3 * ixPointPosition_inFile + 2]);
					if (bFileHasPolypaint)
						pCurrentMesh->Mask.push_back(MRGB_inFile[4 * ixPointPosition_inFile]);
				}
				else
					ixPointPosition = pCurrentMesh->ixPointPosition_lookup[ixPointPosition_inFile];

				pCurrentMesh->PointIndices.push_back(ixPointPosition);

				if (OBJ_ImportUVs) 
					if (nbTripleEls > 1 && *(qTriple[1]) != '\0' && bFileHasUVs) {
						pCurrentMesh->bHasUVs = true;
						int ixUV_inFile = atoi(qTriple[1]) - 1;
						pCurrentMesh->UVs.push_back(UVs_inFile[3 * ixUV_inFile]);
						pCurrentMesh->UVs.push_back(UVs_inFile[3 * ixUV_inFile + 1]);
						pCurrentMesh->UVs.push_back(UVs_inFile[3 * ixUV_inFile + 2]);
					}
					else {
						double u = ((double)rand() / (RAND_MAX)) / 10000; //bug workaround: uvs on 0 can lead to crashes in subdivision.dll for some topologies...
						double v = ((double)rand() / (RAND_MAX)) / 10000;
						pCurrentMesh->UVs.push_back(u);
						pCurrentMesh->UVs.push_back(v);
						pCurrentMesh->UVs.push_back(0.0f);
					}

				if (OBJ_ImportUserNormals) 
					if (nbTripleEls > 2 && *(qTriple[2]) != '\0' && bFileHasNormals) {
						pCurrentMesh->bHasNormals = true;
						int ixNormal_inFile = atoi(qTriple[2]) - 1;
						pCurrentMesh->Normals.push_back(Normals_inFile[3 * ixNormal_inFile]);
						pCurrentMesh->Normals.push_back(Normals_inFile[3 * ixNormal_inFile + 1]);
						pCurrentMesh->Normals.push_back(Normals_inFile[3 * ixNormal_inFile + 2]);
					}
					else {
						pCurrentMesh->Normals.push_back(0.0); //TODO: to be be replaced by actual normal later on
						pCurrentMesh->Normals.push_back(0.0);
						pCurrentMesh->Normals.push_back(0.0);
					}

				if (OBJ_ImportPolypaint || OBJ_ImportMask) 
					if(bFileHasPolypaint) {
						pCurrentMesh->RGBA.push_back(MRGB_inFile[4 * ixPointPosition_inFile + 1]);
						pCurrentMesh->RGBA.push_back(MRGB_inFile[4 * ixPointPosition_inFile + 2]);
						pCurrentMesh->RGBA.push_back(MRGB_inFile[4 * ixPointPosition_inFile + 3]);
						pCurrentMesh->RGBA.push_back(255.0);
					}
			}
			pCurrentMesh->PolygonPointCounts.push_back(nbPolypoints);
		}
		else if (!strcmp(firstToken, new_cluster_tag)) {
			strCurrentMatName = secondToken;
			if (pCurrentMesh->IcsMaterialClusters.count(strCurrentMatName) == 0) {
				pCurrentMesh->IcsMaterialClusters.insert({ strCurrentMatName, CLongArray() });
			}
			pCurrentMatCluster = &pCurrentMesh->IcsMaterialClusters.at(strCurrentMatName);
		}
		else if (!strcmp(firstToken, "v")) {
			if (current_action_id != 2) {
				current_action_id = 2;
				nbModeSwitches2++;
				if (nbModeSwitches2 < 20)
					m_progress.PutCaption("Importing OBJ (Vertex Positions)");
			}

			float x = (float)atof(secondToken);
			PP_inFile.push_back(x);
			PP_inFile.push_back((float)atof(tokens[2]));
			PP_inFile.push_back((float)atof(tokens[3]));

			if (x > lfMaxExtentX) // for auto scale
				lfMaxExtentX = x;

			if (x < lfMinExtentX) // for auto scale
				lfMinExtentX = x;

		}
		else if (!strcmp(firstToken, "vt")) {
			bFileHasUVs = true;

			if (current_action_id != 3) {
				current_action_id = 3;
				nbModeSwitches3++;
				if (nbModeSwitches3 < 20)
					m_progress.PutCaption("Importing OBJ (UVs)");
			}

			UVs_inFile.push_back((float)atof(secondToken));
			UVs_inFile.push_back((float)atof(tokens[2]));
			if (tokens.size() > 3)			
				UVs_inFile.push_back((float)atof(tokens[3]));
			else
				UVs_inFile.push_back(0.0);
		}
		else if (!strcmp(firstToken, "vn")) {
			bFileHasNormals = true;

			if (current_action_id != 4) {
				current_action_id = 4;
				nbModeSwitches4++;
				if (nbModeSwitches4 < 20)
					m_progress.PutCaption("Importing OBJ (Normals)");
			}

			Normals_inFile.push_back((float)atof(secondToken));
			Normals_inFile.push_back((float)atof(tokens[2]));
			Normals_inFile.push_back((float)atof(tokens[3]));	
		}
		else if (!strcmp(firstToken, new_object_tag))
		{
			string name(nbTokens > 1 ? secondToken : m_fileName);

			if (mesh_map.count(name) == 0) {
				if (pCurrentMesh->bIsDefaultMesh)
					pCurrentMesh->name = name;
				else
					pCurrentMesh = new MeshData(name);

				mesh_map.insert({ name, pCurrentMesh });
				pCurrentMesh->bIsDefaultMesh = false;
			} else
				pCurrentMesh = mesh_map[name];
		}		
		else if (!strcmp(firstToken, "#MRGB"))
		{
			bFileHasPolypaint = true;

			if (current_action_id != 6) {
				current_action_id = 6;
				nbModeSwitches6++;
				if (nbModeSwitches6 < 20)
					m_progress.PutCaption("Importing OBJ (ZBrush Polypaint/Weights)");
			}

			for (size_t i = 0, i_max = strlen(secondToken) / 8; i < i_max; i++)
			{
				size_t o = 8 * i;
				MRGB_inFile.push_back(1.0f - (16.0f * hex2dec(secondToken[o]) + hex2dec(secondToken[o + 1])) / 255.0f);
				MRGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 2]) + hex2dec(secondToken[o + 3])) / 255.0f);
				MRGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 4]) + hex2dec(secondToken[o + 5])) / 255.0f);
				MRGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 6]) + hex2dec(secondToken[o + 7])) / 255.0f);
			}
		}

		line_in_file++;
		if (line_in_file % 1000 == 0) { // increment progress bar
			m_progress.Increment(1);
			m_progress.PutValue(pos_in_file_bytes / 10000);
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

		if (pCurrentMesh->PolygonPointCounts.size() == 0)
			continue;

		// use empty mesh to create the imported mesh
		X3DObject xobj;
		root.AddPrimitive("EmptyPolygonMesh", CString(pCurrentMesh->name.c_str()), xobj); //  CString(m_fileName.c_str()), xobj);
					
		Primitive prim = xobj.GetActivePrimitive();
		PolygonMesh mesh = prim.GetGeometry();
		xobj.PutLocalScaling(vAutoScaling);

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

		meshBuilder.AddVertices(pCurrentMesh->PointPositions.size() / 3, &pCurrentMesh->PointPositions[0]);
		meshBuilder.AddPolygons(pCurrentMesh->PolygonPointCounts.size(), &pCurrentMesh->PolygonPointCounts[0], &pCurrentMesh->PointIndices[0]);

		// build mesh

		CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
		if (err != CStatus::OK)
			app.LogMessage("Error building the mesh: " + err.GetDescription());

		for (auto currentMaterial : pCurrentMesh->IcsMaterialClusters) {
			Cluster cls;
			mesh.AddCluster(siPolygonCluster, CString(currentMaterial.first.c_str()), currentMaterial.second, cls);
		}

		CClusterPropertyBuilder cpBuilder = mesh.GetClusterPropertyBuilder();

		if (pCurrentMesh->bHasUVs && pCurrentMesh->UVs.size() > 0) {
			ClusterProperty uv = cpBuilder.AddUV();
			uv.SetValues(&pCurrentMesh->UVs[0], pCurrentMesh->UVs.size() / 3);
		}

		if (OBJ_ImportPolypaint && bFileHasPolypaint) {
			ClusterProperty rgb = cpBuilder.AddVertexColor(CString("PolyPaint"), CString("Vertex_Colors"));
			rgb.SetValues(&pCurrentMesh->RGBA[0], pCurrentMesh->RGBA.size() / 4);
		}

		if (OBJ_ImportMask && bFileHasPolypaint) {
			ClusterProperty weights = cpBuilder.AddWeightMap(CString("Mask"), CString("WeightMapCls"));
			weights.SetValues(&pCurrentMesh->Mask[0], pCurrentMesh->Mask.size());
		}

		if (OBJ_ImportUserNormals && pCurrentMesh->bHasNormals) {
			ClusterProperty normals = cpBuilder.AddUserNormal();
			normals.SetValues(&pCurrentMesh->Normals[0], pCurrentMesh->Normals.size() / 3);
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



