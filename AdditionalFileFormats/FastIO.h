#pragma once

// Fast text parsing and buffered text output for the mesh file formats. The C runtime's
// atof/strtod and to_string/sprintf were the biggest cost of the OBJ and ASCII STL paths.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>

namespace fastio {

// exact powers of ten up to 1e22 (all exactly representable as doubles)
inline double pow10exact(int e)
{
	static const double table[] = { 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11,
		1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22 };
	return e <= 22 ? table[e] : pow(10.0, e);
}

// Parses [-+]digits[.digits][e[-+]digits] like strtod; leading blanks are skipped. Anything else
// (inf, nan, hex floats) is handed to strtod. For inputs with up to 15 significant digits and a
// moderate exponent the result equals strtod's (one correctly rounded operation on exact values).
inline double parseDouble(const char* s, const char** end = NULL)
{
	const char* p = s;
	while (*p == ' ' || *p == '\t')
		p++;

	bool bNegative = false;
	if (*p == '-') { bNegative = true; p++; }
	else if (*p == '+') p++;

	unsigned long long mantissa = 0;
	int nbDigits = 0, exponent = 0;
	bool bAnyDigit = false;

	while (*p >= '0' && *p <= '9') {
		if (nbDigits < 19) { mantissa = mantissa * 10 + (*p - '0'); nbDigits++; }
		else exponent++;
		p++;
		bAnyDigit = true;
	}
	if (*p == '.') {
		p++;
		while (*p >= '0' && *p <= '9') {
			if (nbDigits < 19) { mantissa = mantissa * 10 + (*p - '0'); nbDigits++; exponent--; }
			p++;
			bAnyDigit = true;
		}
	}
	if (!bAnyDigit) {
		char* e = NULL;
		double v = strtod(s, &e);
		if (end) *end = e;
		return v;
	}
	if (*p == 'e' || *p == 'E') {
		const char* q = p + 1;
		bool bExpNegative = false;
		if (*q == '-') { bExpNegative = true; q++; }
		else if (*q == '+') q++;
		if (*q >= '0' && *q <= '9') {
			int e = 0;
			while (*q >= '0' && *q <= '9') { if (e < 10000) e = e * 10 + (*q - '0'); q++; }
			exponent += bExpNegative ? -e : e;
			p = q;
		}
	}
	if (end) *end = p;

	double v = (double)mantissa;
	if (exponent > 0) v *= pow10exact(exponent);
	else if (exponent < 0) v /= pow10exact(-exponent);
	return bNegative ? -v : v;
}

// Parses [-+]digits; leading blanks are skipped; stops at the first other character (like atol).
inline long parseLong(const char* s, const char** end = NULL)
{
	const char* p = s;
	while (*p == ' ' || *p == '\t')
		p++;
	bool bNegative = false;
	if (*p == '-') { bNegative = true; p++; }
	else if (*p == '+') p++;
	long v = 0;
	while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
	if (end) *end = p;
	return bNegative ? -v : v;
}

// writes v as decimal text into out (no terminator); returns the length
inline int formatLong(long long v, char* out)
{
	char tmp[24];
	int n = 0;
	unsigned long long u = v < 0 ? (unsigned long long)(-(v + 1)) + 1 : (unsigned long long)v;
	do { tmp[n++] = (char)('0' + u % 10); u /= 10; } while (u);
	int len = 0;
	if (v < 0) out[len++] = '-';
	while (n) out[len++] = tmp[--n];
	return len;
}

// writes v with a fixed number of decimals (like "%.6f") into out (no terminator); returns the length.
// Values too large for the integer path and non-finite values go through snprintf.
inline int formatFixed(double v, int decimals, char* out)
{
	double scale = pow10exact(decimals);
	double a = fabs(v);
	if (!(a < 1e15)) // also catches inf and nan
		return snprintf(out, 48, "%.*f", decimals, v);

	unsigned long long scaled = (unsigned long long)(a * scale + 0.5);
	unsigned long long intPart = scaled / (unsigned long long)scale;
	unsigned long long fracPart = scaled % (unsigned long long)scale;

	int len = 0;
	if (v < 0 && scaled != 0)
		out[len++] = '-';
	len += formatLong((long long)intPart, out + len);
	if (decimals > 0) {
		out[len++] = '.';
		for (int i = decimals - 1; i >= 0; i--) {
			out[len + i] = (char)('0' + fracPart % 10);
			fracPart /= 10;
		}
		len += decimals;
	}
	return len;
}

// Text output through a large buffer: numbers are formatted in place, the file gets 1 MB writes.
// Call flush() (or let the destructor do it) before closing the file.
class CTextWriter {
public:
	explicit CTextWriter(FILE* file, size_t bufferSize = 1 << 20) : m_file(file), m_bufferSize(bufferSize) { m_buf.reserve(bufferSize + 64); }
	~CTextWriter() { flush(); }

	void put(const char* s) { m_buf.append(s); checkFlush(); }
	void put(const std::string& s) { m_buf.append(s); checkFlush(); }
	void put(char c) { m_buf.push_back(c); }
	void putLong(long long v) { char tmp[24]; m_buf.append(tmp, formatLong(v, tmp)); }
	void putFixed(double v, int decimals = 6) { char tmp[64]; m_buf.append(tmp, formatFixed(v, decimals, tmp)); checkFlush(); }

	void flush()
	{
		if (!m_buf.empty() && m_file)
			fwrite(m_buf.data(), 1, m_buf.size(), m_file);
		m_buf.clear();
	}

private:
	void checkFlush() { if (m_buf.size() >= m_bufferSize) flush(); }

	FILE* m_file;
	size_t m_bufferSize;
	std::string m_buf;
};

} // namespace fastio
