#include <chrono>

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

struct GUITimer {
	// Kinda like PulseGenerator, but uses std::chrono for timing events, since
	// we don't have engineGetSampleTime() for Widget::step().

	//TODO: does rack provide a GUI timer?
	bool status = false; // true == high, false == low
	std::chrono::steady_clock::time_point finishTime;

	void trigger(float duration) {
		// duration in seconds, will be rounded to ms
		finishTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(int(duration * 1000.f));
		status = true;
	}

	bool process() {
		// proecss() takes no parameters, use internal clock to figure out when to go from high to low
		if(status) {
			status = std::chrono::steady_clock::now() < finishTime;
		}

		return status;
	}

	void reset() { status = false; }
};
