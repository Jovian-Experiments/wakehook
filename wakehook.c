/* SPDX-License-Identifier: BSD-3-Clause */
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#define MAX_TRIES 5

struct Context {
	sd_bus* system_bus;
	sd_bus* session_bus;
	bool do_activate;
};

#define LOG(FMT, ...) \
	do { \
		struct timespec ts; \
		clock_gettime(CLOCK_REALTIME, &ts); \
		fprintf(stderr, "[wakehook] %lli.%li: " FMT "\n", (long long) ts.tv_sec, (long) ts.tv_nsec / 1000, __VA_ARGS__); \
	} while (0)

int activate_cb(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
	struct Context* ctx = userdata;
	int is_active;
	int res = sd_bus_message_read_basic(m, 'b', &is_active);
	if (res < 0) {
		LOG("Error reading activation message: %i %s", res, strerror(-res));
		return res;
	}
	LOG("Activated, is current? %i", is_active);
	if (!ctx->do_activate) {
		return res;
	}

	if (is_active) {
		ctx->do_activate = false;
		return 0;
	}
	res = sd_bus_call_method(ctx->session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "makeActiveSource", ret_error, NULL, "");
	if (res < 0) {
		LOG("Failed to call org.kde.plasma.remotecontrollers.CEC.makeActiveSource: %i %s", res, strerror(-res));
		return res;
	}
	return 0;
}

int sleep_cb(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
	struct Context* ctx = userdata;
	int will_sleep;
	int res = sd_bus_message_read_basic(m, 'b', &will_sleep);
	if (res < 0) {
		LOG("Error reading sleep message: %i %s", res, strerror(-res));
		return res;
	}
	LOG("Going to sleep? %i", will_sleep);
	if (will_sleep) {
		return res;
	}

	int try;
	for (try = 0; try < MAX_TRIES; ++try) {
		if (sd_bus_error_is_set(ret_error)) {
			sd_bus_error_free(ret_error);
		}
		res = sd_bus_call_method(ctx->session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "powerOnDevices", ret_error, NULL, "");
		if (res != -EHOSTUNREACH) {
			break;
		}
		LOG("Call org.kde.plasma.remotecontrollers.CEC.powerOnDevices timed out, trying again (%i/%i)", try + 1, MAX_TRIES);
		sleep(1);
	}
	if (res < 0) {
		LOG("Failed to call org.kde.plasma.remotecontrollers.CEC.powerOnDevices: %i %s", res, strerror(-res));
		return res;
	}
	ctx->do_activate = true;
	res = sd_bus_call_method(ctx->session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "makeActiveSource", ret_error, NULL, "");
	if (res < 0) {
		LOG("Failed to call org.kde.plasma.remotecontrollers.CEC.makeActiveSource: %i %s", res, strerror(-res));
		return res;
	}
	return 0;
}

int main(int argc __attribute__((unused)), char* argv[] __attribute__((unused))) {
	int res;
	struct Context ctx = {0};
	sd_bus_slot* sleep_slot = NULL;
	sd_bus_slot* activate_slot = NULL;

	res = sd_bus_open_system(&ctx.system_bus);
	if (res < 0) {
		LOG("Failed to open system bus: %i %s", res, strerror(-res));
		goto shutdown;
	}
	res = sd_bus_open_user(&ctx.session_bus);
	if (res < 0) {
		LOG("Failed to open session bus: %i %s", res, strerror(-res));
		goto shutdown;
	}

	res = sd_bus_match_signal(ctx.session_bus, &activate_slot, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "sourceActivated", activate_cb, &ctx);
	if (res < 0) {
		LOG("Failed to match bus signal org.kde.plasma.remotecontrollers.CEC.sourceActivated: %i %s", res, strerror(-res));
		goto shutdown;
	}

	res = sd_bus_match_signal(ctx.system_bus, &sleep_slot, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep", sleep_cb, &ctx);
	if (res < 0) {
		LOG("Failed to match bus signal org.freedesktop.login1.Manager.PrepareForSleep: %i %s", res, strerror(-res));
		goto shutdown;
	}

	while (res >= 0) {
		res = sd_bus_wait(ctx.system_bus, UINT64_MAX);
		if (res < 0) {
			continue;
		}
		res = sd_bus_process(ctx.system_bus, NULL);
	}
	if (res != -EINTR) {
		LOG("Exiting with error status: %i %s", res, strerror(-res));
	}

shutdown:
	if (activate_slot) {
		sd_bus_slot_unref(activate_slot);
	}
	if (sleep_slot) {
		sd_bus_slot_unref(sleep_slot);
	}
	if (ctx.system_bus) {
		sd_bus_unref(ctx.system_bus);
	}
	if (ctx.session_bus) {
		sd_bus_unref(ctx.session_bus);
	}
	return res ? 1 : 0;
}
