#include "Interpreter.cpp"
#include "Scheduler.cpp"

namespace RootServer {
	optional<string> code;
	optional<deque<Lexer::Token>> tokens;
	optional<NodeSP> tree;
	mutex interpreterMutex;

	struct Request {
		string receiverToken;
		int receiverProcessID = 0,  // 0 - Any, 1+ - specific
			timeout = 0;  // 0 - Infinity, 1+ - seconds
		optional<usize> timeoutTaskID;
		bool reauthReceivers = false,
			 multipleResponses = false;
		unordered_map<int, bool> receiversFDs;  // [Client FD : Responded]
	};

	struct Client {
		int processID = 0;
		enum class State {
			Pending,  // For spawned processes (if dashboard will ever able to have own child processes)
			Connected,
			Unresponsive
		} state;
		usize heartbeatTaskID = 0;
		unordered_set<string> tokens;
		unordered_map<string, Request> requests;

		bool match(const string& token, int processID) {
			return tokens.contains(token) && (processID == 0 || processID == this->processID);
		}
	};

	unordered_map<int, Client> clients;
	mutex clientsMutex;

	// ----------------------------------------------------------------

	optional<usize> scheduleRequestTimeout(int senderFD, string requestID, int timeout) {
		if(timeout == 0) {
			return nullopt;
		}

		return sharedScheduler.schedule([senderFD, requestID](usize timeoutTaskID) {
			lock_guard lock(clientsMutex);

			if(Client* client = find_ptr(clients, senderFD)) {
				auto it = client->requests.find(requestID);

				if(it != client->requests.end()) {
					if(!some(it->second.receiversFDs, [](auto& v) { return v.second; })) {
						println(sharedServer->getLogPrefix(), "Request \"", requestID, "\" from ", senderFD, " has timed out with no response");
					}

					client->requests.erase(it);
				}
			}

			sharedScheduler.cancel(timeoutTaskID);
		}, timeout*1000);
	}

	usize scheduleServerHeartbeat(int clientFD) {
		return sharedScheduler.schedule([clientFD](int taskID) {
			unique_lock lock(clientsMutex);

			if(!sharedServer->isRunning() || !clients.contains(clientFD)) {
				sharedScheduler.cancel(taskID);
			}

			switch(clients[clientFD].state) {
				case Client::State::Connected:
					clients[clientFD].state = Client::State::Unresponsive;
					println(sharedServer->getLogPrefix(), "Cautiously awaiting heartbeat from ", clientFD);
				break;
				case Client::State::Unresponsive:
					lock.unlock();
					sharedServer->disconnect(clientFD);
				break;
			}
		}, 15000, true);
	}

	usize scheduleClientHeartbeat() {
		return sharedScheduler.schedule([](int taskID) {
			if(!sharedClient->isRunning()) {
				sharedScheduler.cancel(taskID);
			}

			auto senderTokens = NodeArray(Interface::preferences.tokens.begin(), Interface::preferences.tokens.end());

			Interface::sendToServer({
				{"type", "notification"},
				{"action", "heartbeat"},
				{"senderTokens", senderTokens}
			});
		}, 7500, true, true);
	}

	// ----------------------------------------------------------------

	void handleServerConnect(int clientFD) {
		lock_guard lock(clientsMutex);
		ucred credentials;
		socklen_t len = sizeof(ucred);

		if(getsockopt(clientFD, SOL_SOCKET, SO_PEERCRED, &credentials, &len) < 0) {
			println(sharedServer->getLogPrefix(), "Can't get credentials of ", clientFD);
		}

		clients[clientFD] = {
			.processID = credentials.pid,
			.state = Client::State::Connected,
			.heartbeatTaskID = scheduleServerHeartbeat(clientFD)
		};
	}

	void handleServerDisconnect(int clientFD) {
		lock_guard lock(clientsMutex);
		Client& client = clients.at(clientFD);

		sharedScheduler.cancel(client.heartbeatTaskID);

		for(auto& [requestID, request] : client.requests) {
			if(request.timeoutTaskID) {
				sharedScheduler.cancel(*request.timeoutTaskID);
			}
		}

		clients.erase(clientFD);
	}

