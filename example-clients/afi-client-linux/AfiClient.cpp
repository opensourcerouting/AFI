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

#include "AfiClient.h"
#include "Utils.h"

AfiClient *AfiClient::instance = NULL;

int AfiClient::setup(void)
{
	AftPortEntryPtr port;
	std::stringstream description;

	openSandbox("green", 8);
	getSandboxPtr()->outputPortByName("punt", _puntToken);

	//
	// Save port tokens
	//
	AftPortTablePtr inputPorts = _sandbox->inputPortTable();
	for (AftIndex i = 0; i < (inputPorts->maxIndex() - 1); i++) {
		inputPorts->portForIndex(i, port);
		assert(port != nullptr);
		description.str("");
		description << "port " << i << ", input";
		addToken(port->nodeToken(), "port", description.str(), 0);
	}
	AftPortTablePtr outputPorts = _sandbox->outputPortTable();
	for (AftIndex i = 0; i < (outputPorts->maxIndex()); i++) {
		outputPorts->portForIndex(i, port);
		assert(port != nullptr);

		if (i < (outputPorts->maxIndex() - 1)) {
			description.str("");
			description << "port " << i << ", output";
			addToken(port->nodeToken(), "port", description.str(),
				 0);
		} else
			addToken(port->nodeToken(), "punt",
				 "punt to AFI client", 0);
	}

	//
	// Add global routing table
	//

	_routeTableToken = addRouteTable("routeTable", _puntToken);

	//
	// Add single discard node for blackhole routes
	//
	_discardToken = addDiscardNode();

	//
	// Add default routes pointing to the punt token.
	// This is necessary due to a bug in AFI.
	//
	struct in_addr defaultRouteV4 = {0};
	struct in6_addr defaultRouteV6 = {0};
	Route route;
	route.prefix.set(defaultRouteV4);
	addRoute(route);
	route.prefix.set(defaultRouteV6);
	addRoute(route);
}

void AfiClient::setupInterface(std::string &interface, unsigned int port,
			       std::string &tap)
{
	setInputPortNextNode(port, _routeTableToken);
	_puntingPorts[port].init(port, tap, interface);
}

//
// @fn
// openSandbox
//
// @brief
// Open AFI sandbox created via Junos CLI configuration
//
// @param[in]
//     sandbox_name Sandbox name
// @param[in]
//     numCfgPorts Number of configured ports
// @return 0 - Success, -1 - Failure
//

int AfiClient::openSandbox(const std::string &sandbox_name,
			   u_int32_t numCfgPorts)
{
	u_int32_t numStdPorts = 1;
	bool status = _transport->alloc("trio", sandbox_name,
					(numCfgPorts + numStdPorts),
					(numCfgPorts + numStdPorts));
	if (status == 0) {
		std::cout << "_transport->alloc failed!" << std::endl;
		return -1;
	}

	if (_transport->open(sandbox_name, _sandbox)) {
		std::cout << "Got Sandbox handle!" << std::endl;
	} else {
		std::cout << "Open Sandbox failed!" << std::endl;
		return -1;
	}

	return 0;
}

//
// @fn
// addRouteTable
//
// @brief
// Add routing table
//
// @param[in]
//     rttName Routing table name
// @param[in]
//     defaultTragetToken  Default route target token
// @return routing table node token
//

AftNodeToken AfiClient::addRouteTable(const std::string &rttName,
				      AftNodeToken defaultTragetToken)
{
	AftNodeToken rttNodeToken;
	AftInsertPtr insert;

	//
	// Allocate an insert context
	//
	insert = AftInsert::create(_sandbox);

	//
	// Create a route lookup tree
	//
	auto treePtr = AftTree::create(AftField("packet.lookupkey"),
				       defaultTragetToken);
	//
	// Stich the optional params
	//
	uint64_t skipBits = 16;
	treePtr->setNodeParameter("rt.app", AftDataString::create("NH"));
	treePtr->setNodeParameter("rt.nhType", AftDataString::create("route"));
	treePtr->setNodeParameter("rt.skipBits", AftDataInt::create(skipBits));

	rttNodeToken = insert->push(treePtr, rttName);

	//
	// Send all the nodes to the sandbox
	//
	_sandbox->send(insert);

	//
	// Save installed token
	//
	addToken(rttNodeToken, "routing-table", "routing table - default VRF",
		 0);

	return rttNodeToken;
}

