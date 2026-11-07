function XSILoadPlugin(in_reg) {
	in_reg.Author = "RClaus.";
	in_reg.Name = "Build QuickAccess Library";
	in_reg.Major = 1;
	in_reg.Minor = 1;
	in_reg.RegisterMenu(siMenuTbGetPrimitiveID, "QuickAccess Library", true, true);
	return true;
}

function QuickAccessLibrary_Init(in_ctxt) {
	// Global JS object to look up paths from menu names ["box" => "C:\Users\Gump\Softimage\Data\ObjLib\box.obj"]
	// Seeing no other way because menu items can't store user data
	SetGlobalObject("UserObjLibMenuPaths", {});

	var oMenu = in_ctxt.source;
	var fso = new ActiveXObject("Scripting.FileSystemObject");
	var bSuccess = false;
	var bAddSeparator = false;

	// preferences made before these options existed do not have them: show the menu, with no custom folder
	var ioPrefs = Application.Preferences.Categories("File Format Options");
	var oShowObjLib = ioPrefs ? ioPrefs.Parameters("OBJLIB_showObjLib") : null;
	if (oShowObjLib && !oShowObjLib.Value) return;
	var oLibPath = ioPrefs ? ioPrefs.Parameters("OBJLIB_DefaultPath") : null;

	// Scan user directory, used "/" to support linux (?)
	libpath = oLibPath ? oLibPath.Value : "";
	if (libpath != "" && ScanDirectory(oMenu, libpath, fso) != -1) {
		bSuccess = true;
		oMenu.AddSeparatorItem();
	}

	// Scan user directory, used "/" to support linux (?)
	libpath = Application.InstallationPath(siUserPath) + "/Data/ObjLib/";
	if (ScanDirectory(oMenu, libpath, fso) != -1) {
		bSuccess = true;
		oMenu.AddSeparatorItem();
	}

	// Scan active workgroups, used "/" to support linux (?)
	aWorkgroups = new VBArray(Application.Workgroups);
	cntWorkgroups = aWorkgroups.ubound(1) + 1
	for (var i = 0; i < cntWorkgroups; i++) {
		var strWorkgroupPath = aWorkgroups.getItem(i) + "/Data/ObjLib/";
		if (ScanDirectory(oMenu, strWorkgroupPath, fso) != -1) {
			bSuccess = true;
			oMenu.AddSeparatorItem();
		}
	}

	if (!bSuccess)
		logmessage("Build QuickAccess Library Addon: No ObjLib folder found in [User location/Data] or any active Workgroup", siWarning)
}

function ScanDirectory(oMenu, folderspec, fso) {
	try { f = fso.GetFolder(folderspec); }
	catch (e) { return -1; }
	var fc = new Enumerator(f.SubFolders);
	ScanFiles(oMenu, folderspec, fso);
	for (; !fc.atEnd(); fc.moveNext()) {
		var oSubMenu = oMenu.AddItem(fc.item().Name, siMenuItemSubmenu);
		ScanDirectory(oSubMenu, fc.item().Path, fso);
	}
}

function ScanFiles(oMenu, folderspecs, fso) {
	var __Paths = GetGlobal("UserObjLibMenuPaths");
	var fi = fso.GetFolder(folderspecs);
	fcs = new Enumerator(fi.files);
	for (; !fcs.atEnd(); fcs.moveNext()) {
		fn = fcs.item().name;
		fldr = fcs.item().ParentFolder.Path;
		ending = fn.slice(-3).toLowerCase();
		if (ending == "obj" || ending == "stl" || ending == "fbx" || ending == "mdl") {
			strMenuName = fn.substr(0, fn.lastIndexOf(".")) + " (" + ending + ")";
			if (__Paths[strMenuName]) // Menu name collision - create unique name for menu
				strMenuName += " (" + fldr.substr(0, fldr.lastIndexOf("Data")) + ")";
			oMenu.AddCallbackItem(strMenuName, "OnObjLibImport");
			__Paths[strMenuName] = fcs.item().path;
		}
	}
}

function OnObjLibImport(in_ctxt) {
	ImportAFile(GetGlobal("UserObjLibMenuPaths")[in_ctxt.source.Name]);
}

function ImportAFile(filename) {
	var re = /(?:\.([^.]+))?$/;
	var ending = re.exec(filename)[0];
	switch (ending.toLowerCase()) {
		case '.obj':
			if (Selection.Count > 0)
				if (XSIUIToolkit.MsgBox("Replace selected object?", siMsgYesNo | siMsgQuestion, "Import OBJ") == siMsgNo)
					DeselectAll();

			ioPrefs = Application.Preferences.Categories("File Format Options");

			new_object_tag = ""
			OBJ_CreateObjectsTag = ioPrefs.OBJ_CreateObjectsTag.value;
			switch (OBJ_CreateObjectsTag) {
				case 1: new_object_tag = "o"; break;
				case 2: new_object_tag = "usemtl"; break;
				case 3: new_object_tag = "_____"; break;
				default: new_object_tag = "g"; break;
			}

			new_cluster_tag = ""
			OBJ_CreateClustersTag = ioPrefs.OBJ_CreateClustersTag.value;
			switch (OBJ_CreateClustersTag) {
				case 0: new_cluster_tag = "g"; break;
				case 1: new_cluster_tag = "o"; break;
				case 3: new_cluster_tag = "_____"; break;
				default: new_cluster_tag = "usemtl"; break;
			}

			ImportOBJ(Selection, filename, ioPrefs.OBJ_ImportUVs.value, ioPrefs.OBJ_ImportUserNormals.value, ioPrefs.OBJ_ImportMask.value, ioPrefs.OBJ_ImportPolypaint.value, new_object_tag, new_cluster_tag, ioPrefs.OBJ_AutoScale.value)

			break;
		case '.emdl':
		case '.mdl':
			ImportModel(filename);
			break;
		case '.stl':
			if (Selection.Count > 0)
				if (XSIUIToolkit.MsgBox("Replace selected object?", siMsgYesNo | siMsgQuestion, "Import STL") == siMsgNo)
					DeselectAll();
			ImportSTL(Selection, filename, Application.Preferences.Categories("File Format Options").STL_AutoScale.value)
			break;
		case '.fbx':
			FBXImport(filename);
			break;
	}
}