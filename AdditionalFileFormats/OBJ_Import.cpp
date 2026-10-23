#include "OBJ.h"
#include "FastIO.h"

using fastio::parseDouble;

// Resolves a 1-based OBJ index token to a 0-based index. Negative values are relative to the
// number of elements defined so far (OBJ spec). Returns -1 when the index is invalid or undefined.
static long resolveObjIndex(const char* token, long nbDefined)
{
	long v = fastio::parseLong(token);
	long ix = v > 0 ? v - 1 : (v < 0 ? nbDefined + v : -1);
	return (ix >= 0 && ix < nbDefined) ? ix : -1;
}

// true if the whole token parses as a number (used to skip the numeric values of MTL texture options)
static bool isNumberToken(const char* token)
{
	char* end = NULL;
	strtod(token, &end);
	return end != token && *end == '\0';
}

// true for [A-Za-z_][A-Za-z0-9_]*: the only material keys that are passed on to the script side
static bool isIdentifier(const char* s)
{
	if (!(isalpha((unsigned char)*s) || *s == '_'))
		return false;
	for (const char* p = s + 1; *p; p++)
		if (!(isalnum((unsigned char)*p) || *p == '_'))
			return false;
	return true;
}

// escapes a value so that it is safe inside a double quoted JScript string literal
static string escapeForJsString(const string& s)
{
	string out;
	out.reserve(s.size() + 8);
	for (size_t i = 0; i < s.size(); i++) {
		unsigned char c = (unsigned char)s[i];
		switch (c) {
		case '\\': out += "\\\\"; break;
		case '"':  out += "\\\""; break;
		case '\n': out += "\\n"; break;
		case '\r': out += "\\r"; break;
		case '\t': out += "\\t"; break;
		default:
			if (c >= 0x20) // other control characters are dropped
				out += (char)c;
		}
	}
	return out;
}

