#ifndef STATUS_STATUS_H
#define STATUS_STATUS_H

namespace SlimeVR::Status {
enum Status {
	LOADING = 1 << 0,
	PAIRING_MODE = 1 << 1,
	READY = 1 << 2,
	RESETTING = 1 << 3,
};

const char* statusToString(Status status);
}  // namespace SlimeVR::Status

#endif
