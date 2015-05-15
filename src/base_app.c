#include <string.h>
#include "base_app.h"
#include "config_manager.h"
#include "timer_pump.h"
#include "socket_manager.h"
#include "action_handler.h"
#include "response_message.h"
#include "event_handler.h"

#define IPCAM_TIMER_CLIENT_NAME "_timer_client"

typedef struct _IpcamBaseAppPrivate
{
    IpcamConfigManager *config_manager;
    IpcamTimerManager *timer_manager;
    IpcamMessageManager *msg_manager;
    GHashTable *req_handler_hash;
    GHashTable *not_handler_hash;
    GMutex mutex;
} IpcamBaseAppPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(IpcamBaseApp, ipcam_base_app, IPCAM_SERVICE_TYPE);

static void ipcam_base_app_server_receive_string_impl(IpcamService *self,
                                                      const gchar *name,
                                                      const gchar *client_id,
                                                      const gchar *string);
static void ipcam_base_app_client_receive_string_impl(IpcamService *self,
                                                      const gchar *name,
                                                      const gchar *string);
static void ipcam_base_app_connect_to_timer(IpcamBaseApp *base_app);
static void ipcam_base_app_load_config(IpcamBaseApp *base_app);
static void ipcam_base_app_apply_config(IpcamBaseApp *base_app);
static void ipcam_base_app_message_manager_clear(GObject *base_app);
static void ipcam_base_app_on_timer(IpcamBaseApp *base_app, const gchar *timer_id);
static void ipcam_base_app_receive_string(IpcamBaseApp *base_app,
                                          const gchar *string,
                                          const gchar *name,
                                          const gint type,
                                          const gchar *client_id);
static void ipcam_base_app_action_handler(IpcamBaseApp *base_app, IpcamMessage *msg);
static void ipcam_base_app_notice_handler(IpcamBaseApp *base_app, IpcamMessage *msg);


