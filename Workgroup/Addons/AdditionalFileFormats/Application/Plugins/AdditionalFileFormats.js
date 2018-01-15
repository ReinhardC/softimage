/*************
** 
** part 1 - OBJ FILE Import/Export Renderer definitions
**
**************/


/**** ARNOLD *****/

/*
** "apply" callback - 
** converts matDefinition from .mtl file to new xsi material and applies it to objList
** (called from C++ import function)
*/
function ApplyShaderTree_arnoldrender(matName, matDefinition, objList, folder)
{	
	var matLib = Application.ActiveProject.ActiveScene.ActiveMaterialLibrary;	
	var newMaterial = matLib.CreateMaterial( "Phong", matName );
	var newPhong = newMaterial.Shaders.Item(0);
	DeleteObj(newPhong);

    var newSurface = CreateShaderFromProgID("Arnold.standard_surface.1.0", newMaterial, null);
	var closure = CreateShaderFromProgID("Arnold.closure.1.0", newMaterial, null);
	SIConnectShaderToCnxPoint(newSurface, closure + ".closure", false);
	SIConnectShaderToCnxPoint(closure, newMaterial + ".surface", false);
	
	if(matDefinition.Kd) 
		applyColor(newSurface.base_color, matDefinition.Kd);
	if(matDefinition.map_Kd)
		applyAImage(newSurface.base_color, folder + matDefinition.map_Kd, newMaterial);
			
	if(matDefinition.Ks) 	
		applyColor(newSurface.specular_color, matDefinition.Ks);
	if(matDefinition.map_Ks) 
		applyAImage(newSurface.specular_color, folder + matDefinition.map_Ks, newMaterial);
		
	if(matDefinition.Kt)
		applyColor(newSurface.transmission_color, matDefinition.Kt);
	if(matDefinition.map_Kt) 
		applyAImage(newSurface.transmission_color, folder + matDefinition.map_Kt, newMaterial);
			
	if(matDefinition.Ns)
		newSurface.specular_roughness.Value = Math.max(0.0, Math.min(1.0, matDefinition.Ns));
	if(matDefinition.map_Ns) 
		applyAImage(newSurface.specular_roughness, folder + matDefinition.map_Ns, newMaterial);	
	
	SIAssignMaterial(objList, newMaterial);	
} 

