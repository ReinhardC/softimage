#include "PLY.h"

#include <map>
#include <vector>
#include <utility>

using namespace std;

size_t sizeof_str(string type) {
	if (type == "char" || type == "uchar" || type == "uchar8" || type == "uint8")
		return sizeof(char);
	else if (type == "float" || type == "float32")
		return sizeof(float);
	else if (type == "double" || type == "double64")
		return sizeof(double);
	else if (type == "int" || type == "int32" || type == "uint")
		return sizeof(int);
	else if (type == "long" || type == "int64")
		return sizeof(long);
	
	return 0;
}

bool CPLY::read_header(FILE* file, vector<elementinfo*> &out_elinfo, bool &bBinary) {

	char line[1025];
	line[0] = '\0';
	line[1024] = '\0';

	Application app;

	elementinfo *pCurrentElement = NULL;
	long currentElement_offset = 0;

	string strline = "";

	while (!feof(file))
	{
		fgets(line, 1024, file);
		strline = line;
		strline = trim(strline);

		vector<string> tokens = split(strline, ' ');
		size_t LAST = tokens.size() - 1;
		
		app.LogMessage(tokens[0].c_str());
		
		if (tokens[0] == "end_header") {

			if (pCurrentElement) {
				pCurrentElement->size = currentElement_offset;
				out_elinfo.push_back(pCurrentElement);
			}
			break;
		}
		if (tokens[0] == "element") {

			if (pCurrentElement) {
				pCurrentElement->size = currentElement_offset;
				out_elinfo.push_back(pCurrentElement);
			}
				
			pCurrentElement = new elementinfo();			
			pCurrentElement->name = tokens[1];
			pCurrentElement->count = atoi(tokens[2].c_str());
			currentElement_offset = 0;
		}
		else if (tokens[0] == "property" && tokens[1] != "list") {
			if (!pCurrentElement)
				return false;

			pCurrentElement->types.push_back(tokens[1]);
			pCurrentElement->names.push_back(tokens[LAST]);

			currentElement_offset += (long)sizeof_str(tokens[1]);
		}
		else if (tokens[0] == "property" && tokens[1] == "list") {
			if (!pCurrentElement)
				return false;
			
			pCurrentElement->types.push_back(tokens[1]);
			pCurrentElement->names.push_back(tokens[LAST]);

			currentElement_offset += (long)sizeof_str(tokens[2]); // count
		}
		else if (tokens[0] == "format") {
			bBinary = (tokens[1] != "ascii");
		}
	}

	return true;
}

