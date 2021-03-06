#include "response_message.h"
#include <string.h>

enum
{
    PROP_0,
    
    IPCAM_RESPONSE_MESSAGE_ACTION = 1,
    IPCAM_RESPONSE_MESSAGE_ID = 2,
    IPCAM_RESPONSE_MESSAGE_CODE = 3,
    
    N_PROPERTIES
};

typedef struct _IpcamResponseMessagePrivate
{
    gchar *action;
    gchar *id;
    gchar *code;
} IpcamResponseMessagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(IpcamResponseMessage, ipcam_response_message, IPCAM_MESSAGE_TYPE);

static GParamSpec *obj_properties[N_PROPERTIES] = {NULL, };

static void ipcam_response_message_finalize(GObject *self)
{
    IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(IPCAM_RESPONSE_MESSAGE(self));
    g_free(priv->action);
    g_free(priv->id);
    g_free(priv->code);
    G_OBJECT_CLASS(ipcam_response_message_parent_class)->finalize(self);
}
static void ipcam_response_message_get_property(GObject *object,
                                                guint property_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
    IpcamResponseMessage *self = IPCAM_RESPONSE_MESSAGE(object);
    IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(self);
    switch(property_id)
    {
    case IPCAM_RESPONSE_MESSAGE_ACTION:
        {
            g_value_set_string(value, priv->action);
        }
        break;
    case IPCAM_RESPONSE_MESSAGE_ID:
        {
            g_value_set_string(value, priv->id);
        }
        break;
    case IPCAM_RESPONSE_MESSAGE_CODE:
        {
            g_value_set_string(value, priv->code);
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}
static void ipcam_response_message_set_property(GObject *object,
                                                guint property_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
    IpcamResponseMessage *self = IPCAM_RESPONSE_MESSAGE(object);
    IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(self);
    switch(property_id)
    {
    case IPCAM_RESPONSE_MESSAGE_ACTION:
        {
            g_free(priv->action);
            priv->action = g_value_dup_string(value);
            /* g_print("ipcam response message action: %s\n", priv->action); */
        }
        break;
    case IPCAM_RESPONSE_MESSAGE_ID:
        {
            g_free(priv->id);
            priv->id = g_value_dup_string(value);
            /* g_print("ipcam response message id: %s\n", priv->id); */
        }
        break;
    case IPCAM_RESPONSE_MESSAGE_CODE:
        {
            g_free(priv->code);
            priv->code = g_value_dup_string(value);
            /* g_print("ipcam response message code: %s\n", priv->code); */
        }
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
        break;
    }
}
static void ipcam_response_message_init(IpcamResponseMessage *self)
{
    g_object_set(G_OBJECT(self), "type", "response", "action", "", "id", "", "code", "", NULL);
}
static void ipcam_response_message_class_init(IpcamResponseMessageClass *klass)
{
    GObjectClass *this_class = G_OBJECT_CLASS(klass);
    this_class->finalize = &ipcam_response_message_finalize;
    this_class->get_property = &ipcam_response_message_get_property;
    this_class->set_property = &ipcam_response_message_set_property;

    obj_properties[IPCAM_RESPONSE_MESSAGE_ACTION] =
        g_param_spec_string("action",
                            "Response message action",
                            "Set response message action",
                            "", // default value
                            G_PARAM_READWRITE);
    obj_properties[IPCAM_RESPONSE_MESSAGE_ID] =
        g_param_spec_string("id",
                            "Response message id",
                            "Set response message id",
                            "", // default value
                            G_PARAM_READWRITE);
    obj_properties[IPCAM_RESPONSE_MESSAGE_CODE] =
        g_param_spec_string("code",
                            "Response message code",
                            "Set response message code",
                            "0", // default value
                            G_PARAM_READWRITE);
    
    g_object_class_install_properties(this_class, N_PROPERTIES, obj_properties);
}

gboolean ipcam_response_message_has_error(IpcamResponseMessage *response_message)
{
    IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(response_message);
    return (0 != strcmp(priv->code, "0"));
}

const gchar *ipcam_response_message_get_action(IpcamResponseMessage *request_message)
{
	IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(request_message);

	return priv->action;
}

const gchar *ipcam_response_message_get_id(IpcamResponseMessage *request_message)
{
	IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(request_message);

	return priv->id;
}

const gchar *ipcam_response_message_get_code(IpcamResponseMessage *request_message)
{
	IpcamResponseMessagePrivate *priv = ipcam_response_message_get_instance_private(request_message);

	return priv->code;
}
