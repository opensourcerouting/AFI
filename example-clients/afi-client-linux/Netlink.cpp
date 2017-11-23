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
#include <err.h>

#include <libmnl/libmnl.h>
#include <linux/if_link.h>
#include <linux/neighbour.h>
#include <linux/rtnetlink.h>

#include "Netlink.h"
#include "AfiClient.h"

Netlink::Netlink(void)
{
	// NOTE: recent versions of libmnl have the mnl_socket_open2()
	// function, which can be used to create non-blocking sockets with
	// the SOCK_NONBLOCK flag.
	socket = mnl_socket_open(NETLINK_ROUTE);
	if (socket == NULL)
		err(1, "mnl_socket_open");

	/* set socket to non-blocking mode */
	int fd = getFd();
	int flags;
	if ((flags = fcntl(fd, F_GETFL)) < 0)
		err(1, "fcntl(F_GETFL) failed for fd %d", fd);
	if (fcntl(fd, F_SETFL, (flags | O_NONBLOCK)) < 0)
		err(1, "fcntl failed setting fd %d non-blocking", fd);
}

Netlink::~Netlink(void)
{
	mnl_socket_close(socket);
}

int Netlink::setRecvBuf(uint32_t newsize)
{
	int fd = getFd();
	u_int32_t oldsize;
	socklen_t newlen = sizeof(newsize);
	socklen_t oldlen = sizeof(oldsize);
	int ret;

	ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &oldsize, &oldlen);
	if (ret < 0) {
		err(1, "Can't get receive buffer size");
		return -1;
	}

	ret = setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE, &newsize,
			 sizeof(newsize));
	if (ret < 0) {
		err(1, "Can't set receive buffer size");
		return -1;
	}

	ret = getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &newsize, &newlen);
	if (ret < 0) {
		err(1, "Can't get receive buffer size");
		return -1;
	}

	std::cout << "Setting netlink socket receive buffer size: " << oldsize
		  << " -> " << newsize << std::endl;

	return 0;
}

int Netlink::getFd(void)
{
	return mnl_socket_get_fd(socket);
}

void Netlink::bind(unsigned int groups)
{
	if (mnl_socket_bind(socket, groups, MNL_SOCKET_AUTOPID) < 0)
		err(1, "mnl_socket_bind");
}

