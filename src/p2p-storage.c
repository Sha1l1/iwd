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
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>

#include <ell/ell.h>

#include "src/iwd.h"
#include "src/p2p-storage.h"
#include "src/storage.h"

#define P2P_STORAGE_DIR		DAEMON_STORAGEDIR "/p2p"
#define P2P_GROUP_EXTENSION	".group"
#define P2P_GROUP_SECTION	"PersistentGroup"

static bool p2p_storage_ensure_dir(void)
{
	if (mkdir(P2P_STORAGE_DIR, 0700) < 0 && errno != EEXIST) {
		l_error("Failed to create P2P storage directory %s: %s",
			P2P_STORAGE_DIR, strerror(errno));
		return false;
	}

	return true;
}

static void p2p_device_addr_to_filename(const uint8_t *addr, char *buf,
					size_t buf_len)
{
	snprintf(buf, buf_len,
		"%s/%02x:%02x:%02x:%02x:%02x:%02x" P2P_GROUP_EXTENSION,
		P2P_STORAGE_DIR,
		addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
}

static bool p2p_parse_device_addr(const char *str, uint8_t *out)
{
	unsigned int a[6];
	int r;

	r = sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
		&a[0], &a[1], &a[2], &a[3], &a[4], &a[5]);

	if (r != 6)
		return false;

	out[0] = a[0]; out[1] = a[1]; out[2] = a[2];
	out[3] = a[3]; out[4] = a[4]; out[5] = a[5];

	return true;
}

static const char *p2p_role_to_str(enum p2p_group_role role)
{
	switch (role) {
	case P2P_GROUP_ROLE_CLIENT:
		return "client";
	case P2P_GROUP_ROLE_GO:
		return "go";
	}

	return "client";
}

static bool p2p_str_to_role(const char *str, enum p2p_group_role *out)
{
	if (!strcmp(str, "client")) {
		*out = P2P_GROUP_ROLE_CLIENT;
		return true;
	}

	if (!strcmp(str, "go")) {
		*out = P2P_GROUP_ROLE_GO;
		return true;
	}

	return false;
}

bool p2p_persistent_group_load(const uint8_t *device_addr,
			struct p2p_persistent_group *out)
{
	char filename[PATH_MAX];
	struct l_settings *settings;
	const char *ssid;
	const char *passphrase;
	const char *addr_str;
	const char *role_str;

	if (!device_addr || !out)
		return false;

	p2p_device_addr_to_filename(device_addr, filename, sizeof(filename));

	settings = l_settings_new();

	if (!l_settings_load_from_file(settings, filename)) {
		l_settings_free(settings);
		return false;
	}

	ssid = l_settings_get_value(settings, P2P_GROUP_SECTION, "SSID");
	passphrase = l_settings_get_value(settings, P2P_GROUP_SECTION,
					"Passphrase");
	addr_str = l_settings_get_value(settings, P2P_GROUP_SECTION,
					"DeviceAddress");
	role_str = l_settings_get_value(settings, P2P_GROUP_SECTION, "Role");

	if (!ssid || !passphrase || !addr_str || !role_str) {
		l_error("Missing fields in P2P group file %s", filename);
		l_settings_free(settings);
		return false;
	}

	memset(out, 0, sizeof(*out));
	l_strlcpy(out->ssid, ssid, sizeof(out->ssid));
	l_strlcpy(out->passphrase, passphrase, sizeof(out->passphrase));

	if (!p2p_parse_device_addr(addr_str, out->device_addr)) {
		l_error("Invalid DeviceAddress in %s", filename);
		l_settings_free(settings);
		return false;
	}

	if (!p2p_str_to_role(role_str, &out->role)) {
		l_error("Invalid Role in %s", filename);
		l_settings_free(settings);
		return false;
	}

	l_settings_free(settings);
	return true;
}

bool p2p_persistent_group_save(const struct p2p_persistent_group *group)
{
	char filename[PATH_MAX];
	struct l_settings *settings;
	char addr_str[18];
	bool r;

	if (!group)
		return false;

	if (!p2p_storage_ensure_dir())
		return false;

	p2p_device_addr_to_filename(group->device_addr, filename,
				sizeof(filename));

	snprintf(addr_str, sizeof(addr_str),
		"%02x:%02x:%02x:%02x:%02x:%02x",
		group->device_addr[0], group->device_addr[1],
		group->device_addr[2], group->device_addr[3],
		group->device_addr[4], group->device_addr[5]);

	settings = l_settings_new();
	l_settings_set_value(settings, P2P_GROUP_SECTION, "SSID",
			group->ssid);
	l_settings_set_value(settings, P2P_GROUP_SECTION, "Passphrase",
			group->passphrase);
	l_settings_set_value(settings, P2P_GROUP_SECTION, "DeviceAddress",
			addr_str);
	l_settings_set_value(settings, P2P_GROUP_SECTION, "Role",
			p2p_role_to_str(group->role));

	r = l_settings_save_to_file(settings, filename);

	if (!r)
		l_error("Failed to save P2P group to %s", filename);
	else
		l_info("P2P persistent group saved: %s role=%s",
			group->ssid, p2p_role_to_str(group->role));

	l_settings_free(settings);
	return r;
}

bool p2p_persistent_group_remove(const uint8_t *device_addr)
{
	char filename[PATH_MAX];

	if (!device_addr)
		return false;

	p2p_device_addr_to_filename(device_addr, filename, sizeof(filename));

	if (unlink(filename) < 0) {
		if (errno != ENOENT)
			l_error("Failed to remove P2P group file %s: %s",
				filename, strerror(errno));
		return false;
	}

	l_info("P2P persistent group removed for "
		"%02x:%02x:%02x:%02x:%02x:%02x",
		device_addr[0], device_addr[1], device_addr[2],
		device_addr[3], device_addr[4], device_addr[5]);

	return true;
}

struct l_queue *p2p_persistent_group_load_all(void)
{
	DIR *dir;
	struct dirent *entry;
	struct l_queue *groups;

	dir = opendir(P2P_STORAGE_DIR);
	if (!dir)
		return NULL;

	groups = l_queue_new();

	while ((entry = readdir(dir))) {
		const char *ext;
		uint8_t addr[6];
		struct p2p_persistent_group *group;
		char *name;
		size_t name_len;

		if (entry->d_type != DT_REG && entry->d_type != DT_UNKNOWN)
			continue;

		ext = strrchr(entry->d_name, '.');
		if (!ext || strcmp(ext, P2P_GROUP_EXTENSION))
			continue;

		/* Extract MAC address from filename */
		name_len = ext - entry->d_name;
		name = l_strndup(entry->d_name, name_len);

		if (!p2p_parse_device_addr(name, addr)) {
			l_free(name);
			continue;
		}

		l_free(name);

		group = l_new(struct p2p_persistent_group, 1);

		if (!p2p_persistent_group_load(addr, group)) {
			l_free(group);
			continue;
		}

		l_queue_push_tail(groups, group);
	}

	closedir(dir);

	if (l_queue_isempty(groups)) {
		l_queue_destroy(groups, NULL);
		return NULL;
	}

	return groups;
}