static GObject *ipcam_base_app_constructor(GType self_type,
                                           guint n_properties,
                                           GObjectConstructParam *properties)
{
    GObject *obj;
    
    obj = G_OBJECT_CLASS(ipcam_base_app_parent_class)->constructor(self_type, n_properties, properties);

    return obj;
}
static void ipcam_base_app_dispose(GObject *self)
{
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(IPCAM_BASE_APP(self));

    if (priv->config_manager) g_clear_object(&priv->config_manager);
    if (priv->timer_manager) g_clear_object(&priv->timer_manager);
    if (priv->msg_manager) g_clear_object(&priv->msg_manager);

    G_OBJECT_CLASS(ipcam_base_app_parent_class)->dispose(self);
}
static void ipcam_base_app_finalize(GObject *self)
{
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(IPCAM_BASE_APP(self));
    g_mutex_clear(&priv->mutex);
    g_hash_table_destroy(priv->req_handler_hash);
    g_hash_table_destroy(priv->not_handler_hash);

    G_OBJECT_CLASS(ipcam_base_app_parent_class)->finalize(self);
}
static void ipcam_base_app_init(IpcamBaseApp *self)
{
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(self);
    priv->config_manager = g_object_new(IPCAM_CONFIG_MANAGER_TYPE, NULL);
    priv->timer_manager = g_object_new(IPCAM_TIMER_MANAGER_TYPE, NULL);
    priv->msg_manager = g_object_new(IPCAM_MESSAGE_MANAGER_TYPE, NULL);
    priv->req_handler_hash = g_hash_table_new(g_str_hash, g_str_equal);
    priv->not_handler_hash = g_hash_table_new(g_str_hash, g_str_equal);
    g_mutex_init(&priv->mutex);

    ipcam_base_app_load_config(self);
    ipcam_base_app_connect_to_timer(self);
    ipcam_base_app_add_timer(self, "clear_message_manager", "10", ipcam_base_app_message_manager_clear);

    ipcam_base_app_apply_config(self);
}
static void ipcam_base_app_class_init(IpcamBaseAppClass *klass)
{
    GObjectClass *this_class = G_OBJECT_CLASS(klass);
    this_class->constructor = &ipcam_base_app_constructor;
    this_class->dispose = &ipcam_base_app_dispose;
    this_class->finalize = &ipcam_base_app_finalize;

    IpcamServiceClass *service_class = IPCAM_SERVICE_CLASS(klass);
    service_class->server_receive_string = &ipcam_base_app_server_receive_string_impl;
    service_class->client_receive_string = &ipcam_base_app_client_receive_string_impl;
}
static void ipcam_base_app_server_receive_string_impl(IpcamService *self,
                                                      const gchar *name,
                                                      const gchar *client_id,
                                                      const gchar *string)
{
    ipcam_base_app_receive_string(IPCAM_BASE_APP(self), string, name, IPCAM_SOCKET_TYPE_SERVER, client_id);
}
static void ipcam_base_app_client_receive_string_impl(IpcamService *self,
                                                      const gchar *name,
                                                      const gchar *string)
{
    IpcamBaseApp *base_app = IPCAM_BASE_APP(self);
    if (0 == strcmp(name, IPCAM_TIMER_CLIENT_NAME))
    {
        ipcam_base_app_on_timer(base_app, string);
    }
    else
    {
        ipcam_base_app_receive_string(base_app, string, name, IPCAM_SOCKET_TYPE_CLIENT, NULL);
    }
}
static void ipcam_base_app_load_config(IpcamBaseApp *base_app)
{
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
    ipcam_config_manager_load_config(priv->config_manager, "config/app.yml");
    ipcam_config_manager_merge(priv->config_manager, "token", G_OBJECT_TYPE_NAME(base_app));
}
static void ipcam_base_app_connect_to_timer(IpcamBaseApp *base_app)
{
    const gchar *token = ipcam_base_app_get_config(base_app, "token");
    ipcam_service_connect_by_name(IPCAM_SERVICE(base_app),
                                  IPCAM_TIMER_CLIENT_NAME,
                                  IPCAM_TIMER_PUMP_ADDRESS,
                                  token);
}
void ipcam_base_app_add_timer(IpcamBaseApp *base_app,
                              const gchar *timer_id,
                              const gchar *interval,
                              TCFunc callback)
{
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
    ipcam_timer_manager_add_timer(priv->timer_manager, timer_id, G_OBJECT(base_app), callback);
    const gchar **strings = (const gchar **)g_new(gpointer, 3);
    strings[0] = timer_id;
    strings[1] = interval;
    strings[2] = NULL;
    const gchar *token = ipcam_base_app_get_config(base_app, "token");
    ipcam_service_send_strings(IPCAM_SERVICE(base_app), IPCAM_TIMER_CLIENT_NAME, strings, token);
    g_free(strings);
}
static void ipcam_base_app_message_manager_clear(GObject *base_app)
{
    g_return_if_fail(IPCAM_IS_BASE_APP(base_app));
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(IPCAM_BASE_APP(base_app));
    ipcam_message_manager_clear(priv->msg_manager);
}
static void ipcam_base_app_on_timer(IpcamBaseApp *base_app, const gchar *timer_id)
{
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
    ipcam_timer_manager_trig_timer(priv->timer_manager, timer_id);
}
static void ipcam_base_app_receive_string(IpcamBaseApp *base_app,
                                          const gchar *string,
                                          const gchar *name,
                                          const gint type,
                                          const gchar *client_id)
{
    IpcamMessage *msg = ipcam_message_parse_from_string(string);
    if (msg)
    {
        if (type == IPCAM_SOCKET_TYPE_SERVER && NULL != client_id)
        {
            gchar *strval;
            g_object_get(G_OBJECT(msg), "token", &strval, NULL);
            if (0 != strcmp(client_id, strval))
            {
                g_object_unref(msg);
                g_free(strval);
                return;
            }
            g_free(strval);
        }

        if (ipcam_message_is_request(msg))
        {
            ipcam_base_app_action_handler(base_app, msg);
        }
        else if (ipcam_message_is_notice(msg))
        {
            ipcam_base_app_notice_handler(base_app, msg);
        }
        else if (ipcam_message_is_response(msg))
        {
            IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
            ipcam_message_manager_handle(priv->msg_manager, msg);
        }
        else
        {
            // do nothing
        }

        g_object_unref(msg);
    }
}
static void ipcam_base_app_action_handler(IpcamBaseApp *base_app, IpcamMessage *msg)
{
    GType action_handler_class_type = G_TYPE_INVALID;
    gchar *strval;
    g_object_get(G_OBJECT(msg), "action", &strval, NULL);
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);

    g_mutex_lock(&priv->mutex);
    action_handler_class_type = (GType)g_hash_table_lookup(priv->req_handler_hash, (gpointer)strval);
    g_mutex_unlock(&priv->mutex);

    g_free(strval);
    if (G_TYPE_INVALID != action_handler_class_type)
    {
        IpcamActionHandler *handler = g_object_new(action_handler_class_type, "service", base_app, NULL);
        if (IPCAM_IS_ACTION_HANDLER(handler))
        {
            ipcam_action_handler_run(handler, msg);
        }
        g_object_unref(handler);
    }
}
static void ipcam_base_app_notice_handler(IpcamBaseApp *base_app, IpcamMessage *msg)
{
    GType event_handler_class_type = G_TYPE_INVALID;
    gchar *strval;
    g_object_get(G_OBJECT(msg), "event", &strval, NULL);
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);

    g_mutex_lock(&priv->mutex);
    event_handler_class_type = (GType)g_hash_table_lookup(priv->not_handler_hash, (gpointer)strval);
    g_mutex_unlock(&priv->mutex);

    g_free(strval);
    if (G_TYPE_INVALID != event_handler_class_type)
    {
        IpcamEventHandler *handler = g_object_new(event_handler_class_type, "service", base_app, NULL);
        if (IPCAM_IS_EVENT_HANDLER(handler))
        {
            ipcam_event_handler_run(handler, msg);
        }
        g_object_unref(handler);
    }
}

