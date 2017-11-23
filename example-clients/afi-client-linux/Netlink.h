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

#ifndef __Netlink__
#define __Netlink__

#include <libmnl/libmnl.h>

#include "Route.h"
#include "Neighbor.h"

class Netlink
{
      public:
	Netlink(void);
	~Netlink(void);

	int setRecvBuf(uint32_t newsize);

	int getFd(void);

	void bind(unsigned int groups);

	void fetch(uint16_t type, int family);

	void handle(void);

	static void processBuffer(const void *buf, size_t numbytes);

      private:
	struct mnl_socket *socket;

	static int data_ipv4_attr_cb(const struct nlattr *attr, void *data);
	static int data_ipv6_attr_cb(const struct nlattr *attr, void *data);
	static int data_cb_route(const struct nlmsghdr *nlh, void *data);
	static int data_neighbor_attr_cb(const struct nlattr *attr, void *data);
	static int data_cb_neighbor(const struct nlmsghdr *nlh, void *data);
	static int data_address_attr_cb(const struct nlattr *attr, void *data);
	static int data_cb_address(const struct nlmsghdr *nlh, void *data);
	static int data_cb(const struct nlmsghdr *nlh, void *data);
};

#endif // __Netlink__
