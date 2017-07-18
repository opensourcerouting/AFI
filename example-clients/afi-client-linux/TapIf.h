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

#ifndef __TapIf__
#define __TapIf__

#include <boost/thread.hpp>

#include <linux/if_tun.h>

#include "jnx/Aft.h"

class TapIf
{
      public:
	TapIf(void);
	~TapIf(void);

	int init(AftIndex portIndex, std::string ifName,
		 std::string outputIfName);

	bool isInitialized(void);

	int ifWrite(int size, uint8_t *data);

	int ifIndex(void) const
	{
		return _ifIndex;
	}

	const uint8_t *macAddr(void) const
	{
		return _macAddr;
	}

	int portIndex(void) const
	{
		return _portIndex;
	}

      private:
	int tapAlloc(std::string ifName, int flags);

	uint8_t *getMacAddress(std::string);

	int ifRead(void);

	std::string _ifName;
	int _ifIndex;
	uint8_t _macAddr[ETH_ALEN];
	AftIndex _portIndex;
	std::string _outputIfName;
	int _tapFd;
	boost::thread *_tapThread;
};

#endif // __TapIf__
