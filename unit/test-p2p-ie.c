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

#include <ell/ell.h>

#include "src/p2putil.h"

/*
 * Test WFD Device Information subelement encoding and decoding.
 * Verifies that wfd_subelem_iter correctly parses a WFD IE
 * containing a Device Information subelement.
 */
static void test_wfd_device_info_parse(const void *data)
{
	/*
	 * WFD IE with Device Information subelement:
	 *   Subelement ID: 0 (WFD Device Information)
	 *   Length: 6
	 *   Device Info Bitmap: 0x0011 (Source, Session Available)
	 *   Session Mgmt Ctrl Port: 7236 (0x1c44)
	 *   Max Throughput: 10
	 */
	static const uint8_t wfd_ie[] = {
		0x00,			/* Subelement ID: Device Info */
		0x00, 0x06,		/* Length: 6 */
		0x00, 0x11,		/* Dev Info: source + session available */
		0x1c, 0x44,		/* Control port: 7236 */
		0x00, 0x0a,		/* Max throughput: 10 */
	};

	struct wfd_subelem_iter iter;

	wfd_subelem_iter_init(&iter, wfd_ie, sizeof(wfd_ie));

	assert(wfd_subelem_iter_next(&iter));
	assert(wfd_subelem_iter_get_type(&iter) ==
			WFD_SUBELEM_WFD_DEVICE_INFORMATION);
	assert(wfd_subelem_iter_get_length(&iter) == 6);

	const uint8_t *subelem_data = wfd_subelem_iter_get_data(&iter);
	uint16_t dev_info = l_get_be16(subelem_data);
	uint16_t port = l_get_be16(subelem_data + 2);
	uint16_t throughput = l_get_be16(subelem_data + 4);

	/* Verify source type */
	assert((dev_info & WFD_DEV_INFO_DEVICE_TYPE) ==
			WFD_DEV_INFO_TYPE_SOURCE);
	/* Verify session available */
	assert((dev_info & WFD_DEV_INFO_SESSION_AVAILABILITY) ==
			WFD_DEV_INFO_SESSION_AVAILABLE);
	/* Verify port */
	assert(port == 7236);
	/* Verify throughput */
	assert(throughput == 10);

	/* No more subelements */
	assert(!wfd_subelem_iter_next(&iter));
}

/*
 * Test WFD IE with multiple subelements:
 * Device Information + Extended Capability (UIBC)
 */
static void test_wfd_multi_subelem_parse(const void *data)
{
	static const uint8_t wfd_ie[] = {
		/* Device Information subelement */
		0x00,			/* Subelement ID: Device Info */
		0x00, 0x06,		/* Length: 6 */
		0x00, 0x11,		/* Source + session available */
		0x1c, 0x44,		/* Port: 7236 */
		0x00, 0x0a,		/* Throughput: 10 */
		/* Extended Capability subelement */
		0x07,			/* Subelement ID: Ext Capability */
		0x00, 0x02,		/* Length: 2 */
		0x00, 0x01,		/* UIBC Support */
	};

	struct wfd_subelem_iter iter;
	int count = 0;

	wfd_subelem_iter_init(&iter, wfd_ie, sizeof(wfd_ie));

	while (wfd_subelem_iter_next(&iter)) {
		count++;

		switch (wfd_subelem_iter_get_type(&iter)) {
		case WFD_SUBELEM_WFD_DEVICE_INFORMATION:
			assert(wfd_subelem_iter_get_length(&iter) == 6);
			break;
		case WFD_SUBELEM_EXTENDED_CAPABILITY:
			assert(wfd_subelem_iter_get_length(&iter) == 2);
			{
				const uint8_t *ext_data =
					wfd_subelem_iter_get_data(&iter);
				uint16_t ext_cap = l_get_be16(ext_data);
				/* UIBC bit should be set */
				assert(ext_cap & 0x0001);
			}
			break;
		default:
			assert(false);
			break;
		}
	}

	assert(count == 2);
}

/*
 * Test WFD Dual Role device type parsing.
 */
static void test_wfd_dual_role(const void *data)
{
	static const uint8_t wfd_ie[] = {
		0x00,			/* Device Info */
		0x00, 0x06,		/* Length: 6 */
		0x00, 0x13,		/* Dual role + session available */
		0x1c, 0x44,		/* Port: 7236 */
		0x00, 0x0a,		/* Throughput: 10 */
	};

	struct wfd_subelem_iter iter;

	wfd_subelem_iter_init(&iter, wfd_ie, sizeof(wfd_ie));
	assert(wfd_subelem_iter_next(&iter));

	const uint8_t *subelem_data = wfd_subelem_iter_get_data(&iter);
	uint16_t dev_info = l_get_be16(subelem_data);

	/* Dual role: bits 0-1 = 0x03 */
	assert((dev_info & WFD_DEV_INFO_DEVICE_TYPE) ==
			WFD_DEV_INFO_TYPE_DUAL_ROLE);
}

/*
 * Test empty WFD IE (zero length).
 */
static void test_wfd_empty_ie(const void *data)
{
	struct wfd_subelem_iter iter;

	wfd_subelem_iter_init(&iter, NULL, 0);
	assert(!wfd_subelem_iter_next(&iter));
}

/*
 * Test P2P attribute iteration for basic capability attribute.
 */
static void test_p2p_capability_attr(const void *data)
{
	static const uint8_t p2p_attrs[] = {
		/* P2P Capability attribute */
		0x02,			/* Attribute ID: Capability */
		0x02, 0x00,		/* Length: 2 */
		0x25,			/* Device caps */
		0x09,			/* Group caps */
	};

	struct p2p_attr_iter iter;

	p2p_attr_iter_init(&iter, p2p_attrs, sizeof(p2p_attrs));

	assert(p2p_attr_iter_next(&iter));
	assert(p2p_attr_iter_get_type(&iter) == P2P_ATTR_P2P_CAPABILITY);
	assert(p2p_attr_iter_get_length(&iter) == 2);

	const uint8_t *attr_data = p2p_attr_iter_get_data(&iter);
	assert(attr_data[0] == 0x25);
	assert(attr_data[1] == 0x09);

	assert(!p2p_attr_iter_next(&iter));
}

int main(int argc, char *argv[])
{
	l_test_init(&argc, &argv);

	l_test_add("wfd/device-info-parse",
			test_wfd_device_info_parse, NULL);
	l_test_add("wfd/multi-subelem-parse",
			test_wfd_multi_subelem_parse, NULL);
	l_test_add("wfd/dual-role",
			test_wfd_dual_role, NULL);
	l_test_add("wfd/empty-ie",
			test_wfd_empty_ie, NULL);
	l_test_add("p2p/capability-attr",
			test_p2p_capability_attr, NULL);

	return l_test_run();
}
