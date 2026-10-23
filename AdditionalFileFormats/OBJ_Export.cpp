#include "OBJ.h"
#include "FastIO.h"

CStatus COBJ::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportMask, bool bExportVertexColors, bool bSeparateFiles, bool bWriteMTLFile, bool bExportLocalCoords)
{
	if (!bSeparateFiles)
		return ExportToFile(inObjects, initFilePathName, bExportMask, bExportVertexColors, bWriteMTLFile, bExportLocalCoords);

	// one file (and MTL) per object, named <folder>\<file name>_<object name>.obj
	XSI::Application app;

	initStrings(initFilePathName);
	string folder = m_filePath, baseName = m_fileName;

	CStatus result = CStatus::OK;
	long nbSkipped = 0;
	for (long iObject = 0, iObject_max = inObjects.GetCount(); iObject < iObject_max; iObject++) {
		X3DObject xobj(inObjects[iObject]);
		PolygonMesh mesh;
		if (xobj.IsValid())
			mesh = xobj.GetActivePrimitive().GetGeometry();
		if (!mesh.IsValid()) {
			nbSkipped++;
			continue;
		}

		CRefArray single;
		single.Add(xobj.GetRef());
		string filePathName = folder + baseName + "_" + string(xobj.GetName().GetAsciiString()) + ".obj";
		if (ExportToFile(single, filePathName, bExportMask, bExportVertexColors, bWriteMTLFile, bExportLocalCoords) != CStatus::OK)
			result = CStatus::Fail;
	}

	if (nbSkipped != 0)
		app.LogMessage("OBJ export: " + CString(nbSkipped) + " selected items were skipped (nulls or other non-mesh items).", siWarningMsg);

	return result;
}

