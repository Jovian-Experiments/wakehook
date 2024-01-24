/* SPDX-License-Identifier: BSD-3-Clause */
#include <stdint.h>
#include <systemd/sd-bus.h>

int cb(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
	sd_bus* session_bus = userdata;
	int sleep;
	int res = sd_bus_message_read_basic(m, 'b', &sleep);
	if (res < 0 || sleep) {
		return res;
	}
	res = sd_bus_call_method(session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "powerOnDevices", ret_error, NULL, "");
	if (res < 0) {
		return res;
	}
	res = sd_bus_call_method(session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "makeActiveSource", ret_error, NULL, "");
	if (res < 0) {
		return res;
	}
	return 0;
}

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused))) {
	int res;
	sd_bus* system_bus = NULL;
	sd_bus* session_bus = NULL;
	sd_bus_slot* slot = NULL;

	res = sd_bus_open_system(&system_bus);
	if (res < 0) {
		goto shutdown;
	}
	res = sd_bus_open_user(&session_bus);
	if (res < 0) {
		goto shutdown;
	}

	res = sd_bus_match_signal(system_bus, &slot, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep", cb, session_bus);
	if (res < 0) {
		goto shutdown;
	}

	while (res >= 0) {
		res = sd_bus_wait(system_bus, UINT64_MAX);
		if (res < 0) {
			continue;
		}
		res = sd_bus_process(system_bus, NULL);
	}

shutdown:
	if (slot) {
		sd_bus_slot_unref(slot);
	}
	if (system_bus) {
		sd_bus_unref(system_bus);
	}
	if (session_bus) {
		sd_bus_unref(session_bus);
	}
	return res ? 1 : 0;
}