CStatus CPLY::Execute_Import(string initFilePathName, bool bImportUVs, bool bImportUserNormals, bool bImportMask, bool bImportPolypaint)
{
	CFileFormat::initStrings(initFilePathName);

	Application app;
	Model root = app.GetActiveSceneRoot();

	m_progress.PutVisible(true);
	m_progress.PutCaption("Importing PLY Data");
	
	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	if ((m_file = fopen(m_filePathName.c_str(), "rb")) == NULL)
		return CStatus::Fail;

	fseek(m_file, 0L, 0);
	
	bool bBinary;
	vector<elementinfo*> header;
	read_header(m_file, header, bBinary);	

	char cProp[4096];
	char cListEntry[4096];

	CDoubleArray PP_inFile;
	CFloatArray UVs_inFile, RGB_inFile;
	CFloatArray UVs_ofObject, RGBA_ofObject;
	CLongArray PP_Ics_ofObject;
	CLongArray Counts_ofObject;

	long next_struct_offset;

	for(int iElem=0, iElem_max = (int)header.size(); iElem<iElem_max; iElem++)
	{
		elementinfo *pCurrHeaderElem = header[iElem];
		app.LogMessage(CString(iElem+1) + ". Element: " + pCurrHeaderElem->name.c_str() + " (size=" + CString(pCurrHeaderElem->size) + "), count = "+CString(pCurrHeaderElem->count));
	
		for (int iData = 0, iData_max = pCurrHeaderElem->count; iData < iData_max; iData++) {
			
			std::vector<std::string> tokens;
			size_t nbTokens;

			next_struct_offset = 0;

			if (bBinary) 
				fread_s(cProp, 4096, pCurrHeaderElem->size, 1, m_file);						
			else {
				fgets(cProp, 180, m_file);
				
				string stdLine(cProp);
				trim(stdLine);
				tokens = split(stdLine, ' ');

				nbTokens = tokens.size();
				if (nbTokens == 0)
					return CStatus::Fail;				
			}

			if (pCurrHeaderElem->name == "vertex") {
								
				bool bX_initialized = false, bY_initialized = false, bZ_initialized = false, bR_initialized = false, bG_initialized = false, bB_initialized = false;
				bool blX_initialized = false, blY_initialized = false, blZ_initialized = false;

				float X, Y, Z;
				unsigned char R, G, B;
				
				for (int iProp = 0, iProp_max = (int)pCurrHeaderElem->names.size(); iProp < iProp_max; iProp++) {
					string& type = pCurrHeaderElem->types[iProp];
					string& name = pCurrHeaderElem->names[iProp];

					if (bBinary) {
						if (name == "x" && (type == "float" || type == "float32")) {
							X = *(float*)(cProp + next_struct_offset);
							bX_initialized = true;
						}
						else if (name == "y" && (type == "float" || type == "float32")) {
							Y = *(float*)(cProp + next_struct_offset);
							bY_initialized = true;
						}
						else if (name == "z" && (type == "float" || type == "float32")) {
							Z = *(float*)(cProp + next_struct_offset);
							bZ_initialized = true;
						}
						else if ((name == "r" || name == "red") && (type == "uchar" || type == "char")) {
							R = *(unsigned char*)(cProp + next_struct_offset);
							bR_initialized = true;
						}
						else if ((name == "g" || name == "green") && (type == "uchar" || type == "char")) {
							G = *(unsigned char*)(cProp + next_struct_offset);
							bG_initialized = true;
						}
						else if ((name == "b" || name == "blue") && (type == "uchar" || type == "char")) {
							B = *(unsigned char*)(cProp + next_struct_offset);
							bB_initialized = true;
						}

						next_struct_offset += (long)sizeof_str(type);
					}
					else {
						if (name == "x") {
							X = (float)atof(tokens[next_struct_offset].c_str());
							bX_initialized = true;
						}
						else if (name == "y") {
							Y = (float)atof(tokens[next_struct_offset].c_str());
							bY_initialized = true;
						}
						else if (name == "z") {
							Z = (float)atof(tokens[next_struct_offset].c_str()); 
							bZ_initialized = true;
						}
						else if (name == "r" || name == "red") {
							R = (unsigned char)atoi(tokens[next_struct_offset].c_str());
							bR_initialized = true;
						}
						else if (name == "g" || name == "green") {
							G = (unsigned char)atoi(tokens[next_struct_offset].c_str());
							bG_initialized = true;
						}
						else if (name == "b" || name == "blue") {
							B = (unsigned char)atoi(tokens[next_struct_offset].c_str());
							bB_initialized = true;
						}

						next_struct_offset += 1;
					}
				}

				// finished reading element vertex

				if (bX_initialized && bY_initialized && bZ_initialized || (blX_initialized && blY_initialized && blZ_initialized)) {

					PP_inFile.Add(X);
					PP_inFile.Add(Y);
					PP_inFile.Add(Z);

					if (X > lfMaxExtentX)
						lfMaxExtentX = X;

					if (X < lfMinExtentX)
						lfMinExtentX = X;
				}
				else return CStatus::Fail; 

				if (bR_initialized && bG_initialized && bB_initialized) {
					RGB_inFile.Add((float)(R / 255.0));
					RGB_inFile.Add((float)(G / 255.0));
					RGB_inFile.Add((float)(B / 255.0));
				}				
			}
			else if(pCurrHeaderElem->name == "face") {	
				
				char vertex_indices_count = 0;

				for (int iProp = 0, iProp_max = (int)pCurrHeaderElem->names.size(); iProp < iProp_max; iProp++) {
					string& type = pCurrHeaderElem->types[iProp];
					string& name = pCurrHeaderElem->names[iProp];

					if (name == "vertex_indices") {
						if (bBinary) {
							vertex_indices_count = *((char*)(cProp + next_struct_offset));
							next_struct_offset += (long)sizeof_str(type);
						}
						else {							
							vertex_indices_count = (char)atoi(tokens[next_struct_offset].c_str());
							next_struct_offset += 1;
						}
					} else if (name == "texcoord") {
						if (bBinary) {
							vertex_indices_count = *((char*)(cProp + next_struct_offset));
							next_struct_offset += (long)sizeof_str(type);
						}
						else {
							vertex_indices_count = (char)atoi(tokens[next_struct_offset].c_str());
							next_struct_offset += 1;
						}
					}
				}
				
				// finished reading element face

				if (vertex_indices_count > 0) {

					Counts_ofObject.Add(vertex_indices_count);

					for (int iListIndex = 0; iListIndex < vertex_indices_count; iListIndex++) {

						unsigned int index;

						if (bBinary) {
							fread_s(cListEntry, 4096, sizeof(unsigned int), 1, m_file);
							index = *((unsigned int*)(cListEntry + 0));
						}
						else
							index = atoi(tokens[next_struct_offset++].c_str());
		
						PP_Ics_ofObject.Add(index);

						if (RGB_inFile.GetCount() > 0) {
							RGBA_ofObject.Add(RGB_inFile[3 * index]);
							RGBA_ofObject.Add(RGB_inFile[3 * index + 1]);
							RGBA_ofObject.Add(RGB_inFile[3 * index + 2]);
							RGBA_ofObject.Add(255.0);
						}
					}
				}
			}	
			else if (pCurrHeaderElem->name == "multi_texture_vertex") {
			
				bool bU_initialized = false, bV_initialized = false, bTx_initialized = false;
				float U, V;
				char tx;

				for (int iProp = 0, iProp_max = (int)pCurrHeaderElem->names.size(); iProp < iProp_max; iProp++) {
					string& type = pCurrHeaderElem->types[iProp];
					string& name = pCurrHeaderElem->names[iProp];

					if (bBinary) {
						if (name == "u" && (type == "float" || type == "float32")) {
							U = *(float*)(cProp + next_struct_offset);
							bU_initialized = true;
						}
						else if (name == "v" && (type == "float" || type == "float32")) {
							V = *(float*)(cProp + next_struct_offset);
							bV_initialized = true;
						}
						else if (name == "tx" && (type == "char" || type == "uchar")) {
							tx = *(char*)(cProp + next_struct_offset);
							bTx_initialized = true;
						}

						next_struct_offset += (long)sizeof_str(pCurrHeaderElem->types[iProp]);
					}
					else {
						if (name == "u") {
							U = (float)atof(tokens[next_struct_offset].c_str());
							bU_initialized = true;
						}
						else if (name == "v") {
							V = (float)atof(tokens[next_struct_offset].c_str());
							bV_initialized = true;
						}
						else if (name == "ty") {
							tx = (char)atoi(tokens[next_struct_offset].c_str());
							bTx_initialized = true;
						}

						next_struct_offset += 1;
					}
				}
				
				// finished reading element multi_texture_vertex

				if (bTx_initialized && bU_initialized && bV_initialized && tx == 0) { // only use texture #0
						UVs_inFile.Add(U);
						UVs_inFile.Add(1.0f-V);
				}
			}
			else if (pCurrHeaderElem->name == "multi_texture_face") {
		
				char listCount = 0;
				bool bTn_initialized = false, bTx_initialized = false;
				char tx = 0;
				int tn = 0;

				for (int iProp = 0, iProp_max = (int)pCurrHeaderElem->names.size(); iProp < iProp_max; iProp++) {
					string& type = pCurrHeaderElem->types[iProp];
					string& name = pCurrHeaderElem->names[iProp];

					if (bBinary) {
						if (name == "texture_vertex_indices") {
							listCount = *((char*)(cProp + next_struct_offset));
							next_struct_offset += (long)sizeof_str(type);
						}
						else if (name == "tx" && (type == "char" || type == "uchar")) {
							tx = *(char*)(cProp + next_struct_offset);
							bTx_initialized = true;
						}
						else if (name == "tn" && (type == "int" || type == "uint")) {
							tn = *(int*)(cProp + next_struct_offset);
							bTn_initialized = true;
						}

						next_struct_offset += (long)sizeof_str(type);
					}
					else {
						if (name == "texture_vertex_indices") {
							listCount = (char)atoi(tokens[next_struct_offset].c_str());
							next_struct_offset += 1;
						}
						else if (name == "tx") {
							tx = (char)atoi(tokens[next_struct_offset].c_str());
							bTx_initialized = true;
						}
						else if (name == "tn" && (type == "int" || type == "uint")) {
							tn = atoi(tokens[next_struct_offset].c_str());
							bTn_initialized = true;
						}
						
						next_struct_offset += 1;
					}
				}

				// finished reading element multi_texture_face

				if (listCount>0 && tx==0) {
					
					for (int iListIndex = 0, iListIndex_max = (int)listCount; iListIndex < iListIndex_max; iListIndex++) {

						unsigned int index;

						if (bBinary) {
							fread_s(cListEntry, 4096, sizeof(unsigned int), 1, m_file);
							index = *((unsigned int*)(cListEntry + 0));
						}
						else
							index = atoi(tokens[next_struct_offset++].c_str());

						PP_Ics_ofObject.Add(index);

						if (UVs_inFile.GetCount() > 0) { // only use texture #0

							UVs_ofObject.Add(UVs_inFile[2 * index]);
							UVs_ofObject.Add(UVs_inFile[2 * index + 1]);
							UVs_ofObject.Add(0.0); // w
						}
					}
				}
			}
		}
	}

	m_progress.PutCaption("Importing PLY (Building Mesh)");

	// use empty mesh to create the imported mesh
	X3DObject xobj;
	root.AddPrimitive("EmptyPolygonMesh", CString(m_fileName.c_str()), xobj);

	// get a mesh builder from the newly created geometry
	Primitive prim = xobj.GetActivePrimitive();
	PolygonMesh mesh = prim.GetGeometry();
	if (!mesh.IsValid())
		return CStatus::False;

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale
											 // auto scale mesh 
	MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0);
	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);

	xobj.PutLocalScaling(vAutoScaling);

	CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

	meshBuilder.AddVertices(PP_inFile.GetCount() / 3, PP_inFile.GetArray());
	meshBuilder.AddPolygons(Counts_ofObject.GetCount(), Counts_ofObject.GetArray(), PP_Ics_ofObject.GetArray());

	// build mesh
	CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
	if (err != CStatus::OK)
		app.LogMessage("Error building the mesh: " + err.GetDescription());

	CClusterPropertyBuilder cpBuilder = mesh.GetClusterPropertyBuilder();
	 
	if (RGBA_ofObject.GetCount() > 0) {
		ClusterProperty rgb = cpBuilder.AddVertexColor(CString("PolyPaint"), CString("Vertex_Colors"));
		rgb.SetValues(RGBA_ofObject.GetArray(), RGBA_ofObject.GetCount() / 4);
	}

	app.LogMessage("UVs: " + UVs_ofObject.GetAsText());
	if (UVs_ofObject.GetCount() > 0) {
		ClusterProperty uv = cpBuilder.AddUV();
		uv.SetValues(UVs_ofObject.GetArray(), UVs_ofObject.GetCount() / 3);
	}

	m_progress.PutVisible(false);

	
	return CStatus::OK;
}



