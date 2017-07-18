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

#ifndef __FpmServer__
#define __FpmServer__

#include <map>

#include "FpmConn.h"

class FpmServer
{
      public:
	static void initialize(void);
	static int accept(void);
	static int getFd(void);
	static FpmConn &getConn(int fd);
	static void deleteConn(int fd);

      private:
	static int fd;
	static std::map<int, FpmConn> connections;
};

#endif // __FpmServer__
