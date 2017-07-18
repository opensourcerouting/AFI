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

#include "Address.h"

class Route
{
      public:
	Route(void)
	    : prefix()
	    , prefixLen()
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

		return nhDev < route.nhDev;
	}

	bool operator>(const Route &route) const
	{
		return route < *this;
	}

	bool operator==(const Route &route) const
	{
		return (*this < route == route < *this);
	}

	Address prefix;
	uint32_t prefixLen;
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
	os << route.prefix << "/" << route.prefixLen;
	if (route.nhDev)
		os << " dev " << route.nhDev;
	if (route.nhAddr.isSet())
		os << " via " << route.nhAddr;
	return os;
}

#endif /* _ROUTE_H_ */