CStatus COBJ::Execute_Import(CRefArray& selectedObjects, string initFilePathName)
{
	Application app;

	string mat_file = "";

	// look up mesh definitions, might reoccur while parsing obj file
	unordered_map<string, MeshData*> mesh_from_name;

	// store material definitions by parsing mat file
	unordered_map<string, string> json_from_mat_name;

	// look up which clusters/objects will be assigned which material
	unordered_map<string, CRefArray> scene_refs_from_mat_name;

	CFileFormat::initStrings(initFilePathName);

	vAutoScaling.Set(1.0, 1.0, 1.0);

	m_progress.PutCaption("OBJ Import - importing " + CString(initFilePathName.c_str()));
	m_progress.PutVisible(true);

	CStatus status = Import(initFilePathName, mesh_from_name, json_from_mat_name, mat_file);
	if (status != CStatus::OK) { // failed or cancelled: build nothing
		for (auto p : mesh_from_name) delete(p.second);
		m_progress.PutVisible(false);
		return status;
	}

	if (mat_file != "") { // use same function so material lib could be defined inside obj file
		string ignoredMatFile;
		if (Import(m_filePath + mat_file, mesh_from_name, json_from_mat_name, ignoredMatFile) != CStatus::OK)
			app.LogMessage("Material library '" + CString((m_filePath + mat_file).c_str()) + "' could not be read. No materials will be applied.", siWarningMsg);
	}

	Model root = app.GetActiveSceneRoot();

	// create the objects in the order their groups appear in the file (the map iterates in hash order)
	vector<MeshData*> meshesInFileOrder;
	for (auto p : mesh_from_name)
		meshesInFileOrder.push_back(p.second);
	sort(meshesInFileOrder.begin(), meshesInFileOrder.end(), [](MeshData* a, MeshData* b) { return a->fileOrder < b->fileOrder; });

	int nbMeshes = (int)meshesInFileOrder.size();

	// replacing a mesh needs exactly one mesh in the file, otherwise all groups would be built into the same object
	if (selectedObjects.GetCount() != 0) {
		int nbMeshesWithPolygons = 0;
		for (MeshData* pMesh : meshesInFileOrder)
			if (!pMesh->PolygonPointCounts.empty())
				nbMeshesWithPolygons++;
		if (nbMeshesWithPolygons > 1) {
			app.LogMessage("The file contains " + CString(nbMeshesWithPolygons) + " objects but replacing a mesh needs exactly one. Set 'Create an Object' to 'Create a Single Object', or deselect all to create new objects.", siErrorMsg);
			for (auto p : mesh_from_name) delete(p.second);
			m_progress.PutVisible(false);
			return CStatus::Fail;
		}
	}

	int ix = 1;
	for (MeshData* pCurrentMesh : meshesInFileOrder) {

		m_progress.PutCaption("Importing OBJ (Building Mesh " + CString(ix++) + " of " + CString(nbMeshes) + ")");

		if (pCurrentMesh->PolygonPointCounts.size() == 0)
			continue;

		X3DObject xobj;
		if (selectedObjects.GetCount() != 0) {
			xobj = X3DObject(selectedObjects[0]);
			PolygonMesh existingMesh = xobj.GetActivePrimitive().GetGeometry();
			if (!existingMesh.IsValid()) {
				app.LogMessage(L"The selected object is not a polygon mesh, nothing was imported.", siErrorMsg);
				status = CStatus::Fail;
				break;
			}
		}
		else
			root.AddPrimitive(L"EmptyPolygonMesh", CString(pCurrentMesh->name.c_str()), xobj);

		if (Prefs.OBJ_AutoScale)
			xobj.PutLocalScaling(vAutoScaling); // after creation: on an empty X3DObject this was a no-op

		Primitive prim = xobj.GetActivePrimitive();

		PolygonMesh mesh = prim.GetGeometry();

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

		bool bImportWithOriginalPointOrder = (nbMeshes == 1 && pCurrentMesh->PointPositions.size() == PP_inFile.size() && pCurrentMesh->Mask.size() == Mask_inFile.size());

		if(bImportWithOriginalPointOrder) {
			app.LogMessage("Found only 1 mesh. We can import using the original point order", siInfoMsg);
			// restore original file vertex order (important for zbrush base mesh reimport for example)
			for (size_t i = 0; i < pCurrentMesh->PointIndices.size(); i++) {
				pCurrentMesh->PointIndices[i] = pCurrentMesh->fileIxFromMeshIx[pCurrentMesh->PointIndices[i]];
			}
			meshBuilder.AddVertices(PP_inFile.size() / 3, &PP_inFile[0]);
		}
		else {
			meshBuilder.AddVertices(pCurrentMesh->PointPositions.size() / 3, &pCurrentMesh->PointPositions[0]);
		}

		meshBuilder.AddPolygons(pCurrentMesh->PolygonPointCounts.size(), &pCurrentMesh->PolygonPointCounts[0], &pCurrentMesh->PointIndices[0]);

		// build mesh

		CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
		if (err != CStatus::OK)
			app.LogMessage("Error building the mesh: " + err.GetDescription());

		for (auto a : pCurrentMesh->IcsMaterialClusters) {
			string mat_name = a.first;
			CLongArray& cluster_indices = a.second;

			long nbPolysInCluster = cluster_indices.GetCount();
			long nbPolysInObject = mesh.GetPolygons().GetCount();

			if (nbPolysInCluster == 0)
				continue;

			if (!scene_refs_from_mat_name.count(mat_name))
				scene_refs_from_mat_name.insert({ mat_name, CRefArray() });

			if (nbPolysInCluster < nbPolysInObject) {
				Cluster cls;
				mesh.AddCluster(siPolygonCluster, CString(mat_name.c_str()), cluster_indices, cls);

				scene_refs_from_mat_name[mat_name].Add(cls);
			}
			else // don't create 100% coverage clusters, use object itself (TODO: add preferernce for this)
				scene_refs_from_mat_name[mat_name].Add(xobj.GetRef()); // not prim.GetParent3DObject(): that yields an invalid ref here
		}

		CClusterPropertyBuilder cpBuilder = mesh.GetClusterPropertyBuilder();

		if (pCurrentMesh->bHasUVs && pCurrentMesh->UVs.size() > 0) {
			ClusterProperty uv = cpBuilder.AddUV();
			uv.SetValues(&pCurrentMesh->UVs[0], pCurrentMesh->UVs.size() / 3);
		}

		if (Prefs.OBJ_ImportPolypaint && bFileHasPolypaint) {
			ClusterProperty rgb = cpBuilder.AddVertexColor(CString("PolyPaint"), CString("Vertex_Colors"));
			rgb.SetValues(&pCurrentMesh->RGBA[0], pCurrentMesh->RGBA.size() / 4);
		}

		if (Prefs.OBJ_ImportMask && bFileHasPolypaint) {
			ClusterProperty weights = cpBuilder.AddWeightMap(CString("Mask"), CString("WeightMapCls"));
			if (bImportWithOriginalPointOrder)
				weights.SetValues(&Mask_inFile[0], Mask_inFile.size());
			else
				weights.SetValues(&pCurrentMesh->Mask[0], pCurrentMesh->Mask.size());
		}

		if (Prefs.OBJ_ImportUserNormals && pCurrentMesh->bHasNormals) {
			if (pCurrentMesh->bNormalsIncomplete)
				app.LogMessage("Mesh '" + CString(pCurrentMesh->name.c_str()) + "': some polygons have no normal indices, user normals are not imported for it (zero normals would shade black).", siWarningMsg);
			else {
				ClusterProperty normals = cpBuilder.AddUserNormal();
				normals.SetValues(&pCurrentMesh->Normals[0], pCurrentMesh->Normals.size() / 3);
			}
		}
	}

	for (auto a : scene_refs_from_mat_name) {
		string mat_name = a.first;
		CRefArray& scene_refs = a.second;

		if (json_from_mat_name.count(mat_name)) {
			// the targets go as a comma separated name list: a CRefArray argument does not reach the
			// script command as a usable collection, so SIAssignMaterial in the callback failed silently
			CString targets;
			for (long i = 0, i_max = scene_refs.GetCount(); i < i_max; i++) {
				if (i > 0)
					targets += L",";
				targets += SIObject(scene_refs[i]).GetFullName();
			}

			CValue rVal;
			CValueArray args;
			args.Add(CString(mat_name.c_str()));
			args.Add(CString(json_from_mat_name[mat_name].c_str()));
			args.Add(targets);
			args.Add(CString(m_filePath.c_str()));
			if (app.ExecuteCommand(L"ApplyShaderTree", args, rVal) != CStatus::OK)
				app.LogMessage("Applying material '" + CString(mat_name.c_str()) + "' failed, see messages above.", siWarningMsg);
		}
	}

	for (auto p : mesh_from_name) delete(p.second);

	m_progress.PutVisible(false);

	return status;
}

