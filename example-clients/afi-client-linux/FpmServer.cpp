//
// Copyright (C) 2017 by Open Source Routing.
//
// All rights reserved.
//
// Notice and Disclaimer: This code is licensed to you under the Apache
// License 2.0 (the "License"). You may not use this code except in compliance
// with the License. This code is not an official Juniper product. You can
// obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
//
// Third-Party Code: This code may depend on other components under separate
// copyright notice and license terms. Your use of the source code for those
// components is subject to the terms and conditions of the respective license
// as noted in the Third-Party source code file.
//

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <assert.h>

#include "fpm.h"

#include "FpmServer.h"
#include "Address.h"

int FpmServer::fd = -1;
std::map<int, FpmConn> FpmServer::connections;

void FpmServer::initialize(void)
{
	struct sockaddr_in sin;

	fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
	if (fd == -1)
		err(1, "socket");

	int enable = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))
	    < 0)
		warn("error setting SO_REUSEADDR");

	sin.sin_family = AF_INET;
	sin.sin_port = htons(FPM_DEFAULT_PORT);
	sin.sin_addr.s_addr = FPM_DEFAULT_IP;
	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		err(1, "bind");
	if (listen(fd, SOMAXCONN) == -1)
		err(1, "listen");
}

int FpmServer::accept(void)
{
	int newfd;
	struct sockaddr_in src;
	socklen_t len = sizeof(src);

	newfd = accept4(fd, (struct sockaddr *)&src, &len, SOCK_NONBLOCK);
	if (newfd == -1) {
		if (errno != EAGAIN && errno != EINTR && errno != ECONNABORTED)
			warn("accept4");
		return -1;
	}

	connections.insert(std::pair<int, FpmConn>(newfd, FpmConn(newfd)));

	std::cout << "accepted connection from FPM (" << Address(src.sin_addr)
		  << ":" << ntohs(src.sin_port) << ")" << std::endl;

	return newfd;
}

int FpmServer::getFd(void)
{
	return fd;
}

FpmConn &FpmServer::getConn(int fd)
{
	return connections.at(fd);
}

void FpmServer::deleteConn(int fd)
{
	connections.erase(fd);
}
