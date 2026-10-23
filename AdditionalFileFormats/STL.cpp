#include "stl.h"

CStatus CSTL::Execute_Import(CRefArray& selectedObjects, string initFilePathName, bool bAutoScale)
{
	initStrings(initFilePathName);

	Application app;
	Model root = app.GetActiveSceneRoot();

	m_progress.PutVisible(true);
	m_progress.PutCaption("Importing STL Data");

	// for computing mesh auto scale (1, 0.1 or 0.01)
	double lfExtentX, lfMaxExtentX = -DBL_MAX, lfMinExtentX = DBL_MAX;

	fopen_s(&m_pFile, m_filePathName.c_str(), "rb");
	if (m_pFile == NULL) {
		app.LogMessage("Could not open '" + CString(m_filePathName.c_str()) + "'.", siErrorMsg);
		m_progress.PutVisible(false);
		return CStatus::Fail;
	}

	// lookup for finding connected polygon points (STL gives no info about polygon connectivity except overlapping position)
	unordered_map<array<float, 3>, int, f3hash> hashmap_positions;

	vector<double> positions; // point positions to give to meshBuilder
	vector<LONG> indices; // indices to give to meshBuilder
	array<float, 3> triple; // this gets put in lookup
	StlTri nextTriangle = {}; // data struct matching STL file data
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
	_fseeki64(m_pFile, 0, SEEK_END);
	long long fileSize = _ftelli64(m_pFile); // 64 bit: binary STL files can exceed 2 GB
	if (fileSize != 84LL + 50LL * numTris1 && idLine.FindString("solid") != UINT_MAX) {
		bBinary = false;
		fseek(m_pFile, 0L, 0);
	}
	else {
		bBinary = true;
		fseek(m_pFile, 84L, SEEK_SET); // skip header

		m_progress.PutMaximum(numTris1);
	}

	// read until no complete triangle can be read anymore (feof() only becomes true after a failed read,
	// so testing it before reading would process the last triangle twice or, for ASCII files without a
	// trailing newline, drop the last triangle)
	for (;;)
	{
		if (bBinary) {
			if (fread(&nextTriangle, 50, 1, m_pFile) != 1)
				break;
		}
		else if (!parseAsciiFacet(nextTriangle, m_pFile))
			break;

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
				m_progress.PutVisible(false);
				return CStatus::False;
			}

		}
	}

	fclose(m_pFile);

	lfExtentX = lfMaxExtentX - lfMinExtentX; // for auto scale
	MATH::CVector3 vAutoScaling(1.0, 1.0, 1.0);
	if (lfExtentX >= 55)
		vAutoScaling.Set(0.1, 0.1, 0.1);
	if (lfExtentX >= 150)
		vAutoScaling.Set(0.01, 0.01, 0.01);

	if (bBinary && (numTris1 != numTris2)) {
		app.LogMessage(L"File has different number of triangles than specified in header (specified=" + CString(numTris1) + L", found=" + CString(numTris2) + L")", siErrorMsg);
		m_progress.PutVisible(false);
		return CStatus::Fail;
	}

	if (numTris2 == 0) {
		app.LogMessage(L"No triangles found in '" + CString(m_filePathName.c_str()) + L"'.", siErrorMsg);
		m_progress.PutVisible(false);
		return CStatus::Fail;
	}

	m_progress.PutCaption("Building Mesh");

	X3DObject xobj;
	if (selectedObjects.GetCount() != 0)
		xobj = X3DObject(selectedObjects[0]);
	else
		root.AddPrimitive(L"EmptyPolygonMesh", CString(m_fileName.c_str()), xobj);

	if (bAutoScale)
		xobj.PutLocalScaling(vAutoScaling); // after creation: on an empty X3DObject this was a no-op

	Primitive prim = xobj.GetActivePrimitive();
	PolygonMesh mesh = prim.GetGeometry();
	if (!mesh.IsValid()) {
		app.LogMessage(L"The selected object is not a polygon mesh.", siErrorMsg);
		m_progress.PutVisible(false);
		return CStatus::False;
	}

	CMeshBuilder meshBuilder = mesh.GetMeshBuilder();

	meshBuilder.AddVertices(ix, positions.data());
	meshBuilder.AddTriangles(numTris2, indices.data());

	// build mesh
	CMeshBuilder::CErrorDescriptor err = meshBuilder.Build(true);
	if (err != CStatus::OK)
		app.LogMessage(L"Error building the mesh: " + err.GetDescription());

	m_progress.PutVisible(false);

	return err;
}

// computes the (unit) facet normal from the 3 corners stored in tri.v[1..3] into tri.v[0]
static void computeFacetNormal(StlTri& tri)
{
	float ax = tri.v[2][0] - tri.v[1][0], ay = tri.v[2][1] - tri.v[1][1], az = tri.v[2][2] - tri.v[1][2];
	float bx = tri.v[3][0] - tri.v[1][0], by = tri.v[3][1] - tri.v[1][1], bz = tri.v[3][2] - tri.v[1][2];
	float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
	float len = sqrtf(nx * nx + ny * ny + nz * nz);
	if (len > 0.0f) { nx /= len; ny /= len; nz /= len; }
	else { nx = ny = nz = 0.0f; }
	tri.v[0][0] = nx; tri.v[0][1] = ny; tri.v[0][2] = nz;
}

