
function XSILoadPlugin( in_reg ) 
	in_reg.Author = "Admin"
	in_reg.Name = "Additional File Formats VBS Plug-in"
	in_reg.Major = 1
	in_reg.Minor = 0 

	in_reg.RegisterProperty "FileFormatOptions"
	in_reg.RegisterEvent "FileFormatOptions_OnStartup", siOnStartup	
	in_reg.RegisterMenu siMenuMainFileImportID,"ShowImportPLYFileBrowser_Menu",false,false
	in_reg.RegisterMenu siMenuMainFileImportID,"ShowImportSTLFileBrowser_Menu",false,false
	in_reg.RegisterMenu siMenuMainFileExportID,"ShowExportSTLFileBrowser_Menu",false,false
	in_reg.RegisterMenu siMenuMainFileImportID,"ShowImportOBJFileBrowser_Menu",false,false
	in_reg.RegisterMenu siMenuMainFileExportID,"ShowExportOBJFileBrowser_Menu",false,false
	in_reg.RegisterMenu siMenuMainFileExportID,"ShowOptions_Menu",false,false
	in_reg.RegisterMenu siMenuMainFileImportID,"ShowOptions_Menu",false,false
	 
	XSILoadPlugin = true
end function 
 
function FileFormatOptions_OnStartup_OnEvent(in_ctxt)
	set ioPrefs = Application.Preferences.Categories("File Format Options")

	if ioPrefs is Nothing then  
		logmessage "Installing Custom Preferences"
		set customProperty = ActiveSceneRoot.AddProperty("FileFormatOptions", false, "FileFormatOptions")
		Application.LogMessage "AdditionalFileFormats plugin: Installing custom preferences"
		InstallCustomPreferences customProperty, "File Format Options"
	else
		Application.LogMessage "AdditionalFileFormats plugin: Refreshing custom preferences"
		RefreshCustomPreferences 
	end if
	FileFormatOptionsOptions_OnStartup_OnEvent = false
end function

function XSIUnloadPlugin( in_reg )

	dim strPluginName
	strPluginName = in_reg.Name
	Application.LogMessage strPluginName & " has been unloaded.",siVerbose
	XSIUnloadPlugin = true
end function
 
function FileFormatOptions_Define( in_ctxt )

	dim oCustomProperty
	set oCustomProperty = in_ctxt.Source
	 
	'OBJ 
	call oCustomProperty.AddParameter3( "OBJ_DefaultPath", siString )		
	call oCustomProperty.AddParameter3( "OBJ_LastUsedPath", siString )
	call oCustomProperty.AddParameter3( "OBJ_rememberLastUsedPath", siBool, 1, , , false ) 
	call oCustomProperty.AddParameter3( "OBJ_WriteMTL", siBool, 0, 0, 1, false )	
	call oCustomProperty.AddParameter3( "OBJ_LocalCoords", siBool, 0, 0, 1, false )
	call oCustomProperty.AddParameter3( "OBJ_ImportPolypaint", siBool, 1, 0, 1, false )
	call oCustomProperty.AddParameter3( "OBJ_ImportMask", siBool, 1, 0, 1, false )
	call oCustomProperty.AddParameter3( "OBJ_ImportUserNormals", siBool, 1, 0, 1, false )
	call oCustomProperty.AddParameter3( "OBJ_ImportUVs", siBool, 1, 0, 1, false )
	call oCustomProperty.AddParameter3( "OBJ_CreateObjectsTag", siInt4, 0, 0, 3, false )
	call oCustomProperty.AddParameter3( "OBJ_CreateClustersTag", siInt4, 2, 0, 3, false )
	
	'STL
	call oCustomProperty.AddParameter3( "STL_DefaultPath", siString )		
	call oCustomProperty.AddParameter3( "STL_LastUsedPath", siString )
	call oCustomProperty.AddParameter3( "STL_rememberLastUsedPath", siBool, 1, , , false ) 
	call oCustomProperty.AddParameter3( "STL_Format", siInt4, 0, 0, 1, false )	
	call oCustomProperty.AddParameter3( "STL_LocalCoords", siBool, 0, 0, 1, false )	

	'PLY
	call oCustomProperty.AddParameter3( "PLY_DefaultPath", siString )		
	call oCustomProperty.AddParameter3( "PLY_LastUsedPath", siString )
	call oCustomProperty.AddParameter3( "PLY_rememberLastUsedPath", siBool, 1, , , false ) 
	call oCustomProperty.AddParameter3( "PLY_Format", siInt4, 0, 0, 1, false )	
	call oCustomProperty.AddParameter3( "PLY_LocalCoords", siBool, 0, 0, 1, false )	

	FileFormatOptions_Define = true
