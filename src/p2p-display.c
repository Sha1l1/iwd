/*
 *
 *  Wireless daemon for Linux
 *
 *  Copyright (C) 2024  Intel Corporation. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>

#include <ell/ell.h>

#include "src/iwd.h"
#include "src/dbus.h"
#include "src/module.h"
#include "src/p2putil.h"
#include "src/p2p.h"
#include "src/p2p-display.h"

#define WFD_SESSION_TIMEOUT	120	/* 120 second session timeout */

#define IWD_P2P_DISPLAY_SESSION_INTERFACE \
				"net.connman.iwd.p2p.DisplaySession"

static struct l_queue *wfd_session_list;
static uint32_t next_session_id;

static const char *wfd_session_state_to_str(enum wfd_session_state state)
{
	switch (state) {
	case WFD_SESSION_STATE_IDLE:
		return "idle";
	case WFD_SESSION_STATE_CONNECTING:
		return "connecting";
	case WFD_SESSION_STATE_ESTABLISHED:
		return "established";
	case WFD_SESSION_STATE_TEARDOWN:
		return "teardown";
	}

	return "unknown";
}

static void wfd_session_timeout_cb(struct l_timeout *timeout, void *user_data)
{
	struct wfd_session *session = user_data;

	l_warn("WFD session %u timed out in state %s",
		session->session_id,
		wfd_session_state_to_str(session->state));

	wfd_session_set_state(session, WFD_SESSION_STATE_TEARDOWN);
}

struct wfd_session *wfd_session_new(struct p2p_device *dev,
				struct p2p_peer *peer,
				uint16_t rtsp_port)
{
	struct wfd_session *session;

	if (!dev || !peer)
		return NULL;

	if (rtsp_port == 0) {
		l_error("WFD session requires non-zero RTSP port");
		return NULL;
	}

	session = l_new(struct wfd_session, 1);
	session->dev = dev;
	session->peer = peer;
	session->rtsp_port = rtsp_port;
	session->rtp_port = 0;
	session->state = WFD_SESSION_STATE_IDLE;
	session->session_id = ++next_session_id;
	session->session_path = l_strdup_printf(
			IWD_BASE_PATH "/p2p/display_session/%u",
			session->session_id);

	session->session_timeout = l_timeout_create(WFD_SESSION_TIMEOUT,
						wfd_session_timeout_cb,
						session, NULL);

	l_queue_push_tail(wfd_session_list, session);

	l_info("WFD session %u created with RTSP port %u",
		session->session_id, rtsp_port);

	return session;
}

void wfd_session_free(struct wfd_session *session)
{
	if (!session)
		return;

	l_debug("Destroying WFD session %u", session->session_id);

	l_queue_remove(wfd_session_list, session);
	l_timeout_remove(session->session_timeout);
	l_dbus_unregister_object(dbus_get_bus(), session->session_path);
	l_free(session->session_path);
	l_free(session);
}

bool wfd_session_is_active(struct p2p_device *dev)
{
	const struct l_queue_entry *entry;

	if (!wfd_session_list)
		return false;

	for (entry = l_queue_get_entries(wfd_session_list); entry;
			entry = entry->next) {
		struct wfd_session *session = entry->data;

		if (session->dev == dev &&
				session->state != WFD_SESSION_STATE_IDLE &&
				session->state != WFD_SESSION_STATE_TEARDOWN)
			return true;
	}

	return false;
}

enum wfd_session_state wfd_session_get_state(
				const struct wfd_session *session)
{
	if (!session)
		return WFD_SESSION_STATE_IDLE;

	return session->state;
}

void wfd_session_set_state(struct wfd_session *session,
			enum wfd_session_state new_state)
{
	enum wfd_session_state old_state;

	if (!session)
		return;

	old_state = session->state;

	if (old_state == new_state)
		return;

	session->state = new_state;

	l_info("WFD session %u: %s -> %s", session->session_id,
		wfd_session_state_to_str(old_state),
		wfd_session_state_to_str(new_state));

	/*
	 * Reset the timeout on each state transition.
	 * The timeout ensures that stuck sessions get cleaned up.
	 */
	l_timeout_modify(session->session_timeout, WFD_SESSION_TIMEOUT);

