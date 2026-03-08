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

#include <stdint.h>
#include <stdbool.h>

struct p2p_device;
struct p2p_peer;
struct p2p_wfd_properties;

enum wfd_session_state {
	WFD_SESSION_STATE_IDLE,
	WFD_SESSION_STATE_CONNECTING,
	WFD_SESSION_STATE_ESTABLISHED,
	WFD_SESSION_STATE_TEARDOWN,
};

/*
 * Wi-Fi Display session representation.
 * Managed per p2p_device, tracks RTSP/RTP port negotiation
 * and session lifecycle per the WFD Technical Specification v2.1.
 */
struct wfd_session {
	struct p2p_device *dev;
	struct p2p_peer *peer;
	uint16_t rtsp_port;		/* RTSP control port */
	uint16_t rtp_port;		/* RTP media port */
	enum wfd_session_state state;
	struct l_timeout *session_timeout;
	char *session_path;		/* D-Bus object path */
	uint32_t session_id;
};

/* WFD Device Information subelement as per WFD Spec v2.1, Table 28 */
struct wfd_device_info {
	uint16_t device_info_bitmap;
	uint16_t session_mgmt_ctrl_port;
	uint16_t max_throughput;
};

/*
 * Initialize and register the WFD session management module.
 * Called from p2p_init().
 */
bool p2p_display_init(void);
void p2p_display_exit(void);

/*
 * Start a WFD session with a discovered peer.
 * @dev:       P2P device
 * @peer:      target peer (must have WFD capabilities)
 * @rtsp_port: local RTSP port for session control
 *
 * Returns a new wfd_session on success, NULL on failure.
 */
struct wfd_session *wfd_session_new(struct p2p_device *dev,
				struct p2p_peer *peer,
				uint16_t rtsp_port);

/*
 * Stop and destroy a WFD session.
 * Sends session teardown if the session is established.
 */
void wfd_session_free(struct wfd_session *session);

/*
 * Query whether a WFD session is active on the given device.
 */
bool wfd_session_is_active(struct p2p_device *dev);

/*
 * Get the current session state.
 */
enum wfd_session_state wfd_session_get_state(
				const struct wfd_session *session);

/*
 * Transition the session to a new state.
 * Emits D-Bus signals as appropriate.
 */
void wfd_session_set_state(struct wfd_session *session,
			enum wfd_session_state new_state);