//
// @fn
// setInputPortNextNode
//
// @brief
// Set next node for an input port
//
// @param[in]
//     inputPortIndex Input port index
// @param[in]
//     nextToken Next node token
// @return 0 - Success, -1 - Error
//

int AfiClient::setInputPortNextNode(AftIndex inputPortIndex,
				    AftNodeToken nextToken)
{
	AftPortEntryPtr port;

	_sandbox->setInputPortByIndex(inputPortIndex, nextToken);

	//
	// Update port token
	//
	AftPortTablePtr inputPorts = _sandbox->inputPortTable();
	inputPorts->portForIndex(inputPortIndex, port);
	assert(port != nullptr);
	tokens[port->nodeToken()].next = nextToken;

	return 0;
}

//
// @fn
// getOuputPortToken
//
// @brief
// Get token for an output port
//
// @param[in]
//     outputPortIndex Output port index
// @return Outport port's token
//

AftNodeToken AfiClient::getOuputPortToken(AftIndex outputPortIndex)
{
	AftNodeToken outputPortToken;

	_sandbox->outputPortByIndex(outputPortIndex, outputPortToken);

	return outputPortToken;
}

//
// @fn
// addEtherEncapNode
//
// @brief
// Add Ethernet encap node
//
// @param[in]
//     dst_mac Destination MAC
// @param[in]
//     src_mac Source MAC
// @param[in]
//     nextToken Next node token
// @return Ethernet encap node's token
//

AftNodeToken AfiClient::addEtherEncapNode(const AftDataBytes &dst_mac,
					  const AftDataBytes &src_mac,
					  AftNodeToken nextToken)
{
	AftNodeToken outListToken;
	AftInsertPtr insert;

	//
	// Allocate an insert context
	//
	insert = AftInsert::create(_sandbox);

	//
	// Create a key vector of ethernet data
	//
	AftKeyVector encapKeys = {AftKey(AftField("packet.ether.saddr"),
					 AftDataEtherAddr::create(src_mac)),
				  AftKey(AftField("packet.ether.daddr"),
					 AftDataEtherAddr::create(dst_mac))};
	//
	// Create aft encap node
	//
	auto aftEncapPtr = AftEncap::create("ethernet", encapKeys);

	//
	// Stich the optional params
	//
	static u_int64_t nhId = 100;
	nhId++;
	aftEncapPtr->setNodeParameter("meta.nhid", AftDataInt::create(nhId));

	//
	// Set the List node token as next pointer to encap
	//
	aftEncapPtr->setNodeNext(nextToken);

	AftNodeToken nhEncapToken = insert->push(aftEncapPtr);

	//
	// Send all the nodes to the sandbox
	//
	_sandbox->send(insert);

	return nhEncapToken;
}

//
// @fn
// addDiscardNode
//
// @brief
// Add discard node
//
// @param[in] void
// @return Discard node token
//

AftNodeToken AfiClient::addDiscardNode(void)
{
	AftNodeToken discardNodeToken;
	AftInsertPtr insert;

	//
	// Allocate an insert context
	//
	insert = AftInsert::create(_sandbox);

	//
	// Create aft discard node
	//
	AftNodePtr discard = AftDiscard::create();

	//
	// Send all the nodes to the sandbox
	//
	discardNodeToken = insert->push(discard, "Discard");
	_sandbox->send(insert);

	//
	// Save installed token
	//
	addToken(discardNodeToken, "discard", "discard (drop) packets", 0);

	return discardNodeToken;
}

//
// @fn
// recvHostPathPacket
//
// @brief
// Receive hostpath packet
//
// @param[in]
//     pkt Aft packet where received packet will be copied to
// @return 0 - Success, -1 - Error
//

