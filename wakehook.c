/* SPDX-License-Identifier: BSD-3-Clause */
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <systemd/sd-bus.h>
#include <unistd.h>

#define MAX_TRIES 5

struct Context {
	sd_bus* system_bus;
	sd_bus* session_bus;
	bool do_activate;
};

int activate_cb(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
	struct Context* ctx = userdata;
	int is_active;
	int res = sd_bus_message_read_basic(m, 'b', &is_active);
	if (res < 0 || !ctx->do_activate) {
		return res;
	}

	if (is_active) {
		ctx->do_activate = false;
		return 0;
	}
	res = sd_bus_call_method(ctx->session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "makeActiveSource", ret_error, NULL, "");
	if (res < 0) {
		return res;
	}
	return 0;
}

int sleep_cb(sd_bus_message* m, void* userdata, sd_bus_error* ret_error) {
	struct Context* ctx = userdata;
	int will_sleep;
	int res = sd_bus_message_read_basic(m, 'b', &will_sleep);
	if (res < 0 || will_sleep) {
		return res;
	}

	int try;
	for (try = 0; try < MAX_TRIES; ++try) {
		res = sd_bus_call_method(ctx->session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "powerOnDevices", ret_error, NULL, "");
		if (res == -ETIMEDOUT) {
			sleep(1);
			continue;
		}
		if (res < 0) {
			return res;
		}
		break;
	}
	ctx->do_activate = true;
	res = sd_bus_call_method(ctx->session_bus, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "makeActiveSource", ret_error, NULL, "");
	if (res < 0) {
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
		goto shutdown;
	}
	res = sd_bus_open_user(&ctx.session_bus);
	if (res < 0) {
		goto shutdown;
	}

	res = sd_bus_match_signal(ctx.session_bus, &activate_slot, "org.kde.plasma.remotecontrollers", "/CEC", "org.kde.plasma.remotecontrollers.CEC", "sourceActivated", activate_cb, &ctx);
	if (res < 0) {
		goto shutdown;
	}

	res = sd_bus_match_signal(ctx.system_bus, &sleep_slot, "org.freedesktop.login1", "/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep", sleep_cb, &ctx);
	if (res < 0) {
		goto shutdown;
	}

	while (res >= 0) {
		res = sd_bus_wait(ctx.system_bus, UINT64_MAX);
		if (res < 0) {
			continue;
		}
		res = sd_bus_process(ctx.system_bus, NULL);
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
