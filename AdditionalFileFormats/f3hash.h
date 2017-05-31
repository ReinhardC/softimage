#pragma once

#include <array>

struct f3hash {
	size_t operator()(const std::tr1::array<float, 3> t) const {
		union {
			float f;
			size_t s;
		};
		size_t hash = 0;
		f = t[0]; hash += s;
		f = t[1]; hash += s;
		f = t[2]; hash += s;
		return hash;
	}
}; 
