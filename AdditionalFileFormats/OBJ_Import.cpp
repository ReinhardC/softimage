#include "OBJ.h"

//TODO: remove memory leaks (news without delete)
//TODO: reserve memory for push_back for performance

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

	Import(initFilePathName, mesh_from_name, json_from_mat_name, mat_file);
	if (mat_file != "") // use same function so material lib could be defined inside obj file
		Import(m_filePath + mat_file, mesh_from_name, json_from_mat_name, mat_file);

	Model root = app.GetActiveSceneRoot();
	
	int nbMeshes = mesh_from_name.size();

	int ix = 1;
	for (auto p : mesh_from_name) {

		MeshData* pCurrentMesh = p.second;

		m_progress.PutCaption("Importing OBJ (Building Mesh " + CString(ix++) + " of " + CString(nbMeshes) + ")");

		if (pCurrentMesh->PolygonPointCounts.size() == 0)
			continue;

		Primitive prim;
		if (selectedObjects.GetCount() != 0) {
			X3DObject xobj(selectedObjects[0]);
			xobj.PutLocalScaling(vAutoScaling);
			prim = xobj.GetActivePrimitive();
		}
		else {
			X3DObject xobj;
			xobj.PutLocalScaling(vAutoScaling);
			root.AddPrimitive(L"EmptyPolygonMesh", CString(m_fileName.c_str()), xobj);
			prim = xobj.GetActivePrimitive();
		}
		
		PolygonMesh mesh = prim.GetGeometry();

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

		if(nbMeshes == 1) {
			if(pCurrentMesh->PointPositions.size() == PP_inFile.size()) {
				/* restore original file vertex order (important for zbrush base mesh reimport for example) */
				for (auto i = 0; i < pCurrentMesh->PointIndices.size(); i++) {
					pCurrentMesh->PointIndices[i] = pCurrentMesh->fileIxFromMeshIx_lookup[pCurrentMesh->PointIndices[i]];
				}
				meshBuilder.AddVertices(PP_inFile.size() / 3, &PP_inFile[0]);
			}
			else {
				app.LogMessage("Warning couldn't restore original point order because the file contains unused vertices.", siWarningMsg);
				meshBuilder.AddVertices(pCurrentMesh->PointPositions.size() / 3, &pCurrentMesh->PointPositions[0]);
			}
		}
		else
			meshBuilder.AddVertices(pCurrentMesh->PointPositions.size() / 3, &pCurrentMesh->PointPositions[0]);

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
				scene_refs_from_mat_name[mat_name].Add(prim.GetParent3DObject());
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
			weights.SetValues(&pCurrentMesh->Mask[0], pCurrentMesh->Mask.size());
		}

		if (Prefs.OBJ_ImportUserNormals && pCurrentMesh->bHasNormals) {
			ClusterProperty normals = cpBuilder.AddUserNormal();
			normals.SetValues(&pCurrentMesh->Normals[0], pCurrentMesh->Normals.size() / 3);
		}
	}

	for (auto a : scene_refs_from_mat_name) {
		string mat_name = a.first;
		CRefArray& scene_refs = a.second;

		if (json_from_mat_name.count(mat_name)) {
			CValue rVal;
			CValueArray& args = CValueArray();
			args.Add(CString(mat_name.c_str()));
			args.Add(CString(json_from_mat_name[mat_name].c_str()));
			args.Add(scene_refs);
			args.Add(CString(m_filePath.c_str()));
			app.ExecuteCommand(L"ApplyShaderTree", args, rVal);
		}
	}

	for (auto p : mesh_from_name) delete(p.second);

	m_progress.PutVisible(false);

	return CStatus::OK;
}

