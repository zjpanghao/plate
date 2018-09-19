/*
  A trivial static http webserver using Libevent's evhttp.

  This is not the best code in the world, and it does some fairly stupid stuff
  that you would never want to do in a production webserver. Caveat hackor!

 */

/* Compatibility for possible missing IPv6 declarations */
//#include "../util-internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#ifndef S_ISDIR
#define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif
#else
#include <sys/stat.h>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#endif

#include <event2/event.h>
#include <event2/http.h>
#include <event2/buffer.h>
#include <event2/util.h>
#include <event2/keyvalq_struct.h>

#ifdef EVENT__HAVE_NETINET_IN_H
#include <netinet/in.h>
# ifdef _XOPEN_SOURCE_EXTENDED
#  include <arpa/inet.h>
# endif
#endif
#include "event2/http_compat.h"
#include "plate.h"
#include "base64/base64.h"
#include "json/json.h"
#include "event2/http.h"

#include<opencv2/opencv.hpp>
#include<opencv/highgui.h>

#ifdef _WIN32
#ifndef stat
#define stat _stat
#endif
#ifndef fstat
#define fstat _fstat
#endif
#ifndef open
#define open _open
#endif
#ifndef close
#define close _close
#endif
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#endif
#endif
#include <thread>
#include <vector>
#include <glog/logging.h>

#define MAX_IMG_SIZE 1024*1024*50
class PlateArg {
  public:
   PlateArg()  
     :url("/identify?"),
      dataMem((char*)mem + strlen(url)) {
      strcpy(mem, url);
   }
  int channel;
  char mem[MAX_IMG_SIZE];
  const char *url = "/identify?";
  char * const dataMem;
};


char uri_root[512];

static const struct table_entry {
	const char *extension;
	const char *content_type;
} content_type_table[] = {
	{ "txt", "text/plain" },
	{ "c", "text/plain" },
	{ "h", "text/plain" },
	{ "html", "text/html" },
	{ "htm", "text/htm" },
	{ "css", "text/css" },
	{ "gif", "image/gif" },
	{ "jpg", "image/jpeg" },
	{ "jpeg", "image/jpeg" },
	{ "png", "image/png" },
	{ "pdf", "application/pdf" },
	{ "ps", "application/postscript" },
	{ NULL, NULL },
};

using namespace cv;

/* Try to guess a good content-type for 'path' */
static const char *
guess_content_type(const char *path)
{
	const char *last_period, *extension;
	const struct table_entry *ent;
	last_period = strrchr(path, '.');
	if (!last_period || strchr(last_period, '/'))
		goto not_found; /* no exension */
	extension = last_period + 1;
	for (ent = &content_type_table[0]; ent->extension; ++ent) {
		if (!evutil_ascii_strcasecmp(ent->extension, extension))
			return ent->content_type;
	}

not_found:
	return "application/misc";
}

void logFile(char *name,  char*data, int rows, int cols) {
  Mat m(rows,cols, CV_8UC3);
  std::cout << m.channels();
  MatIterator_<Vec3b> it = m.begin<Vec3b>();
  while (it != m.end<Vec3b>()){
    
      (*it)[2] = *data++;
      (*it)[1] = *data++;
     (*it)[0] = *data++;
  
    it++;
  }
  imwrite(name, m);
}

#define STRLEN(c) (c == NULL ? -1 : strlen(c))
#define NULL_VALUE(c) (c == NULL ? "null" : c)
#define TO_LOWER(c) ((c) >= 'A' && (c) <= 'Z' ? (c) + 'a' - 'A' : (c))
// #define TO_LOWER(c) ((c))
 #define ctoi(c) \
  (((c) >= '0' && (c) <= '9') ? (c) -'0' : ((c) >= 'a' && (c) <= 'f' ? (c) - 'a' + 10 : 0))

 static std::vector<unsigned char> errorData;