void ipcam_base_app_register_request_handler(IpcamBaseApp *base_app,
                                             const gchar *handler_name,
                                             GType handler_class_type)
{
    g_return_if_fail(IPCAM_IS_BASE_APP(base_app));
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);

    g_mutex_lock(&priv->mutex);
    if (!g_hash_table_contains(priv->req_handler_hash, (gpointer)handler_name))
        g_hash_table_insert(priv->req_handler_hash, (gpointer)handler_name, (gpointer)handler_class_type);
    g_mutex_unlock(&priv->mutex);
}

void ipcam_base_app_register_notice_handler(IpcamBaseApp *base_app,
                                            const gchar *handler_name,
                                            GType handler_class_type)
{
    g_return_if_fail(IPCAM_IS_BASE_APP(base_app));
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);

    g_mutex_lock(&priv->mutex);
    if (!g_hash_table_contains(priv->not_handler_hash, (gpointer)handler_name))
        g_hash_table_insert(priv->not_handler_hash, (gpointer)handler_name, (gpointer)handler_class_type);
    g_mutex_unlock(&priv->mutex);
}

void ipcam_base_app_send_message(IpcamBaseApp *base_app,
                                 IpcamMessage *msg,
                                 const gchar *name,
                                 const gchar *client_id,
                                 MsgHandler callback,
                                 guint timeout)
{
    g_return_if_fail(IPCAM_IS_BASE_APP(base_app));
    g_return_if_fail(IPCAM_IS_MESSAGE(msg));
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
    gboolean is_server = ipcam_service_is_server(IPCAM_SERVICE(base_app), name);
    const gchar *token = "";
    if (!is_server)
    {
        token = ipcam_base_app_get_config(base_app, "token");
    }
    g_object_set(G_OBJECT(msg), "token", token, NULL);
    if (ipcam_message_is_request(msg))
    {
        ipcam_message_manager_register(priv->msg_manager, msg, G_OBJECT(base_app), callback, timeout);
    }
    gchar *strings[2];
    strings[0] = (gchar *)ipcam_message_to_string(msg);
    strings[1] = NULL;
    ipcam_service_send_strings(IPCAM_SERVICE(base_app), name, strings, client_id);
    g_free(strings[0]);
}

