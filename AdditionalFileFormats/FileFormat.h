#pragma once

#include <xsi_string.h>
#include <xsi_application.h>
#include <xsi_uitoolkit.h>
#include <xsi_progressbar.h>
#include <xsi_primitive.h>
#include <xsi_polygonmesh.h>
#include <xsi_x3dobject.h>
#include <xsi_material.h>
 
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <string>
#include <sstream>
#include <vector>
#include <iterator>

#include "f3hash.h"

using namespace XSI;
using namespace std;

template<typename Out>
void split(const string &s, char delim, Out result) {
	stringstream ss;
	ss.str(s);
	string item;
	while (getline(ss, item, delim)) {
		*(result++) = item;
	}
}

class CFileFormat 
{
public:

	CFileFormat() {
		Application app;

		// set up a progress bar	
		UIToolkit kit = app.GetUIToolkit();

		m_progress = kit.GetProgressBar();
		m_progress.PutStep(1);
	}

	virtual string getFormatName() = 0;

	CRef getVertexColorProperty(X3DObject xobj);

protected:
	void initStrings(string in_filePathName) {

		vector<std::string> tokens = split(in_filePathName, '\\');		
		
		m_filePath = "";
		for (auto i = 0; i < tokens.size() - 1; i++)
			m_filePath += tokens[i] + "\\";
		m_filePathName = in_filePathName;
		m_fileNameWithExt = tokens[tokens.size() - 1];

		vector<std::string> filenameAndExt = split(m_fileNameWithExt, '.');

		m_fileName = filenameAndExt[0];
	}


	// trim from start
	static inline string &CFileFormat::ltrim(string &s) {
		s.erase(s.begin(), find_if(s.begin(), s.end(),
			not1(ptr_fun<int, int>(isspace))));
		return s;
	}

	// trim from end
	static inline string &CFileFormat::rtrim(string &s) {
		s.erase(find_if(s.rbegin(), s.rend(),
			not1(ptr_fun<int, int>(isspace))).base(), s.end());
		return s;
	}

	// trim from both ends
	static inline string &CFileFormat::trim(string &s) {
		return ltrim(rtrim(s));
	}

	vector<string> CFileFormat::split(const string &s, char delim) {
		vector<string> elems;
		::split(s, delim, back_inserter(elems));
		return elems;
	}

	string m_filePathName;
	string m_filePath;
	string m_fileNameWithExt;
	string m_fileName;

	ProgressBar m_progress;
};