static void identifyCb(struct evhttp_request *req, void *arg) {
  struct evkeyvalq keys = {0};
  evbuffer *response = evbuffer_new();
  int rc = 0;
  std::vector<TH_PlateIDResult> result;
  auto it = result.begin();
  Json::Value root;
  Json::Value res;
  evbuffer *buf = evhttp_request_get_input_buffer(req);
  std::vector<unsigned char> out;
  PlateArg *plateArg = (PlateArg*)arg;
  char *postBuf = plateArg->mem;
  char *pbuf = plateArg->dataMem;
  *pbuf = 0;
  int total = 0;
	while (evbuffer_get_length(buf)) {
		int n;
		char cbuf[128];
		n = evbuffer_remove(buf, cbuf, sizeof(cbuf));
    if (n <= 0) {
      continue;
    }
    total += n;
    if (total >= MAX_IMG_SIZE - 1024) {
      evbuffer_add_printf(response, "{\"error_code:\"%s}", "-1");
	    evhttp_send_reply(req, 200, "OK", response);
      return;
    }
		if (n > 0) {
      memcpy(pbuf, cbuf, n);
      pbuf += n;
    }
	}
  if (total >= 1) {
    *pbuf = 0;
  }
  evhttp_parse_query(postBuf, &keys);
  const char *num = evhttp_find_header(&keys, "num");
  const char *data = evhttp_find_header(&keys, "data");
  const char *width = evhttp_find_header(&keys, "width");
  const char *height = evhttp_find_header(&keys, "height");
  LOG(INFO) << "num:"<< NULL_VALUE(num) << "width:" << NULL_VALUE(width) << "height:" << NULL_VALUE(height) << "data size:" << STRLEN(data);

  if (num == NULL || data == NULL || width == NULL || height == NULL) {
    evbuffer_add_printf(response, "{\"error_code:\"%s}", "-2");
	  evhttp_send_reply(req, 200, "OK", response);
    goto done;
  }
  LOG(INFO) << data + (strlen(data) - 10);
  {
    int datalen = strlen(data);
    out.reserve(datalen / 2 + 1);
     while (datalen > 1) {
       unsigned char c = (ctoi(TO_LOWER(*data))<< 4) | ctoi(TO_LOWER(*data + 1));
       out.push_back(c);
       data += 2;
       datalen -= 2;
     }
  }
  
  if (out.size() != atoi(width) *atoi(height) *3) {
    LOG(INFO) << "decode size:" << out.size();
    evbuffer_add_printf(response, "{\"error_code:\"%s}", "-3");
    evhttp_send_reply(req, 200, "OK", response);
    goto done;
  }
  static int inx = 0;
  char name[64];
  sprintf(name, "/home/panghao/pic/%d-%d.jpg", plateArg->channel, inx);
  inx = (inx + 1) % 100;
 
  // logFile(name, (char*)&out[0], atoi(height), atoi(width));
  rc = identify(plateArg->channel, &out[0], atoi(width), atoi(height), atoi(num),  &result);
  if (rc != 0) {
    Json::Value responseValue;
    responseValue["error_code"] = -4;
    responseValue["ep_err"] = rc;
    evbuffer_add_printf(response, "%s", responseValue.toStyledString().c_str());
    evhttp_send_reply(req, 200, "OK", response);
    goto done;
  }
 
  root["channel"] = plateArg->channel;
  root["error_code"] = 0;
  it = result.begin();
  while (it != result.end()) {
    Json::Value item;
    Json::Value rect;
    item["license"] = it->license;
    LOG(INFO) << name <<" "<< it->license << " marks" << it->nConfidence;
    item["confidence"] = it->nConfidence;
    rect["top"] = it->rcLocation.top;
    rect["bottom"] = it->rcLocation.bottom;
    rect["left"] = it->rcLocation.left;
    rect["right"] = it->rcLocation.right;
    item["rect"] = rect;
    item["platecolor"] = it->nColor;
    res.append(item);
    it++;
  }
  root["data"] = res;
  evbuffer_add_printf(response, "%s", root.toStyledString().c_str());
	evhttp_send_reply(req, 200, "OK", response);

  done:
    evhttp_clear_headers(&keys);
    //if (buf) {
    //    evbuffer_free(buf);
   // }
}

/* This callback gets invoked when we get any http request that doesn't match
 * any other callback.  Like any evhttp server callback, it has a simple job:
 * it must eventually call evhttp_send_error() or evhttp_send_reply().
 */
