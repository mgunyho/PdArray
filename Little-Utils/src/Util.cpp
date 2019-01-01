#include "Util.hpp"

// from https://stackoverflow.com/a/12468109
std::string randomString(size_t len) {
	auto randchar = []() -> char {
		const char charset[] = 
			"0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz";
		const size_t end = (sizeof(charset) - 1);
		return charset[rand() % end];
	};
	std::string str(len, 0);
	std::generate_n( str.begin(), len, randchar);
	return str;
}
