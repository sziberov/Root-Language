#include "Interpreter.cpp"
#include "Scheduler.cpp"

optional<string> code;
optional<deque<Lexer::Token>> tokens;
optional<NodeSP> tree;
mutex interpreterMutex;
struct Request {
	string receiverToken;
	int receiverProcessID = 0,  // 0 - Any, 1+ - specific
		timeout = 0;  // 0 - Infinity, 1+ - seconds
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
	usize heartbeatTaskID;
	unordered_set<string> tokens;
	unordered_map<string, Request> requests;

	bool match(const string& token, int processID) {
		return tokens.contains(token) && (processID == 0 || processID == this->processID);
	}
};
unordered_map<int, Client> clients;
mutex clientsMutex;
int clientHeartbeatTaskID = -1;

thread getServerThread() {
	sharedServer = SP<Socket>(*Interface::preferences.socketPath, Socket::Mode::Server);

	sharedServer->setConnectionHandler(
		[](int clientFD) {
			ucred credentials;
			socklen_t len = sizeof(ucred);
			if(getsockopt(clientFD, SOL_SOCKET, SO_PEERCRED, &credentials, &len) < 0) {
				println(sharedServer->getLogPrefix(), "Can't get credentials of ", clientFD);
			}
			lock_guard lock(clientsMutex);
			clients[clientFD] = {
				credentials.pid,
				Client::State::Connected,
				sharedScheduler.schedule([clientFD](int taskID) {
					unique_lock lock(clientsMutex);

					if(!sharedServer->isRunning() || !clients.contains(clientFD)) {
						sharedScheduler.cancel(taskID);
					}

					switch(clients[clientFD].state) {
						case Client::State::Connected:
							clients[clientFD].state = Client::State::Unresponsive;
							println(sharedServer->getLogPrefix(), "Client ", clientFD, " has been marked as unresponsive");
						break;
						case Client::State::Unresponsive:
							lock.unlock();
							sharedServer->disconnect(clientFD);
						break;
					}
				}, 10000, true)
			};
		},
		[](int clientFD) {
			lock_guard lock(clientsMutex);
			sharedScheduler.cancel(clients[clientFD].heartbeatTaskID);
			clients.erase(clientFD);
		}
	);
	sharedServer->setMessageHandler([](int senderFD, const string& rawMessage) {
		lock_guard lock(clientsMutex);
		Client& sender = clients.at(senderFD);

		if(NodeSP message = NodeParser(rawMessage).parse()) {
			string type = message->get("type"),
				   action = message->get("action"),
				   receiverToken = message->get("receiverToken");
			int receiverProcessID = message->get("receiverProcessID");

			if(receiverToken.empty() && receiverProcessID == 0) {  // Server-side message (no rerouting)
				if(type == "notification") {
					if(action == "heartbeat") {
						if(NodeArraySP senderTokens = message->get("senderTokens")) {
							sender.state = Client::State::Connected;
							sender.tokens = unordered_set<string>(senderTokens->begin(), senderTokens->end());

							sharedScheduler.reset(sender.heartbeatTaskID);
						}
					} else
					if(action == "cancelRequest") {
						string requestID = message->get("requestID");

						sender.requests.erase(requestID);  // Unregister pending request
					}
				}
			} else {  // Client-side message (rerouting)
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
						println(sharedServer->getLogPrefix(), "Request with ID \"", requestID, "\" is already created by ", senderFD, " and will not be replaced");

						return;
					}

					int timeout = message->get("timeout");
					bool reauthReceivers = message->get("reauthReceivers"),
						 multipleResponses = message->get("multipleResponses");
					Request& request = (sender.requests[requestID] = {  // Register request
						receiverToken,
						receiverProcessID,
						timeout,
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
				}
			}
		}
	});

	return thread(&Socket::start, sharedServer.get());
}

thread getClientThread() {
	sharedClient = SP<Socket>(*Interface::preferences.socketPath, Socket::Mode::Client);

	sharedClient->setConnectionHandler(
		[](int) {
			clientHeartbeatTaskID = sharedScheduler.schedule([](int taskID) {
				if(!sharedClient->isRunning()) {
					sharedScheduler.cancel(taskID);
				}

				auto senderTokens = NodeArray(Interface::preferences.tokens.begin(), Interface::preferences.tokens.end());

				/*
				Interface::sendToServer({
					{"type", "notification"},
					{"action", "heartbeat"},
					{"senderTokens", senderTokens}
				});
				*/
			}, 10000, true, true);
		},
		[](int) {
			sharedScheduler.cancel(clientHeartbeatTaskID);
		}
	);
	sharedClient->setMessageHandler([&](int, const string& rawMessage) {
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
			}
		} else
		if(type == "request") {
			if(action == "evaluate") {
				string code = message->get("code");
				auto tokens = Lexer(code).tokenize();
				auto tree = Parser(tokens).parse();

				SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true), code, tokens, tree)->interpret();
			}
		}
	});

	return thread(&Socket::start, sharedClient.get());
}

thread getClientAutoThread() {
	return thread([]() {
		while(true) {
			thread clientThread;

			int clientAutoTaskID = sharedScheduler.schedule([&clientThread](int taskID) {
				if(sharedClient && sharedClient->isRunning()) {
					sharedScheduler.cancel(taskID);

					return;
				}

				if(clientThread.joinable()) {
					clientThread.join();  // Wait for failed thread to stop
				}

				clientThread = getClientThread();
			}, 5000, true, true);

			sharedScheduler.await(clientAutoTaskID);

			if(clientThread.joinable()) {
				clientThread.join();  // Wait for started thread to fail
			}
		}
	});
}

int main(int argc, char* argv[]) {
	if(!Interface::parseArguments(argc, argv)) {
		return 1;
	}

	Interface::printPreferences();

	switch(Interface::preferences.mode) {
		case Interface::Preferences::Mode::Interpret: {
			optional<thread> clientAutoThread;

			if(Interface::preferences.socketPath) {
				clientAutoThread = getClientAutoThread();
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
			if(clientAutoThread) {
				clientAutoThread->join();
			}

			break;
		}
		case Interface::Preferences::Mode::Dashboard: {
			thread serverThread = getServerThread(),
				   clientAutoThread = getClientAutoThread();

			clientAutoThread.join();
			serverThread.join();

			break;
		}
	}

	return 0;
}