#pragma once
#include <string>

struct ObjPreferences {
	bool OBJ_ImportUVs;
	bool OBJ_ImportUserNormals;
	bool OBJ_ImportMask;
	bool OBJ_ImportPolypaint;
	bool OBJ_AutoScale; // scale objects by 0.1 or 0.01 when the mesh is wider than 55 or 150 units in x
	CString OBJ_CreateObjectsTag;
	CString OBJ_CreateClustersTag;
};