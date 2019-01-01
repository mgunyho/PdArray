// some utility functions

#include "rack.hpp"
#include <algorithm> // std::generate_n

using namespace rack;

//sgn() function based on https://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
inline constexpr
int signum(float val) {
	return (0.f < val) - (val < 0.f);
}

// Helper function for adding a small LED to the upper right corner of a port
// usage in module widget constructor:
// addChild(createTinyLightForPort(position_of_port_center, ... other params as in createLightCentered() ...))
template <class TLightColor>
TinyLight<TLightColor> *createTinyLightForPort(Vec portCenterPos, Module *module, int firstLightId) {
	constexpr float offset = 3.6f;
	return createLightCentered<TinyLight<TLightColor>>(
			portCenterPos.plus(Vec(15 - offset, offset - 15)),
			module, firstLightId);
}

// generate random alphanumeric string
std::string randomString(size_t len);