/*


CStatus CPLY::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportVertexColors, bool bSeparateFiles, bool bExportLocalCoords, bool bBinary)
{
	XSI::Application app;

	initStrings(initFilePathName);

	if ((m_file = fopen(m_filePathName.c_str(), "wb")) == NULL)
		return CStatus::Fail;

	// output header
	Output(m_file, "ply\n");
	Output(m_file, "format " + string(bBinary ? "binary_little_endian" : "ascii") + " 1.0\n");
	Output(m_file, "comment File exported by Softimage\n");
	
	m_progress.PutVisible(true);

	X3DObject xobj(inObjects[0]);

	// Get a geometry accessor from the selected object	
	XSI::Primitive prim = xobj.GetActivePrimitive();
	XSI::PolygonMesh mesh = prim.GetGeometry();
	if (!mesh.IsValid()) return CStatus::False;

	m_progress.PutCaption("Exporting PLY (" + xobj.GetName() + ")");
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

	if (bExportVertexColors) {
		ClusterProperty foundProperty = CFileFormat::getVertexColorProperty(xobj);
		bExportVertexColors = foundProperty.IsValid();
	}

	Output(m_file, "element vertex " + to_string(VP.GetCount()) + "\n");
	Output(m_file, "\tproperty float x\n");
	Output(m_file, "\tproperty float y\n");
	Output(m_file, "\tproperty float z\n");

	if (bExportVertexColors) {
		Output(m_file, "\tproperty uchar r\n");
		Output(m_file, "\tproperty uchar g\n");
		Output(m_file, "\tproperty uchar b\n");
	}

	Output(m_file, "element face " + to_string(LA_Counts.GetCount()) + "\n");
	Output(m_file, "\tproperty list uchar int vertex_indices\n");

	for (LONG l = 0, l_max = ga.GetUVs().GetCount(); l < l_max; l++) {

	}

	// ************************************************
	// * Export Vertex Positions

	Output(m_file, "\ng " + string(xobj.GetName().GetAsciiString()) + "\n");

	m_progress.PutCaption("Exporting OBJ (Vertex Positions of '" + xobj.GetName() + "')");
	m_progress.Increment();

	Output(m_file, "\n# vertex positions\n");

	MATH::CTransformation xfo = xobj.GetKinematics().GetGlobal().GetTransform();

	for (long iVertexPosition = 0, i_max = VP.GetCount() / 3; iVertexPosition < i_max; iVertexPosition++) {

		MATH::CVector3 v(VP[3 * iVertexPosition + 0], VP[3 * iVertexPosition + 1], VP[3 * iVertexPosition + 2]);

		if (!bExportLocalCoords)
			v = MATH::MapObjectPositionToWorldSpace(xfo, v);


		Output(m_file, "v " + to_string(v.GetX()) + " " + to_string(v.GetY()) + " " + to_string(v.GetZ()) + "\n");
	}
	Output(m_file, "# end vertex positions (" + to_string(VP.GetCount() / 3) + ")\n");

	tr1::array<float, 3> triple;

	if (bExportPolypaint) {

		m_progress.PutCaption("Exporting OBJ (Vertex Colors of '" + xobj.GetName() + "')");

		ClusterProperty foundProperty = CFileFormat::getVertexColorProperty(xobj);
		bExportPolypaint = foundProperty.IsValid();
	}
		if (bExportPolypaint) {

			char hex[] = "0123456789abcdef";

			Output(m_file, "\n# The following MRGB block contains ZBrush Vertex Color (Polypaint) and masking output as 4 hexadecimal values per vertex. The vertex color format is MMRRGGBB with up to 64 entries per MRGB line.\n");

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
					Output(m_file, strLine + "\n");
					strLine = "#MRGB ";
					nColors = 0;
				}
			}

			if (nColors != 0)
				Output(m_file, strLine + "\n");

			Output(m_file, "# End of MRGB block \n");
		}
	}

	// ************************
	// * Export UVs

	ClusterProperty UVCP(ga.GetUVs()[0]);
	CFloatArray FA_UVs;
	CBitArray flags_UVs;

	bool bExportUVs = UVCP.IsValid();
	if (bExportUVs) {

		UVCP.GetValues(FA_UVs, flags_UVs);

		m_progress.PutCaption("Exporting OBJ (Texture Positions of '" + xobj.GetName() + "')");

		Output(m_file, "\n# texture positions\n");

		long ix = 0;
		for (long iUVCoord = 0, l_max = FA_UVs.GetCount() / 3; iUVCoord < l_max; iUVCoord++) {

			float u = FA_UVs[3 * iUVCoord + 0];
			float v = FA_UVs[3 * iUVCoord + 1];
			float w = FA_UVs[3 * iUVCoord + 2];
			triple = { u, v, w };
			
		}
	}

	// ************************
	// * Export User Normals

	ClusterProperty UNCP(ga.GetUserNormals()[0]);
	CFloatArray FA_UserNormals;
	CBitArray flags_UserNormals;
	
	bool bExportUserNormals = UNCP.IsValid();
	if (bExportUserNormals)
	{
		ga.GetNodeNormals(FA_UserNormals);

		m_progress.PutCaption("Exporting OBJ (Normals of '" + xobj.GetName() + "')");

		Output(m_file, "\n# normals\n");

		long ix = 0;
		for (long iUserNormal = 0, l_max = FA_UserNormals.GetCount() / 3; iUserNormal < l_max; iUserNormal++) {
			float nx = FA_UserNormals[3 * iUserNormal + 0];
			float ny = FA_UserNormals[3 * iUserNormal + 1];
			float nz = FA_UserNormals[3 * iUserNormal + 2];
			triple = { nx, ny, nz };
			
		}
	}

	CRefArray Materials = ga.GetMaterials();

	Output(m_file, string("\n# ") + to_string(LA_Counts.GetCount()) + " polygons using " + to_string(Materials.GetCount()) + " Material(s)\n");

	// ************************
	// * Export Polygons

	m_progress.PutCaption("Exporting OBJ (Polygons of '" + xobj.GetName() + "')");
	m_progress.Increment();

	fclose(m_file);

	if (fails == 0)
		return CStatus::OK;
	else {
		app.LogMessage("OBJ File output for " + CString(fails) + " objects failed.", siErrorMsg);
		return CStatus::Fail;
	}
}

void CPLY::Output(FILE* file, string str) {
	fprintf_s(file, str.c_str());
}

*/