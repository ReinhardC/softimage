#pragma once

#include <array>
#include <cstring>

// hash for float triples (positions, uvs, normals) used as unordered_map keys
struct f3hash {
	size_t operator()(const std::array<float, 3>& t) const {
		size_t hash = 0;
		for (int i = 0; i < 3; i++) {
			float f = t[i] + 0.0f; // -0.0 and 0.0 compare equal, so they must hash equal too
			unsigned int bits;
			std::memcpy(&bits, &f, sizeof(bits));
			hash ^= (size_t)bits + 0x9e3779b9u + (hash << 6) + (hash >> 2);
		}
		return hash;
	}
}; 
