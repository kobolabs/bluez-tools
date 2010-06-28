/*
 *
 *  bluez-tools - a set of tools to manage bluetooth devices for linux
 *
 *  Copyright (C) 2010  Alexander Orlenko <zxteam@gmail.com>
 *
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "dbus-common.h"
#include "marshallers.h"
#include "audio.h"

#define BLUEZ_DBUS_AUDIO_INTERFACE "org.bluez.Audio"

#define AUDIO_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), AUDIO_TYPE, AudioPrivate))

struct _AudioPrivate {
	DBusGProxy *dbus_g_proxy;

	/* Properties */
	gchar *state;
};

G_DEFINE_TYPE(Audio, audio, G_TYPE_OBJECT);

enum {
	PROP_0,

	PROP_DBUS_OBJECT_PATH, /* readwrite, construct only */
	PROP_STATE /* readonly */
};

enum {
	PROPERTY_CHANGED,

	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

static void _audio_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void _audio_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);

static void property_changed_handler(DBusGProxy *dbus_g_proxy, const gchar *name, const GValue *value, gpointer data);

static void audio_dispose(GObject *gobject)
{
	Audio *self = AUDIO(gobject);

	/* DBus signals disconnection */
	dbus_g_proxy_disconnect_signal(self->priv->dbus_g_proxy, "PropertyChanged", G_CALLBACK(property_changed_handler), self);

	/* Properties free */
	g_free(self->priv->state);

	/* Chain up to the parent class */
	G_OBJECT_CLASS(audio_parent_class)->dispose(gobject);
}

static void audio_class_init(AudioClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	gobject_class->dispose = audio_dispose;

	g_type_class_add_private(klass, sizeof(AudioPrivate));

	/* Properties registration */
	GParamSpec *pspec;

	gobject_class->get_property = _audio_get_property;
	gobject_class->set_property = _audio_set_property;

	/* object DBusObjectPath [readwrite, construct only] */
	pspec = g_param_spec_string("DBusObjectPath", "dbus_object_path", "Adapter D-Bus object path", NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
	g_object_class_install_property(gobject_class, PROP_DBUS_OBJECT_PATH, pspec);

	/* string State [readonly] */
	pspec = g_param_spec_string("State", NULL, NULL, NULL, G_PARAM_READABLE);
	g_object_class_install_property(gobject_class, PROP_STATE, pspec);

	/* Signals registation */
	signals[PROPERTY_CHANGED] = g_signal_new("PropertyChanged",
			G_TYPE_FROM_CLASS(gobject_class),
			G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
			0, NULL, NULL,
			g_cclosure_bluez_marshal_VOID__STRING_BOXED,
			G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_VALUE);
}

static void audio_init(Audio *self)
{
	self->priv = AUDIO_GET_PRIVATE(self);

	g_assert(conn != NULL);
}

static void audio_post_init(Audio *self)
{
	g_assert(self->priv->dbus_g_proxy != NULL);

	/* DBus signals connection */

	/* PropertyChanged(string name, variant value) */
	dbus_g_proxy_add_signal(self->priv->dbus_g_proxy, "PropertyChanged", G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(self->priv->dbus_g_proxy, "PropertyChanged", G_CALLBACK(property_changed_handler), self, NULL);

	/* Properties init */
	GError *error = NULL;
	GHashTable *properties = audio_get_properties(self, &error);
	g_assert(error == NULL);
	g_assert(properties != NULL);

	/* string State [readonly] */
	if (g_hash_table_lookup(properties, "State")) {
		self->priv->state = g_value_dup_string(g_hash_table_lookup(properties, "State"));
	} else {
		self->priv->state = g_strdup("undefined");
	}

	g_hash_table_unref(properties);
}

static void _audio_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	Audio *self = AUDIO(object);

	switch (property_id) {
	case PROP_DBUS_OBJECT_PATH:
		g_value_set_string(value, audio_get_dbus_object_path(self));
		break;

	case PROP_STATE:
		g_value_set_string(value, audio_get_state(self));
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

static void _audio_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	Audio *self = AUDIO(object);

	switch (property_id) {
	case PROP_DBUS_OBJECT_PATH:
	{
		const gchar *dbus_object_path = g_value_get_string(value);
		g_assert(dbus_object_path != NULL);
		g_assert(self->priv->dbus_g_proxy == NULL);
		self->priv->dbus_g_proxy = dbus_g_proxy_new_for_name(conn, BLUEZ_DBUS_NAME, dbus_object_path, BLUEZ_DBUS_AUDIO_INTERFACE);
		audio_post_init(self);
	}
		break;

	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
		break;
	}
}

/* Methods */

/* void Connect() */
void audio_connect(Audio *self, GError **error)
{
	g_assert(AUDIO_IS(self));

	dbus_g_proxy_call(self->priv->dbus_g_proxy, "Connect", error, G_TYPE_INVALID, G_TYPE_INVALID);
}

/* void Disconnect() */
void audio_disconnect(Audio *self, GError **error)
{
	g_assert(AUDIO_IS(self));

	dbus_g_proxy_call(self->priv->dbus_g_proxy, "Disconnect", error, G_TYPE_INVALID, G_TYPE_INVALID);
}

/* dict GetProperties() */
GHashTable *audio_get_properties(Audio *self, GError **error)
{
	g_assert(AUDIO_IS(self));

	GHashTable *ret;
	if (!dbus_g_proxy_call(self->priv->dbus_g_proxy, "GetProperties", error, G_TYPE_INVALID, DBUS_TYPE_G_STRING_VARIANT_HASHTABLE, &ret, G_TYPE_INVALID)) {
		return NULL;
	}

	return ret;
}

/* Properties access methods */
const gchar *audio_get_dbus_object_path(Audio *self)
{
	g_assert(AUDIO_IS(self));

	return dbus_g_proxy_get_path(self->priv->dbus_g_proxy);
}

const gchar *audio_get_state(Audio *self)
{
	g_assert(AUDIO_IS(self));

	return self->priv->state;
}

/* Signals handlers */
static void property_changed_handler(DBusGProxy *dbus_g_proxy, const gchar *name, const GValue *value, gpointer data)
{
	Audio *self = AUDIO(data);

	if (g_strcmp0(name, "State") == 0) {
		g_free(self->priv->state);
		self->priv->state = g_value_dup_string(value);
	}

	g_signal_emit(self, signals[PROPERTY_CHANGED], 0, name, value);
}

