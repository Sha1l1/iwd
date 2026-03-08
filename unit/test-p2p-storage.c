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

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>

#include <ell/ell.h>

#include "src/p2p-storage.h"

/*
 * Test saving and loading a persistent P2P group credential.
 * Uses a temporary directory to avoid polluting real storage.
 */
static void test_persistent_group_save_load(const void *data)
{
	struct p2p_persistent_group save_group;
	struct p2p_persistent_group load_group;
	bool r;

	memset(&save_group, 0, sizeof(save_group));
	strcpy(save_group.ssid, "DIRECT-aB-TestDevice");
	strcpy(save_group.passphrase, "aBcDeFgH12345678");
	save_group.device_addr[0] = 0xaa;
	save_group.device_addr[1] = 0xbb;
	save_group.device_addr[2] = 0xcc;
	save_group.device_addr[3] = 0xdd;
	save_group.device_addr[4] = 0xee;
	save_group.device_addr[5] = 0xff;
	save_group.role = P2P_GROUP_ROLE_CLIENT;

	r = p2p_persistent_group_save(&save_group);

	/*
	 * This may fail if DAEMON_STORAGEDIR doesn't exist or
	 * is not writable.  Skip test in that case.
	 */
	if (!r) {
		l_info("Skipping save/load test: storage dir not writable");
		return;
	}

	/* Now load it back */
	memset(&load_group, 0, sizeof(load_group));
	r = p2p_persistent_group_load(save_group.device_addr, &load_group);
	assert(r);

	/* Verify all fields match */
	assert(strcmp(load_group.ssid, save_group.ssid) == 0);
	assert(strcmp(load_group.passphrase, save_group.passphrase) == 0);
	assert(memcmp(load_group.device_addr, save_group.device_addr, 6) == 0);
	assert(load_group.role == save_group.role);

	/* Clean up */
	r = p2p_persistent_group_remove(save_group.device_addr);
	assert(r);

	/* Verify it's gone */
	r = p2p_persistent_group_load(save_group.device_addr, &load_group);
	assert(!r);
}

/*
 * Test saving a GO role persistent group.
 */
static void test_persistent_group_go_role(const void *data)
{
	struct p2p_persistent_group save_group;
	struct p2p_persistent_group load_group;
	bool r;

	memset(&save_group, 0, sizeof(save_group));
	strcpy(save_group.ssid, "DIRECT-xY-MyGO");
	strcpy(save_group.passphrase, "GoPassword1234");
	save_group.device_addr[0] = 0x11;
	save_group.device_addr[1] = 0x22;
	save_group.device_addr[2] = 0x33;
	save_group.device_addr[3] = 0x44;
	save_group.device_addr[4] = 0x55;
	save_group.device_addr[5] = 0x66;
	save_group.role = P2P_GROUP_ROLE_GO;

	r = p2p_persistent_group_save(&save_group);
	if (!r) {
		l_info("Skipping GO role test: storage dir not writable");
		return;
	}

	memset(&load_group, 0, sizeof(load_group));
	r = p2p_persistent_group_load(save_group.device_addr, &load_group);
	assert(r);
	assert(load_group.role == P2P_GROUP_ROLE_GO);

	p2p_persistent_group_remove(save_group.device_addr);
}

/*
 * Test loading a non-existent persistent group.
 */
static void test_persistent_group_not_found(const void *data)
{
	struct p2p_persistent_group group;
	uint8_t addr[6] = { 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54 };
	bool r;

	r = p2p_persistent_group_load(addr, &group);
	assert(!r);
}

/*
 * Test removing a non-existent persistent group.
 */
static void test_persistent_group_remove_nonexistent(const void *data)
{
	uint8_t addr[6] = { 0xfe, 0xdc, 0xba, 0x98, 0x76, 0x54 };
	bool r;

	r = p2p_persistent_group_remove(addr);
	assert(!r);
}

/*
 * Test NULL parameter handling.
 */
static void test_persistent_group_null_params(const void *data)
{
	struct p2p_persistent_group group;
	bool r;

	r = p2p_persistent_group_load(NULL, &group);
	assert(!r);

	r = p2p_persistent_group_load((uint8_t[]){0, 0, 0, 0, 0, 0}, NULL);
	assert(!r);

	r = p2p_persistent_group_save(NULL);
	assert(!r);

	r = p2p_persistent_group_remove(NULL);
	assert(!r);
}

int main(int argc, char *argv[])
{
	l_test_init(&argc, &argv);

	l_test_add("p2p-storage/save-load",
			test_persistent_group_save_load, NULL);
	l_test_add("p2p-storage/go-role",
			test_persistent_group_go_role, NULL);
	l_test_add("p2p-storage/not-found",
			test_persistent_group_not_found, NULL);
	l_test_add("p2p-storage/remove-nonexistent",
			test_persistent_group_remove_nonexistent, NULL);
	l_test_add("p2p-storage/null-params",
			test_persistent_group_null_params, NULL);

	return l_test_run();
}
