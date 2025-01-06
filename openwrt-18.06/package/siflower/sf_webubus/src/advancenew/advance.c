/*
 * rpcd - UBUS RPC server
 *
 *   Copyright (C) 2013 Jo-Philipp Wich <jow@openwrt.org>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define _GNU_SOURCE /* crypt() */

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/statvfs.h>
#include <dirent.h>
#include <arpa/inet.h>
#include <signal.h>
#include <glob.h>
#include <libubox/blobmsg_json.h>
#include <libubox/avl-cmp.h>
#include <libubus.h>
#include <uci.h>

#include <rpcd/plugin.h>

static const struct rpc_daemon_ops *ops;

static struct blob_buf buf;
static struct uci_context *cursor;

enum {
	RPC_C_CMD,
	__RPC_C_MAX
};

static const struct blobmsg_policy rpc_cmd_policy[__RPC_C_MAX] = {
	[RPC_C_CMD]   = { .name = "cmd",  .type = BLOBMSG_TYPE_STRING },
};

enum {
	RPC_D_DATA,
	__RPC_D_MAX
};

static const struct blobmsg_policy rpc_data_policy[__RPC_D_MAX] = {
	[RPC_D_DATA]   = { .name = "data",  .type = BLOBMSG_TYPE_STRING },
};

static int
rpc_web_cmd(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
	struct blob_attr *tb[__RPC_C_MAX];
	const char *cmd;
	int ret;
	blob_buf_init(&buf, 0);

	blobmsg_parse(rpc_cmd_policy, __RPC_C_MAX, tb,
	              blob_data(msg), blob_len(msg));

	if (!tb[RPC_C_CMD] || blobmsg_data_len(tb[RPC_C_CMD]) >= 128)
		return UBUS_STATUS_INVALID_ARGUMENT;

	cmd = blobmsg_data(tb[RPC_C_CMD]);
	ret = system(cmd);

	if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        blobmsg_add_string(&buf, "result", "success");
    } else {
        blobmsg_add_string(&buf, "result", "failure");
    }
	ubus_send_reply(ctx, req, buf.head);

	return 0;
}

static int
rpc_web_getddns(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
        int ddns=0;

	if (access("/usr/lib/ddns", F_OK) == 0)
        {
		ddns = 1;
        }
        else
        {
		ddns = 0;
        }

        blob_buf_init(&buf, 0);
        blobmsg_add_u8(&buf, "result", ddns);
        ubus_send_reply(ctx, req, buf.head);
        return 0;
}

static int
rpc_web_delete_tmp_bin(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
	const char *cmd = "rm tmp/firmware.bin";
	int ret;
	blob_buf_init(&buf, 0);

	ret = system(cmd);

	if (WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        blobmsg_add_string(&buf, "result", "success");
    } else {
        blobmsg_add_string(&buf, "result", "failure");
    }
	ubus_send_reply(ctx, req, buf.head);

	return 0;
}
static int
rpc_web_get_img_version(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
	FILE *f;
	char img_version[128], line[128];
	const char *prefix = "DISTRIB_DESCRIPTION=";

	blob_buf_init(&buf, 0);

	if ((f = fopen("/etc/openwrt_release", "r")) != NULL)
	{
		while (fgets(line, sizeof(line) - 1, f))
		{
			if (strncmp(line, prefix, strlen(prefix)) == 0)
			{
				strncpy(img_version, line + strlen(prefix), sizeof(img_version) - 1);
				img_version[strcspn(img_version, "\r\n")] = '\0';
				blobmsg_add_string(&buf, "img_version", img_version);
				break;
			}
		}

		fclose(f);
	}

	ubus_send_reply(ctx, req, buf.head);

	return 0;
}


