#pragma once

#include "FileFormat.h"

using namespace XSI;

#define log app.LogMessage

CRef CFileFormat::getVertexColorProperty(X3DObject xobj)
{
	Application app;

	CRef foundPropertyRef;

	CString cavName = xobj.GetMaterial().GetParameterValue("CAV"); // dev fail, should be actual ClusterProperty not String, object could have two ClusterProperties of same name in 2 clusters!
	CRefArray sampleClusters;
	Primitive prim = xobj.GetActivePrimitive();
	PolygonMesh mesh = prim.GetGeometry();

	// iterate sample clusters
	mesh.GetClusters().Filter(siSampledPointCluster, CStringArray(), "", sampleClusters);
	for (long iCluster = 0, iCluster_max = sampleClusters.GetCount(); iCluster < iCluster_max; iCluster++) {
		
		Cluster sampleCluster(sampleClusters[iCluster]);
		CRefArray cavProperties;
		sampleCluster.GetProperties().Filter(siVertexcolorType, CStringArray(), "", cavProperties);

		// iterate color at vertex properties of clusters
		for (long iProperty = 0, iProperty_max = cavProperties.GetCount(); iProperty < iProperty_max; iProperty++) {

			CRef& propertyRef = cavProperties.GetItem(iProperty);
			ClusterProperty property(propertyRef);

			if (!foundPropertyRef.IsValid()) 
				foundPropertyRef = propertyRef;

			if (cavName == property.GetName()) 
				return propertyRef;
		}
	}

	return foundPropertyRef;
}
