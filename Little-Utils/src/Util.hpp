// some utility functions

#include "rack.hpp"

//sgn() function based on https://stackoverflow.com/questions/1903954/is-there-a-standard-sign-function-signum-sgn-in-c-c
inline constexpr
int signum(float val) {
	return (0.f < val) - (val < 0.f);
}