	void handleServerMessage(int senderFD, const string& rawMessage) {
		lock_guard lock(clientsMutex);
		Client& sender = clients.at(senderFD);

		if(NodeSP message = NodeParser(rawMessage).parse()) {
			string type = message->get("type"),
				   action = message->get("action");

			if(!message->contains("receiverToken") && !message->contains("receiverProcessID")) {  // Server-side message (no rerouting)
				println(sharedServer->getLogPrefix(), "Handling server-side message: ", rawMessage);

				if(type == "notification") {
					if(action == "heartbeat") {
						sharedScheduler.reset(sender.heartbeatTaskID);

						if(sender.state == Client::State::Unresponsive) {
							sender.state = Client::State::Connected;
							println(sharedServer->getLogPrefix(), "Late heartbeat from ", senderFD);
						} else {
							println(sharedServer->getLogPrefix(), "Heartbeat from ", senderFD);
						}

						if(NodeArraySP senderTokens = message->get("senderTokens")) {
							sender.tokens = unordered_set<string>(senderTokens->begin(), senderTokens->end());
						}
					} else
					if(action == "cancelRequest") {
						string requestID = message->get("requestID");

						sender.requests.erase(requestID);  // Unregister pending request
					} else {
						println(sharedServer->getLogPrefix(), "Unknown server-side notification action from ", senderFD, ": \"", action, "\"");
					}
				} else {
					println(sharedServer->getLogPrefix(), "Unknown server-side message type from ", senderFD, ": \"", type, "\"");
				}
			} else {  // Client-side message (rerouting)
				println(sharedServer->getLogPrefix(), "Handling client-side message: ", rawMessage);

				string receiverToken = message->get("receiverToken");
				int receiverProcessID = message->get("receiverProcessID");

				if(type == "notification") {
					for(auto& [clientFD, client] : clients) {
						if(clientFD != senderFD && client.match(receiverToken, receiverProcessID)) {
							sharedServer->send(clientFD, rawMessage);  // Notify
						}
					}
				} else
				if(type == "request") {
					string requestID = message->get("requestID");

					if(sender.requests.contains(requestID)) {
						println(sharedServer->getLogPrefix(), "Request \"", requestID, "\" is already created by ", senderFD, " and will not be replaced");

						return;
					}

					int timeout = message->get("timeout");
					bool reauthReceivers = message->get("reauthReceivers"),
						 multipleResponses = message->get("multipleResponses");
					Request& request = (sender.requests[requestID] = {  // Register request
						receiverToken,
						receiverProcessID,
						timeout,
						scheduleRequestTimeout(senderFD, requestID, timeout),
						reauthReceivers,
						multipleResponses
					});

					for(auto& [clientFD, client] : clients) {
						if(clientFD != senderFD && client.match(receiverToken, receiverProcessID)) {
							request.receiversFDs[clientFD] = false;  // Register responder
						}
					}
					for(auto& [receiverFD, responded] : request.receiversFDs) {
						sharedServer->send(receiverFD, rawMessage);  // Request responce
					}
				} else
				if(type == "response") {
					string responseID = message->get("responseID");

					for(auto& [clientFD, client] : clients) {
						auto it = client.requests.find(responseID);

						if(it == client.requests.end()) {
							continue;
						}

						Request& request = it->second;

						if(request.receiversFDs.contains(senderFD)) {
							continue;
						}
						if(request.reauthReceivers && !sender.match(request.receiverToken, request.receiverProcessID)) {
							println(sharedServer->getLogPrefix(), "Responder ", senderFD, " has failed to reauthentificate");

							continue;
						}

						sharedServer->send(clientFD, rawMessage);  // Respond
						request.receiversFDs.at(senderFD) = true;

						if(!request.multipleResponses || !some(request.receiversFDs, [](auto& v) { return !v.second; })) {
							client.requests.erase(it);  // Unregister fulfilled request
						}
					}
				} else {
					println(sharedServer->getLogPrefix(), "Unknown client-side message type from ", senderFD, ": \"", type, "\"");
				}
			}
		}
	}