end function

function FileFormatOptions_DefineLayout( in_ctxt )

	dim oLayout,oItem
	set oLayout = in_ctxt.Source
	 
	oLayout.Clear 
	

		
	oLayout.AddTab "OBJ" 
		
	oLayout.AddGroup "OBJ Folder"
		set oItem = oLayout.AddItem( "OBJ_DefaultPath", "Default Path", siControlFolder ) 
		rem set oItem = oLayout.AddItem( "STL_LastUsedPath", "Last Used Path", siControlFolder ) 
		set oItem = oLayout.AddItem ("OBJ_rememberLastUsedPath", "Remember Last Used Path")
	oLayout.EndGroup

	oLayout.AddGroup "OBJ Export"
		set oItem = oLayout.AddItem( "OBJ_WriteMTL", "Write MTL File")
		set oItem = oLayout.AddItem( "OBJ_LocalCoords", "Use Local Coordinates")			
	oLayout.EndGroup
	
	oLayout.AddGroup "OBJ Import (warning: some combinations don't make sense)"
		aComboItems = Array("for each G tag (default)", 0, "for each O tag", 1, "for each USEMTL tag", 2, "Create a Single Object", 3)
		set oItem = oLayout.AddEnumControl("OBJ_CreateObjectsTag", aComboItems, "Create an Object")
		aComboItems = Array("for each G tag", 0, "for each O tag", 1, "for each USEMTL tag (default)", 2, "Do not create Clusters", 3)
		set oItem = oLayout.AddEnumControl("OBJ_CreateClustersTag", aComboItems, "Create a Cluster")		
		set oItem = OLayout.AddItem( "OBJ_ImportPolypaint", "Import ZBrush Polypaint")
		set oItem = OLayout.AddItem( "OBJ_ImportMask", "Import ZBrush Mask")
		set oItem = OLayout.AddItem( "OBJ_ImportUserNormals", "Import User Normals")
		set oItem = OLayout.AddItem( "OBJ_ImportUVs", "Import UVs")		
	oLayout.EndGroup
	
	oLayout.AddTab "STL"	
	
	oLayout.AddGroup "Folder"
		set oItem = oLayout.AddItem( "STL_DefaultPath", "Default Path", siControlFolder ) 		
		rem set oItem = oLayout.AddItem( "STL_LastUsedPath", "Last Used Path", siControlFolder ) 
		set oItem = oLayout.AddItem ("STL_rememberLastUsedPath", "Remember Last Used Path")
	oLayout.EndGroup
	
	oLayout.AddGroup "Import"	 		
		set oItem = oLayout.AddString( "Description",  ,true, 200 )
	oLayout.EndGroup
	
	oLayout.AddGroup "Export"
		oLayout.AddGroup "File Format"
			set oItem = oLayout.AddEnumControl( "STL_Format", _
				Array("Binary STL Format", 0,_
					 "ASCII STL Format", 1 ), "", siControlRadio )
			call oItem.SetAttribute( siUINoLabel, true )
			set oItem = oLayout.AddItem( "STL_LocalCoords", "Use Local Coordinates")
		
		oLayout.EndGroup
	oLayout.EndGroup
	
		
	oLayout.AddTab "PLY"	
	
	oLayout.AddGroup "Folder"
		set oItem = oLayout.AddItem( "PLY_DefaultPath", "Default Path", siControlFolder ) 		
		rem set oItem = oLayout.AddItem( "STL_LastUsedPath", "Last Used Path", siControlFolder ) 
		set oItem = oLayout.AddItem ("PLY_rememberLastUsedPath", "Remember Last Used Path")
	oLayout.EndGroup
	
	oLayout.AddGroup "Import"	 		
		set oItem = oLayout.AddString( "Description",  ,true, 200 )
	oLayout.EndGroup
	
	oLayout.AddGroup "Export"
		oLayout.AddGroup "File Format"
			set oItem = oLayout.AddEnumControl( "PLY_Format", _
				Array("Binary PLY Format", 0,_
					 "ASCII PLY Format", 1 ), "", siControlRadio )
			call oItem.SetAttribute( siUINoLabel, true )
			set oItem = oLayout.AddItem( "PLY_LocalCoords", "Use Local Coordinates")
		
		oLayout.EndGroup
	oLayout.EndGroup

	
    oLayout.SetAttribute siUIHelpFile, "<FactoryPath>/Doc/<DocLangPref>/xsidocs.chm::/menubar14.htm"
	
	FileFormatOptions_DefineLayout = true