CStatus COBJ::Import(string filePathNam,
	unordered_map<string, MeshData*>& mesh_map,
	unordered_map<string, string>& material_map,
	string& mat_file)
{
	Application app;

	if ((m_pFile = fopen(filePathNam.c_str(), "rb")) == NULL)
		return CStatus::Fail;

	fseek(m_pFile, 0L, SEEK_END);
	long file_size_bytes = ftell(m_pFile);
	rewind(m_pFile);

	m_progress.PutValue(0);
	m_progress.PutMaximum(file_size_bytes / 10000);

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
	MeshData* pPreviousMesh = NULL;

	string currentMatName = "";
	string currentMatTagName = "";

	vector<float> UVs_inFile, MRGB_inFile, Normals_inFile;
	long nbPP_inFile = 0, nbUVs_inFile = 0, nbMRGB_inFile = 0, nbNormals_inFile = 0; // probably faster than using Count() methods

	unordered_map<string, bool> unsupportedTokens;

	long current_action_id = 0; // inactve
	long nbModeSwitches2 = 0, nbModeSwitches3 = 0, nbModeSwitches4 = 0, nbModeSwitches5 = 0, nbModeSwitches6 = 0;
	int pos_in_file_bytes = 0, last_pos_in_file_bytes, actual_bytes_read;
	int line_in_file = 0;
	do {
		if (!fgets(pbuf, max_line_length, m_pFile))
			break;

		last_pos_in_file_bytes = pos_in_file_bytes;
		pos_in_file_bytes = ftell(m_pFile);
		actual_bytes_read = pos_in_file_bytes - last_pos_in_file_bytes + 1;

		// realloc if buffer is too small
		while (actual_bytes_read == max_line_length && *(pbuf + max_line_length - 2) != '\n') {
			max_line_length += increment_bytes;
			pbuf = (char*)realloc(pbuf, max_line_length);

			if (!fgets(pbuf + max_line_length - increment_bytes - 1, increment_bytes + 1, m_pFile))
				break;

			pos_in_file_bytes = ftell(m_pFile);
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

			if (!mesh_map.size())
				mesh_map.insert({ pCurrentMesh->name, pCurrentMesh });

			if (pCurrentMatCluster)
				pCurrentMatCluster->Add(pCurrentMesh->ixPolygon++); // add current polygon index to material cluster

			size_t nbPolypoints = nbTokens - 1;

			for (int i = 1; i <= nbPolypoints; i++) {

				vector<char*>& qTriple = quickSplit(tokens[i], '/'); // speed opt std::string => char*

																	 // ** Face format: ********** 																     
																	 // (PointIndex/UVIndex/NormalIndex)

																	 // example line:  * f 110/37073/2992 766/37074/2958 658/37075/2885 657/37076/2818

																	 // alt. examples: * 110 (no UV, no Normals)  
																	 //				   * 110/37073 (UVs but no Normals)
																	 //				   * 110//2992 (Normals but no UVs)   

																	 // ** Note these are indices of elements in the 3 main file arrays, not the individual objects.

				size_t nbTripleEls = qTriple.size();
				
				long fileIx = atol(qTriple[0]) - 1;
				long meshIx;
				if (pCurrentMesh->meshIxFromFileIx_lookup.count(fileIx) == 0) {
					meshIx = pCurrentMesh->next_meshIx;
					pCurrentMesh->next_meshIx++;
					pCurrentMesh->fileIxFromMeshIx_lookup.insert({ meshIx, fileIx });
					pCurrentMesh->meshIxFromFileIx_lookup.insert({ fileIx, meshIx });					
					if (fileIx < nbPP_inFile) {
						pCurrentMesh->PointPositions.push_back(PP_inFile[3 * fileIx]);
						pCurrentMesh->PointPositions.push_back(PP_inFile[3 * fileIx + 1]);
						pCurrentMesh->PointPositions.push_back(PP_inFile[3 * fileIx + 2]);
					}
					else {
						app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing point position vector entries. Import is incomplete.", siErrorMsg);
						return CStatus::Fail;
					}

					if (bFileHasPolypaint && fileIx < nbMRGB_inFile)
						pCurrentMesh->Mask.push_back(MRGB_inFile[4 * fileIx]);
				}
				else
					meshIx = pCurrentMesh->meshIxFromFileIx_lookup[fileIx];

				pCurrentMesh->PointIndices.push_back(meshIx);

				if (Prefs.OBJ_ImportUVs)
					if (nbTripleEls > 1 && *(qTriple[1]) != '\0' && bFileHasUVs) {
						pCurrentMesh->bHasUVs = true;
						int UVIndex = atoi(qTriple[1]) - 1;
						if (UVIndex < nbUVs_inFile) {
							pCurrentMesh->UVs.push_back(UVs_inFile[3 * UVIndex]);
							pCurrentMesh->UVs.push_back(UVs_inFile[3 * UVIndex + 1]);
							pCurrentMesh->UVs.push_back(UVs_inFile[3 * UVIndex + 2]);
						}
						else {
							app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing UV coordinates. Loading UVs is disabled for the rest of this import.", siErrorMsg);
							Prefs.OBJ_ImportUVs = false;
							//return CStatus::Fail; .. stop importing UVs instead of fail
						}
					}
					else {
						double u = ((double)rand() / (RAND_MAX)) / 10000; //bug workaround: uvs on 0 can lead to crashes in subdivision.dll for some topologies...
						double v = ((double)rand() / (RAND_MAX)) / 10000;
						pCurrentMesh->UVs.push_back((float)u);
						pCurrentMesh->UVs.push_back((float)v);
						pCurrentMesh->UVs.push_back(0.0f);
					}

					if (Prefs.OBJ_ImportUserNormals)
						if (nbTripleEls > 2 && *(qTriple[2]) != '\0' && bFileHasNormals) {
							pCurrentMesh->bHasNormals = true;
							int NormalIndex = atoi(qTriple[2]) - 1;
							if (NormalIndex < nbNormals_inFile) {
								pCurrentMesh->Normals.push_back(Normals_inFile[3 * NormalIndex]);
								pCurrentMesh->Normals.push_back(Normals_inFile[3 * NormalIndex + 1]);
								pCurrentMesh->Normals.push_back(Normals_inFile[3 * NormalIndex + 2]);
							}
							else {
								app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing normal vector entries. Loading normals is disabled for the rest of this import.", siErrorMsg);
								Prefs.OBJ_ImportUserNormals = false;
								// return CStatus::Fail; .. stop importing normals instead of fail
							}
						}
						else {
							pCurrentMesh->Normals.push_back(0.0); //TODO: to be be replaced by actual normal later on
							pCurrentMesh->Normals.push_back(0.0);
							pCurrentMesh->Normals.push_back(0.0);
						}

						if (Prefs.OBJ_ImportPolypaint || Prefs.OBJ_ImportMask)
							if (bFileHasPolypaint) {
								if (fileIx < nbMRGB_inFile) {
									pCurrentMesh->RGBA.push_back(MRGB_inFile[4 * fileIx + 1]);
									pCurrentMesh->RGBA.push_back(MRGB_inFile[4 * fileIx + 2]);
									pCurrentMesh->RGBA.push_back(MRGB_inFile[4 * fileIx + 3]);
									pCurrentMesh->RGBA.push_back(255.0);
								}
								else {
									app.LogMessage(L"A polygon in line " + CString(line_in_file + 1) + " inside " + CString(m_fileNameWithExt.c_str()) + L" is defined with missing polypaint color entries. Loading polypaint/mask is diabled for the rest of this import.", siErrorMsg);
									Prefs.OBJ_ImportPolypaint = false;
									Prefs.OBJ_ImportMask = false;
									// return CStatus::Fail; .. continue instead of failing
								}
							}
			}
			pCurrentMesh->PolygonPointCounts.push_back((long)nbPolypoints);
		}
		else if (STR_EQUAL(firstToken, "v")) {
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
			
			if (tokens.size() == 7) {
				bFileHasPolypaint = true;
				MRGB_inFile.push_back(1.0);
				MRGB_inFile.push_back((float)atof(tokens[4]));
				MRGB_inFile.push_back((float)atof(tokens[5]));
				MRGB_inFile.push_back((float)atof(tokens[6]));
				nbMRGB_inFile++;
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

			UVs_inFile.push_back((float)atof(secondToken));
			UVs_inFile.push_back((float)atof(tokens[2]));
			if (tokens.size() > 3)
				UVs_inFile.push_back((float)atof(tokens[3]));
			else
				UVs_inFile.push_back(0.0);

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

			Normals_inFile.push_back((float)atof(secondToken));
			Normals_inFile.push_back((float)atof(tokens[2]));
			Normals_inFile.push_back((float)atof(tokens[3]));

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
				MRGB_inFile.push_back(1.0f - (16.0f * hex2dec(secondToken[o]) + hex2dec(secondToken[o + 1])) / 255.0f);
				MRGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 2]) + hex2dec(secondToken[o + 3])) / 255.0f);
				MRGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 4]) + hex2dec(secondToken[o + 5])) / 255.0f);
				MRGB_inFile.push_back((16.0f * hex2dec(secondToken[o + 6]) + hex2dec(secondToken[o + 7])) / 255.0f);

				nbMRGB_inFile++;
			}
		}
		else if (STR_EQUAL(firstToken, new_object_tag))
		{
			string name(nbTokens > 1 ? secondToken : m_fileName);

			if (mesh_map.count(name) == 0) {
				if (pCurrentMesh->bIsDefaultMesh) {
					mesh_map.erase(pCurrentMesh->name);
					pCurrentMesh->name = name;
					
				}
				else
					pCurrentMesh = new MeshData(name);

				mesh_map.insert({ name, pCurrentMesh });
				pCurrentMesh->bIsDefaultMesh = false;
			}
			else
				pCurrentMesh = mesh_map[name];
		}
		else if (STR_EQUAL(firstToken, new_cluster_tag)) {
			strCurrentMatName = secondToken;
			if (pCurrentMesh->IcsMaterialClusters.count(strCurrentMatName) == 0) {
				pCurrentMesh->IcsMaterialClusters.insert({ strCurrentMatName, CLongArray() });
			}
			pCurrentMatCluster = &pCurrentMesh->IcsMaterialClusters[strCurrentMatName];
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
		else if (STR_EQUAL(firstToken, "#")) {
			// ignore comments
		}
		else /* material tags like Ka, Kd, ... */ {
			if (currentMatName != "") {
				if (currentMatTagName != "")
					material_map[currentMatName].append(",\n");

				currentMatTagName = string(firstToken);

				material_map[currentMatName].append("\t");
				material_map[currentMatName].append(firstToken);
				material_map[currentMatName].append(": ");

				if (!strncmp(firstToken, "map", 3))
				{
					material_map[currentMatName].append("\"");
					int i = 1;
					while (i < nbTokens) { // recombine tokens with spaces
						material_map[currentMatName] += tokens[i++];
						if (i < nbTokens)
							material_map[currentMatName] += " ";
					}
					material_map[currentMatName].append("\"");
				}
				else if (nbTokens == 2)
				{
					material_map[currentMatName].append(secondToken);
				}
				else if (nbTokens == 4) {
					material_map[currentMatName].append("[");
					material_map[currentMatName].append(tokens[1]);
					material_map[currentMatName].append(", ");
					material_map[currentMatName].append(tokens[2]);
					material_map[currentMatName].append(", ");
					material_map[currentMatName].append(tokens[3]);
					material_map[currentMatName].append("]");
				}
				else {
					app.LogMessage("Unhandled material token (" + CString(firstToken) + ") processed as string.", siVerboseMsg);
					material_map[currentMatName].append("\"");
					int i = 1;
					while (i < nbTokens) { // recombine tokens with spaces
						material_map[currentMatName] += tokens[i++];
						if (i < nbTokens)
							material_map[currentMatName] += " ";
					}
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
			m_progress.PutValue(pos_in_file_bytes / 10000);
			if (m_progress.IsCancelPressed()) {
				for (auto p : mesh_map) delete(p.second);
				return CStatus::False;
			}
		}
	} while (!feof(m_pFile));

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale

	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);

	fclose(m_pFile);

	for (auto& m : material_map)
		m.second.append("\n};");

	return CStatus::OK;
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