// Parses an OBJ (or MTL, same syntax) file into mesh_map / material_map.
// Returns CStatus::Fail on a malformed file and CStatus::False when the user cancelled;
// in both cases the caller owns (and must delete) whatever is in mesh_map.
CStatus COBJ::Import(string filePathNam,
	unordered_map<string, MeshData*>& mesh_map,
	unordered_map<string, string>& material_map,
	string& mat_file)
{
	Application app;

	if ((m_pFile = fopen(filePathNam.c_str(), "rb")) == NULL) {
		app.LogMessage("Could not open '" + CString(filePathNam.c_str()) + "'.", siErrorMsg);
		return CStatus::Fail;
	}

	_fseeki64(m_pFile, 0, SEEK_END);
	long long file_size_bytes = _ftelli64(m_pFile); // 64 bit: files over 2 GB are common for scans
	rewind(m_pFile);

	m_progress.PutValue(0);
	m_progress.PutMaximum((long)(file_size_bytes / 10000));

	const char* new_object_tag = Prefs.OBJ_CreateObjectsTag.GetAsciiString();
	const char* new_cluster_tag = Prefs.OBJ_CreateClustersTag.GetAsciiString();

	// if a line is longer that 4096 bytes (can happen for ngons), increment buffer repeatedly until line fits
	int max_line_length = 4096;
	const int increment_bytes = 2048;

	char* pbuf = (char*)malloc(max_line_length);

	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	CLongArray* pCurrentMatCluster = NULL;
	string strCurrentMatName;
	MeshData* pCurrentMesh = new MeshData(m_fileName);
	MeshData* pDefaultMesh = pCurrentMesh; // deleted at the end unless it made it into mesh_map
	MeshData* pPreviousMesh = NULL;

	string currentMatName = "";
	string currentMatTagName = "";

	vector<float> UVs_inFile, Normals_inFile;
	long nbPP_inFile = 0, nbUVs_inFile = 0, nbRGB_inFile = 0, nbMask_inFile = 0, nbNormals_inFile = 0; // probably faster than using Count() methods

	unordered_map<string, bool> unsupportedTokens;

	long current_action_id = 0; // inactve
	long nbModeSwitches2 = 0, nbModeSwitches3 = 0, nbModeSwitches4 = 0, nbModeSwitches5 = 0, nbModeSwitches6 = 0;
	long long pos_in_file_bytes = 0;
	int line_in_file = 0;
	long nextFileOrder = 0;

	CStatus status = CStatus::OK;
	bool bAbort = false;

	do {
		if (!fgets(pbuf, max_line_length, m_pFile))
			break;

		// grow the buffer until the whole line is in (long lines happen with big ngons); the line
		// length tracks the file position, which saves a file position query per line
		size_t len = strlen(pbuf);
		while (len > 0 && pbuf[len - 1] != '\n' && !feof(m_pFile)) {
			max_line_length += increment_bytes;
			pbuf = (char*)realloc(pbuf, max_line_length);

			if (!fgets(pbuf + len, max_line_length - (int)len, m_pFile))
				break;

			len += strlen(pbuf + len);
		}
		pos_in_file_bytes += len;

		vector<char*> tokens = quickSplit(pbuf, ' '); // speed opt std::string => char*

		size_t nbTokens = tokens.size();
		if (nbTokens < 2) {
			line_in_file++;
			continue;
		}

		char* firstToken = tokens[0];
		char* secondToken = tokens[1];

		if (STR_EQUAL(firstToken, "f"))
		{
			if (current_action_id != 5) {
				current_action_id = 5;
				if (pPreviousMesh != pCurrentMesh) {
					nbModeSwitches5++;
					if (nbModeSwitches5 < 25)
						m_progress.PutCaption("Importing OBJ (Polygons of " + CString(pCurrentMesh->name.c_str()) + ")");
					else if (nbModeSwitches5 == 25)
						m_progress.PutCaption("Importing OBJ (Polygons)"); // too much logging, switch to simple log
				}
				pPreviousMesh = pCurrentMesh;
			}

			if (!mesh_map.size()) {
				pCurrentMesh->fileOrder = nextFileOrder++;
				mesh_map.insert({ pCurrentMesh->name, pCurrentMesh });
			}

			// index of the polygon about to be added (PolygonPointCounts grows at the end of this block)
			if (pCurrentMatCluster)
				pCurrentMatCluster->Add((long)pCurrentMesh->PolygonPointCounts.size());

			size_t nbPolypoints = nbTokens - 1;

			for (int i = 1; i <= nbPolypoints; i++) {

				// ** Face format: (PointIndex/UVIndex/NormalIndex), e.g.
				//    f 110/37073/2992 766/37074/2958 658/37075/2885 657/37076/2818
				// alt. examples: 110 (no UV, no Normals), 110/37073 (UVs but no Normals), 110//2992 (Normals but no UVs)
				// ** Note these are indices of elements in the 3 main file arrays, not the individual objects.
				// keepEmpty=true so that "110//2992" yields 3 fields with an empty UV field

				vector<char*> qTriple = quickSplit(tokens[i], '/', true); // speed opt std::string => char*

				size_t nbTripleEls = qTriple.size();

				long fileIx = resolveObjIndex(qTriple[0], nbPP_inFile);
				if (fileIx < 0) {
					app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" refers to a point position that is not defined (yet). Import aborted.", siErrorMsg);
					status = CStatus::Fail;
					bAbort = true;
					break;
				}

				// file vertex -> point of the current mesh (created on first use)
				if (fileIx >= (long)m_ownerOfFileIx.size()) {
					m_ownerOfFileIx.resize(nbPP_inFile, NULL);
					m_meshIxOfFileIx.resize(nbPP_inFile, -1);
				}

				long meshIx;
				bool bNewPoint = false;
				MeshData* pOwner = m_ownerOfFileIx[fileIx];
				if (pOwner == pCurrentMesh)
					meshIx = m_meshIxOfFileIx[fileIx];
				else if (pOwner == NULL) {
					meshIx = (long)pCurrentMesh->fileIxFromMeshIx.size();
					m_ownerOfFileIx[fileIx] = pCurrentMesh;
					m_meshIxOfFileIx[fileIx] = meshIx;
					bNewPoint = true;
				}
				else { // the vertex already belongs to another mesh (rare): per mesh map for these
					auto found = pCurrentMesh->sharedMeshIxFromFileIx.find(fileIx);
					if (found != pCurrentMesh->sharedMeshIxFromFileIx.end())
						meshIx = found->second;
					else {
						meshIx = (long)pCurrentMesh->fileIxFromMeshIx.size();
						pCurrentMesh->sharedMeshIxFromFileIx.insert({ fileIx, meshIx });
						bNewPoint = true;
					}
				}

				if (bNewPoint) {
					pCurrentMesh->fileIxFromMeshIx.push_back(fileIx);
					pCurrentMesh->PointPositions.push_back(PP_inFile[3 * fileIx]);
					pCurrentMesh->PointPositions.push_back(PP_inFile[3 * fileIx + 1]);
					pCurrentMesh->PointPositions.push_back(PP_inFile[3 * fileIx + 2]);

					if (bFileHasPolypaint && fileIx < nbMask_inFile)
						pCurrentMesh->Mask.push_back(Mask_inFile[fileIx]);
				}

				pCurrentMesh->PointIndices.push_back(meshIx);

				if (Prefs.OBJ_ImportUVs) {
					long UVIndex = -2; // -2: no UV index on this corner, -1: invalid UV index
					if (nbTripleEls > 1 && *(qTriple[1]) != '\0' && bFileHasUVs)
						UVIndex = resolveObjIndex(qTriple[1], nbUVs_inFile);

					if (UVIndex >= 0) {
						pCurrentMesh->bHasUVs = true;
						pCurrentMesh->UVs.push_back(UVs_inFile[3 * UVIndex]);
						pCurrentMesh->UVs.push_back(UVs_inFile[3 * UVIndex + 1]);
						pCurrentMesh->UVs.push_back(UVs_inFile[3 * UVIndex + 2]);
					}
					else {
						if (UVIndex == -1) {
							app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing UV coordinates. Loading UVs is disabled for the rest of this import.", siErrorMsg);
							Prefs.OBJ_ImportUVs = false;
						}
						double u = ((double)rand() / (RAND_MAX)) / 10000; //bug workaround: uvs on 0 can lead to crashes in subdivision.dll for some topologies...
						double v = ((double)rand() / (RAND_MAX)) / 10000;
						pCurrentMesh->UVs.push_back((float)u);
						pCurrentMesh->UVs.push_back((float)v);
						pCurrentMesh->UVs.push_back(0.0f);
					}
				}

				if (Prefs.OBJ_ImportUserNormals) {
					long NormalIndex = -2; // -2: no normal index on this corner, -1: invalid normal index
					if (nbTripleEls > 2 && *(qTriple[2]) != '\0' && bFileHasNormals)
						NormalIndex = resolveObjIndex(qTriple[2], nbNormals_inFile);

					if (NormalIndex >= 0) {
						pCurrentMesh->bHasNormals = true;
						pCurrentMesh->Normals.push_back(Normals_inFile[3 * NormalIndex]);
						pCurrentMesh->Normals.push_back(Normals_inFile[3 * NormalIndex + 1]);
						pCurrentMesh->Normals.push_back(Normals_inFile[3 * NormalIndex + 2]);
					}
					else {
						if (NormalIndex == -1) {
							app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing normal vector entries. Loading normals is disabled for the rest of this import.", siErrorMsg);
							Prefs.OBJ_ImportUserNormals = false;
						}
						// keep the array aligned with the samples; the mesh is flagged so no user normals get created from it
						pCurrentMesh->bNormalsIncomplete = true;
						pCurrentMesh->Normals.push_back(0.0);
						pCurrentMesh->Normals.push_back(0.0);
						pCurrentMesh->Normals.push_back(0.0);
					}
				}

				if (Prefs.OBJ_ImportPolypaint || Prefs.OBJ_ImportMask)
					if (bFileHasPolypaint) {
						if (fileIx < nbRGB_inFile) {
							pCurrentMesh->RGBA.push_back(RGB_inFile[3 * fileIx]);
							pCurrentMesh->RGBA.push_back(RGB_inFile[3 * fileIx + 1]);
							pCurrentMesh->RGBA.push_back(RGB_inFile[3 * fileIx + 2]);
							pCurrentMesh->RGBA.push_back(1.0f); // alpha, same 0..1 range as the colors
						}
						else {
							app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing polypaint color entries. Loading polypaint/mask is diabled for the rest of this import.", siErrorMsg);
							Prefs.OBJ_ImportPolypaint = false;
							Prefs.OBJ_ImportMask = false;
							// return CStatus::Fail; .. continue instead of failing
						}
					}
			}
			if (bAbort)
				break;

			pCurrentMesh->PolygonPointCounts.push_back((long)nbPolypoints);
		}
		else if (STR_EQUAL(firstToken, "v")) {
			if (current_action_id != 2) {
				current_action_id = 2;
				nbModeSwitches2++;
				if (nbModeSwitches2 < 20)
					m_progress.PutCaption("Importing OBJ (Vertex Positions)");
			}

			if (nbTokens < 4) {
				app.LogMessage(L"The point position in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" has fewer than 3 coordinates. Import aborted.", siErrorMsg);
				status = CStatus::Fail;
				break;
			}

			// kept as double: float would lose precision for scans with large coordinates
			double x = parseDouble(secondToken);
			PP_inFile.push_back(x);
			PP_inFile.push_back(parseDouble(tokens[2]));
			PP_inFile.push_back(parseDouble(tokens[3]));

			if (x > lfMaxExtentX) // for auto scale
				lfMaxExtentX = x;

			if (x < lfMinExtentX) // for auto scale
				lfMinExtentX = x;

			if (tokens.size() == 7) {
				bFileHasPolypaint = true;
				Mask_inFile.push_back(1.0);
				RGB_inFile.push_back((float)parseDouble(tokens[4]));
				RGB_inFile.push_back((float)parseDouble(tokens[5]));
				RGB_inFile.push_back((float)parseDouble(tokens[6]));
				nbRGB_inFile++;
				nbMask_inFile++;
			}

			nbPP_inFile++;
		}
		else if (STR_EQUAL(firstToken, "vt")) {
			bFileHasUVs = true;

			if (current_action_id != 3) {
				current_action_id = 3;
				nbModeSwitches3++;
				if (nbModeSwitches3 < 20)
					m_progress.PutCaption("Importing OBJ (UVs)");
			}

			// v and w are optional in the OBJ spec, missing components default to 0
			UVs_inFile.push_back((float)parseDouble(secondToken));
			UVs_inFile.push_back(nbTokens > 2 ? (float)parseDouble(tokens[2]) : 0.0f);
			UVs_inFile.push_back(nbTokens > 3 ? (float)parseDouble(tokens[3]) : 0.0f);

			nbUVs_inFile++;
		}
		else if (STR_EQUAL(firstToken, "vn")) {
			bFileHasNormals = true;

			if (current_action_id != 4) {
				current_action_id = 4;
				nbModeSwitches4++;
				if (nbModeSwitches4 < 20)
					m_progress.PutCaption("Importing OBJ (Normals)");
			}

			Normals_inFile.push_back((float)parseDouble(secondToken));
			Normals_inFile.push_back(nbTokens > 2 ? (float)parseDouble(tokens[2]) : 0.0f);
			Normals_inFile.push_back(nbTokens > 3 ? (float)parseDouble(tokens[3]) : 0.0f);

			nbNormals_inFile++;
		}
		else if (STR_EQUAL(firstToken, "#MRGB"))
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
				Mask_inFile.push_back(1.0f - (16.0f * hex2dec(secondToken[o]) + hex2dec(secondToken[o + 1])) / 255.0f);
				RGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 2]) + hex2dec(secondToken[o + 3])) / 255.0f);
				RGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 4]) + hex2dec(secondToken[o + 5])) / 255.0f);
				RGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 6]) + hex2dec(secondToken[o + 7])) / 255.0f);
				nbMask_inFile++;
				nbRGB_inFile++;
			}
		}
		else if (STR_EQUAL(firstToken, new_object_tag) || STR_EQUAL(firstToken, new_cluster_tag))
		{
			if (STR_EQUAL(firstToken, new_object_tag)) {
				string name(secondToken);

				if (mesh_map.count(name) == 0) {
					if (pCurrentMesh->bIsDefaultMesh) {
						mesh_map.erase(pCurrentMesh->name);
						pCurrentMesh->name = name;
					}
					else
						pCurrentMesh = new MeshData(name);

					pCurrentMesh->fileOrder = nextFileOrder++;
					mesh_map.insert({ name, pCurrentMesh });
					pCurrentMesh->bIsDefaultMesh = false;
				}
				else
					pCurrentMesh = mesh_map[name];
			}

			if (STR_EQUAL(firstToken, new_cluster_tag))
				strCurrentMatName = secondToken;

			// (re)bind the material cluster to the current mesh: in OBJ the current material
			// stays active across object/group switches, so a new mesh needs its own cluster for it
			pCurrentMatCluster = NULL;
			if (strCurrentMatName != "") {
				if (pCurrentMesh->IcsMaterialClusters.count(strCurrentMatName) == 0)
					pCurrentMesh->IcsMaterialClusters.insert({ strCurrentMatName, CLongArray() });
				pCurrentMatCluster = &pCurrentMesh->IcsMaterialClusters[strCurrentMatName];
			}
		}
		else if (STR_EQUAL(firstToken, "mtllib")) {
			string name;
			if (nbTokens == 1)
				name = m_fileName + ".mtl";
			else {
				int i = 1;
				while (i < nbTokens) { // recombine tokens with spaces
					name += tokens[i++];
					if (i < nbTokens)
						name += " ";
				}
			}
			mat_file = name;
		}
		else if (STR_EQUAL(firstToken, "newmtl")) {
			string name(nbTokens > 1 ? secondToken : "default");

			if (material_map.count(name) != 0) {
				app.LogMessage("Multiple definition of Material " + CString(name.c_str()) + " found. Using first.", siVerboseMsg);
				name = "";
			}
			else
				material_map.insert({ name, "var material = {\n" });

			currentMatName = name;
			currentMatTagName = "";
		}
		else if (*firstToken == '#') {
			// ignore comments, with or without a space after the '#'
		}
		else /* material tags like Ka, Kd, ... */ {
			if (currentMatName != "") {
				// Every value becomes an escaped string literal under a validated key, so a crafted .mtl cannot
				// run code when the definition is evaluated on the script side. Numeric values are converted
				// back by normalizeMaterialDefinition() in AdditionalFileFormats.js.
				if (!isIdentifier(firstToken)) {
					app.LogMessage("Material token (" + CString(firstToken) + ") ignored: not a valid name.", siVerboseMsg);
				}
				else {
					size_t i = 1;
					if (!strncmp(firstToken, "map", 3) || STR_EQUAL(firstToken, "bump") || STR_EQUAL(firstToken, "disp") || STR_EQUAL(firstToken, "decal") || STR_EQUAL(firstToken, "refl"))
					{
						// skip texture options such as "-s 1 1 1", "-bm 0.5" or "-clamp on": what remains is the file name
						while (i < nbTokens && tokens[i][0] == '-' && tokens[i][1] != '\0' && !isdigit((unsigned char)tokens[i][1])) {
							const char* option = tokens[i++];
							if (STR_EQUAL(option, "-o") || STR_EQUAL(option, "-s") || STR_EQUAL(option, "-t") || STR_EQUAL(option, "-mm")) {
								while (i < nbTokens && isNumberToken(tokens[i])) // 1 to 3 values
									i++;
							}
							else if (i < nbTokens) // every other option takes exactly one value
								i++;
						}
					}

					string value;
					while (i < nbTokens) { // recombine tokens with spaces
						value += tokens[i++];
						if (i < nbTokens)
							value += " ";
					}

					if (currentMatTagName != "")
						material_map[currentMatName].append(",\n");
					currentMatTagName = string(firstToken);

					material_map[currentMatName].append("\t\"");
					material_map[currentMatName].append(firstToken);
					material_map[currentMatName].append("\": \"");
					material_map[currentMatName].append(escapeForJsString(value));
					material_map[currentMatName].append("\"");
				}
			}
			else if (unsupportedTokens.count(string(firstToken)) == 0 && *firstToken != '#') {
				app.LogMessage("Unhandled token (" + CString(firstToken) + ") ignored.", siVerboseMsg);
				unsupportedTokens.insert({ string(firstToken), true });
			}
		}

		line_in_file++;
		if (line_in_file % 1000 == 0) { // increment progress bar
			m_progress.Increment(1);
			m_progress.PutValue((long)(pos_in_file_bytes / 10000));
			if (m_progress.IsCancelPressed()) {
				status = CStatus::False;
				break;
			}
		}
	} while (!feof(m_pFile));

	fclose(m_pFile);
	m_pFile = NULL;
	free(pbuf);

	// the mesh created at the start of this pass is only owned by mesh_map if a polygon was added to it
	// (never the case when parsing a material library)
	bool bDefaultMeshInMap = false;
	for (auto& p : mesh_map)
		if (p.second == pDefaultMesh)
			bDefaultMeshInMap = true;
	if (!bDefaultMeshInMap)
		delete pDefaultMesh;

	if (status != CStatus::OK)
		return status;

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale

	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);

	// close the material object literals (only once: materials may come from the obj and the mtl pass)
	for (auto& m : material_map)
		if (m.second.size() < 2 || m.second.compare(m.second.size() - 2, 2, "};") != 0)
			m.second.append("\n};");

	return CStatus::OK;
}

void COBJ::Output(FILE* file, string str) {
	fputs(str.c_str(), file); // not fprintf: the string may contain '%' (names, texture paths)
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