static void
send_document_cb(struct evhttp_request *req, void *arg)
{
	struct evbuffer *evb = NULL;
	const char *docroot = (const char*)arg;
	const char *uri = evhttp_request_get_uri(req);
	struct evhttp_uri *decoded = NULL;
	const char *path;
	char *decoded_path;
	char *whole_path = NULL;
	size_t len;
	int fd = -1;
	struct stat st;

	if (evhttp_request_get_command(req) != EVHTTP_REQ_GET) {
		//login_request_cb(req, arg);
		return;
	}

	printf("Got a GET request for <%s>\n",  uri);

	/* Decode the URI */
	decoded = evhttp_uri_parse(uri);
	if (!decoded) {
		printf("It's not a good URI. Sending BADREQUEST\n");
		evhttp_send_error(req, HTTP_BADREQUEST, 0);
		return;
	}

	/* Let's see what path the user asked for. */
	path = evhttp_uri_get_path(decoded);
	if (!path) path = "/";

	/* We need to decode it, to see what path the user really wanted. */
	decoded_path = evhttp_uridecode(path, 0, NULL);
	if (decoded_path == NULL)
		goto err;
	/* Don't allow any ".."s in the path, to avoid exposing stuff outside
	 * of the docroot.  This test is both overzealous and underzealous:
	 * it forbids aceptable paths like "/this/one..here", but it doesn't
	 * do anything to prevent symlink following." */
	if (strstr(decoded_path, ".."))
		goto err;

	len = strlen(decoded_path)+strlen(docroot)+2;
	if (!(whole_path = (char*)malloc(len))) {
		perror("malloc");
		goto err;
	}
	evutil_snprintf(whole_path, len, "%s/%s", docroot, decoded_path);

	if (stat(whole_path, &st)<0) {
		goto err;
	}

	/* This holds the content we're sending. */
	evb = evbuffer_new();

	if (S_ISDIR(st.st_mode)) {
		/* If it's a directory, read the comments and make a little
		 * index page */
#ifdef _WIN32
		HANDLE d;
		WIN32_FIND_DATAA ent;
		char *pattern;
		size_t dirlen;
#else
		DIR *d;
		struct dirent *ent;
#endif
		const char *trailing_slash = "";

		if (!strlen(path) || path[strlen(path)-1] != '/')
			trailing_slash = "/";

#ifdef _WIN32
		dirlen = strlen(whole_path);
		pattern = (char*)malloc(dirlen+3);
		memcpy(pattern, whole_path, dirlen);
		pattern[dirlen] = '\\';
		pattern[dirlen+1] = '*';
		pattern[dirlen+2] = '\0';
		d = FindFirstFileA(pattern, &ent);
		free(pattern);
		if (d == INVALID_HANDLE_VALUE)
			goto err;
#else
		if (!(d = opendir(whole_path)))
			goto err;
#endif

		evbuffer_add_printf(evb,
                    "<!DOCTYPE html>\n"
                    "<html>\n <head>\n"
                    "  <meta charset='utf-8'>\n"
		    "  <title>%s</title>\n"
		    "  <base href='%s%s'>\n"
		    " </head>\n"
		    " <body>\n"
		    "  <h1>%s</h1>\n"
		    "  <ul>\n",
		    decoded_path, /* XXX html-escape this. */
		    path, /* XXX html-escape this? */
		    trailing_slash,
		    decoded_path /* XXX html-escape this */);
#ifdef _WIN32
		do {
			const char *name = ent.cFileName;
#else
		while ((ent = readdir(d))) {
			const char *name = ent->d_name;
#endif
			evbuffer_add_printf(evb,
			    "    <li><a href=\"%s\">%s</a>\n",
			    name, name);/* XXX escape this */
#ifdef _WIN32
		} while (FindNextFileA(d, &ent));
#else
		}
#endif
		evbuffer_add_printf(evb, "</ul></body></html>\n");
#ifdef _WIN32
		FindClose(d);
#else
		closedir(d);
#endif
		evhttp_add_header(evhttp_request_get_output_headers(req),
		    "Content-Type", "text/html");
	} else {
		/* Otherwise it's a file; add it to the buffer to get
		 * sent via sendfile */
		const char *type = guess_content_type(decoded_path);
		if ((fd = open(whole_path, O_RDONLY)) < 0) {
			perror("open");
			goto err;
		}

		if (fstat(fd, &st)<0) {
			/* Make sure the length still matches, now that we
			 * opened the file :/ */
			perror("fstat");
			goto err;
		}
		evhttp_add_header(evhttp_request_get_output_headers(req),
		    "Content-Type", type);
		evbuffer_add_file(evb, fd, 0, st.st_size);
	}

	evhttp_send_reply(req, 200, "OK", evb);
	goto done;
err:
	evhttp_send_error(req, 404, "Document was not found");
	if (fd>=0)
		close(fd);
done:
	if (decoded)
		evhttp_uri_free(decoded);
	if (decoded_path)
		free(decoded_path);
	if (whole_path)
		free(whole_path);
	if (evb)
		evbuffer_free(evb);
}