	void startServer() {
		sharedServer = SP<Socket>(*Interface::preferences.socketPath, Socket::Mode::Server);

		sharedServer->setConnectionHandler(&handleServerConnect, &handleServerDisconnect);
		sharedServer->setMessageHandler(&handleServerMessage);

		sharedServer->start();
	}

	// ----------------------------------------------------------------

	int clientHeartbeatTaskID = -1;

	void handleClientConnect(int) {
		clientHeartbeatTaskID = scheduleClientHeartbeat();
	}

	void handleClientDisconnect(int) {
		sharedScheduler.cancel(clientHeartbeatTaskID);
	}

	void handleClientMessage(int, const string& rawMessage) {
		NodeSP message = NodeParser(rawMessage).parse();

		if(!message) {
			return;
		}

		string receiverToken = message->get("receiverToken");

		if(!contains(Interface::preferences.tokens, receiverToken)) {  // Do not trust to server with 100% confidence
			println(sharedClient->getLogPrefix(), "Tokens list does not contain '", receiverToken, "', message ignored");

			return;
		}

		string type = message->get("type"),
			   action = message->get("action");

		if(type == "notification") {
			if(action == "lex") {
				lock_guard lock(interpreterMutex);

				code = message->get<string>("code");
				tokens = Lexer(*code).tokenize();
			} else
			if(action == "parse") {
				lock_guard lock(interpreterMutex);

				if(tokens) {
					tree = Parser(*tokens).parse();
				}
			} else
			if(action == "interpret") {
				lock_guard lock(interpreterMutex);

				if(code && tokens && tree) {
					sharedInterpreter->clean();
					SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true, true, true, true), *code, *tokens, *tree)->interpret();
				}
			} else {
				println(sharedClient->getLogPrefix(), "Unknown notification action: \"", action, "\"");
			}
		} else
		if(type == "request") {
			if(action == "evaluate") {
				string code = message->get("code");
				auto tokens = Lexer(code).tokenize();
				auto tree = Parser(tokens).parse();

				SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true), code, tokens, tree)->interpret();
			} else {
				println(sharedClient->getLogPrefix(), "Unknown request action: \"", action, "\"");
			}
		} else {
			println(sharedServer->getLogPrefix(), "Unknown message type: \"", type, "\"");
		}
	}

	void startClient() {
		sharedClient = SP<Socket>(*Interface::preferences.socketPath, Socket::Mode::Client);

		sharedClient->setConnectionHandler(&handleClientConnect, &handleClientDisconnect);
		sharedClient->setMessageHandler(&handleClientMessage);

		while(true) {
			sharedClient->start();
			this_thread::sleep_for(5s);
		}
	}
};

int main(int argc, char* argv[]) {
	if(!Interface::parseArguments(argc, argv)) {
		return 1;
	}

	Interface::printPreferences();

	using namespace RootServer;

	switch(Interface::preferences.mode) {
		case Interface::Preferences::Mode::Interpret: {
			optional<thread> clientThread;

			if(Interface::preferences.socketPath) {
				clientThread = thread(&startClient);
			}
			if(Interface::preferences.scriptPath) {
				lock_guard lock(interpreterMutex);

				if(optional<string> code = read_file(*Interface::preferences.scriptPath)) {
					tokens = Lexer(*code).tokenize();
					tree = Parser(*tokens).parse();

					sharedInterpreter->clean();
					SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true, true, true, true), *code, *tokens, *tree)->interpret();
				}
			}
			if(clientThread) {
				clientThread->join();
			}

			break;
		}
		case Interface::Preferences::Mode::Dashboard: {
			thread serverThread(&startServer),
				   clientThread(&startClient);

			clientThread.join();
			serverThread.join();

			break;
		}
	}

	return 0;
}