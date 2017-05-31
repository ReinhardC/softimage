#pragma once

#include <xsi_pluginregistrar.h>
#include <xsi_status.h>
#include <xsi_decl.h>
#include <xsi_math.h>
#include <xsi_comapihandler.h>

#include <xsi_application.h>
#include <xsi_context.h>
#include <xsi_status.h>
#include <xsi_model.h>
#include <xsi_string.h>
#include <xsi_command.h>

#include <sstream>

using namespace XSI;

class CFileBrowser{

protected:
	CComAPIHandler ccomXSIUtils;
	CComAPIHandler fileBrowser;

public:
	CFileBrowser();
	~CFileBrowser();
	CStatus Show(CString strCaption, CString strLastFolderVar, CString strFilter, CString strMode);

	CString filePathName;
	CString filePath;
	CString fileNameWithExt;
	CString fileName;
};