	/*
	 * Emit D-Bus property change for the session state.
	 */
	if (session->session_path)
		l_dbus_property_changed(dbus_get_bus(),
				session->session_path,
				IWD_P2P_DISPLAY_SESSION_INTERFACE,
				"State");

	/*
	 * If tearing down, clean up after a short delay to allow
	 * the signal to be emitted.
	 */
	if (new_state == WFD_SESSION_STATE_TEARDOWN)
		l_timeout_modify(session->session_timeout, 1);
}

/*
 * D-Bus property getters for DisplaySession interface
 */
static bool wfd_session_property_get_state(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_builder *builder,
				void *user_data)
{
	struct wfd_session *session = user_data;
	const char *state_str;

	state_str = wfd_session_state_to_str(session->state);
	l_dbus_message_builder_append_basic(builder, 's', state_str);

	return true;
}

static bool wfd_session_property_get_rtsp_port(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_builder *builder,
				void *user_data)
{
	struct wfd_session *session = user_data;
	uint16_t port = session->rtsp_port;

	l_dbus_message_builder_append_basic(builder, 'q', &port);

	return true;
}

static bool wfd_session_property_get_rtp_port(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_builder *builder,
				void *user_data)
{
	struct wfd_session *session = user_data;
	uint16_t port = session->rtp_port;

	l_dbus_message_builder_append_basic(builder, 'q', &port);

	return true;
}

static bool wfd_session_property_get_session_id(struct l_dbus *dbus,
				struct l_dbus_message *message,
				struct l_dbus_message_builder *builder,
				void *user_data)
{
	struct wfd_session *session = user_data;
	uint32_t id = session->session_id;

	l_dbus_message_builder_append_basic(builder, 'u', &id);

	return true;
}

/*
 * D-Bus method: Stop a WFD session
 */
static struct l_dbus_message *wfd_session_method_stop(struct l_dbus *dbus,
				struct l_dbus_message *message,
				void *user_data)
{
	struct wfd_session *session = user_data;

	if (!l_dbus_message_get_arguments(message, ""))
		return dbus_error_invalid_args(message);

	if (session->state == WFD_SESSION_STATE_TEARDOWN ||
			session->state == WFD_SESSION_STATE_IDLE)
		return dbus_error_not_available(message);

	wfd_session_set_state(session, WFD_SESSION_STATE_TEARDOWN);

	return l_dbus_message_new_method_return(message);
}

static void wfd_session_interface_setup(struct l_dbus_interface *interface)
{
	l_dbus_interface_method(interface, "Stop", 0,
				wfd_session_method_stop, "", "");

	l_dbus_interface_property(interface, "State", 0, "s",
				wfd_session_property_get_state, NULL);
	l_dbus_interface_property(interface, "RtspPort", 0, "q",
				wfd_session_property_get_rtsp_port, NULL);
	l_dbus_interface_property(interface, "RtpPort", 0, "q",
				wfd_session_property_get_rtp_port, NULL);
	l_dbus_interface_property(interface, "SessionId", 0, "u",
				wfd_session_property_get_session_id, NULL);
}

bool p2p_display_init(void)
{
	struct l_dbus *dbus = dbus_get_bus();

	if (!l_dbus_register_interface(dbus,
				IWD_P2P_DISPLAY_SESSION_INTERFACE,
				wfd_session_interface_setup,
				NULL, false)) {
		l_error("Unable to register the %s interface",
			IWD_P2P_DISPLAY_SESSION_INTERFACE);
		return false;
	}

	wfd_session_list = l_queue_new();
	next_session_id = 0;

	l_info("P2P Display (WFD) session management initialized");

	return true;
}

void p2p_display_exit(void)
{
	struct l_dbus *dbus = dbus_get_bus();

	l_queue_destroy(wfd_session_list, (l_queue_destroy_func_t)
			wfd_session_free);
	wfd_session_list = NULL;

	l_dbus_unregister_interface(dbus,
			IWD_P2P_DISPLAY_SESSION_INTERFACE);
}