end function  

function FileFormatOptions_OnInit( )	
	FileFormatOptions_ObjImport_rememberLastUsedPath_OnChanged
end function

function FileFormatOptions_OnClosed( )
	Application.LogMessage "FileFormatOptions_OnClosed called",siVerbose
end function
 
function FileFormatOptions_ObjImport_rememberLastUsedPath_OnChanged()
	PPG.Stl_LastUsedPath.Enable(PPG.Stl_rememberLastUsedPath) 	
end function

function ShowExportSTLFileBrowser( in_ctxt )

	set oFileBrowser = XSIUIToolkit.FileBrowser
	oFileBrowser.DialogTitle = "Select STL File to Export to" 
	oFileBrowser.InitialDirectory = getInitialDir( "STL" ) 
	oFileBrowser.Filter = "Stereolithography Format (*.stl)|*.stl||"
	oFileBrowser.ShowSave() 
	bExportBinary = (GetValue("preferences.File Format Options.STL_Format") = 0)
	bUseLocalCoords = GetValue("preferences.File Format Options.STL_LocalCoords")
	if oFileBrowser.FilePathName <> ""  then
		ExportSTL Selection, oFileBrowser.FilePathName, bExportBinary, bUseLocalCoords
		saveInitialDir "STL", oFileBrowser.FilePath
	end if

	ShowExportSTLFileBrowser = true
end function

function ShowImportSTLFileBrowser( in_ctxt )
	
	set oFileBrowser = XSIUIToolkit.FileBrowser
	oFileBrowser.DialogTitle = "Select STL File to Import" 
	oFileBrowser.InitialDirectory = getInitialDir( "STL" ) 
	oFileBrowser.Filter = "Stereolithography Format (*.stl)|*.stl||"
	oFileBrowser.ShowOpen() 
	if oFileBrowser.FilePathName <> ""  then
		ImportSTL oFileBrowser.FilePathName
		saveInitialDir "STL", oFileBrowser.FilePath
	end if

	ShowImportSTLFileBrowser = true
end function

function ShowExportOBJFileBrowser( in_ctxt )
	
	set oFileBrowser = XSIUIToolkit.FileBrowser
	oFileBrowser.DialogTitle = "Select OBJ File to Export to" 
	oFileBrowser.InitialDirectory = getInitialDir( "OBJ" ) 
	oFileBrowser.Filter = "Wavefront OBJ Format (*.obj)|*.obj||"
	oFileBrowser.ShowSave() 
	bUseLocalCoords = GetValue("preferences.File Format Options.OBJ_LocalCoords")
	bWriteMTL = GetValue("preferences.File Format Options.OBJ_WriteMTL")
	if oFileBrowser.FilePathName <> ""  then
		ExportOBJ Selection, oFileBrowser.FilePathName, , , bWriteMTL, bUseLocalCoords
		saveInitialDir "OBJ", oFileBrowser.FilePath
	end if

	ShowExportOBJFileBrowser = true
end function