/*
** "parse" callback - 
** parses xsi material tree returns a textual .mat file material definition like e.g. "Kd 1 1 1\nKs ...." without the "newmtl" line
** (called from C++ export function)
*/
function ParseShaderTree_arnoldrender(instance, material, folder)
{	
	var Kd = "Kd 1 1 1\n"; var map_Kd = "";
	var Kt = ""; var map_Kt = "";
	var Kr = ""; var map_Kr = "";
	var Ks = ""; var map_Ks = "";
	var Ka = ""; var map_Ka = "";
	var Ns = ""; var map_Ns = ""; 		
	
	var i=0;
	while(i < material.GetAllShaders().Count) {
		var shader = material.GetAllShaders().Item(i);
		if(	shader.ProgID ==  "Arnold.standard_surface.1.0" )
		{
			Kd = "Kd "  + round6(shader.base_color.parameters("red").Value) + " " + round6(shader.base_color.parameters('green').Value) + " " + round6(shader.base_color.parameters('blue').Value) + "\n";			
			if(shader.base_color.Sources.Count != 0 && shader.base_color.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0")
				map_Kd = "map_Kd "  + getSIImageFileName(shader.base_color.Sources.Item(0).Parent).replace(folder, "") + "\n";
			else if(shader.base_color.Sources.Count != 0 && shader.base_color.Sources.Item(0).Parent.ProgID == "Arnold.image.1.0")
				map_Kd = "map_Kd "  + getAImageFileName(shader.base_color.Sources.Item(0).Parent).replace(folder, "") + "\n";
			
			Ks = "Ks "  + round6(shader.specular_color.parameters("red").Value) + " " + round6(shader.specular_color.parameters('green').Value) + " " + round6(shader.specular_color.parameters('blue').Value) + "\n";
			if(shader.specular_color.Sources.Count != 0 && shader.specular_color.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
				map_Ks = "map_Ks " + getSIImageFileName(shader.specular_color.Sources.Item(0).Parent).replace(folder, "") + "\n";			
			else if(shader.specular_color.Sources.Count != 0 && shader.specular_color.Sources.Item(0).Parent.ProgID == "Arnold.image.1.0")
				map_Ks = "map_Ks "  + getAImageFileName(shader.specular_color.Sources.Item(0).Parent).replace(folder, "") + "\n";
			
			Kt = "Kt "  + round6(shader.transmission_color.parameters("red").Value) + " " + round6(shader.transmission_color.parameters('green').Value) + " " + round6(shader.transmission_color.parameters('blue').Value) + "\n";
			if(shader.transmission_color.Sources.Count != 0 && shader.transmission_color.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
				map_Kt = "map_Kt " + getSIImageFileName(shader.transmission_color.Sources.Item(0).Parent).replace(folder, "") + "\n";			
			else if(shader.transmission_color.Sources.Count != 0 && shader.transmission_color.Sources.Item(0).Parent.ProgID == "Arnold.image.1.0")
				map_Kt = "map_Kt "  + getAImageFileName(shader.transmission_color.Sources.Item(0).Parent).replace(folder, "") + "\n";
		
			Ns = "Ns " + shader.parameters('specular_roughness').Value + "\n";
			if(shader.parameters('specular_roughness').Sources.Count != 0 && shader.parameters('specular_roughness').Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0")
				map_Ns = "map_Ns " + getSIImageFileName(shader.parameters('specular_roughness').Sources.Item(0).Parent).replace(folder, "") + "\n";
			else if(shader.specular_roughness.Sources.Count != 0 && specular_roughness.specular_color.Sources.Item(0).Parent.ProgID == "Arnold.image.1.0")
				map_Ns = "map_Ns "  + getAImageFileName(shader.specular_roughness.Sources.Item(0).Parent).replace(folder, "") + "\n";
		
			break; // use only first shader found
		}
		i++;
	}		
	
	return Kd + Ks + Ka + Ns + map_Kd + map_Ks + map_Ka + map_Ns + "\n";
} 


/**** MENTAL RAY *****/

/*
** "apply" callback - 
** converts matDefinition from .mtl file to new xsi material and applies it to objList
** (called from C++ import function)
*/
function ApplyShaderTree_mentalray(matName, matDefinition, objList, folder)
{	
	var matLib = Application.ActiveProject.ActiveScene.ActiveMaterialLibrary;	
	var newMaterial = matLib.CreateMaterial( "Phong", matName );
	var newPhong = newMaterial.Shaders.Item(0);
	
	if(matDefinition.Kd) 
		applyColor(newPhong.diffuse, matDefinition.Kd);
	if(matDefinition.map_Kd) 
		applySIImage(newPhong.diffuse, folder + matDefinition.map_Kd, newMaterial);
	
	if(matDefinition.Ks) 	
		applyColor(newPhong.specular, matDefinition.Ks);
	if(matDefinition.map_Ks) 
		applySIImage(newPhong.specular, folder + matDefinition.map_Ks, newMaterial);
	
	if(matDefinition.Ka) 
		applyColor(newPhong.ambient, matDefinition.Ka);
	if(matDefinition.map_Ka) 
		applySIImage(newPhong.ambient, folder + matDefinition.map_Ka, newMaterial);
			
	if(matDefinition.Ns)
		newPhong.shiny.Value = matDefinition.Ns;	
	if(matDefinition.map_Ns) 
		applySIImage(newPhong.shiny, folder + matDefinition.map_Ns, newMaterial);
	
	if(matDefinition.Kt)
		applyColor(newPhong.transparency, matDefinition.Kt);
	
	if(matDefinition.map_Kt) 
		applySIImage(newPhong.transparency, folder + matDefinition.map_Kt, newMaterial);	
	if(matDefinition.Kr)
		applyColor(newPhong.reflectivity, matDefinition.Kr);		
	if(matDefinition.map_Kr) 
		applySIImage(newPhong.reflectivity, folder + matDefinition.map_Kr, newMaterial);
		
	SIAssignMaterial(objList, newMaterial);	
} 

/*
** "parse" callback - 
** parses xsi material tree returns a textual .mat file material definition like e.g. "Kd 1 1 1\nKs ...." without the "newmtl" line
** (called from C++ export function)
*/
function ParseShaderTree_mentalray(instance, material, folder)
{
	var Kd = "Kd 1 1 1\n"; var map_Kd = "";
	var Kt = ""; var map_Kt = "";
	var Kr = ""; var map_Kr = "";
	var Ks = ""; var map_Ks = "";
	var Ka = ""; var map_Ka = "";
	var Ns = ""; var map_Ns = ""; 		
	
	var i=0;
	while(i < material.Shaders.Count) {
	
		var shader = material.Shaders.Item(i);
		
		if(	shader.ProgID ==  "Softimage.material-phong.1.0" || 
			shader.ProgID == "Softimage.material-lambert.1.0" || 
			shader.ProgID == "Softimage.material-blinn.1.0")
		{
			Kd = "Kd "  + round6(shader.diffuse.parameters("red").Value) + " " + round6(shader.diffuse.parameters('green').Value) + " " + round6(shader.diffuse.parameters('blue').Value) + "\n";			
			if(shader.diffuse.Sources.Count != 0 && shader.diffuse.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0")
				map_Kd = "map_Kd "  + getSIImageFileName(shader.diffuse.Sources.Item(0).Parent).replace(folder, "") + "\n";

			Ka = "Ka "  + round6(shader.ambient.parameters("red").Value) + " " + round6(shader.ambient.parameters('green').Value) + " " + round6(shader.ambient.parameters('blue').Value) + "\n";	
			if(shader.ambient.Sources.Count != 0 && shader.ambient.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
				map_Ka = "map_Ka " + getSIImageFileName(shader.ambient.Sources.Item(0).Parent).replace(folder, "") + "\n";
					
			if(shader.ProgID != "Softimage.material-lambert.1.0") {
			 logmessage("phong");
				Ks = "Ks "  + round6(shader.specular.parameters("red").Value) + " " + round6(shader.specular.parameters('green').Value) + " " + round6(shader.specular.parameters('blue').Value) + "\n";
				if(shader.specular.Sources.Count != 0 && shader.specular.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
					map_Ks = "map_Ks " + getSIImageFileName(shader.specular.Sources.Item(0).Parent).replace(folder, "") + "\n";			
					
				Kt = "Kt "  + round6(shader.transparency.parameters("red").Value) + " " + round6(shader.transparency.parameters('green').Value) + " " + round6(shader.transparency.parameters('blue').Value) + "\n";
				if(shader.transparency.Sources.Count != 0 && shader.transparency.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
					map_Kt = "map_Kt " + getSIImageFileName(shader.transparency.Sources.Item(0).Parent).replace(folder, "") + "\n";
				if(Kt == "Kt 0 0 0\n" && map_Kt == "")
					Kt = "";
					
				Kr = "Kr "  + round6(shader.reflectivity.parameters("red").Value) + " " + round6(shader.reflectivity.parameters('green').Value) + " " + round6(shader.reflectivity.parameters('blue').Value) + "\n";
				if(shader.reflectivity.Sources.Count != 0 && shader.reflectivity.Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
					map_Kr = "map_Kr " + getSIImageFileName(shader.reflectivity.Sources.Item(0).Parent).replace(folder, "") + "\n";			
				if(Kr == "Kr 0 0 0\n" && map_Kr == "")
					Kr = "";				
			}	
			
			if(shader.ProgID ==  "Softimage.material-phong.1.0") {
				Ns = "Ns " + shader.parameters('shiny').Value + "\n";
				if( shader.parameters('shiny').Sources.Count != 0 && shader.parameters('shiny').Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0") 
					map_Ns = "map_Ns " + getSIImageFileName(shader.parameters('shiny').Sources.Item(0).Parent).replace(folder, "") + "\n";
			}

			if(shader.ProgID == "Softimage.material-blinn.1.0") {
				Ns = "Ns " + shader.parameters('roughness').Value + "\n";
				if(shader.parameters('roughness').Sources.Count != 0 && shader.parameters('roughness').Sources.Item(0).Parent.ProgID == "Softimage.txt2d-image-explicit.1.0")
					map_Ns = "map_Ns " + getSIImageFileName(shader.parameters('roughness').Sources.Item(0).Parent).replace(folder, "") + "\n";
			}
			
			break; // use only first shader found
		}
		i++;
	}		
	
	return Kd + Ks + Ka + Ns + Kr + Kt + map_Kd + map_Ks + map_Ka + map_Ns + map_Kr + map_Kt + "\n";
}  



/*************
** 
** part 2 - Plugin
**
**************/

   
function XSILoadPlugin( in_reg )
{
	in_reg.Author = "Admin";
	in_reg.Name = "Additional File Formats JS Plug-in";
	in_reg.Major = 1;
	in_reg.Minor = 0;

	in_reg.RegisterProperty ("FileFormatOptions");
	in_reg.RegisterEvent ("FileFormatOptions_OnStartup", siOnStartup);
	in_reg.RegisterMenu (siMenuMainFileImportID,"ShowImportPLYFileBrowser_Menu", false, false);
	in_reg.RegisterMenu (siMenuMainFileImportID,"ShowImportSTLFileBrowser_Menu", false, false);
	in_reg.RegisterMenu (siMenuMainFileExportID,"ShowExportSTLFileBrowser_Menu", false, false);
	in_reg.RegisterMenu (siMenuMainFileImportID,"ShowImportOBJFileBrowser_Menu", false, false);
	in_reg.RegisterMenu (siMenuMainFileExportID,"ShowExportOBJFileBrowser_Menu", false, false);
	in_reg.RegisterMenu (siMenuMainFileExportID,"ShowOptions_Menu", false, false);
	in_reg.RegisterMenu (siMenuMainFileImportID,"ShowOptions_Menu", false, false);
	
	// this is used as a callback, called from inside cpp command ImportOBJ
	in_reg.RegisterCommand ("ParseShaderTree", "ParseShaderTree");
	in_reg.RegisterCommand ("ApplyShaderTree", "ApplyShaderTree");
	 
	return true; 
}

function XSIUnloadPlugin( in_reg )
{
	var strPluginName = in_reg.Name;
	Application.LogMessage (strPluginName + " has been unloaded.", siVerbose);
	
	return true;
}
   
function ParseShaderTree_Init( in_ctxt )
{
	var oCmd = in_ctxt.Source;
	oCmd.Description = "";
	oCmd.Arguments.Add("instance");
	oCmd.Arguments.Add("material");
	oCmd.Arguments.Add("folder");
	oCmd.ReturnValue = true;
	
	return true;
}

function ParseShaderTree_Execute( instance, material, folder )
{	
	var strRenderer = String(GetCurrentPass().Renderer).replace(" ", "").toLowerCase();

		
	try {
		return eval("ParseShaderTree_" + strRenderer + "(instance, material, folder);"); // applies material to element	
	}
	catch(e)
	{
		if(strRenderer != "" && e.number>>>0 == 0x1111800A138F) {
			LogMessage("AdditionalFileFormats.js: Callback 'ParseShaderTree_" + strRenderer + "()' not implemented for Current Pass Renderer - Default Materials will be written.", siWarningMsg);
			return "Kd 1 1 1\nKs 0.5 0.5 0.5\nKa 0 0 0\n";
		}
		else
			throw(e);
	}
}

function ApplyShaderTree_Init( in_ctxt )
{
	var oCmd = in_ctxt.Source;
	oCmd.Description = "";
	oCmd.Arguments.Add("matName");
	oCmd.Arguments.Add("matDefinitionAsJson");
	oCmd.Arguments.Add("objList");
	oCmd.Arguments.Add("folder");
	oCmd.ReturnValue = true;
	
	return true;
}

// this is used as a callback, called from inside cpp command ImportOBJ
function ApplyShaderTree_Execute( matName, matDefinitionAsJson, objList, folder )
{
	var strRenderer = String(GetCurrentPass().Renderer).replace(" ", "").toLowerCase();
	
	try {
		eval(matDefinitionAsJson); // evals "material"	
	}
	catch(e)
	{
		LogMessage("Error in Material Definition \"" + matDefinitionAsJson + "\"", siWarningMsg);
		return;
	}

	eval("ApplyShaderTree_" + strRenderer + "(matName, material, objList, folder);"); // applies material to element		
	return;
	
	try {
		eval("ApplyShaderTree_" + strRenderer + "(matName, material, objList, folder);"); // applies material to element	
	}
	catch(e)
	{
		if(strRenderer != "" && e.number>>>0 == 0x800A138F)
			LogMessage("AdditionalFileFormats.js: Callback 'ApplyShaderTree_" + strRenderer + "()' not implemented for Current Pass Renderer - No Materials will be created.", siWarningMsg);
		else
			throw(e);
	}
}
 
function FileFormatOptions_OnStartup_OnEvent(in_ctxt)
{
	var ioPrefs = Application.Preferences.Categories("File Format Options");
	if (!ioPrefs) {
		logmessage ("Additional File Formats JS Plug-in first time run: Installing Custom Preferences");
		var customProperty = ActiveSceneRoot.AddProperty("FileFormatOptions", false, "FileFormatOptions");
		Application.LogMessage ("AdditionalFileFormats plugin: Installing custom preferences");
		InstallCustomPreferences (customProperty, "File Format Options");
	}
	else { 
		Application.LogMessage ("Additional File Formats JS Plug-in: AdditionalFileFormats plugin: Refreshing custom preferences");
		RefreshCustomPreferences();
	} 
	
	return false;
}

function FileFormatOptions_Define( in_ctxt )
{
	var oCustomProperty = in_ctxt.Source;
	 
	//OBJ 
	oCustomProperty.AddParameter3("OBJ_DefaultPath", siString );		
	oCustomProperty.AddParameter3("OBJ_LastUsedPath", siString );
	oCustomProperty.AddParameter3("OBJ_rememberLastUsedPath", siBool, 1, null,  null, false );
	oCustomProperty.AddParameter3("OBJ_WriteMTL", siBool, 0, 0, 1, false );
	oCustomProperty.AddParameter3("OBJ_LocalCoords", siBool, 0, 0, 1, false );
	oCustomProperty.AddParameter3("OBJ_ImportPolypaint", siBool, 1, 0, 1, false );
	oCustomProperty.AddParameter3("OBJ_ImportMask", siBool, 1, 0, 1, false );
	oCustomProperty.AddParameter3("OBJ_ImportUserNormals", siBool, 1, 0, 1, false );
	oCustomProperty.AddParameter3("OBJ_ImportUVs", siBool, 1, 0, 1, false );
	oCustomProperty.AddParameter3("OBJ_CreateObjectsTag", siInt4, 0, 0, 3, false );
	oCustomProperty.AddParameter3("OBJ_CreateClustersTag", siInt4, 2, 0, 3, false );
	  
	//STL
	oCustomProperty.AddParameter3("STL_DefaultPath", siString );		
	oCustomProperty.AddParameter3("STL_LastUsedPath", siString );
	oCustomProperty.AddParameter3("STL_rememberLastUsedPath", siBool, 1, null,  null, false );
	oCustomProperty.AddParameter3("STL_Format", siInt4, 0, 0, 1, false );
	oCustomProperty.AddParameter3("STL_LocalCoords", siBool, 0, 0, 1, false );

	//PLY
	oCustomProperty.AddParameter3("PLY_DefaultPath", siString );
	oCustomProperty.AddParameter3("PLY_LastUsedPath", siString );
	oCustomProperty.AddParameter3("PLY_rememberLastUsedPath", siBool, 1, null,  null, false );
	oCustomProperty.AddParameter3("PLY_Format", siInt4, 0, 0, 1, false );
	oCustomProperty.AddParameter3("PLY_LocalCoords", siBool, 0, 0, 1, false );

	return true;
}

function FileFormatOptions_DefineLayout( in_ctxt )
{
	var oLayout = in_ctxt.Source;
	 
	oLayout.Clear();
	
	oLayout.AddTab ("OBJ");
		
	oLayout.AddGroup ("OBJ Folder");
	
		oItem = oLayout.AddItem( "OBJ_DefaultPath", "Default Path", siControlFolder );
		oItem = oLayout.AddItem ("OBJ_rememberLastUsedPath", "Remember Last Used Path");
		
	oLayout.EndGroup();

	oLayout.AddGroup ("OBJ Export");
	
		oItem = oLayout.AddItem( "OBJ_WriteMTL", "Write MTL File");
		oItem = oLayout.AddItem( "OBJ_LocalCoords", "Use Local Coordinates");
		
	oLayout.EndGroup();
	
	oLayout.AddGroup ("OBJ Import (warning: some combinations don't make sense)");
	
		aComboItems = Array("for each G tag (default)", 0, "for each O tag", 1, "for each USEMTL tag", 2, "Create a Single Object", 3)
		oItem = oLayout.AddEnumControl("OBJ_CreateObjectsTag", aComboItems, "Create an Object")
		
		aComboItems = Array("for each G tag", 0, "for each O tag", 1, "for each USEMTL tag (default)", 2, "Do not create Clusters", 3)
		oItem = oLayout.AddEnumControl("OBJ_CreateClustersTag", aComboItems, "Create a Cluster")		
		
		oItem = oLayout.AddItem( "OBJ_ImportPolypaint", "Import ZBrush Polypaint")
		oItem = oLayout.AddItem( "OBJ_ImportMask", "Import ZBrush Mask")
		oItem = oLayout.AddItem( "OBJ_ImportUserNormals", "Import User Normals")
		oItem = oLayout.AddItem( "OBJ_ImportUVs", "Import UVs")		
		
	oLayout.EndGroup();
	
	oLayout.AddTab ("STL");
	
	oLayout.AddGroup ("Folder");
	
		oItem = oLayout.AddItem( "STL_DefaultPath", "Default Path", siControlFolder );
		oItem = oLayout.AddItem ("STL_rememberLastUsedPath", "Remember Last Used Path");
		
	oLayout.EndGroup();
	
	oLayout.AddGroup ("Import");
	
		oItem = oLayout.AddString( "Description", true, true, 200 );
		
	oLayout.EndGroup();
	
	oLayout.AddGroup ("Export");
		oLayout.AddGroup ("File Format");
		
			oItem = oLayout.AddEnumControl("STL_Format", Array("Binary STL Format", 0, "ASCII STL Format", 1 ), "", siControlRadio );
			oItem.SetAttribute( siUINoLabel, true );
			oItem = oLayout.AddItem( "STL_LocalCoords", "Use Local Coordinates")
			
		oLayout.EndGroup();
	oLayout.EndGroup();
	
	oLayout.AddTab ("PLY");
	
	oLayout.AddGroup ("Folder");
	
		var oItem = oLayout.AddItem( "PLY_DefaultPath", "Default Path", siControlFolder );
		oItem = oLayout.AddItem ("PLY_rememberLastUsedPath", "Remember Last Used Path");
		
	oLayout.EndGroup();
	
	oLayout.AddGroup ("Import");	
	
		oItem = oLayout.AddString( "Description", true, true, 200 );
		
	oLayout.EndGroup();
	
	oLayout.AddGroup ("Export");
		oLayout.AddGroup ("File Format");
			
			var oItem = oLayout.AddEnumControl( "PLY_Format", Array("Binary PLY Format", 0, "ASCII PLY Format", 1 ), "", siControlRadio );
			oItem.SetAttribute( siUINoLabel, true );
			oItem = oLayout.AddItem( "PLY_LocalCoords", "Use Local Coordinates");		
	
		oLayout.EndGroup();
	oLayout.EndGroup();
	
    oLayout.SetAttribute (siUIHelpFile, "<FactoryPath>/Doc/<DocLangPref>/xsidocs.chm::/menubar14.htm");
	
	return true;
}  

function FileFormatOptions_OnInit( )
{	
	FileFormatOptions_ObjImport_rememberLastUsedPath_OnChanged
}

function FileFormatOptions_OnClosed( )
{
	Application.LogMessage ("FileFormatOptions_OnClosed called", siVerbose);
}
 
function FileFormatOptions_ObjImport_rememberLastUsedPath_OnChanged()
{
	PPG.Stl_LastUsedPath.Enable(PPG.Stl_rememberLastUsedPath);
}

function ShowExportSTLFileBrowser( in_ctxt )
{
	var oUIToolkit = new ActiveXObject("XSI.UIToolKit") ;
	var oFileBrowser = oUIToolkit.FileBrowser ;
	oFileBrowser.DialogTitle = "Select STL File to Export to";
	oFileBrowser.InitialDirectory = getInitialDir( "STL" );
	oFileBrowser.Filter = "Stereolithography Format (*.stl)|*.stl||"
	oFileBrowser.ShowSave();
	bExportBinary = (GetValue("preferences.File Format Options.STL_Format") == 0);
	bUseLocalCoords = GetValue("preferences.File Format Options.STL_LocalCoords");
	if (oFileBrowser.FilePathName != "") {
		ExportSTL (Selection, oFileBrowser.FilePathName, bExportBinary, bUseLocalCoords);
		saveInitialDir ("STL", oFileBrowser.FilePath);
	}

	return true;
}

function ShowImportSTLFileBrowser( in_ctxt )
{
	var oUIToolkit = new ActiveXObject("XSI.UIToolKit") ;
	var oFileBrowser = oUIToolkit.FileBrowser ;
	oFileBrowser.DialogTitle = "Select STL File to Import";
	oFileBrowser.InitialDirectory = getInitialDir( "STL" );
	oFileBrowser.Filter = "Stereolithography Format (*.stl)|*.stl||";
	oFileBrowser.ShowOpen();
	if (oFileBrowser.FilePathName != "") {
		ImportSTL (oFileBrowser.FilePathName);
		saveInitialDir ("STL", oFileBrowser.FilePath);
	}

	return true;
}

function ShowExportOBJFileBrowser( in_ctxt )
{
	var oUIToolkit = new ActiveXObject("XSI.UIToolKit") ;
	var oFileBrowser = oUIToolkit.FileBrowser ;
	oFileBrowser.DialogTitle = "Select OBJ File to Export to";
	oFileBrowser.InitialDirectory = getInitialDir( "OBJ" );
	oFileBrowser.Filter = "Wavefront OBJ Format (*.obj)|*.obj||";
	oFileBrowser.ShowSave();
	bUseLocalCoords = GetValue("preferences.File Format Options.OBJ_LocalCoords");
	bWriteMTL = GetValue("preferences.File Format Options.OBJ_WriteMTL");
	if (oFileBrowser.FilePathName != "") {
		ExportOBJ (Selection, oFileBrowser.FilePathName, null, null , bWriteMTL, bUseLocalCoords);
		saveInitialDir ("OBJ", oFileBrowser.FilePath);
	}

	return true;
}

function ShowImportOBJFileBrowser( in_ctxt )
{
	var oUIToolkit = new ActiveXObject("XSI.UIToolKit") ;
	var oFileBrowser = oUIToolkit.FileBrowser ;
	oFileBrowser.DialogTitle = "Select OBJ File to Import";
	oFileBrowser.InitialDirectory = getInitialDir( "OBJ" );
	oFileBrowser.Filter = "Wavefront OBJ Format (*.obj)|*.obj||";
	oFileBrowser.ShowOpen();
	
	var createObjectsTag;
	switch(parseInt(Application.Preferences.GetPreferenceValue("File Format Options.OBJ_CreateObjectsTag")))
	{
		case 1:
			createObjectsTag = "o"; break;
		case 2:
			createObjectsTag = "usemtl"; break;
		case 3:
			createObjectsTag = "______"; break;
		default:
			createObjectsTag = "g"; break;
	}
	
	var createClustersTag;
	switch(parseInt(Application.Preferences.GetPreferenceValue("File Format Options.OBJ_CreateClustersTag")))
	{
		case 0:
			createClustersTag = "g"; break;
		case 1:
			createClustersTag = "o"; break;
		case 3:
			createClustersTag = "______"; break;
		default:
			createClustersTag = "usemtl"; break;
	}

	if (oFileBrowser.FilePathName != "") {
		ImportOBJ (oFileBrowser.FilePathName, 
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportUVs"), 
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportUserNormals"), 
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportMask"), 
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportPolypaint"), 
			createObjectsTag, createClustersTag);
	
		saveInitialDir ("OBJ", oFileBrowser.FilePath);
	}

	return true;
}

function ShowImportPLYFileBrowser( in_ctxt )
{
	var oUIToolkit = new ActiveXObject("XSI.UIToolKit") ;
	var oFileBrowser = oUIToolkit.FileBrowser ;
	oFileBrowser.DialogTitle = "Select PLY File to Import" 
	oFileBrowser.InitialDirectory = getInitialDir( "PLY" ) 
	oFileBrowser.Filter = "PLY Format (*.ply)|*.ply||"
	oFileBrowser.ShowOpen() 
	if (oFileBrowser.FilePathName != "") {
		ImportPLY (oFileBrowser.FilePathName);
		saveInitialDir ("PLY", oFileBrowser.FilePath);
	}

	return true;
}

function ShowFileFormatOptions( in_ctxt )
{
	InspectObj("Preferences.File Format Options");
	return true;
}

function ShowExportSTLFileBrowser_Menu_Init( in_ctxt )
{
	var oMenu = in_ctxt.Source;
	oMenu.AddCallbackItem ("Export STL", "ShowExportSTLFileBrowser");
	return true;
}

function ShowImportSTLFileBrowser_Menu_Init( in_ctxt )
{
	var oMenu = in_ctxt.Source;
	oMenu.AddCallbackItem ("Import STL","ShowImportSTLFileBrowser");
	return true;
}

function ShowExportOBJFileBrowser_Menu_Init( in_ctxt )
{
	var oMenu = in_ctxt.Source;
	oMenu.AddCallbackItem ("Export OBJ","ShowExportOBJFileBrowser");
	return true;
}

function ShowImportOBJFileBrowser_Menu_Init( in_ctxt )
{
	var oMenu = in_ctxt.Source;
	oMenu.AddCallbackItem ("Import OBJ","ShowImportOBJFileBrowser");
	return true;
}

function ShowImportPLYFileBrowser_Menu_Init( in_ctxt )
{
	var oMenu = in_ctxt.Source;
	oMenu.AddCallbackItem ("Import PLY","ShowImportPLYFileBrowser");
	return true;
}

function ShowOptions_Menu_Init( in_ctxt )
{
	var oMenu = in_ctxt.Source;
	oMenu.AddCallbackItem ("File Format Options","ShowFileFormatOptions");
	return true;
}


// ============================= Helpers ============================= 

function round6(x)
{
	return Math.round(x*1000000)/1000000;
}

function getInitialDir( fileTypeEnding )
{
	defaultPath = GetValue("preferences.File Format Options." + fileTypeEnding + "_DefaultPath")
	lastUsedPath = GetValue("preferences.File Format Options." + fileTypeEnding + "_LastUsedPath")
	bRememberLastUsedPath = GetValue("preferences.File Format Options." + fileTypeEnding + "_rememberLastUsedPath")
 
	var initialDir;
	if (bRememberLastUsedPath && lastUsedPath != "")
		initialDir = lastUsedPath;
	else if (defaultPath != "")
		initialDir = defaultPath;
	else 
		initialDir = "Desktop";
	
	defaultPath = GetValue("preferences.File Format Options." + fileTypeEnding + "_DefaultPath");
	
	return initialDir;
}

function saveInitialDir( fileTypeEnding, folder )
{
	var bRememberLastUsedPath = GetValue("preferences.File Format Options." + fileTypeEnding + "_rememberLastUsedPath");
	if(bRememberLastUsedPath) 
		SetValue ("preferences.File Format Options." + fileTypeEnding + "_LastUsedPath", folder);	
}

function applySIImage(colorParameter, texturePath, material)
{
	var newImageNode = CreateShaderFromProgID("Softimage.txt2d-image-explicit.1.0", material, "Image");
	SetInstanceDataValue(null, newImageNode+".tspace_id", "Texture_Projection");
	
	var newClip = SICreateImageClip2(texturePath)(0);	
	newClip.oglmaxsize.Value = 4096;
	
	SIConnectShaderToCnxPoint(newClip, newImageNode+".tex", false);
	SIConnectShaderToCnxPoint(newImageNode+".out", colorParameter, false);	
	
	DisconnectAndDeleteOrUnnestShaders("Clips.noIcon_pic", material);
}

function applyAImage(colorParameter, texturePath, material)
{
	var newImageNode = CreateShaderFromProgID("Arnold.image.1.0", material, "Image");
	newImageNode.filename = texturePath;	
	
	var newClip = SICreateImageClip2(texturePath)(0);
	newClip.oglmaxsize.Value = 4096;	
	
	material.TextureSel.Value = 3;
	material.ImageClipName.Value = String(newClip);
	SetInstanceDataValue(null, material+".UV", "Texture_Projection");
	
	SIConnectShaderToCnxPoint(newImageNode+".out", colorParameter, false);	
}

function getSIImageFileName(siImageNode)
{
	return siImageNode.tex.Sources(0).Source.FileName.Value;				
}

function getAImageFileName(aImageNode)
{
	return aImageNode.filename.Value;				
}

function applyColor(colorParameter, colorArray)
{
	colorParameter.parameters("red").Value = colorArray[0];
	colorParameter.parameters("green").Value = colorArray[1];
	colorParameter.parameters("blue").Value = colorArray[2];
}


