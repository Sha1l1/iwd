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

struct l_queue;

enum p2p_group_role {
	P2P_GROUP_ROLE_CLIENT,
	P2P_GROUP_ROLE_GO,
};

/*
 * Persistent P2P group credential record.
 * Stored under /var/lib/iwd/p2p/<peer_device_address>.group
 */
struct p2p_persistent_group {
	char ssid[33];			/* DIRECT-xx-... */
	char passphrase[64];
	uint8_t device_addr[6];		/* Peer device address */
	enum p2p_group_role role;
};

/*
 * Load a persistent group credential from storage.
 * @device_addr: 6-byte peer device MAC address
 * @out:         output structure populated on success
 *
 * Returns true if a stored group was found and loaded.
 */
bool p2p_persistent_group_load(const uint8_t *device_addr,
			struct p2p_persistent_group *out);

/*
 * Save a persistent group credential to storage.
 * @group: credential record to persist
 *
 * Returns true on success.
 */
bool p2p_persistent_group_save(const struct p2p_persistent_group *group);

/*
 * Remove a stored persistent group credential.
 * @device_addr: 6-byte peer device MAC address
 *
 * Returns true if the file was removed.
 */
bool p2p_persistent_group_remove(const uint8_t *device_addr);

/*
 * Load all persistent group credentials into a queue.
 * Caller takes ownership of the returned queue and its entries.
 *
 * Returns a queue of struct p2p_persistent_group, or NULL on error.
 */
struct l_queue *p2p_persistent_group_load_all(void);