function ShowImportOBJFileBrowser( in_ctxt )
	
	set oFileBrowser = XSIUIToolkit.FileBrowser
	oFileBrowser.DialogTitle = "Select OBJ File to Import" 
	oFileBrowser.InitialDirectory = getInitialDir( "OBJ" ) 
	oFileBrowser.Filter = "Wavefront OBJ Format (*.obj)|*.obj||"
	oFileBrowser.ShowOpen() 
	if oFileBrowser.FilePathName <> ""  then
		ImportOBJ oFileBrowser.FilePathName, _
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportUVs"), _
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportUserNormals"), _
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportMask"), _
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_ImportPolypaint"), _
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_CreateObjectsTag"), _
			Application.Preferences.GetPreferenceValue("File Format Options.OBJ_CreateClustersTag")
	
		saveInitialDir "OBJ", oFileBrowser.FilePath
	end if

	ShowImportOBJFileBrowser = true
end function

function ShowImportPLYFileBrowser( in_ctxt )
	
	set oFileBrowser = XSIUIToolkit.FileBrowser
	oFileBrowser.DialogTitle = "Select PLY File to Import" 
	oFileBrowser.InitialDirectory = getInitialDir( "PLY" ) 
	oFileBrowser.Filter = "PLY Format (*.ply)|*.ply||"
	oFileBrowser.ShowOpen() 
	if oFileBrowser.FilePathName <> ""  then
		ImportPLY oFileBrowser.FilePathName
		saveInitialDir "PLY", oFileBrowser.FilePath
	end if

	ShowImportPLYFileBrowser = true
end function

function ShowFileFormatOptions( in_ctxt )
	InspectObj("Preferences.File Format Options")
	ShowFileFormatOptions = true
end function

function ShowExportSTLFileBrowser_Menu_Init( in_ctxt )

	set oMenu = in_ctxt.Source
	oMenu.AddCallbackItem "Export STL","ShowExportSTLFileBrowser"
	ShowExportSTLFileBrowser_Menu_Init = true
end function

function ShowImportSTLFileBrowser_Menu_Init( in_ctxt )

	set oMenu = in_ctxt.Source
	oMenu.AddCallbackItem "Import STL","ShowImportSTLFileBrowser"
	ShowImportSTLFileBrowser_Menu_Init = true
end function

function ShowExportOBJFileBrowser_Menu_Init( in_ctxt )

	set oMenu = in_ctxt.Source
	oMenu.AddCallbackItem "Export OBJ","ShowExportOBJFileBrowser"
	ShowExportOBJFileBrowser_Menu_Init = true
end function

function ShowImportOBJFileBrowser_Menu_Init( in_ctxt )

	set oMenu = in_ctxt.Source
	oMenu.AddCallbackItem "Import OBJ","ShowImportOBJFileBrowser"
	ShowImportOBJFileBrowser_Menu_Init = true
end function

function ShowImportPLYFileBrowser_Menu_Init( in_ctxt )

	set oMenu = in_ctxt.Source
	oMenu.AddCallbackItem "Import PLY","ShowImportPLYFileBrowser"
	ShowImportPLYFileBrowser_Menu_Init = true
end function

function ShowOptions_Menu_Init( in_ctxt )

	set oMenu = in_ctxt.Source
	oMenu.AddCallbackItem "File Format Options","ShowFileFormatOptions"
	ShowOptions_Menu_Init = true
end function


REM ============================= Helpers ============================= 

function getInitialDir( fileTypeEnding )
	defaultPath = GetValue("preferences.File Format Options." & fileTypeEnding & "_DefaultPath")
	lastUsedPath = GetValue("preferences.File Format Options." & fileTypeEnding & "_LastUsedPath")
	bRememberLastUsedPath = GetValue("preferences.File Format Options." & fileTypeEnding & "_rememberLastUsedPath")
 
	dim initialDir
	if bRememberLastUsedPath and lastUsedPath <> "" then
		initialDir = lastUsedPath
	elseif defaultPath <> "" then
		initialDir = defaultPath
	else 
		initialDir = "Desktop"
	end if
	defaultPath = GetValue("preferences.File Format Options." & fileTypeEnding & "_DefaultPath")
	
	logmessage defaultPath
	getInitialDir = initialDir
end function

function saveInitialDir( fileTypeEnding, folder )
	bRememberLastUsedPath = GetValue("preferences.File Format Options." & fileTypeEnding & "_rememberLastUsedPath")
	if(bRememberLastUsedPath) then
		SetValue "preferences.File Format Options." & fileTypeEnding & "_LastUsedPath", folder
	end if	
end function


