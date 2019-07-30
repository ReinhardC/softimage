#include "stl.h"

CStatus CSTL::Execute_Import(CRefArray& selectedObjects, string initFilePathName)
{
	initStrings(initFilePathName);

	Application app;
	Model root = app.GetActiveSceneRoot();

	m_progress.PutVisible(true);
	m_progress.PutCaption("Importing STL Data");

	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	fopen_s(&m_pFile, m_filePathName.c_str(), "rb");
	if (m_pFile == NULL)
		return CStatus::Fail;

	// lookup for finding connected polygon points (STL gives no info about polygon connectivity except overlapping position)
	unordered_map<tr1::array<float, 3>, int, f3hash> hashmap_positions;

	vector<double> positions; // point positions to give to meshBuilder
	vector<LONG> indices; // indices to give to meshBuilder
	tr1::array<float, 3> triple; // this gets put in lookup
	StlTri nextTriangle; // data struct matching STL file data
	ULONG numTris1 = 0, numTris2 = 0; // two ways to compute # of tris, to be sure
	long ix = 0; // index counter

	// check if binary STL format
	bool bBinary;
	char buf[81];
	buf[80] = '\0';
	fgets(buf, 80, m_pFile);
	CString idLine = buf;
	fseek(m_pFile, 80L, SEEK_SET); // skip header
	fread(&numTris1, sizeof(ULONG), 1, m_pFile);
	fseek(m_pFile, 0L, SEEK_END);
	long fileSize = ftell(m_pFile);
	if (fileSize != 84 + 50 * numTris1 && idLine.FindString("solid") != UINT_MAX) {
		bBinary = false;
		fseek(m_pFile, 0L, 0);
	}
	else {
		bBinary = true;
		fseek(m_pFile, 84L, SEEK_SET); // skip header

		m_progress.PutMaximum(numTris1);
	}

	while (!feof(m_pFile))
	{
		if (bBinary)
			fread(&nextTriangle, 50, 1, m_pFile);
		else
			parseAsciiFacet(nextTriangle, m_pFile);

		// starting at index 1 because index 0 is normal vector which is ignored
		for (auto i = 1; i < 4; i++) {

			triple = { nextTriangle.v[i][0], nextTriangle.v[i][2], -nextTriangle.v[i][1] };

			if (triple[0] > lfMaxExtentX) // for auto scale
				lfMaxExtentX = triple[0];

			if (triple[0] < lfMinExtentX) // for auto scale
				lfMinExtentX = triple[0];

			// hashmap lookup - could use positions array too but this is faster
			if (hashmap_positions.count(triple) == 0) {
				positions.push_back(triple[0]); // new position, but in both positions and lookup hashmap
				positions.push_back(triple[1]);
				positions.push_back(triple[2]);
				hashmap_positions.insert({ triple, ix++ });
			}

			// get new OR existing index 
			indices.push_back(hashmap_positions[triple]);
		}

		numTris2++;
		if (numTris2 % 100000 == 0) { // increment progress bar
			m_progress.Increment(100000);
			if (m_progress.IsCancelPressed()) {
				fclose(m_pFile);
				return CStatus::False;
			}

		}
	}

	numTris2--;

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale

	if (bBinary && (numTris1 != numTris2)) {
		app.LogMessage(L"File has different number of triangles than specified in header (specified=" + CString(numTris1) + L", found=" + CString(numTris2) + L")");
		fclose(m_pFile);
		return CStatus::Fail;
	}

	m_progress.PutCaption("Building Mesh");

	if (selectedObjects.GetCount() != 0) {
		X3DObject xobj(selectedObjects[0]);

		// get a mesh builder from the newly created geometry
		Primitive prim = xobj.GetActivePrimitive();
		PolygonMesh mesh = prim.GetGeometry();
		if (!mesh.IsValid()) {
			fclose(m_pFile);
			return CStatus::False;
		}

		// auto scale mesh 
		MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0);
		if (lfExtentX >= 55)
			vAutoScaling.Set(0.1, 0.1, 0.1);
		if (lfExtentX >= 150)
			vAutoScaling.Set(0.01, 0.01, 0.01);

		xobj.PutLocalScaling(vAutoScaling);

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

		meshBuilder.AddVertices(ix, positions.data());
		meshBuilder.AddTriangles(numTris2, indices.data());

		// build mesh
		CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
		if (err != CStatus::OK)
			app.LogMessage(L"Error building the mesh: " + err.GetDescription());

		m_progress.PutVisible(false);

		fclose(m_pFile);

		return err;
	}
	else {
		X3DObject xobj;
		root.AddPrimitive(L"EmptyPolygonMesh", CString(m_fileName.c_str()), xobj);

		// get a mesh builder from the newly created geometry
		Primitive prim = xobj.GetActivePrimitive();
		PolygonMesh mesh = prim.GetGeometry();
		if (!mesh.IsValid()) {
			fclose(m_pFile);
			return CStatus::False;
		}

		// auto scale mesh 
		MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0);
		if (lfExtentX >= 55)
			vAutoScaling.Set(0.1, 0.1, 0.1);
		if (lfExtentX >= 150)
			vAutoScaling.Set(0.01, 0.01, 0.01);

		xobj.PutLocalScaling(vAutoScaling);

		CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

		meshBuilder.AddVertices(ix, positions.data());
		meshBuilder.AddTriangles(numTris2, indices.data());

		// build mesh
		CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
		if (err != CStatus::OK)
			app.LogMessage(L"Error building the mesh: " + err.GetDescription());

		m_progress.PutVisible(false);

		fclose(m_pFile);

		return err;
	}
}

