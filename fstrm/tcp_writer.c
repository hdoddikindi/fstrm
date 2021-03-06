/*
 * Copyright (c) 2016 by Infoblox Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <netinet/in.h>

#include "fstrm-private.h"

struct fstrm_tcp_writer_options {
        char                    *socket_addr;
};

struct fstrm__tcp_writer {
	bool			connected;
	int			fd;
	struct sockaddr_in	sa;
};

struct fstrm_tcp_writer_options *
fstrm_tcp_writer_options_init(void)
{
	return my_calloc(1, sizeof(struct fstrm_tcp_writer_options));
}

void
fstrm_tcp_writer_options_destroy(struct fstrm_tcp_writer_options **twopt)
{
	if (*twopt != NULL) {
		my_free((*twopt)->socket_addr);
		my_free(*twopt);
	}
}

void
fstrm_tcp_writer_options_set_socket_addr(
	struct fstrm_tcp_writer_options *twopt,
        const char *socket_addr)
{
	my_free(twopt->socket_addr);
	if (socket_addr != NULL)
		twopt->socket_addr = my_strdup(socket_addr);
}

static fstrm_res
fstrm__tcp_writer_op_open(void *obj)
{
	struct fstrm__tcp_writer *w = obj;

	/* Nothing to do if the socket is already connected. */
	if (w->connected)
		return fstrm_res_success;

	/* Open an AF_TCP socket. Request socket close-on-exec if available. */
#if defined(SOCK_CLOEXEC)
	w->fd = socket(AF_INET, SOCK_STREAM, 0);
	if (w->fd < 0 && errno == EINVAL)
		w->fd = socket(AF_INET, SOCK_STREAM, 0);
#else
	w->fd = socket(AF_INET, SOCK_STREAM, 0);
#endif
	if (w->fd < 0)
		return fstrm_res_failure;

	/*
	 * Request close-on-exec if available. There is nothing that can be done
	 * if the F_SETFD call to fcntl() fails, so don't bother checking the
	 * return value.
	 *
	 * https://lwn.net/Articles/412131/
	 * [ Ghosts of Unix past, part 2: Conflated designs ]
	 */
#if defined(FD_CLOEXEC)
	int flags = fcntl(w->fd, F_GETFD, 0);
	if (flags != -1) {
		flags |= FD_CLOEXEC;
		(void) fcntl(w->fd, F_SETFD, flags);
	}
#endif

#if defined(SO_NOSIGPIPE)
	/*
	 * Ugh, no signals, please!
	 *
	 * https://lwn.net/Articles/414618/
	 * [ Ghosts of Unix past, part 3: Unfixable designs ]
	 */
	static const int on = 1;
	if (setsockopt(w->fd, SOL_SOCKET, SO_NOSIGPIPE, &on, sizeof(on)) != 0) {
		close(w->fd);
		return fstrm_res_failure;
	}
#endif

	/* Connect the AF_INET socket. */
	if (connect(w->fd, (struct sockaddr *) &w->sa, sizeof(w->sa)) < 0) {
		close(w->fd);
		return fstrm_res_failure;
	}

	w->connected = true;
	return fstrm_res_success;
}

static fstrm_res
fstrm__tcp_writer_op_close(void *obj)
{
	struct fstrm__tcp_writer *w = obj;
	if (w->connected) {
		w->connected = false;
		if (close(w->fd) != 0)
			return fstrm_res_failure;
		return fstrm_res_success;
	}
	return fstrm_res_failure;
}

static fstrm_res
fstrm__tcp_writer_op_read(void *obj, void *buf, size_t nbytes)
{
	struct fstrm__tcp_writer *w = obj;
	if (likely(w->connected)) {
		if (read_bytes(w->fd, buf, nbytes))
			return fstrm_res_success;
	}
	return fstrm_res_failure;
}

static fstrm_res
fstrm__tcp_writer_op_write(void *obj, const struct iovec *iov, int iovcnt)
{
	struct fstrm__tcp_writer *w = obj;

	size_t nbytes = 0;
	ssize_t written = 0;
	int cur = 0;
	struct msghdr msg = {
		.msg_iov = (struct iovec *) /* Grr! */ iov,
		.msg_iovlen = iovcnt,
	};

	if (unlikely(!w->connected))
		return fstrm_res_failure;

	for (int i = 0; i < iovcnt; i++)
		nbytes += iov[i].iov_len;

	for (;;) {
		do {
			written = sendmsg(w->fd, &msg, MSG_NOSIGNAL);
		} while (written == -1 && errno == EINTR);
		if (written == -1)
			return fstrm_res_failure;
		if (cur == 0 && written == (ssize_t) nbytes)
			return fstrm_res_success;

		while (written >= (ssize_t) msg.msg_iov[cur].iov_len)
		       written -= msg.msg_iov[cur++].iov_len;

		if (cur == iovcnt)
			return fstrm_res_success;

		msg.msg_iov[cur].iov_base = (void *)
			((char *) msg.msg_iov[cur].iov_base + written);
		msg.msg_iov[cur].iov_len -= written;
	}
}

static fstrm_res
fstrm__tcp_writer_op_destroy(void *obj)
{
	struct fstrm__tcp_writer *w = obj;
	my_free(w);
	return fstrm_res_success;
}

#define FSTRM_INET_LEN   15  /* "aaa.bbb.ccc.ddd" */

struct fstrm_writer *
fstrm_tcp_writer_init(const struct fstrm_tcp_writer_options *twopt,
                      const struct fstrm_writer_options *wopt)
{
	struct fstrm_rdwr *rdwr;
	struct fstrm__tcp_writer *tw;
        char *p, buf[FSTRM_INET_LEN + 1];
        int len;

	if (twopt->socket_addr == NULL)
		return NULL;

        p = strchr(twopt->socket_addr, ':');

        if (!p)
            return NULL;

        len = ((p - twopt->socket_addr) < FSTRM_INET_LEN) ? 
            (p - twopt->socket_addr) : FSTRM_INET_LEN;

        strncpy(&buf[0], twopt->socket_addr, len);
        buf[len] = 0;

	tw = my_calloc(1, sizeof(*tw));

        tw->sa.sin_family      = AF_INET;
        tw->sa.sin_port        = htons(atoi(p + 1));
        tw->sa.sin_addr.s_addr = inet_addr(&buf[0]);

	rdwr = fstrm_rdwr_init(tw);
	fstrm_rdwr_set_destroy(rdwr, fstrm__tcp_writer_op_destroy);
	fstrm_rdwr_set_open(rdwr, fstrm__tcp_writer_op_open);
	fstrm_rdwr_set_close(rdwr, fstrm__tcp_writer_op_close);
	fstrm_rdwr_set_read(rdwr, fstrm__tcp_writer_op_read);
	fstrm_rdwr_set_write(rdwr, fstrm__tcp_writer_op_write);
	return fstrm_writer_init(wopt, &rdwr);
}