int AfiClient::recvHostPathPacket(AftPacketPtr &pkt)
{
	size_t recvlen;
	enum { max_length = 2000 };
	char _data[max_length];
	BOOST_UDP::endpoint sender_endpoint;

	// Block until data has been received successfully or an error occurs.
	recvlen = _hpUdpSock.receive_from(
		boost::asio::buffer(_data, max_length), sender_endpoint);

	memcpy(pkt->header(), _data, pkt->headerSize());
	memcpy(pkt->data(), _data + pkt->headerSize(),
	       (recvlen - pkt->headerSize()));

	pkt->headerParse();

	TapIf &tapIf = _puntingPorts[pkt->portIndex()];
	if (tapIf.isInitialized())
		tapIf.ifWrite(pkt->dataSize(), pkt->data());

	return 0;
}

void AfiClient::addToken(AftNodeToken token, std::string type,
			 std::string description, AftNodeToken next)
{
	struct AfiToken nodeToken;
	nodeToken.type = type;
	nodeToken.description = description;
	nodeToken.next = next;

	tokens[token] = nodeToken;
}

void AfiClient::delToken(AftNodeToken token)
{
	tokens.erase(token);
}

TapIf *AfiClient::findTap(int ifindex)
{
	auto it = find_if(
		_puntingPorts.begin(), _puntingPorts.end(),
		[&](const TapIf &t) -> bool { return t.ifIndex() == ifindex; });
	if (it == std::end(_puntingPorts))
		return NULL;
	return &(*it);
}

bool AfiClient::filterNeighbor(Neighbor &neighbor)
{
	if (neighbor.ipAddr.isSpecial())
		return true;
	if (!findTap(neighbor.ifIndex))
		return true;

	return false;
}

//
// @fn
// addNeighbor
//
// @brief
// Add or update neighbor
//
// @param[in]
//     neighbor Neighbor
// @return void
//

void AfiClient::addNeighbor(Neighbor &neighbor)
{
	if (filterNeighbor(neighbor))
		return;

	auto old = neighbors.find(neighbor);
	if (old != neighbors.end()) {
		// Ignore message if neighbor exists and didn't change
		if (old->first == neighbor)
			return;

		syslog(LOG_INFO, "[neighbor] update: %s",
		       neighbor.str().c_str());
	} else
		syslog(LOG_INFO, "[neighbor] add: %s", neighbor.str().c_str());

	installNeighbor(neighbor);

	for (auto it = routes.begin(); it != routes.end(); ++it) {
		Route route = it->first;
		if (route.nhAddr == neighbor.ipAddr)
			installRoute(route);
	}
}

//
// @fn
// delNeighbor
//
// @brief
// Delete neighbor
//
// @param[in]
//     neighbor Neighbor
// @return void
//

void AfiClient::delNeighbor(Neighbor &neighbor)
{
	if (filterNeighbor(neighbor))
		return;

	auto it = neighbors.find(neighbor);
	if (it == neighbors.end())
		return;

	syslog(LOG_INFO, "[neighbor] del: %s", neighbor.str().c_str());

	for (auto it = routes.begin(); it != routes.end(); ++it) {
		Route route = it->first;
		if (route.nhAddr == neighbor.ipAddr)
			uninstallRoute(it);
	}

	uninstallNeighbor(it);
	neighbors.erase(neighbor);
}

int AfiClient::installNeighbor(const Neighbor &neighbor)
{
	//
	// Find output interface.
	//
	TapIf *tap = findTap(neighbor.ifIndex);
	AftIndex outputPortIndex = tap->portIndex();
	AftNodeToken outputPortToken = getOuputPortToken(outputPortIndex);

	//
	// Get source and destination MAC addresses.
	//
	AftDataBytes src_mac(tap->macAddr(), tap->macAddr() + ETH_ALEN);
	AftDataBytes dst_mac(neighbor.macAddr, neighbor.macAddr + ETH_ALEN);

	//
	// Add AftEncap node.
	//
	AftNodeToken encapToken =
		addEtherEncapNode(dst_mac, src_mac, outputPortToken);
	neighbors[neighbor] = encapToken;

	//
	// Save installed token
	//
	std::stringstream description;
	description << "src " << printMacAddress(tap->macAddr());
	description << ", dst " << printMacAddress(neighbor.macAddr);
	addToken(encapToken, "encap", description.str(), outputPortToken);

	return 0;
}