CStatus CSTL::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportBinary, bool bExportLocalCoords)
{
	XSI::Application app;

	initStrings(initFilePathName);

	if ((m_pFile = fopen(m_filePathName.c_str(), "wb")) == NULL) {
		app.LogMessage("Could not create '" + CString(m_filePathName.c_str()) + "'.", siErrorMsg);
		return CStatus::Fail;
	}

	string description("Original Name: " + m_fileName);
	if (bExportBinary)
	{
		char header[80];
		for (int i = 0; i < 80; i++)
			header[i] = '\0';

		strncpy(header, description.c_str(), sizeof(header) - 1);

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
		if (!mesh.IsValid()) {
			fails++;
			continue;
		}

		CGeometryAccessor ga = mesh.GetGeometryAccessor(XSI::siConstructionModeSecondaryShape, XSI::siCatmullClark, 0);

		CDoubleArray VP;
		CLongArray LA_TriIndices;

		ga.GetTriangleVertexIndices(LA_TriIndices);
		ga.GetVertexPositions(VP);

		ULONG numTris1 = LA_TriIndices.GetCount() / 3, numTris2 = 0;

		totaltris += numTris1;

		m_progress.PutMaximum(numTris1);
		m_progress.PutValue(0);
		m_progress.PutVisible(true);
		m_progress.PutCaption("Exporting STL Data (Object " + CString(iObject+1) + CString(" of ") + CString(iObject_max) + CString(")"));

		StlTri nextTriangle = {}; // zero-initialized: normal and attribute count are written to the file too

		MATH::CVector3 v;
		MATH::CTransformation xfo = xobj.GetKinematics().GetGlobal().GetTransform();

		for (long iVertexPosition = 0, i_max = LA_TriIndices.GetCount() / 3; iVertexPosition < i_max; iVertexPosition++)
		{
			// Softimage is Y-up, STL files are conventionally Z-up: (x, y, z) -> (x, -z, y), inverse of the import
			for (int iCorner = 0; iCorner < 3; iCorner++) {
				long pointIx = LA_TriIndices[3 * iVertexPosition + iCorner];
				v.Set(VP[3 * pointIx], VP[3 * pointIx + 1], VP[3 * pointIx + 2]);
				if (!bExportLocalCoords)
					v = MATH::MapObjectPositionToWorldSpace(xfo, v);

				nextTriangle.v[iCorner + 1][0] = (float)v.GetX();
				nextTriangle.v[iCorner + 1][1] = (float)-v.GetZ();
				nextTriangle.v[iCorner + 1][2] = (float)v.GetY();
			}

			computeFacetNormal(nextTriangle);

			if (bExportBinary)
				fwrite(&nextTriangle, 50, 1, m_pFile);
			else {
				// same coordinates as the binary format so both formats round-trip through the importer
				Output(m_pFile, string("facet normal ") + to_string(nextTriangle.v[0][0]) + " " + to_string(nextTriangle.v[0][1]) + " " + to_string(nextTriangle.v[0][2]) + "\n\touter loop\n");
				Output(m_pFile, string("\t\tvertex ") + to_string(nextTriangle.v[1][0]) + " " + to_string(nextTriangle.v[1][1]) + " " + to_string(nextTriangle.v[1][2]) + "\n");
				Output(m_pFile, string("\t\tvertex ") + to_string(nextTriangle.v[2][0]) + " " + to_string(nextTriangle.v[2][1]) + " " + to_string(nextTriangle.v[2][2]) + "\n");
				Output(m_pFile, string("\t\tvertex ") + to_string(nextTriangle.v[3][0]) + " " + to_string(nextTriangle.v[3][1]) + " " + to_string(nextTriangle.v[3][2]) + "\n");
				Output(m_pFile, "\tendloop\nendfacet\n");
			}

			numTris2++;
			if (numTris2 % 100000 == 0) { // increment progress bar
				m_progress.Increment(100000);
				if (m_progress.IsCancelPressed()) {
					fclose(m_pFile);
					m_progress.PutVisible(false);
					return CStatus::False;
				}
			}
		}
	}

	if (!bExportBinary)
		Output(m_pFile, "endsolid " + m_fileName + "\n\n");

	if (bExportBinary) {
		fseek(m_pFile, 80, SEEK_SET);
		fwrite(&totaltris, sizeof(ULONG), 1, m_pFile);
	}

	m_progress.PutVisible(false);

	fclose(m_pFile);

	if (fails != 0)
		app.LogMessage(L"STL file export successful but " + CString(fails) + L" objects were skipped (nulls or other non-mesh items were selected).", siWarningMsg);

	return CStatus::OK;
}

// parser for ASCII STL format: reads the next 3 "vertex x y z" lines; returns false if the file ends first
bool CSTL::parseAsciiFacet(StlTri& nextTriangle, FILE* file)
{
	char buf[256];

	int ix = 0;
	while (ix < 3 && fgets(buf, sizeof(buf), file)) {
		float x, y, z;
		if (sscanf(buf, " vertex %f %f %f", &x, &y, &z) == 3) {
			nextTriangle.v[ix + 1][0] = x;
			nextTriangle.v[ix + 1][1] = y;
			nextTriangle.v[ix + 1][2] = z;
			ix++;
		}
	}

	return ix == 3;
}

void CSTL::Output(FILE* file, string str) {
	fputs(str.c_str(), file); // not fprintf: the string may contain '%'
}
