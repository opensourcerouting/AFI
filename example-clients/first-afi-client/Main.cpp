//
// Main.cpp
//
// Advanced Forwarding Interface : AFI client examples
//
// Created by Sandesh Kumar Sodhi, January 2017
// Copyright (c) [2017] Juniper Networks, Inc. All rights reserved.
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

#include "jnx/Aft.h"
#include "jnx/AfiTransport.h"


//
// This example client code will make following sandbox topology
//
//               Sandbox: green
//        +------------------------------+
//        |                              |
//        |            list              |
//     p0 o------->+----------+          |
//  Input |        | counter  |          |
//   port |        +----------+          |
//        |        |       ---|--------->o p1
//        |        +----------+          | Output
//        |                              | port
//        +------------------------------+
//
//

int
main(int argc, char *argv[])
{
    // The first thing an AFI client needs to do is initialize transport
    // connection to server. The AftTransportPtr is a smart pointer to the Afi
    // path context. As a rule, the Afi library uses C++ STL smart pointers
    // for the persistent state that it manages in order to avoid the
    // potential for memory issues.
    AftTransportPtr transport = AfiTransport::create("128.0.0.16:50051");

    // To open a sandbox, AFI client simply calls the transport open()
    // method with the name of the sandbox AFI client wants to attach to.
    transport->alloc("trio",   // Name of the engine
                     "green",  // Sandbox name
                      2,       // Number of input ports
                      2);      // Number of output ports

    AftSandboxPtr sandbox;
    transport->open("green",   // Name of sandbox transport controls
                     sandbox); //

    //
    // Allocate an insert context
    //
    AftInsertPtr  insert = AftInsert::create(sandbox);


    // This creates a simple counter with initial byte/packet count
    // as 0/0

    AftNodePtr counter =  AftCounter::create(0, 0, false);

    // Add it to insert context
    insert->push(counter);

    //
    // Get the token for output port "1"
    //
    AftNodeToken outputPortToken;
    AftIndex outputPortIndex = 1;
    sandbox->outputPortByIndex(outputPortIndex, outputPortToken);

    //
    // Build a list to describe the flow of lookup for this sandbox.
    // For every packet, a counter will be increment and then packet
    // exits via the outputPort.
    //
    AftNodePtr list = AftList::create({counter->nodeToken(), outputPortToken});
    insert->push(list);

    //
    // Send all our nodes to the sandbox
    //
    sandbox->send(insert);

    //
    // Connect all the input port 0 to the list we just created
    //
    AftIndex inputPortIndex = 0;
    sandbox->setInputPortByIndex(inputPortIndex, list->nodeToken());
}