int AfiClient::uninstallNeighbor(neighbors_iterator it)
{
	AftNodeToken encapToken = it->second;

	//
	// Delete saved token
	//
	delToken(encapToken);

	//
	// Send delete operation to the sandbox
	//
	AftRemovePtr remove = AftRemove::create();
	remove->push(encapToken);
	_sandbox->send(remove);

	return 0;
}

bool AfiClient::filterRoute(const Route &route)
{
	//
	// Filter special routes
	//
	if (route.prefix.isSpecial())
		return true;

	//
	// Filter link-local routes
	//
	if (route.prefix.af == AF_INET6
	    && IN6_IS_ADDR_LINKLOCAL(&route.prefix.u.v6))
		return true;

	//
	// Accept blackhole routes
	//
	if (route.blackhole)
		return false;

	//
	// Filter routes whose nexthop interface is not a tap interface
	//
	if (!findTap(route.nhDev))
		return true;

	return false;
}

//
// @fn
// addRoute
//
// @brief
// Add or update route
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::addRoute(const Route &route)
{
	if (filterRoute(route))
		return -1;

	auto old = routes.find(route);
	if (old != routes.end()) {
		// Ignore message if route exists and didn't change
		if (old->first == route)
			return 0;

		syslog(LOG_INFO, "[route] update: %s", route.str().c_str());
	} else
		syslog(LOG_INFO, "[route] add: %s", route.str().c_str());

	return installRoute(route);
}

//
// @fn
// delRoute
//
// @brief
// Delete route from the routing table
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::delRoute(Route &route)
{
	if (filterRoute(route))
		return -1;

	auto it = routes.find(route);
	if (it == routes.end())
		return -1;

	syslog(LOG_INFO, "[route] del: %s", route.str().c_str());

	uninstallRoute(it);
	routes.erase(route);

	return 0;
}

//
// @fn
// installRoute
//
// @brief
// Install route to the routing table
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::installRoute(const Route &route)
{
	AftNodeToken routeTargetToken;

	//
	// Routes that don't have a nexthop address can't be installed in the
	// fast-path because ARP is required to resolve MAC addresses on a
	// per-packet basis.
	//
	if (route.blackhole)
		routeTargetToken = _discardToken;
	else if (!route.nhAddr.isSet())
		routeTargetToken = _puntToken;
	else {
		Neighbor n;
		n.ipAddr = route.nhAddr;
		auto neighbor = neighbors.find(n);
		if (neighbor == neighbors.end())
			return -1;

		routeTargetToken = neighbor->second;
	}

	int prefix_len;
	std::string key_name;
	switch (route.prefix.af) {
	case AF_INET:
		prefix_len = 4;
		key_name = "packet.ip4.daddr";
		break;
	case AF_INET6:
		prefix_len = 16;
		key_name = "packet.ipv6.daddr";
		break;
	default:
		warnx("%s: unknown address-family", __func__);
		return -1;
	}
	AftDataBytes prefix_bytes(route.prefix.u.bytes,
				  route.prefix.u.bytes + prefix_len);
	AftDataPtr data_prefix =
		AftDataPrefix::create(prefix_bytes, route.prefixLen);
	AftKey key = AftKey(AftField(key_name), data_prefix);

	//
	// Allocate an insert context
	//
	AftInsertPtr insert = AftInsert::create(_sandbox);

	//
	// Create a route
	//
	AftEntryPtr entryPtr =
		AftEntry::create(_routeTableToken, key, routeTargetToken);

	//
	// Set the optional params for Entry
	//
	uint8_t hwFlush = 0;
	entryPtr->setEntryParameter("route.string",
				    AftDataString::create("IPv4 route"));
	entryPtr->setEntryParameter("route.hwFlush",
				    AftDataInt::create(hwFlush));
	insert->push(entryPtr);

	//
	// Send all the nodes to the sandbox
	//
	_sandbox->send(insert);

	struct AfiRouteInfo afiRouteInfo;
	afiRouteInfo.insertPtr = insert;
	afiRouteInfo.outputToken = routeTargetToken;
	routes[route] = afiRouteInfo;

	syslog(LOG_INFO, "[route] install: %s", route.str().c_str());

	return 0;
}

//
// @fn
// uninstallRoute
//
// @brief
// Delete route from the routing table
//
// @param[in]
//     route Route
// @return 0 - Success, -1 - Error
//

