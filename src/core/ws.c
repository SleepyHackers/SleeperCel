#include <pthread.h>
#include <libwebsockets.h>
#include <stdio.h>
#include <string.h>

#include "ws.h"


volatile int force_exit = 0;
struct lws_context *context;

#if defined(LWS_OPENSSL_SUPPORT) && defined(LWS_HAVE_SSL_CTX_set1_param)
char crl_path[1024] = "";
#endif

#define LOCAL_RESOURCE_PATH "libwebsockets-test-server"
char *resource_path = LOCAL_RESOURCE_PATH;

/* mutex for locking threads that touch the websockets list */
pthread_mutex_t ws_mutex;

int debug_level = 7;

/* Lock websocket threads */
void ws_lock(int iCare)
{
	if (iCare)
		pthread_mutex_lock(&ws_mutex);
}

/* Unlock websocket threads */
void ws_unlock(int iCare)
{
	if (iCare)
		pthread_mutex_unlock(&ws_mutex);
}

/*
 * This demo server shows how to use libwebsockets for one or more
 * websocket protocols in the same server
 *
 * It defines the following websocket protocols:
 *
 *  dumb-increment-protocol:  once the socket is opened, an incrementing
 *				ascii string is sent down it every 50ms.
 *				If you send "reset\n" on the websocket, then
 *				the incrementing number is reset to 0.
 *
 *  lws-mirror-protocol: copies any received packet to every connection also
 *				using this protocol, including the sender
 */

enum ws_protocols {
	/* always first */
	PROTOCOL_HTTP = 0,
	PROTOCOL_CHAT,

	/* always last */
	DEMO_PROTOCOL_COUNT
};

struct per_session_data__http {
	lws_fop_fd_t fop_fd;

  	unsigned int client_finished:1;


	struct lws_spa *spa;
	char result[500 + LWS_PRE];
	int result_len;

	char filename[256];
	long file_length;
	lws_filefd_type post_fd;
};

struct per_session_data__chat {
	struct per_session_data__chat *list;
	struct timeval tv_established;
	int last;
	char ip[270];
	char user_agent[512];
	const char *pos;
	int len;
};


int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user,
		  void *in, size_t len)
{
}

int callback_chat(struct lws *wsi, enum lws_callback_reasons reason, void *user,
		  void *in, size_t len)
{
}

/* list of supported protocols and callbacks */

static struct lws_protocols protocols[] = {
	/* first protocol must always be HTTP handler */

	{
		"http-only",		/* name */
		callback_http,		/* callback */
		sizeof (struct per_session_data__http),	/* per_session_data_size */
		0,			/* max frame size / rx buffer */
	},
	{
		"chat",
		callback_chat,
		sizeof(struct per_session_data__chat),
		128,
	},
	{ NULL, NULL, 0, 0 } /* terminator */
};


void *thread_service(void *threadid)
{
	while (lws_service_tsi(context, 50, (int)(long)threadid) >= 0 && !force_exit)
		;

	pthread_exit(NULL);
}

void sighandler(int sig)
{
	force_exit = 1;
	lws_cancel_service(context);
}

static const struct lws_extension exts[] = {
	{
		"permessage-deflate",
		lws_extension_callback_pm_deflate,
		"permessage-deflate; client_no_context_takeover; client_max_window_bits"
	},
	{
		"deflate-frame",
		lws_extension_callback_pm_deflate,
		"deflate_frame"
	},
	{ NULL, NULL, NULL /* terminator */ }
};

int init_websockets(MAIN_CONFIG *config)
{
}

int amain(int argc, char **argv)
{
	struct lws_context_creation_info info;
	char interface_name[128] = "";
	const char *iface = NULL;
	pthread_t pthread_dumb, pthread_service[32];
	char cert_path[1024];
	char key_path[1024];
	int threads = 1;
	int use_ssl = 0;
	void *retval;
	int opts = 0;
	int n = 0;

	/*
	 * take care to zero down the info struct, he contains random garbaage
	 * from the stack otherwise
	 */
	memset(&info, 0, sizeof info);
	info.port = 7681;

	pthread_mutex_init(&ws_mutex, NULL);


	signal(SIGINT, sighandler);


	/* tell the library what debug level to emit and to send it to syslog */
	lws_set_log_level(debug_level, lwsl_emit_syslog);
	lwsl_notice("libwebsockets test server pthreads - license LGPL2.1+SLE\n");
	lwsl_notice("(C) Copyright 2010-2016 Andy Green <andy@warmcat.com>\n");

	printf("Using resource path \"%s\"\n", resource_path);

	info.iface = iface;
	info.protocols = protocols;
	info.extensions = exts;

	info.ssl_cert_filepath = NULL;
	info.ssl_private_key_filepath = NULL;

	if (use_ssl) {
		if (strlen(resource_path) > sizeof(cert_path) - 32) {
			lwsl_err("resource path too long\n");
			return -1;
		}
		sprintf(cert_path, "%s/libwebsockets-test-server.pem",
			resource_path);
		if (strlen(resource_path) > sizeof(key_path) - 32) {
			lwsl_err("resource path too long\n");
			return -1;
		}
		sprintf(key_path, "%s/libwebsockets-test-server.key.pem",
			resource_path);

		info.ssl_cert_filepath = cert_path;
		info.ssl_private_key_filepath = key_path;
	}
	info.gid = -1;
	info.uid = -1;
	info.options = opts;
	info.count_threads = threads;
	info.extensions = exts;
	info.max_http_header_pool = 4;

	context = lws_create_context(&info);
	if (context == NULL) {
		lwsl_err("libwebsocket init failed\n");
		return -1;
	}


	/*
	 * notice the actual number of threads may be capped by the library,
	 * so use lws_get_count_threads() to get the actual amount of threads
	 * initialized.
	 */

	for (n = 0; n < lws_get_count_threads(context); n++)
		if (pthread_create(&pthread_service[n], NULL, thread_service,
				   (void *)(long)n))
			lwsl_err("Failed to start service thread\n");

	/* wait for all the service threads to exit */

	while ((--n) >= 0)
		pthread_join(pthread_service[n], &retval);

	/* wait for pthread_dumb to exit */
	pthread_join(pthread_dumb, &retval);

done:
	lws_context_destroy(context);
	pthread_mutex_destroy(&ws_mutex);

	lwsl_notice("libwebsockets-test-server exited cleanly\n");


	return 0;
}
