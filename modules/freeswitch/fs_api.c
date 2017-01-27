/*
 * FreeSWITCH API
 *
 * Copyright (C) 2017 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 *
 * History:
 * --------
 *  2017-01-23 initial version (liviu)
 */

#include "../../parser/parse_uri.h"
#include "../../resolve.h"
#include "../../forward.h"
#include "../../ut.h"

#include "fs_api.h"

struct list_head *fs_boxes;

typedef struct _fs_mod_ref {
	str tag;
	struct list_head list;
} fs_mod_ref;

static fs_mod_ref *mk_fs_mod_ref(str *tag);
static void free_fs_mod_ref(fs_mod_ref *mod_tag);
static fs_evs *find_fs_evs(str *hostport);

static fs_mod_ref *mk_fs_mod_ref(str *tag)
{
	fs_mod_ref *fs_tag = NULL;

	fs_tag = shm_malloc(sizeof *fs_tag + tag->len);
	if (!fs_tag) {
		LM_ERR("out of mem\n");
		return NULL;
	}

	fs_tag->tag.s = (char *)(fs_tag + 1);
	fs_tag->tag.len = tag->len;
	memcpy(fs_tag->tag.s, tag->s, tag->len);

	return fs_tag;
}

static void free_fs_mod_ref(fs_mod_ref *mod_tag)
{
	mod_tag->tag.s = NULL;
	shm_free(mod_tag);
}

static int has_tag(fs_evs *evs, str *tag)
{
	struct list_head *ele;
	fs_mod_ref *mtag;

	list_for_each(ele, &evs->modlist) {
		mtag = list_entry(ele, fs_mod_ref, list);

		if (str_strcmp(&mtag->tag, tag) == 0) {
			return 0;
		}
	}

	return -1;
}

static fs_evs *find_fs_evs(str *hostport)
{
	struct list_head *ele;
	fs_evs *evs;

	list_for_each(ele, fs_boxes) {
		evs = list_entry(ele, fs_evs, list);

		if (str_strcmp(hostport, &evs->host) == 0) {
			return evs;
		}
	}

	return NULL;
}

static fs_evs *mk_fs_evs(str *hostport)
{
	fs_evs *evs;
	char *p;
	str st;
	unsigned int port;

	p = memchr(hostport->s, ':', hostport->len);
	if (p != NULL) {
		st.s = p + 1;
		st.len = hostport->len - (p + 1 - hostport->s);

		if (str2int(&st, &port) != 0) {
			LM_ERR("failed to parse port '%.*s' %d in host '%.*s'\n",
			       st.len, st.s, st.len, hostport->len, hostport->s);
			return NULL;
		}

		st.s = hostport->s;
		st.len = p - hostport->s;
	} else {
		st = *hostport;
		port = FS_DEFAULT_EVS_PORT;
	}


	evs = shm_malloc(sizeof *evs + st.len + 1);
	if (!evs) {
		LM_ERR("out of mem\n");
		return NULL;
	}
	memset(evs, 0, sizeof *evs);

	LM_DBG("new FS box: host=%.*s, port=%d\n", st.len, st.s, port);

	evs->host.s = (char *)(evs + 1);
	evs->host.len = st.len;
	memcpy(evs->host.s, st.s, st.len);
	evs->host.s[evs->host.len] = '\0';

	evs->port = port;
	return evs;
}

fs_evs *add_fs_event_sock(str *evs_str, str *tag, enum fs_evs_types type,
                               ev_hrbeat_cb_f scb, void *info)
{
	fs_evs *evs;
	fs_mod_ref *mtag;

	if (!evs_str->s || evs_str->len == 0 || !tag) {
		LM_ERR("bad params: '%.*s', %.*s\n", evs_str->len, evs_str->s,
		       tag->len, tag->s);
		return NULL;
	}

	evs = find_fs_evs(evs_str);
	if (evs) {
		if (!has_tag(evs, tag)) {
			mtag = mk_fs_mod_ref(tag);
			if (!mtag) {
				LM_ERR("mk tag failed\n");
				return NULL;
			}

			list_add(&mtag->list, &evs->modlist);
		}
	} else {
		evs = mk_fs_evs(evs_str);
		if (!evs) {
			LM_ERR("failed to create FS box!\n");
			return NULL;
		}
		evs->type = type;

		list_add(&evs->list, fs_boxes);
	}

	return evs;
}

int del_fs_event_sock(fs_evs *evs, str *tag)
{
	return 0;
}