int AfiClient::uninstallRoute(routes_iterator it)
{
	syslog(LOG_INFO, "[route] uninstall: %s", it->first.str().c_str());

	struct AfiRouteInfo afiRouteInfo = it->second;

	//
	// Send delete operation to the sandbox
	//
	AftRemovePtr remove = AftRemove::create(afiRouteInfo.insertPtr);
	_sandbox->send(remove);

	return 0;
}

//
// @fn
// printRoutes
//
// @brief
// Display routes
//
// @param[in] void
// @return void
//

void AfiClient::printRoutes(void)
{
	char ifname[IFNAMSIZ];

	std::cout << std::left;
	std::cout << std::setw(5) << "AF";
	std::cout << std::setw(21) << "Prefix";
	std::cout << std::setw(16) << "Interface";
	std::cout << std::setw(27) << "Nexthop";
	std::cout << "Output Token\n";
	for (auto it = routes.begin(); it != routes.end(); ++it) {
		Route route = it->first;
		struct AfiRouteInfo afiRouteInfo = it->second;
		;

		std::stringstream prefix;
		prefix << route.prefix << "/" << route.prefixLen;

		std::cout << std::left;
		std::cout << std::setw(5) << Address::printAf(route.prefix.af);
		std::cout << std::setw(21) << prefix.str();

		std::cout << std::setw(16);
		if (route.nhDev)
			std::cout << if_indextoname(route.nhDev, ifname);
		else
			std::cout << "-";

		std::cout << std::setw(27);
		if (route.nhAddr.isSet())
			std::cout << route.nhAddr;
		else
			std::cout << "-";

		if (afiRouteInfo.outputToken != 0)
			std::cout << std::dec << afiRouteInfo.outputToken;
		else
			std::cout << "-";
		std::cout << "\n";
	}
}

//
// @fn
// printNeighbors
//
// @brief
// Display neighbors
//
// @param[in] void
// @return void
//

void AfiClient::printNeighbors(void)
{
	char ifname[IFNAMSIZ];

	std::cout << std::left;
	std::cout << std::setw(5) << "AF";
	std::cout << std::setw(27) << "IP Address";
	std::cout << std::setw(16) << "Interface";
	std::cout << std::setw(21) << "MAC Address";
	std::cout << "Encap Token\n";
	for (auto it = neighbors.begin(); it != neighbors.end(); ++it) {
		Neighbor neighbor = it->first;
		AftNodeToken encapToken = it->second;

		std::cout << std::left;
		std::cout << std::setw(5)
			  << Address::printAf(neighbor.ipAddr.af);
		std::cout << std::setw(27) << neighbor.ipAddr;
		std::cout << std::setw(16)
			  << if_indextoname(neighbor.ifIndex, ifname);
		std::cout << std::setw(21) << printMacAddress(neighbor.macAddr);
		if (encapToken)
			std::cout << std::dec << encapToken;
		else
			std::cout << "-";
		std::cout << "\n";
	}
}

//
// @fn
// printTokens
//
// @brief
// Display tokens
//
// @param[in] void
// @return void
//

void AfiClient::printTokens(void)
{
	std::cout << std::left;
	std::cout << std::setw(8) << "Token";
	std::cout << std::setw(15) << "Type";
	std::cout << std::setw(46) << "Description";
	std::cout << "Next Token\n";
	for (auto it = tokens.begin(); it != tokens.end(); ++it) {
		AftNodeToken token = it->first;
		struct AfiToken tokenInfo = it->second;

		std::cout << std::left;
		std::cout << std::setw(8) << std::dec << token;
		std::cout << std::setw(15) << tokenInfo.type;
		std::cout << std::setw(46) << tokenInfo.description;
		if (tokenInfo.next == 0)
			std::cout << "-";
		else
			std::cout << tokenInfo.next;
		std::cout << "\n";
	}
}

//
// @fn
// hostPathUDPSrvr
//
// @brief
// Hostpath UDP server
//
// @param[in] void
// @return void
//

void AfiClient::hostPathUDPSrvr(void)
{
	for (;;) {
		//
		// Allocate packet context *and* buffer
		//
		AftPacketPtr pkt = AftPacket::createReceive();

		recvHostPathPacket(pkt);
	}
}
