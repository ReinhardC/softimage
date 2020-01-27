#include "OBJ.h"

#define log app.LogMessage

CStatus COBJ::Execute_Export(CRefArray& inObjects, string initFilePathName, bool bExportVertexColors, bool bSeparateFiles, bool bWriteMTLFile, bool bExportLocalCoords)
{
	XSI::Application app;

	initStrings(initFilePathName);

	if ((m_pFile = fopen(m_filePathName.c_str(), "wb")) == NULL)
		return CStatus::Fail;

	if (bWriteMTLFile)
		if ((m_matfile = fopen((m_filePath + m_fileName + string(".mtl")).c_str(), "wb")) == NULL)
			return CStatus::Fail;

	// output header
	Output(m_pFile, "# Custom Wavefront OBJ Exporter\r\n");

	if (bWriteMTLFile)
		Output(m_pFile, "\r\nmtllib " + m_fileName + ".mtl\r\n");

	Output(m_pFile, "\r\no " + m_fileName + "\r\n");

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

		Output(m_pFile, "\r\ng " + string(xobj.GetName().GetAsciiString()) + "\r\n");

		m_progress.PutCaption("Exporting OBJ (Vertex Positions of '" + xobj.GetName() + "')");
		m_progress.Increment();

		Output(m_pFile, "\r\n# vertex positions\r\n");

		MATH::CTransformation xfo = xobj.GetKinematics().GetGlobal().GetTransform();

		for (long iVertexPosition = 0, i_max = VP.GetCount() / 3; iVertexPosition < i_max; iVertexPosition++) {

			MATH::CVector3 v(VP[3 * iVertexPosition + 0], VP[3 * iVertexPosition + 1], VP[3 * iVertexPosition + 2]);

			if (!bExportLocalCoords)
				v = MATH::MapObjectPositionToWorldSpace(xfo, v);


			Output(m_pFile, "v " + to_string(v.GetX()) + " " + to_string(v.GetY()) + " " + to_string(v.GetZ()) + "\r\n");
		}
		Output(m_pFile, "# end vertex positions (" + to_string(VP.GetCount() / 3) + ")\r\n");

		tr1::array<float, 3> triple;

		// ************************************************************************************************
		// * Export Mask and Vertex Colors in ZBrush Polypaint format
		// * can be read by 3dcoat, sculptgl, xnormal, meshmixer, but NOT by ZBrush!

		if (bExportVertexColors) {

			ClusterProperty foundProperty(CFileFormat::getVertexColorProperty(xobj));

			if (foundProperty.IsValid()) {

				m_progress.PutCaption("Exporting OBJ (Vertex Colors of '" + xobj.GetName() + "')");

				char hex[] = "0123456789abcdef";

				Output(m_pFile, "\r\n# The following MRGB block contains ZBrush Vertex Color (Polypaint) and masking output as 4 hexadecimal values per vertex. The vertex color format is MMRRGGBB with up to 64 entries per MRGB line.\r\n");

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
						Output(m_pFile, strLine + "\r\n");
						strLine = "#MRGB ";
						nColors = 0;
					}
				}

				if (nColors != 0)
					Output(m_pFile, strLine + "\r\n");

				Output(m_pFile, "# End of MRGB block \r\n");
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

			Output(m_pFile, "\r\n# texture positions\r\n");

			long ix = 0;
			for (long iUVCoord = 0, l_max = FA_UVs.GetCount() / 3; iUVCoord < l_max; iUVCoord++) {

				float u = FA_UVs[3 * iUVCoord + 0];
				float v = FA_UVs[3 * iUVCoord + 1];
				float w = FA_UVs[3 * iUVCoord + 2];
				triple = { u, v, w };
				if (m_hashmap_uv.count(triple) == 0) {

					m_hashmap_uv.insert({ triple, ix++ });
					//Output(m_file, "vt " + to_string(u) + " " + to_string(v) + " " + to_string(w) + "\r\n");
					Output(m_pFile, "vt " + to_string(u) + " " + to_string(v) + "\r\n");
				}
			}

			Output(m_pFile, "# end texture positions (" + to_string(m_hashmap_uv.size()) + ", down from " + to_string(FA_UVs.GetCount() / 3) + " with duplicates)\r\n");
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

			Output(m_pFile, "\r\n# normals\r\n");

			long ix = 0;
			for (long iUserNormal = 0, l_max = FA_UserNormals.GetCount() / 3; iUserNormal < l_max; iUserNormal++) {
				float nx = FA_UserNormals[3 * iUserNormal + 0];
				float ny = FA_UserNormals[3 * iUserNormal + 1];
				float nz = FA_UserNormals[3 * iUserNormal + 2];
				triple = { nx, ny, nz };
				if (m_hashmap_normals.count(triple) == 0) {

					m_hashmap_normals.insert({ triple, ix++ });
					Output(m_pFile, string("vn ") + to_string(nx) + " " + to_string(ny) + " " + to_string(nz) + "\r\n");
				}
			}

			Output(m_pFile, string("# end normals (") + to_string(m_hashmap_normals.size()) + ", down from " + to_string(FA_UserNormals.GetCount() / 3) + " with duplicates)\r\n");
		}

		CRefArray Materials = ga.GetMaterials();

		Output(m_pFile, string("\r\n# ") + to_string(LA_Counts.GetCount()) + " polygons using " + to_string(Materials.GetCount()) + " Material(s)\r\n");

		// ************************
		// * Export Polygons

		m_progress.PutCaption("Exporting OBJ (Polygons of '" + xobj.GetName() + "')");
		m_progress.Increment();

		// loop materials
		for (long iMaterial = 0, i_max = Materials.GetCount(); iMaterial < i_max; iMaterial++) {

			Material material(Materials[iMaterial]);

			CString materialName = material.GetName();

			// avoid confusion with Scene_Material
			if (materialName.FindString("Scene_Material") != UINT_MAX)
				materialName = "default";

			// if(bWriteMTLFile) - commented out because usemtl might be used for other purposes
			Output(m_pFile, string("usemtl ") + string(materialName.GetAsciiString()) + "\r\n");


			CValue rVal;
			CValueArray& args = CValueArray();
			args.Add(xobj.GetRef());
			args.Add(material.GetRef());
			args.Add(CString(m_filePath.c_str()));
			app.ExecuteCommand(L"ParseShaderTree", args, rVal);

			CString strMaterial(rVal);

			if (bWriteMTLFile)
				Output(m_matfile, (L"newmtl " + materialName + L"\r\n" + strMaterial + L"\r\n").GetAsciiString());

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

					Output(m_pFile, strTmp);
				}

				ix += polyVertexCount;
			} // for each polygon
		} // for each material4

		m_vertices_base_ix_group += VP.GetCount() / 3;
		m_uvs_base_ix_group += m_hashmap_uv.size();
		m_normals_base_ix_group += m_hashmap_normals.size();
	}

	fclose(m_pFile);

	if (bWriteMTLFile)
		fclose(m_matfile);

	if (fails != 0)
		app.LogMessage("OBJ file export successful but " + CString(fails) + " objects failed (nulls or other items were selected).", siWarningMsg);

	return CStatus::OK;
}
