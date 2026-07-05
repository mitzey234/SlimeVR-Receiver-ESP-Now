#include "StatusManager.h"

namespace SlimeVR::Status {
void StatusManager::setStatus(Status status, bool value) {
	if (value) {
		if (m_Status & status) {
			return;
		}

		// Serial.printf("Added status %s\n", statusToString(status));

		m_Status |= status;
	} else {
		if (!(m_Status & status)) {
			return;
		}

		// Serial.printf("Removed status %s\n", statusToString(status));

		m_Status &= ~status;
	}
}

bool StatusManager::hasStatus(Status status) { return (m_Status & status) == status; }
}  // namespace SlimeVR::Status