gboolean ipcam_base_app_wait_response(IpcamBaseApp *base_app,
                                      const char *msg_id,
                                      gint64 timeout_ms,
                                      IpcamMessage **response)
{
	IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
	pthread_t cur_thread = pthread_self();
	pthread_t svr_thread = ipcam_base_service_get_thread(IPCAM_BASE_SERVICE(base_app));
	gboolean ret = FALSE;

	if (pthread_equal(cur_thread, svr_thread)) {
		g_warning("Should not call %s() in the same thread.\n", __func__);
		return FALSE;
	}

	ret = ipcam_message_manager_wait_for(priv->msg_manager, msg_id, timeout_ms, response);

	return ret;
}

const gchar *ipcam_base_app_get_config(IpcamBaseApp *base_app,
                                       const gchar *config_name)
{
    g_return_val_if_fail(IPCAM_IS_BASE_APP(base_app), NULL);
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
    return ipcam_config_manager_get(priv->config_manager, config_name);
}
GHashTable *ipcam_base_app_get_configs(IpcamBaseApp *base_app,
                                       const gchar *config_name)
{
    g_return_val_if_fail(IPCAM_IS_BASE_APP(base_app), NULL);
    IpcamBaseAppPrivate *priv = ipcam_base_app_get_instance_private(base_app);
    return ipcam_config_manager_get_collection(priv->config_manager, config_name);
}
static void ipcam_base_app_bind(gpointer key, gpointer value, gpointer user_data)
{
    IpcamBaseApp *base_app = IPCAM_BASE_APP(user_data);
    ipcam_service_bind_by_name(IPCAM_SERVICE(base_app), (gchar *)key, (gchar *)value);
}
static void ipcam_base_app_bind_publish(gpointer key, gpointer value, gpointer user_data)
{
    IpcamBaseApp *base_app = IPCAM_BASE_APP(user_data);
    ipcam_service_publish_by_name(IPCAM_SERVICE(base_app), (gchar *)key, (gchar *)value);
}
static void ipcam_base_app_connect(gpointer key, gpointer value, gpointer user_data)
{
    IpcamBaseApp *base_app = IPCAM_BASE_APP(user_data);
    const gchar *token = ipcam_base_app_get_config(base_app, "token");
    ipcam_service_connect_by_name(IPCAM_SERVICE(base_app), (gchar *)key, (gchar *)value, token);
}
static void ipcam_base_app_subscribe(gpointer key, gpointer value, gpointer user_data)
{
    IpcamBaseApp *base_app = IPCAM_BASE_APP(user_data);
    ipcam_service_subscirbe_by_name(IPCAM_SERVICE(base_app), (gchar *)key, (gchar *)value);
}
static void ipcam_base_app_apply_config(IpcamBaseApp *base_app)
{
    GHashTable *collection;
    
    collection = ipcam_base_app_get_configs(base_app, "bind");
    g_hash_table_foreach(collection, ipcam_base_app_bind, base_app);
    collection = ipcam_base_app_get_configs(base_app, "connect");
    g_hash_table_foreach(collection, ipcam_base_app_connect, base_app);
    collection = ipcam_base_app_get_configs(base_app, "publish");
    g_hash_table_foreach(collection, ipcam_base_app_bind_publish, base_app);
    collection = ipcam_base_app_get_configs(base_app, "subscribe");
    g_hash_table_foreach(collection, ipcam_base_app_subscribe, base_app);
}