static int
rpc_web_get_device_speed(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
	FILE *f;
	void *c, *d;
	char *mac, *ups, *downs, line[128];

	blob_buf_init(&buf, 0);
	c = blobmsg_open_array(&buf, "speeds");

	if ((f = fopen("/proc/ts", "r")) != NULL)
	{
		/* skip header line */
		fgets(line, sizeof(line) - 1, f);

		while (fgets(line, sizeof(line) - 1, f))
		{
			mac = strtok(line, " \t"); /* mac*/

			strtok(NULL, " \t"); /* upload */
			strtok(NULL, " \t"); /* download */

			ups = strtok(NULL, " \t"); /* upspeed*/
			downs = strtok(NULL, " \t"); /* downspeed*/

			strtok(NULL, " \t"); /* maxups */
			strtok(NULL, " \t\n"); /* maxdowns */

			d = blobmsg_open_table(&buf, NULL);
			blobmsg_add_string(&buf, "macaddr", mac);
			blobmsg_add_string(&buf, "ups", ups);
			blobmsg_add_string(&buf, "downs", downs);
			blobmsg_close_table(&buf, d);
		}

		fclose(f);
	}

	blobmsg_close_array(&buf, c);
	ubus_send_reply(ctx, req, buf.head);

	return 0;
}

static int
rpc_web_get_test(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
	char lan_ip[16] = "";
	struct uci_package *p;
	struct uci_ptr ptr = {
		.package = "network",
		.section = "lan",
		.option  = "ipaddr"
	};

	if (uci_load(cursor, ptr.package, &p) || !p)
		goto out;

	uci_lookup_ptr(cursor, &ptr, NULL, true);

	if (ptr.o && ptr.o->type == UCI_TYPE_STRING)
		strcpy(lan_ip, ptr.o->v.string);

	uci_unload(cursor, p);

out:
	blob_buf_init(&buf, 0);
	blobmsg_add_string(&buf, "ipaddr", lan_ip);
	ubus_send_reply(ctx, req, buf.head);
	return 0;
}

static int
rpc_web_set_test(struct ubus_context *ctx, struct ubus_object *obj,
                   struct ubus_request_data *req, const char *method,
                   struct blob_attr *msg)
{
	struct blob_attr *tb[__RPC_D_MAX];

	struct uci_package *p;
	struct uci_ptr ptr = {
		.package = "network",
		.section = "lan",
		.option  = "ipaddr",
		.value  = "192.168.6.1"
	};

	printf("====value1===%s\n", ptr.value);
	blobmsg_parse(rpc_data_policy, __RPC_D_MAX, tb,
	              blob_data(msg), blob_len(msg));

	if (!tb[RPC_D_DATA] || blobmsg_data_len(tb[RPC_D_DATA]) >= 128)
		return UBUS_STATUS_INVALID_ARGUMENT;

	ptr.value = blobmsg_data(tb[RPC_D_DATA]);
	printf("====value2===%s\n", ptr.value);

	uci_load(cursor, ptr.package, &p);
	uci_set(cursor, &ptr);
	uci_save(cursor, p);
	uci_commit(cursor, &p, true);
	uci_unload(cursor, p);

	return 0;
}

static int
rpc_web_api_init(const struct rpc_daemon_ops *o, struct ubus_context *ctx)
{
	int rv = 0;

	static const struct ubus_method web_advance_methods[] = {
		UBUS_METHOD_NOARG("get_test",         rpc_web_get_test),
		UBUS_METHOD_NOARG("getddns",          rpc_web_getddns),
		UBUS_METHOD_NOARG("get_device_speed",          rpc_web_get_device_speed),
		UBUS_METHOD_NOARG("get_img_version",          rpc_web_get_img_version),
		UBUS_METHOD_NOARG("delete_tmp_bin",          rpc_web_delete_tmp_bin),
		UBUS_METHOD("set_test",               rpc_web_set_test, rpc_data_policy),
		UBUS_METHOD("cmd",                    rpc_web_cmd, rpc_cmd_policy),
	};

	static struct ubus_object_type web_advance_type =
		UBUS_OBJECT_TYPE("siflower-rpc-web-advance", web_advance_methods);

	static struct ubus_object advance_obj = {
		.name = "web.advance",
		.type = &web_advance_type,
		.methods = web_advance_methods,
		.n_methods = ARRAY_SIZE(web_advance_methods),
	};

	cursor = uci_alloc_context();

	if (!cursor)
		return UBUS_STATUS_UNKNOWN_ERROR;

	ops = o;

	rv |= ubus_add_object(ctx, &advance_obj);

	return rv;
}

struct rpc_plugin rpc_plugin = {
	.init = rpc_web_api_init
};
