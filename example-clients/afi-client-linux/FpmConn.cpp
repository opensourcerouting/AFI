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

#include <iostream>
#include <algorithm>

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>
#include <assert.h>

#include "fpm.h"

#include "FpmConn.h"
#include "Netlink.h"
#include "AfiClient.h"

FpmConn::FpmConn(int fd)
    : bufSize(FPM_MAX_MSG_LEN * 64)
    , pos(0)
{
	this->fd = fd;
	buffer.resize(bufSize);
}

FpmConn::~FpmConn(void)
{
}

int FpmConn::getFd(void)
{
	return fd;
}

int FpmConn::handle(void)
{
	ssize_t n;
	fpm_msg_hdr_t *hdr;
	size_t msg_len;
	size_t start = 0, left;

	n = read(fd, &buffer[pos], bufSize - pos);
	if (n == 0) {
		return -1;
	} else if (n < 0) {
		if (errno != EAGAIN && errno != EINTR) {
			warn("read");
			return -1;
		}
		return 0;
	}
	pos += n;

	while (true) {
		hdr = (fpm_msg_hdr_t *)(&buffer[start]);
		left = pos - start;

		if (!fpm_msg_ok(hdr, left))
			break;

		msg_len = fpm_msg_data_len(hdr);

		switch (hdr->msg_type) {
		case FPM_MSG_TYPE_NETLINK:
			Netlink::processBuffer(fpm_msg_data(hdr), msg_len);
			break;
		case FPM_MSG_TYPE_PROTOBUF:
			FpmConn::processProtobuf(fpm_msg_data(hdr), msg_len);
			break;
		default:
			warnx("%s: unknown message type", __func__);
			break;
		}
		start += msg_len + FPM_MSG_HDR_LEN;
	}

	pos -= start;
	std::rotate(buffer.begin(), buffer.begin() + start, buffer.end());

	return 0;
}

void FpmConn::processProtobuf(const void *buf, size_t numbytes)
{
	fpm::Message msg;
	if (!msg.ParseFromArray(buf, numbytes)) {
		warnx("%s: error parsing protobuf message", __func__);
		return;
	}

	if (msg.has_add_route())
		FpmConn::processProtobufAddRoute(msg.add_route());
	else if (msg.has_delete_route())
		FpmConn::processProtobufDelRoute(msg.delete_route());
}

void FpmConn::processProtobufAddRoute(const fpm::AddRoute &fpm_route)
{
	AfiClient *afiClient = AfiClient::getInstance();
	Route route;
	int af;

	switch (fpm_route.address_family()) {
	case qpb::IPV4:
		af = AF_INET;
		break;
	case qpb::IPV6:
		af = AF_INET6;
		break;
	default:
		warnx("%s: unknown address-family", __func__);
		return;
	}

	std::string prefix = fpm_route.key().prefix().bytes();
	route.prefix.set(af, prefix.c_str(), prefix.length());
	route.prefixLen = fpm_route.key().prefix().length();

	if (fpm_route.route_type() == fpm::BLACKHOLE)
		route.blackhole = true;
	else if (fpm_route.nexthops().size() > 0) {
		fpm::Nexthop fpm_nexthop = fpm_route.nexthops(0);

		if (fpm_nexthop.has_if_id())
			route.nhDev = fpm_nexthop.if_id().index();
		if (fpm_nexthop.has_address()) {
			if (fpm_nexthop.address().has_v4()) {
				struct in_addr address;
				address.s_addr = ntohl(
					fpm_nexthop.address().v4().value());
				route.nhAddr.set(address);
			} else if (fpm_nexthop.address().has_v6()) {
				std::string address =
					fpm_nexthop.address().v6().bytes();
				route.nhAddr.set(AF_INET6, address.c_str(),
						 address.length());
			}
		}
	}

	afiClient->addRoute(route);
}

void FpmConn::processProtobufDelRoute(const fpm::DeleteRoute &fpm_route)
{
	AfiClient *afiClient = AfiClient::getInstance();
	Route route;
	int af;

	switch (fpm_route.address_family()) {
	case qpb::IPV4:
		af = AF_INET;
		break;
	case qpb::IPV6:
		af = AF_INET6;
		break;
	default:
		warnx("%s: unknown address-family", __func__);
		return;
	}

	std::string prefix = fpm_route.key().prefix().bytes();
	route.prefix.set(af, prefix.c_str(), prefix.length());
	route.prefixLen = fpm_route.key().prefix().length();

	afiClient->delRoute(route);
}