// writes all given objects into one OBJ file (plus a MTL file of the same name when asked for)
CStatus COBJ::ExportToFile(CRefArray& inObjects, string initFilePathName, bool bExportMask, bool bExportVertexColors, bool bWriteMTLFile, bool bExportLocalCoords)
{
	XSI::Application app;

	initStrings(initFilePathName);

	// indices in an OBJ file are global, every object in the same file continues after the previous one
	m_vertices_base_ix_group = 0;
	m_uvs_base_ix_group = 0;
	m_normals_base_ix_group = 0;

	if ((m_pFile = fopen(m_filePathName.c_str(), "wb")) == NULL) {
		app.LogMessage("Could not create '" + CString(m_filePathName.c_str()) + "'.", siErrorMsg);
		return CStatus::Fail;
	}

	if (bWriteMTLFile) {
		string matFilePathName = m_filePath + m_fileName + string(".mtl");
		if ((m_matfile = fopen(matFilePathName.c_str(), "wb")) == NULL) {
			app.LogMessage("Could not create '" + CString(matFilePathName.c_str()) + "'.", siErrorMsg);
			fclose(m_pFile);
			return CStatus::Fail;
		}
	}

	// all text goes through a 1 MB buffer with in-place number formatting (to_string + fputs per line was the bottleneck)
	fastio::CTextWriter w(m_pFile);

	// output header
	w.put("# Custom Wavefront OBJ Exporter\r\n");

	if (bWriteMTLFile) {
		w.put("\r\nmtllib ");
		w.put(m_fileName);
		w.put(".mtl\r\n");
	}

	w.put("\r\no ");
	w.put(m_fileName);
	w.put("\r\n");

	long fails = 0;

	m_progress.PutVisible(true);
	m_progress.PutMaximum(3 * inObjects.GetCount());

	for (long iObject = 0, iObject_max = inObjects.GetCount(); iObject < iObject_max; iObject++)
	{
		X3DObject xobj(inObjects[iObject]);
		if (!xobj.IsValid()) {
			fails++;
			continue;
		}

		// Get a geometry accessor from the selected object
		XSI::Primitive prim = xobj.GetActivePrimitive();
		XSI::PolygonMesh mesh = prim.GetGeometry();
		if (!mesh.IsValid()) {
			fails++;
			continue;
		}

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

		w.put("\r\ng ");
		w.put(xobj.GetName().GetAsciiString());
		w.put("\r\n");

		m_progress.PutCaption("Exporting OBJ (Vertex Positions of '" + xobj.GetName() + "')");
		m_progress.Increment();

		w.put("\r\n# vertex positions\r\n");

		MATH::CTransformation xfo = xobj.GetKinematics().GetGlobal().GetTransform();

		for (long iVertexPosition = 0, i_max = VP.GetCount() / 3; iVertexPosition < i_max; iVertexPosition++) {

			MATH::CVector3 v(VP[3 * iVertexPosition + 0], VP[3 * iVertexPosition + 1], VP[3 * iVertexPosition + 2]);

			if (!bExportLocalCoords)
				v = MATH::MapObjectPositionToWorldSpace(xfo, v);

			w.put("v ");
			w.putFixed(v.GetX());
			w.put(' ');
			w.putFixed(v.GetY());
			w.put(' ');
			w.putFixed(v.GetZ());
			w.put("\r\n");
		}
		w.put("# end vertex positions (");
		w.putLong(VP.GetCount() / 3);
		w.put(")\r\n");

		array<float, 3> triple;

		// ************************************************************************************************
		// * Export Mask and Vertex Colors in ZBrush Polypaint format
		// * can be read by 3dcoat, sculptgl, xnormal, meshmixer, but NOT by ZBrush!

		if (bExportVertexColors || bExportMask) {

			ClusterProperty foundMaskProperty(CFileFormat::getMaskProperty(xobj));
			ClusterProperty foundRGBProperty(CFileFormat::getVertexColorProperty(xobj));

			// each part is written only if it was asked for and the object has the property
			bool bMaskValid = bExportMask && foundMaskProperty.IsValid();
			bool bRGBValid = bExportVertexColors && foundRGBProperty.IsValid();

			if (bMaskValid || bRGBValid) {

				m_progress.PutCaption("Exporting OBJ (Vertex Colors of '" + xobj.GetName() + "')");

				char hex[] = "0123456789abcdef";

				w.put("\r\n# The following MRGB block contains ZBrush Vertex Color (Polypaint) and masking output as 4 hexadecimal values per vertex. The vertex color format is MMRRGGBB with up to 64 entries per MRGB line.\r\n");

				CFloatArray FA_RGB_perPoint;
				CLongArray LA_RGBCounts_perPoint;
				CFloatArray FA_Mask, FA_RGBA_perSample;
				CBitArray flags_Mask, flags_RGBA_perSample;

				int i_max = 0;

				if (bMaskValid) {
					foundMaskProperty.GetValues(FA_Mask, flags_Mask);
					i_max = FA_Mask.GetCount();
				}

				if (bRGBValid) {
					foundRGBProperty.GetValues(FA_RGBA_perSample, flags_RGBA_perSample);

					FA_RGB_perPoint.Resize(3 * mesh.GetPoints().GetCount());
					for (long i = 0, n = FA_RGB_perPoint.GetCount(); i < n; i++)
						FA_RGB_perPoint[i] = 0.0;

					LA_RGBCounts_perPoint.Resize(mesh.GetPoints().GetCount());
					for (long i = 0, n = LA_RGBCounts_perPoint.GetCount(); i < n; i++)
						LA_RGBCounts_perPoint[i] = 0;

					// reorder sample property to vertices (average)
					for (long iRGBA = 0, iRGBA_max = FA_RGBA_perSample.GetCount() / 4; iRGBA < iRGBA_max; iRGBA++) {

						LA_RGBCounts_perPoint[LA_Indices[iRGBA]] += 1;
						FA_RGB_perPoint[3 * LA_Indices[iRGBA] + 0] += FA_RGBA_perSample[4 * iRGBA + 0];
						FA_RGB_perPoint[3 * LA_Indices[iRGBA] + 1] += FA_RGBA_perSample[4 * iRGBA + 1];
						FA_RGB_perPoint[3 * LA_Indices[iRGBA] + 2] += FA_RGBA_perSample[4 * iRGBA + 2];
					}

					i_max = FA_RGB_perPoint.GetCount() / 3;
				}

				string strLine = "#MRGB ";
				int nColors = 0;
				for (int i = 0; i < i_max; i++) {

					if (bMaskValid) {
						int m = (int)(255.0 * (1.0 - FA_Mask[i]));
						strLine += hex[(m >> 4) & 0xf];
						strLine += hex[m & 0xf];
					}
					else
						strLine += "ff";

					if (bRGBValid && LA_RGBCounts_perPoint[i] > 0) {
						int r = (int)(255.0 * (FA_RGB_perPoint[3 * i] / LA_RGBCounts_perPoint[i]));
						strLine += hex[(r >> 4) & 0xf];
						strLine += hex[r & 0xf];
						int g = (int)(255.0 * (FA_RGB_perPoint[3 * i + 1] / LA_RGBCounts_perPoint[i]));
						strLine += hex[(g >> 4) & 0xf];
						strLine += hex[g & 0xf];
						int b = (int)(255.0 * (FA_RGB_perPoint[3 * i + 2] / LA_RGBCounts_perPoint[i]));
						strLine += hex[(b >> 4) & 0xf];
						strLine += hex[b & 0xf];
					}
					else
						strLine += "ffffff";

					if (nColors++ == 63) {
						w.put(strLine);
						w.put("\r\n");
						strLine = "#MRGB ";
						nColors = 0;
					}
				}

				if (nColors != 0) {
					w.put(strLine);
					w.put("\r\n");
				}

				w.put("# End of MRGB block \r\n");
			}
		}

		// ************************
		// * Export UVs
		// the file index of every node's uv is kept in a table, so the polygon loop below needs no lookups

		ClusterProperty UVCP(ga.GetUVs()[0]);
		CFloatArray FA_UVs;
		CBitArray flags_UVs;
		vector<long> uvIxOfNode;
		m_hashmap_uv.clear();

		bool bExportUVs = UVCP.IsValid();
		if (bExportUVs) {

			UVCP.GetValues(FA_UVs, flags_UVs);

			m_progress.PutCaption("Exporting OBJ (Texture Positions of '" + xobj.GetName() + "')");

			w.put("\r\n# texture positions\r\n");

			long nbNodes = FA_UVs.GetCount() / 3;
			uvIxOfNode.resize(nbNodes);
			m_hashmap_uv.reserve(nbNodes);

			long ix = 0;
			for (long iNode = 0; iNode < nbNodes; iNode++) {

				float u = FA_UVs[3 * iNode + 0];
				float v = FA_UVs[3 * iNode + 1];
				float w3 = FA_UVs[3 * iNode + 2];
				triple = { u, v, w3 };
				auto found = m_hashmap_uv.find(triple);
				if (found == m_hashmap_uv.end()) {
					m_hashmap_uv.insert({ triple, ix });
					uvIxOfNode[iNode] = ix++;
					w.put("vt ");
					w.putFixed(u);
					w.put(' ');
					w.putFixed(v);
					w.put("\r\n");
				}
				else
					uvIxOfNode[iNode] = found->second;
			}

			w.put("# end texture positions (");
			w.putLong((long long)m_hashmap_uv.size());
			w.put(", down from ");
			w.putLong(nbNodes);
			w.put(" with duplicates)\r\n");
		}

		// ************************
		// * Export User Normals

		ClusterProperty UNCP(ga.GetUserNormals()[0]);
		CFloatArray FA_UserNormals;
		vector<long> normalIxOfNode;
		m_hashmap_normals.clear();

		bool bExportUserNormals = UNCP.IsValid();
		if (bExportUserNormals)
		{
			ga.GetNodeNormals(FA_UserNormals);

			m_progress.PutCaption("Exporting OBJ (Normals of '" + xobj.GetName() + "')");

			w.put("\r\n# normals\r\n");

			long nbNodes = FA_UserNormals.GetCount() / 3;
			normalIxOfNode.resize(nbNodes);
			m_hashmap_normals.reserve(nbNodes);

			long ix = 0;
			for (long iNode = 0; iNode < nbNodes; iNode++) {
				float nx = FA_UserNormals[3 * iNode + 0];
				float ny = FA_UserNormals[3 * iNode + 1];
				float nz = FA_UserNormals[3 * iNode + 2];
				triple = { nx, ny, nz };
				auto found = m_hashmap_normals.find(triple);
				if (found == m_hashmap_normals.end()) {
					m_hashmap_normals.insert({ triple, ix });
					normalIxOfNode[iNode] = ix++;
					w.put("vn ");
					w.putFixed(nx);
					w.put(' ');
					w.putFixed(ny);
					w.put(' ');
					w.putFixed(nz);
					w.put("\r\n");
				}
				else
					normalIxOfNode[iNode] = found->second;
			}

			w.put("# end normals (");
			w.putLong((long long)m_hashmap_normals.size());
			w.put(", down from ");
			w.putLong(nbNodes);
			w.put(" with duplicates)\r\n");
		}

		CRefArray Materials = ga.GetMaterials();

		w.put("\r\n# ");
		w.putLong(LA_Counts.GetCount());
		w.put(" polygons using ");
		w.putLong(Materials.GetCount());
		w.put(" Material(s)\r\n");

		// ************************
		// * Export Polygons

		m_progress.PutCaption("Exporting OBJ (Polygons of '" + xobj.GetName() + "')");
		m_progress.Increment();

		// polygons bucketed by material once (instead of scanning all polygons for every material)
		long nbPolygons = LA_Counts.GetCount();
		long nbMaterials = Materials.GetCount();
		vector<long> polyStart(nbPolygons + 1);
		polyStart[0] = 0;
		for (long iPoly = 0; iPoly < nbPolygons; iPoly++)
			polyStart[iPoly + 1] = polyStart[iPoly] + LA_Counts[iPoly];

		vector<vector<long> > polysOfMaterial(nbMaterials > 0 ? nbMaterials : 1);
		for (long iPoly = 0; iPoly < nbPolygons; iPoly++) {
			long iMaterial = LA_Materials[iPoly];
			if (iMaterial < 0 || iMaterial >= nbMaterials)
				iMaterial = 0;
			polysOfMaterial[iMaterial].push_back(iPoly);
		}

		// loop materials
		for (long iMaterial = 0, i_max = Materials.GetCount(); iMaterial < i_max; iMaterial++) {

			Material material(Materials[iMaterial]);

			CString materialName = material.GetName();

			// avoid confusion with Scene_Material
			if (materialName.FindString("Scene_Material") != UINT_MAX)
				materialName = "default";

			// if(bWriteMTLFile) - commented out because usemtl might be used for other purposes
			w.put("usemtl ");
			w.put(materialName.GetAsciiString());
			w.put("\r\n");

			CValue rVal;
			CValueArray args;
			args.Add(xobj.GetRef());
			args.Add(material.GetRef());
			args.Add(CString(m_filePath.c_str()));
			app.ExecuteCommand(L"ParseShaderTree", args, rVal);

			CString strMaterial(rVal);

			if (bWriteMTLFile)
				Output(m_matfile, (L"newmtl " + materialName + L"\r\n" + strMaterial + L"\r\n").GetAsciiString());

			const vector<long>& polys = polysOfMaterial[iMaterial];
			for (size_t iInBucket = 0; iInBucket < polys.size(); iInBucket++) {

				long iPoly = polys[iInBucket];
				long ix = polyStart[iPoly];
				long polyVertexCount = LA_Counts[iPoly];

				w.put("f ");

				// loop single polygon
				for (long iPolyVertexcount = 0; iPolyVertexcount < polyVertexCount; iPolyVertexcount++) {

					long iNode = LA_Nodes[ix + iPolyVertexcount];

					// vertex index
					w.putLong(LA_Indices[ix + iPolyVertexcount] + 1 + (long long)m_vertices_base_ix_group);

					w.put('/');
					if (bExportUVs)
						w.putLong(uvIxOfNode[iNode] + 1 + (long long)m_uvs_base_ix_group);

					if (bExportUserNormals) {
						w.put('/');
						w.putLong(normalIxOfNode[iNode] + 1 + (long long)m_normals_base_ix_group);
					}

					w.put(' ');
				} // for each polygon point

				w.put("\r\n");
			} // for each polygon of the material
		} // for each material

		m_vertices_base_ix_group += VP.GetCount() / 3;
		m_uvs_base_ix_group += m_hashmap_uv.size();
		m_normals_base_ix_group += m_hashmap_normals.size();
	}

	w.flush();
	fclose(m_pFile);

	if (bWriteMTLFile)
		fclose(m_matfile);

	m_progress.PutVisible(false);

	if (fails != 0)
		app.LogMessage("OBJ file export successful but " + CString(fails) + " objects failed (nulls or other items were selected).", siWarningMsg);

	return CStatus::OK;
}
