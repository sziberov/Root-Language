#include "Interpreter.cpp"

optional<string> code;
optional<deque<Lexer::Token>> tokens;
optional<NodeSP> tree;
mutex interpreterLock;

thread getServerThread() {
	sharedServer = SP<Socket>(*Interface::preferences.socketPath, Socket::Mode::Server);

	sharedServer->setMessageHandler([](int, const string& message) {
		cout << "[Server] Received message: " << message << endl;

		Interface::sendToClients(message);
	});

	return thread(&Socket::start, sharedServer.get());
}

thread getClientThread() {
	sharedClient = SP<Socket>(*Interface::preferences.socketPath, Socket::Mode::Client);

	sharedClient->setConnectionHandler(
		[](int) {
			thread([]() {
				while(sharedClient->isRunning()) {
					auto tokens = NodeArray(Interface::preferences.tokens.begin(), Interface::preferences.tokens.end());

					Interface::sendToServer({
						{"action", "heartbeat"},
						{"tokens", tokens}
					});

					this_thread::sleep_for(10s);
				}
			}).detach();
		},
		nullptr
	);
	sharedClient->setMessageHandler([&](int, const string& message) {
		cout << "[Client] Received message: " << message << endl;

		NodeSP node = NodeParser(message).parse();

		if(!node) {
			return;
		}

		string token = node->get("token");

		if(!contains(Interface::preferences.tokens, token)) {
			cerr << "Tokens list does not contain '" << token << "', message ignored" << endl;

			return;
		}

		string action = node->get("action");

		if(action == "lex") {
			auto lock = lock_guard<mutex>(interpreterLock);

			code = node->get<string>("code");
			tokens = Lexer(*code).tokenize();
		} else
		if(action == "parse") {
			auto lock = lock_guard<mutex>(interpreterLock);

			if(tokens) {
				tree = Parser(*tokens).parse();
			}
		} else
		if(action == "interpret") {
			auto lock = lock_guard<mutex>(interpreterLock);

			if(code && tokens && tree) {
				sharedInterpreter->clean();
				SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true, true, true, true), *code, *tokens, *tree)->interpret();
			}
		} else
		if(action == "evaluate") {
			string code = node->get("code");
			auto tokens = Lexer(code).tokenize();
			auto tree = Parser(tokens).parse();

			SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true), code, tokens, tree)->interpret();
		}
	});

	return thread(&Socket::start, sharedClient.get());
}

int main(int argc, char* argv[]) {
	if(!Interface::parseArguments(argc, argv)) {
		return 1;
	}

	Interface::printPreferences();

	switch(Interface::preferences.mode) {
		case Interface::Preferences::Mode::Interpret: {
			optional<thread> clientThread;

			if(Interface::preferences.socketPath) {
				clientThread = getClientThread();
			}
			if(Interface::preferences.scriptPath) {
				auto lock = lock_guard<mutex>(interpreterLock);

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
			thread serverThread = getServerThread();
			this_thread::sleep_for(1s);
			thread clientThread = getClientThread();

			clientThread.join();
			serverThread.join();

			break;
		}
	}

	return 0;
}