CStatus CSTL::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportBinary, bool bExportLocalCoords)
{
	XSI::Application app;

	initStrings(initFilePathName);

	if ((m_pFile = fopen(m_filePathName.c_str(), "wb")) == NULL)
		return CStatus::Fail;


	string description("Original Name: " + m_fileName);
	if (bExportBinary)
	{
		char header[80];
		for (int i = 0; i < 80; i++)
			header[i] = '\0';

		strcpy(header, description.c_str());

		fwrite(header, 1, 80, m_pFile);
	}
	else
		Output(m_pFile, "solid " + m_fileName + "\n\n");

	long fails = 0;

	ULONG dummy = 0L;
	if (bExportBinary)
		fwrite(&dummy, sizeof(ULONG), 1, m_pFile);

	ULONG totaltris = 0;

	for (long iObject = 0, iObject_max = inObjects.GetCount(); iObject < iObject_max; iObject++) {
		X3DObject xobj(inObjects[iObject]);

		// Get a geometry accessor from the selected object	
		Primitive prim = xobj.GetActivePrimitive();
		PolygonMesh mesh = prim.GetGeometry();
		if (!mesh.IsValid()) return CStatus::False;

		CGeometryAccessor ga = mesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, 0);

		CDoubleArray VP;
		CLongArray LA_TriIndices, LA_TriNodeIndices;
		CFloatArray FA_Normals;

		ga.GetNodeNormals(FA_Normals);
		ga.GetTriangleVertexIndices(LA_TriIndices);
		ga.GetTriangleNodeIndices(LA_TriNodeIndices);
		ga.GetVertexPositions(VP);
		
		ULONG numTris1 = LA_TriIndices.GetCount() / 3, numTris2 = 0;
		
		totaltris += numTris1;
		
		m_progress.PutMaximum(numTris1);
		m_progress.PutValue(0);
		m_progress.PutVisible(true);
		m_progress.PutCaption("Exporting STL Data (Object " + CString(iObject+1) + CString(" of ") + CString(iObject_max) + CString(")"));

		StlTri nextTriangle;

		MATH::CVector3 v;
		MATH::CTransformation xfo = xobj.GetKinematics().GetGlobal().GetTransform();

		for (long iVertexPosition = 0, i_max = LA_TriIndices.GetCount() / 3; iVertexPosition < i_max; iVertexPosition++)
		{
			v.Set(VP[3 * LA_TriIndices[3 * iVertexPosition]], VP[3 * LA_TriIndices[3 * iVertexPosition] + 1], VP[3 * LA_TriIndices[3 * iVertexPosition] + 2]);
			if (!bExportLocalCoords)
				v = MATH::MapObjectPositionToWorldSpace(xfo, v);

			nextTriangle.v[1][0] = (float)v.GetX();
			nextTriangle.v[1][1] = (float)-v.GetZ();
			nextTriangle.v[1][2] = (float)v.GetY();

			v.Set(VP[3 * LA_TriIndices[3 * iVertexPosition + 1]], VP[3 * LA_TriIndices[3 * iVertexPosition + 1] + 1], VP[3 * LA_TriIndices[3 * iVertexPosition + 1] + 2]);
			if (!bExportLocalCoords)
				v = MATH::MapObjectPositionToWorldSpace(xfo, v);

			nextTriangle.v[2][0] = (float)v.GetX();
			nextTriangle.v[2][1] = (float)-v.GetZ();
			nextTriangle.v[2][2] = (float)v.GetY();

			v.Set(VP[3 * LA_TriIndices[3 * iVertexPosition + 2]], VP[3 * LA_TriIndices[3 * iVertexPosition + 2] + 1], VP[3 * LA_TriIndices[3 * iVertexPosition + 2] + 2]);
			if (!bExportLocalCoords)
				v = MATH::MapObjectPositionToWorldSpace(xfo, v);

			nextTriangle.v[3][0] = (float)v.GetX();
			nextTriangle.v[3][1] = (float)-v.GetZ();
			nextTriangle.v[3][2] = (float)v.GetY();

			if (bExportBinary)
				fwrite(&nextTriangle, 50, 1, m_pFile);
			else {
				Output(m_pFile, "facet normal 0.0 0.0 0.0\n\touter loop\n");
				Output(m_pFile, string("\t\tvertex ") + to_string(nextTriangle.v[1][0]) + " " + to_string(nextTriangle.v[1][2]) + " " + to_string(-nextTriangle.v[1][1]) + "\n");
				Output(m_pFile, string("\t\tvertex ") + to_string(nextTriangle.v[2][0]) + " " + to_string(nextTriangle.v[2][2]) + " " + to_string(-nextTriangle.v[2][1]) + "\n");
				Output(m_pFile, string("\t\tvertex ") + to_string(nextTriangle.v[3][0]) + " " + to_string(nextTriangle.v[3][2]) + " " + to_string(-nextTriangle.v[3][1]) + "\n");
				Output(m_pFile, "\tendloop\nendfacet");
			}

			numTris2++;
			if (numTris2 % 100000 == 0) { // increment progress bar
				m_progress.Increment(100000);
				if (m_progress.IsCancelPressed())
					return CStatus::False;
			}
		}
	}

	if (!bExportBinary)
		Output(m_pFile, "endsolid " + m_fileName + "\n\n");
	
	fseek(m_pFile, 80, SEEK_SET);

	if (bExportBinary)
		fwrite(&totaltris, sizeof(ULONG), 1, m_pFile);

	m_progress.PutVisible(false);

	fclose(m_pFile);

	if (fails == 0)
		return CStatus::OK;
	else {
		app.LogMessage(L"OBJ File output for " + CString(fails) + L" objects failed.", siErrorMsg);
		return CStatus::Fail;
	}

}

// parser for ASCII STL format
void CSTL::parseAsciiFacet(StlTri& nextTriangle, FILE* file)
{
	char buf[181];
	buf[180] = '\0';

	auto ix = 0;
	CString line;
	do {
		fgets(buf, 180, file);
		line = buf;
		if (line.FindString("vertex") == UINT_MAX)
			continue;

		CStringArray tokens = line.Split(CString(L" "));
		nextTriangle.v[ix + 1][0] = (float)atof(tokens[1].GetAsciiString());
		nextTriangle.v[ix + 1][1] = (float)atof(tokens[2].GetAsciiString());
		nextTriangle.v[ix + 1][2] = (float)atof(tokens[3].GetAsciiString());
		ix++;

	} while (!feof(file) && line.FindString("endfacet") == UINT_MAX);

}

void CSTL::Output(FILE* file, string str) {
	fprintf_s(file, str.c_str());
}