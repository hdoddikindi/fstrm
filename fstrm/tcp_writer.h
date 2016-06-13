/*
 * Copyright (c) 2016 Infoblox Inc.
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

#ifndef FSTRM_TCP_WRITER_H
#define FSTRM_TCP_WRITER_H

/**
 * \defgroup fstrm_tcp_writer fstrm_tcp_writer
 *
 * `fstrm_tcp_writer` is an interface for opening an \ref fstrm_writer object
 * that is backed by I/O on a stream-oriented (`SOCK_STREAM`) network socket.
 *
 * @{
 */

/**
 * Initialize an `fstrm_tcp_writer_options` object, which is needed to
 * configure the socket address to be opened by the writer.
 *
 * \return
 *	`fstrm_tcp_writer_options` object.
 */
struct fstrm_tcp_writer_options *
fstrm_tcp_writer_options_init(void);

/**
 * Destroy an `fstrm_tcp_writer_options` object.
 * 
 * \param twopt
 *	Pointer to `fstrm_tcp_writer_options` object.
 */
void
fstrm_tcp_writer_options_destroy(
	struct fstrm_tcp_writer_options **twopt);

/**
 * Set the `socket_addr` option. This is the network endpoint that will be
 * connected to as an `AF_INET` socket.
 *
 * \param twopt
 *	`fstrm_tcp_writer_options` object.
 * \param socket_addr
 *	The network endpoint for the `AF_INET` socket.
 */
void
fstrm_tcp_writer_options_set_socket_addr(
	struct fstrm_tcp_writer_options *twopt,
        const char *socket_addr);

/**
 * Initialize the `fstrm_writer` object. Note that the `AF_INET` socket will not
 * actually be opened until a subsequent call to fstrm_writer_open().
 *
 * \param twopt
 *	`fstrm_tcp_writer_options` object. Must be non-NULL, and have the
 *	`socket_addr` option set.
 * \param wopt
 *	`fstrm_writer_options` object. May be NULL, in which chase default
 *	values will be used.
 *
 * \return
 *	`fstrm_writer` object.
 * \retval
 *	NULL on failure.
 */
struct fstrm_writer *
fstrm_tcp_writer_init(
	const struct fstrm_tcp_writer_options *twopt,
	const struct fstrm_writer_options *wopt);

/**@}*/

#endif /* FSTRM_TCP_WRITER_H */
