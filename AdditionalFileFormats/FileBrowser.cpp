#include "FileBrowser.h"

CFileBrowser::CFileBrowser()
{
	this->ccomXSIUtils.CreateInstance(L"XSI.UIToolKit");
	fileBrowser = ccomXSIUtils.GetProperty(L"FileBrowser");
}

CFileBrowser::~CFileBrowser()
{
	ccomXSIUtils.Detach();
	fileBrowser.Detach();
} 

CStatus CFileBrowser::Show(CString strCaption, CString strInitialDirectory, CString strFilter, CString strMode)
{
	Application app;
	Model root = app.GetActiveSceneRoot();

	fileBrowser.PutProperty(L"DialogTitle", strCaption);
	fileBrowser.PutProperty(L"strInitialDirectory", strInitialDirectory!=L""? strInitialDirectory : L"Desktop");
	fileBrowser.PutProperty(L"Filter", strFilter);

	CValue ioID;
	CValueArray inArgs3;
	fileBrowser.Call(strMode, ioID, inArgs3);
	
	filePathName = fileBrowser.GetProperty(L"FilePathName");
	filePath = fileBrowser.GetProperty(L"FilePath");
	fileNameWithExt = fileBrowser.GetProperty(L"FileName");
	fileName = fileNameWithExt.Split(CString(L"."))[0];

	if(filePathName.IsEmpty())
		return CStatus::OK;

	return CStatus::OK;
}
