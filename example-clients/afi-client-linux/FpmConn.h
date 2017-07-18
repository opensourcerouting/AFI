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

#ifndef __FpmConn__
#define __FpmConn__

#include <vector>

#include "fpm/fpm.pb.h"

class FpmConn
{
      public:
	FpmConn(int fd);
	~FpmConn(void);
	int getFd(void);
	int handle(void);
	static void processProtobuf(const void *buf, size_t numbytes);
	static void processProtobufAddRoute(const fpm::AddRoute &fpm_route);
	static void processProtobufDelRoute(const fpm::DeleteRoute &fpm_route);

      private:
	int fd;
	std::vector<char> buffer;
	unsigned int bufSize;
	size_t pos;
};

#endif // __FpmConn__