static void
syntax(void)
{
	fprintf(stdout, "Syntax: http-server <docroot>\n");
}

void httpThread(void *param) {
  LOG(INFO) << "new Thread";
  struct event_base *base = (struct event_base *)param;
  event_base_dispatch(base);
}
void ev_server_start_multhread(int port, int nThread) {
  if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return;
  evutil_socket_t fd;
  struct evconnlistener *listener = NULL;
  for (int i = 0; i < nThread; i++) {
    struct event_base *base = event_base_new();
  	if (!base) {
  		LOG(ERROR) << "Couldn't create an event_base: exiting";
  		return;
  	}

      /* Create a new evhttp object to handle requests. */
  	struct evhttp *http = evhttp_new(base);
  	if (!http) {
  		LOG(ERROR) << "couldn't create evhttp. Exiting.";
  		return;
  	}
     /* The /dump URI will dump all requests to stdout and say 200 ok. */
    PlateArg *arg = new PlateArg();
    arg->channel = i;
    evhttp_set_cb(http, "/identify", identifyCb, (void*)arg);
    if (i == 0) {
      struct evhttp_bound_socket *bound;
      bound = evhttp_bind_socket_with_handle(http, "0.0.0.0", port);
      if (!bound) {
        return;
      }
      fd = evhttp_bound_socket_get_fd(bound);
    } else {
      evhttp_accept_socket(http, fd);
    }

   
    std::thread t(httpThread, base);
    t.detach();    
  }
}


int ev_server_start(int port)
{
	struct event_base *base;
	struct evhttp *http;
	struct evhttp_bound_socket *handle;

#ifdef _WIN32
	WSADATA WSAData;
	WSAStartup(0x101, &WSAData);
#else
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		return (1);
#endif

	base = event_base_new();
	if (!base) {
		fprintf(stderr, "Couldn't create an event_base: exiting\n");
		return 1;
	}

	/* Create a new evhttp object to handle requests. */
	http = evhttp_new(base);
	if (!http) {
		fprintf(stderr, "couldn't create evhttp. Exiting.\n");
		return 1;
	}

	/* The /dump URI will dump all requests to stdout and say 200 ok. */
  evhttp_set_cb(http, "/identify", identifyCb, NULL);


	/* We want to accept arbitrary requests, so we need to set a "generic"
	 * cb.  We can also add callbacks for specific paths. */
	//evhttp_set_gencb(http, send_document_cb, argv[1]);

	/* Now we tell the evhttp what port to listen on */
	handle = evhttp_bind_socket_with_handle(http, "0.0.0.0", port);
	if (!handle) {
		fprintf(stderr, "couldn't bind to port %d. Exiting.\n",
		    (int)port);
		return 1;
	}

	{
		/* Extract and display the address we're listening on. */
		struct sockaddr_storage ss;
		evutil_socket_t fd;
		ev_socklen_t socklen = sizeof(ss);
		char addrbuf[128];
		void *inaddr;
		const char *addr;
		int got_port = -1;
		fd = evhttp_bound_socket_get_fd(handle);
		memset(&ss, 0, sizeof(ss));
		if (getsockname(fd, (struct sockaddr *)&ss, &socklen)) {
			perror("getsockname() failed");
			return 1;
		}
		if (ss.ss_family == AF_INET) {
			got_port = ntohs(((struct sockaddr_in*)&ss)->sin_port);
			inaddr = &((struct sockaddr_in*)&ss)->sin_addr;
		} else if (ss.ss_family == AF_INET6) {
			got_port = ntohs(((struct sockaddr_in6*)&ss)->sin6_port);
			inaddr = &((struct sockaddr_in6*)&ss)->sin6_addr;
		} else {
			fprintf(stderr, "Weird address family %d\n",
			    ss.ss_family);
			return 1;
		}
		addr = evutil_inet_ntop(ss.ss_family, inaddr, addrbuf,
		    sizeof(addrbuf));
		if (addr) {
			printf("Listening on %s:%d\n", addr, got_port);
			evutil_snprintf(uri_root, sizeof(uri_root),
			    "http://%s:%d",addr,got_port);
		} else {
			fprintf(stderr, "evutil_inet_ntop failed\n");
			return 1;
		}
	}

	event_base_dispatch(base);

	return 0;
}
