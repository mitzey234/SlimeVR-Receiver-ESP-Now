#include "Status.h"

namespace SlimeVR::Status {
const char* statusToString(Status status) {
	switch (status) {
		case LOADING:
			return "LOADING";
		case PAIRING_MODE:
			return "PAIRING_MODE";
		case READY:
			return "READY";
		default:
			return "UNKNOWN";
	}
}
}  // namespace SlimeVR::Status