int Netlink::data_ipv4_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);

	// skip unsupported attribute in user-space
	if (mnl_attr_type_valid(attr, RTA_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case RTA_DST:
	case RTA_OIF:
	case RTA_GATEWAY:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			warn("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	default:
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

int Netlink::data_ipv6_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);

	// skip unsupported attribute in user-space
	if (mnl_attr_type_valid(attr, RTA_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case RTA_OIF:
		if (mnl_attr_validate(attr, MNL_TYPE_U32) < 0) {
			warn("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	case RTA_DST:
	case RTA_GATEWAY:
		if (mnl_attr_validate2(attr, MNL_TYPE_BINARY,
				       sizeof(struct in6_addr))
		    < 0) {
			warn("mnl_attr_validate2");
			return MNL_CB_ERROR;
		}
		break;
	default:
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

int Netlink::data_cb_route(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[RTA_MAX + 1] = {};
	struct rtmsg *rm = (struct rtmsg *)mnl_nlmsg_get_payload(nlh);

	/* ignore connected routes (RTN_LOCAL) */
	switch (rm->rtm_type) {
	case RTN_UNICAST:
	case RTN_UNSPEC:
	case RTN_BLACKHOLE:
		break;
	case RTN_LOCAL:
	case RTN_UNREACHABLE:
	case RTN_PROHIBIT:
	default:
		return MNL_CB_OK;
	}

	if (rm->rtm_table != RT_TABLE_MAIN && rm->rtm_table != RT_TABLE_LOCAL
	    && rm->rtm_table != RT_TABLE_UNSPEC)
		return MNL_CB_OK;

	Route route;

	switch (rm->rtm_family) {
	case AF_INET:
		mnl_attr_parse(nlh, sizeof(*rm), data_ipv4_attr_cb, tb);
		if (!tb[RTA_DST])
			return MNL_CB_OK;

		route.prefix.set(
			*(struct in_addr *)mnl_attr_get_payload(tb[RTA_DST]));
		if (tb[RTA_OIF])
			route.nhDev = mnl_attr_get_u32(tb[RTA_OIF]);
		if (tb[RTA_GATEWAY])
			route.nhAddr.set(
				*(struct in_addr *)mnl_attr_get_payload(
					tb[RTA_GATEWAY]));
		break;
	case AF_INET6:
		mnl_attr_parse(nlh, sizeof(*rm), data_ipv6_attr_cb, tb);
		if (!tb[RTA_DST])
			return MNL_CB_OK;

		route.prefix.set(
			*(struct in6_addr *)mnl_attr_get_payload(tb[RTA_DST]));
		if (tb[RTA_OIF])
			route.nhDev = mnl_attr_get_u32(tb[RTA_OIF]);
		if (tb[RTA_GATEWAY])
			route.nhAddr.set(
				*(struct in6_addr *)mnl_attr_get_payload(
					tb[RTA_GATEWAY]));
		break;
	default:
		errx(1, "%s: unknown address-family", __func__);
		break;
	}

	route.prefixLen = rm->rtm_dst_len;
	if (rm->rtm_type == RTN_BLACKHOLE)
		route.blackhole = true;

	AfiClient *afiClient = AfiClient::getInstance();
	if (nlh->nlmsg_type == RTM_NEWROUTE)
		afiClient->addRoute(route);
	else
		afiClient->delRoute(route);

	return MNL_CB_OK;
}

int Netlink::data_neighbor_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);

	// skip unsupported attribute in user-space
	if (mnl_attr_type_valid(attr, NDA_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case NDA_LLADDR:
		if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0) {
			warn("mnl_attr_validate NDA_LLADDR");
			return MNL_CB_ERROR;
		}
	default:
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

int Netlink::data_cb_neighbor(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[NDA_MAX + 1] = {};
	struct ndmsg *nm = (struct ndmsg *)mnl_nlmsg_get_payload(nlh);

	mnl_attr_parse(nlh, sizeof(*nm), data_neighbor_attr_cb, tb);
	if (!tb[NDA_DST])
		return MNL_CB_OK;

	Neighbor neighbor;
	neighbor.ifIndex = nm->ndm_ifindex;

	switch (nm->ndm_family) {
	case AF_INET:
		neighbor.ipAddr.set(
			*(struct in_addr *)mnl_attr_get_payload(tb[NDA_DST]));
		break;
	case AF_INET6:
		neighbor.ipAddr.set(
			*(struct in6_addr *)mnl_attr_get_payload(tb[NDA_DST]));
		break;
	default:
		errx(1, "%s: unknown address-family", __func__);
		break;
	}

	if (tb[NDA_LLADDR]
	    && mnl_attr_get_payload_len(tb[NDA_LLADDR]) == ETH_ALEN)
		memcpy(&neighbor.macAddr,
		       (uint8_t *)mnl_attr_get_payload(tb[NDA_LLADDR]),
		       sizeof(neighbor.macAddr));

	AfiClient *afiClient = AfiClient::getInstance();
	if (nlh->nlmsg_type == RTM_NEWNEIGH && tb[NDA_LLADDR])
		afiClient->addNeighbor(neighbor);
	else
		afiClient->delNeighbor(neighbor);

	return MNL_CB_OK;
}

int Netlink::data_address_attr_cb(const struct nlattr *attr, void *data)
{
	const struct nlattr **tb = (const struct nlattr **)data;
	int type = mnl_attr_get_type(attr);

	/* skip unsupported attribute in user-space */
	if (mnl_attr_type_valid(attr, IFA_MAX) < 0)
		return MNL_CB_OK;

	switch (type) {
	case IFA_ADDRESS:
		if (mnl_attr_validate(attr, MNL_TYPE_BINARY) < 0) {
			warn("mnl_attr_validate");
			return MNL_CB_ERROR;
		}
		break;
	default:
		break;
	}
	tb[type] = attr;
	return MNL_CB_OK;
}

int Netlink::data_cb_address(const struct nlmsghdr *nlh, void *data)
{
	struct nlattr *tb[IFLA_MAX + 1] = {};
	struct ifaddrmsg *ifa = (struct ifaddrmsg *)mnl_nlmsg_get_payload(nlh);

	mnl_attr_parse(nlh, sizeof(*ifa), data_address_attr_cb, tb);
	if (!tb[IFA_ADDRESS])
		return MNL_CB_OK;

	Route route;
	route.nhDev = ifa->ifa_index;

	void *addr = mnl_attr_get_payload(tb[IFA_ADDRESS]);
	switch (ifa->ifa_family) {
	case AF_INET:
		route.prefix.set(*(struct in_addr *)addr);
		route.prefixLen = 32;
		break;
	case AF_INET6:
		route.prefix.set(*(struct in6_addr *)addr);
		route.prefixLen = 128;
		break;
	default:
		errx(1, "%s: unknown address-family", __func__);
		break;
	}

	AfiClient *afiClient = AfiClient::getInstance();
	if (nlh->nlmsg_type == RTM_NEWADDR)
		afiClient->addRoute(route);
	else
		afiClient->delRoute(route);

	return MNL_CB_OK;
}

int Netlink::data_cb(const struct nlmsghdr *nlh, void *data)
{
	switch (nlh->nlmsg_type) {
	case RTM_NEWROUTE:
	case RTM_DELROUTE:
		return data_cb_route(nlh, data);
	case RTM_NEWNEIGH:
	case RTM_DELNEIGH:
		return data_cb_neighbor(nlh, data);
	case RTM_NEWADDR:
	case RTM_DELADDR:
		return data_cb_address(nlh, data);
	default:
		return MNL_CB_OK;
	}
}

void Netlink::fetch(uint16_t type, int family)
{
	char buf[MNL_SOCKET_BUFFER_SIZE];
	struct nlmsghdr *nlh;
	struct rtmsg *rtm;
	int ret;
	unsigned int seq, portid;

	nlh = mnl_nlmsg_put_header(buf);
	nlh->nlmsg_type = type;
	nlh->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;
	nlh->nlmsg_seq = seq = time(NULL);
	rtm = (struct rtmsg *)mnl_nlmsg_put_extra_header(nlh,
							 sizeof(struct rtmsg));
	rtm->rtm_family = family;
	portid = mnl_socket_get_portid(socket);

	if (mnl_socket_sendto(socket, nlh, nlh->nlmsg_len) < 0)
		err(1, "mnl_socket_sendto");

	ret = mnl_socket_recvfrom(socket, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, seq, portid, data_cb, NULL);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(socket, buf, sizeof(buf));
	}
	if (ret == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		err(1, "mnl_socket_recvfrom");
	}
}

void Netlink::handle(void)
{
	char buf[sysconf(_SC_PAGESIZE)];
	int ret;

	ret = mnl_socket_recvfrom(socket, buf, sizeof(buf));
	while (ret > 0) {
		ret = mnl_cb_run(buf, ret, 0, 0, data_cb, NULL);
		if (ret <= MNL_CB_STOP)
			break;
		ret = mnl_socket_recvfrom(socket, buf, sizeof(buf));
	}
	if (ret == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		err(1, "mnl_socket_recvfrom");
	}
}

void Netlink::processBuffer(const void *buf, size_t numbytes)
{
	if (mnl_cb_run(buf, numbytes, 0, 0, data_cb, NULL) == MNL_CB_ERROR)
		warn("mnl_cb_run");
}
