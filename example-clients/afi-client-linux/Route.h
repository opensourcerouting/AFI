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

#ifndef _ROUTE_H_
#define _ROUTE_H_

#include <iostream>
#include <sstream>

#include <net/if.h>

#include "Address.h"

class Route
{
      public:
	Route(void)
	    : prefix()
	    , prefixLen()
	    , blackhole()
	    , nhAddr()
	    , nhDev()
	{
	}

	bool operator<(const Route &route) const
	{
		if (prefix < route.prefix)
			return true;
		if (prefix > route.prefix)
			return false;

		if (prefixLen < route.prefixLen)
			return true;
		if (prefixLen > route.prefixLen)
			return false;

		if (nhAddr < route.nhAddr)
			return true;
		if (nhAddr > route.nhAddr)
			return false;

		if (nhDev < route.nhDev)
			return true;
		if (nhDev > route.nhDev)
			return false;

		return blackhole < route.blackhole;
	}

	bool operator>(const Route &route) const
	{
		return route < *this;
	}

	bool operator==(const Route &route) const
	{
		return (*this < route == route < *this);
	}

	std::string str(void) const
	{
		std::stringstream ss;
		char ifname[IFNAMSIZ];

		ss << prefix << "/" << std::dec << prefixLen;
		if (blackhole)
			ss << " blackhole";
		if (nhDev)
			ss << " interface " << if_indextoname(nhDev, ifname);
		if (nhAddr.isSet())
			ss << " via " << nhAddr;
		return ss.str();
	}

	Address prefix;
	uint32_t prefixLen;
	bool blackhole;
	Address nhAddr;
	int nhDev;
};

struct RouteCompare {
	bool operator()(const Route &a, const Route &b) const
	{
		if (a.prefix < b.prefix)
			return true;
		if (a.prefix > b.prefix)
			return false;

		return a.prefixLen < b.prefixLen;
	}
};

inline std::ostream &operator<<(std::ostream &os, const Route &route)
{
	os << route.str();
	return os;
}

#endif /* _ROUTE_H_ */
