#pragma once

#define ASIO_STANDALONE

#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <websocketpp/server.hpp>
#include <websocketpp/client.hpp>

using WSServer = websocketpp::server<websocketpp::config::asio>;
using WSClient = websocketpp::client<websocketpp::config::asio_client>;
using WSClientMessageSP = websocketpp::config::asio_client::message_type::ptr;
using WSConnection = websocketpp::connection_hdl;

struct WSConnectionHash {
    std::size_t operator()(const WSConnection& connection) const {
        return std::hash<void*>()(connection.lock().get());
    }
};

struct WSConnectionEqual {
    bool operator()(const WSConnection& LHS, const WSConnection& RHS) const {
        return !LHS.owner_before(RHS) && !RHS.owner_before(LHS);
    }
};

static WSServer sharedServer;
static WSClient sharedClient;