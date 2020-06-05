/*
 * idevicepair.c
 * Manage pairings with devices and this host
 *
 * Copyright (c) 2010-2019 Nikias Bassen, All Rights Reserved.
 * Copyright (c) 2014 Martin Szulecki, All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define TOOL_NAME "idevicepair"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#ifndef WIN32
#include <signal.h>
#endif
#include "common/userpref.h"

#include <libimobiledevice/libimobiledevice.h>
#include <libimobiledevice/lockdown.h>

static char *udid = NULL;

static void print_error_message(lockdownd_error_t err)
{
	switch (err) {
		case LOCKDOWN_E_PASSWORD_PROTECTED:
			printf("ERROR: Could not validate with device %s because a passcode is set. Please enter the passcode on the device and retry.\n", udid);
			break;
		case LOCKDOWN_E_INVALID_CONF:
		case LOCKDOWN_E_INVALID_HOST_ID:
			printf("ERROR: Device %s is not paired with this host\n", udid);
			break;
		case LOCKDOWN_E_PAIRING_DIALOG_RESPONSE_PENDING:
			printf("ERROR: Please accept the trust dialog on the screen of device %s, then attempt to pair again.\n", udid);
			break;
		case LOCKDOWN_E_USER_DENIED_PAIRING:
			printf("ERROR: Device %s said that the user denied the trust dialog.\n", udid);
			break;
		default:
			printf("ERROR: Device %s returned unhandled error code %d\n", udid, err);
			break;
	}
}

static void print_usage(int argc, char **argv)
{
	char *name = NULL;

	name = strrchr(argv[0], '/');
	printf("Usage: %s [OPTIONS] COMMAND\n", (name ? name + 1: argv[0]));
	printf("\n");
	printf("Manage host pairings with devices and usbmuxd.\n");
	printf("\n");
	printf("Where COMMAND is one of:\n");
	printf("  systembuid   print the system buid of the usbmuxd host\n");
	printf("  hostid       print the host id for target device\n");
	printf("  pair         pair device with this host\n");
	printf("  validate     validate if device is paired with this host\n");
	printf("  unpair       unpair device with this host\n");
	printf("  list         list devices paired with this host\n");
	printf("\n");
	printf("The following OPTIONS are accepted:\n");
	printf("  -u, --udid UDID  target specific device by UDID\n");
	printf("  -d, --debug      enable communication debugging\n");
	printf("  -h, --help       prints usage information\n");
	printf("  -v, --version    prints version information\n");
	printf("\n");
	printf("Homepage:    <" PACKAGE_URL ">\n");
	printf("Bug Reports: <" PACKAGE_BUGREPORT ">\n");
}

static void parse_opts(int argc, char **argv)
{
	int c = 0;
	static struct option longopts[] = {
		{ "help",    no_argument,       NULL, 'h' },
		{ "udid",    required_argument, NULL, 'u' },
		{ "debug",   no_argument,       NULL, 'd' },
		{ "version", no_argument,       NULL, 'v' },
		{ NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "hu:dv", longopts, NULL)) != -1) {
		switch (c) {
		case 'h':
			print_usage(argc, argv);
			exit(EXIT_SUCCESS);
		case 'u':
			if (!*optarg) {
				fprintf(stderr, "ERROR: UDID must not be empty!\n");
				print_usage(argc, argv);
				exit(2);
			}
			if (udid)
				free(udid);
			udid = strdup(optarg);
			break;
		case 'd':
			idevice_set_debug_level(1);
			break;
		case 'v':
			printf("%s %s\n", TOOL_NAME, PACKAGE_VERSION);
			exit(EXIT_SUCCESS);
		default:
			print_usage(argc, argv);
			exit(EXIT_SUCCESS);
		}
	}
}

int main(int argc, char **argv)
{
	lockdownd_client_t client = NULL;
	idevice_t device = NULL;
	idevice_error_t ret = IDEVICE_E_UNKNOWN_ERROR;
	lockdownd_error_t lerr;
	int result;

	char *type = NULL;
	char *cmd;
	typedef enum {
		OP_NONE = 0, OP_PAIR, OP_VALIDATE, OP_UNPAIR, OP_LIST, OP_HOSTID, OP_SYSTEMBUID
	} op_t;
	op_t op = OP_NONE;

#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
#endif
	parse_opts(argc, argv);

	if ((argc - optind) < 1) {
		printf("ERROR: You need to specify a COMMAND!\n");
		print_usage(argc, argv);
		exit(EXIT_FAILURE);
	}

	cmd = (argv+optind)[0];

	if (!strcmp(cmd, "pair")) {
		op = OP_PAIR;
	} else if (!strcmp(cmd, "validate")) {
		op = OP_VALIDATE;
	} else if (!strcmp(cmd, "unpair")) {
		op = OP_UNPAIR;
	} else if (!strcmp(cmd, "list")) {
		op = OP_LIST;
	} else if (!strcmp(cmd, "hostid")) {
		op = OP_HOSTID;
	} else if (!strcmp(cmd, "systembuid")) {
		op = OP_SYSTEMBUID;
	} else {
		printf("ERROR: Invalid command '%s' specified\n", cmd);
		print_usage(argc, argv);
		exit(EXIT_FAILURE);
	}

	if (op == OP_SYSTEMBUID) {
		char *systembuid = NULL;
		userpref_read_system_buid(&systembuid);

		printf("%s\n", systembuid);

		if (systembuid)
			free(systembuid);

		return EXIT_SUCCESS;
	}

	if (op == OP_LIST) {
		unsigned int i;
		char **udids = NULL;
		unsigned int count = 0;
		userpref_get_paired_udids(&udids, &count);
		for (i = 0; i < count; i++) {
			printf("%s\n", udids[i]);
			free(udids[i]);
		}
		if (udids)
			free(udids);
		if (udid)
			free(udid);
		return EXIT_SUCCESS;
	}

	if (udid) {
		ret = idevice_new(&device, udid);
		if (ret != IDEVICE_E_SUCCESS) {
			printf("No device found with udid %s, is it plugged in?\n", udid);
			free(udid);
			return EXIT_FAILURE;
		}
		free(udid);
		udid = NULL;
	} else {
		ret = idevice_new(&device, NULL);
		if (ret != IDEVICE_E_SUCCESS) {
			printf("No device found, is it plugged in?\n");
			return EXIT_FAILURE;
		}
	}

	ret = idevice_get_udid(device, &udid);
	if (ret != IDEVICE_E_SUCCESS) {
		printf("ERROR: Could not get device udid, error code %d\n", ret);
		result = EXIT_FAILURE;
		goto leave;
	}

	if (op == OP_HOSTID) {
		plist_t pair_record = NULL;
		char *hostid = NULL;

		userpref_read_pair_record(udid, &pair_record);
		pair_record_get_host_id(pair_record, &hostid);

		printf("%s\n", hostid);

		if (hostid)
			free(hostid);

		if (pair_record)
			plist_free(pair_record);

		return EXIT_SUCCESS;
	}

	lerr = lockdownd_client_new(device, &client, "idevicepair");
	if (lerr != LOCKDOWN_E_SUCCESS) {
		idevice_free(device);
		printf("ERROR: Could not connect to lockdownd, error code %d\n", lerr);
		return EXIT_FAILURE;
	}

	result = EXIT_SUCCESS;

	lerr = lockdownd_query_type(client, &type);
	if (lerr != LOCKDOWN_E_SUCCESS) {
		printf("QueryType failed, error code %d\n", lerr);
		result = EXIT_FAILURE;
		goto leave;
	} else {
		if (strcmp("com.apple.mobile.lockdown", type)) {
			printf("WARNING: QueryType request returned '%s'\n", type);
		}
		if (type) {
			free(type);
		}
	}

	switch(op) {
		default:
		case OP_PAIR:
		lerr = lockdownd_pair(client, NULL);
		if (lerr == LOCKDOWN_E_SUCCESS) {
			printf("SUCCESS: Paired with device %s\n", udid);
		} else {
			result = EXIT_FAILURE;
			print_error_message(lerr);
		}
		break;

		case OP_VALIDATE:
		lockdownd_client_free(client);
		client = NULL;
		lerr = lockdownd_client_new_with_handshake(device, &client, TOOL_NAME);
		if (lerr == LOCKDOWN_E_SUCCESS) {
			printf("SUCCESS: Validated pairing with device %s\n", udid);
		} else {
			result = EXIT_FAILURE;
			print_error_message(lerr);
		}
		break;

		case OP_UNPAIR:
		lerr = lockdownd_unpair(client, NULL);
		if (lerr == LOCKDOWN_E_SUCCESS) {
			printf("SUCCESS: Unpaired with device %s\n", udid);
		} else {
			result = EXIT_FAILURE;
			print_error_message(lerr);
		}
		break;
	}

leave:
	lockdownd_client_free(client);
	idevice_free(device);
	if (udid) {
		free(udid);
	}
	return result;
}

