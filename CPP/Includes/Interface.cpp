#pragma once

#include "Node.cpp"

namespace Interface {
	struct Socket {
		string host = "localhost";
		usize port = 3007;
	};

	struct Preferences {
		enum class Mode {
			Undefined,
			Interpret,
			Dashboard
		} mode = Mode::Undefined;

		optional<filesystem::path> scriptPath;
		vector<string> scriptArguments;

		optional<Socket> socket;
		vector<string> tokens;

		usize callStackSize = 128,
			  reportsLevel = 3,
			  metaprogrammingLevel = 3;
		bool preciseArithmetics = false;
	} preferences;

	void printUsage() {
		cout << "Usage:\n"
			 << "    RootServer (--interpret [PATH] | --dashboard)\n"
			 << "               [--socket [HOST | PORT]] [--token [TOKEN]]\n"
			 << "               [--callStackSize NUMBER] [--reportsLevel NUMBER] [--metaprogrammingLevel NUMBER] [--preciseArithmetics]\n"
			 << "               [--arguments [ARGUMENT]...]\n\n"

			 << "Modes:\n"
			 << "    (-i | --interpret) [PATH]                Interpret script on given path\n"
			 << "    (-d | --dashboard)                       Set up dashboard interface\n\n"

			 << "Debugging:\n"
			 << "    (-s | --socket) [HOST | PORT]            Host or port of the socket (default - localhost:3007 if enabled; always enabled for dashboard)\n"
			 << "    (-t | --token) [TOKEN]                   Security token (required if socket enabled, either as argument or standard input)\n\n"

			 << "Execution:\n"
			 << "    (-css | --callStackSize) NUMBER          Call stack size (default - 128)\n"
			 << "    (-rl | --reportsLevel) NUMBER            Reports level (default - 2): 0 - information, 1 - warning, 2 - error\n"
			 << "    (-ml | --metaprogrammingLevel) NUMBER    Metaprogramming level (default - 2): 0 - disabled, 1 - read, 2 - write\n"
			 << "    (-pa | --preciseArithmetics)             Precise string-based arithmetic (default - disabled)\n"
			 << "    (-a | --arguments) [ARGUMENT]...         Script arguments\n\n"

			 << "Help:\n"
			 << "    (-h | --help)                            Show this help message and exit\n";
	}

	void printPreferences() {
		cout << "                 Mode: " << (preferences.mode == Preferences::Mode::Interpret ? "Interpret" : "Dashboard") << endl;

		if(preferences.scriptPath) {
			cout << "          Script Path: " << *preferences.scriptPath << endl;
		}
		if(!preferences.scriptArguments.empty()) {
			cout << "     Script Arguments:";

			for(string& argument : preferences.scriptArguments) {
				cout << " " << argument;
			}

			cout << endl;
		}

		if(preferences.socket) {
			cout << "          Socket Mode: " << (preferences.mode == Preferences::Mode::Dashboard ? "Server" : "Client") << endl;
			cout << "          Socket Host: " << preferences.socket->host << endl;
			cout << "          Socket Port: " << to_string(preferences.socket->port) << endl;
		}

		cout << "                Token: " << preferences.tokens[0] << endl;  // TODO: Should be hidden
		cout << "      Call Stack Size: " << preferences.callStackSize << endl;
		cout << "        Reports Level: " << preferences.reportsLevel << endl;
		cout << "Metaprogramming Level: " << preferences.metaprogrammingLevel << endl;
		cout << "  Precise Arithmetics: " << (preferences.preciseArithmetics ? "Enabled" : "Disabled") << endl;
	}

	bool isPositiveInteger(const string& s) {
		for(char c : s) {
			if(!isdigit(c)) {
				return false;
			}
		}

		return true;
	}

	bool parseArguments(int argc, char* argv[]) {
		unordered_map<string, string> aliases = {
			{"-i", "--interpret"},
			{"-d", "--dashboard"},
			{"-s", "--socket"},
			{"-t", "--token"},
			{"-css", "--callStackSize"},
			{"-rl", "--reportsLevel"},
			{"-ml", "--metaprogrammingLevel"},
			{"-pa", "--preciseArithmetics"},
			{"-a", "--arguments"},
			{"-h", "--help"}
		};

		unordered_map<string, function<bool(int&)>> rules = {
			{"--interpret", [&](int& i) {
				preferences.mode = Preferences::Mode::Interpret;

				if(i+1 < argc && argv[i+1][0] != '-') {
					preferences.scriptPath = argv[++i];
				}

				return true;
			}},
			{"--dashboard", [&](int&) {
				preferences.mode = Preferences::Mode::Dashboard;
				preferences.socket = Socket();

				return true;
			}},
			{"--socket", [&](int& i) {
				preferences.socket = Socket();

				if(i+1 < argc && argv[i+1][0] != '-') {
					string next = argv[++i];
					auto colonPos = next.find(':');

					if(colonPos != string::npos) {
						string hostPart = next.substr(0, colonPos),
							   portPart = next.substr(colonPos+1);

						if(!isPositiveInteger(portPart)) {
							cerr << "Invalid port in --socket: " << portPart << endl;

							return false;
						}

						preferences.socket->host = hostPart;
						preferences.socket->port = stoi(portPart);
					} else
					if(isPositiveInteger(next)) {
						preferences.socket->port = stoi(next);
					} else {
						preferences.socket->host = next;
					}
				}

				return true;
			}},
			{"--token", [&](int& i) {
				if(i+1 < argc && argv[i+1][0] != '-') {
					preferences.tokens.emplace_back(argv[++i]);
				} else {
					preferences.tokens.emplace_back();
				}

				return true;
			}},
			{"--callStackSize", [&](int& i) {
				if(i+1 >= argc || !isPositiveInteger(argv[i+1])) {
					return false;
				}

				preferences.callStackSize = stoi(argv[++i]);

				return true;
			}},
			{"--reportsLevel", [&](int& i) {
				if(i+1 >= argc || !isPositiveInteger(argv[i+1])) {
					return false;
				}

				preferences.reportsLevel = stoi(argv[++i]);

				return true;
			}},
			{"--metaprogrammingLevel", [&](int& i) {
				if(i+1 >= argc || !isPositiveInteger(argv[i+1])) {
					return false;
				}

				preferences.metaprogrammingLevel = stoi(argv[++i]);

				return true;
			}},
			{"--preciseArithmetics", [&](int&) {
				preferences.preciseArithmetics = true;

				return true;
			}},
			{"--arguments", [&](int& i) {
				i++;

				for(; i < argc; i++) {
					preferences.scriptArguments.emplace_back(argv[i]);
				}

				return true;
			}},
			{"--help", [&](int&) {
				printUsage();
				exit(0);

				return true;
			}}
		};

		for(int i = 1; i < argc; i++) {
			string raw = argv[i],
				   arg = aliases.contains(raw) ? aliases.at(raw) : raw;

			if(rules.contains(arg)) {
				if(!rules[arg](i)) {
					cerr << "Invalid or missing argument for: " << raw << endl;

					return false;
				}
			} else {
				cerr << "Unknown argument: " << raw << endl;

				return false;
			}
		}

		if(preferences.mode == Preferences::Mode::Undefined) {
			cerr << "Error: --interpret or --dashboard must be specified\n";

			return false;
		}

		if(preferences.socket && preferences.tokens.empty()) {
			string token;

			cout << "Token: ";

			getline(cin, token);

			if(cin.fail()) {
				cin.clear();
			}

			preferences.tokens.push_back(token);
		}

		return true;
	}

	// ----------------------------------------------------------------

	/*
	int main() {
		const char* path = "/full/path/to/this_binary";  // или используй readlink("/proc/self/exe", ...)
		char* argv[] = { const_cast<char*>(path), const_cast<char*>("arg1"), nullptr };

		pid_t pid;
		int status = posix_spawn(&pid, path, nullptr, nullptr, argv, environ);
		if (status == 0) {
			waitpid(pid, &status, 0);
			cout << "Child exited with status: " << status << endl;
		} else {
			perror("posix_spawn failed");
		}

		return 0;
	}
	*/

	// ----------------------------------------------------------------

	WSConnection serverConnection;
    unordered_set<WSConnection, WSConnectionHash, WSConnectionEqual> clientsConnections;
	function<void(Node)> serverSideHandler,
						 clientSideHandler;

	void sendToClients(const string& output) {
		for(const auto& connection : clientsConnections) {
			try {
				sharedServer.send(connection, output, websocketpp::frame::opcode::text);
			} catch(const exception& e) {
				cerr << "Send (to clients) error: " << e.what() << endl;
			}
		}
	}

	void sendToServer(const string& input) {
		try {
			sharedClient.send(serverConnection, input, websocketpp::frame::opcode::text);
		} catch(const exception& e) {
			cerr << "Send (to server) error: " << e.what() << endl;
		}
	}

	void sendToClients(const Node& node) {
		sendToClients(to_string(node));
	}

	void sendToServer(const Node& node) {
		sendToServer(to_string(node));
	}

	void registerClient(WSConnection connection) {
		clientsConnections.insert(connection);
	}

	void unregisterClient(WSConnection connection) {
		clientsConnections.erase(connection);
	}

	void handleServerSide(const string& message) {
		/*
		if(auto JSON = glz::read_json<glz::json_t>(message)) {
			for(auto& handler : serverSideHandlers) {
				handler(JSON.value());
			}
		}
		*/
		sendToClients(message);
	}

	void handleClientSide(const string& message) {
		if(NodeSP node = NodeParser(message).parse()) {
			string token = node->get("token");

			if(contains(preferences.tokens, token)) {
				clientSideHandler(*node);
			} else {
				cerr << "Tokens list does not contain '" << token << "', message ignored" << endl;
			}
		}
	}

	void becomeServer() {
		sharedServer.init_asio();
		sharedServer.set_open_handler([](WSConnection connection) {
			registerClient(connection);
		});
		sharedServer.set_close_handler([](WSConnection connection) {
			unregisterClient(connection);
		});
		sharedServer.set_message_handler([](WSConnection connection, WSServer::message_ptr message) {
			if(message->get_opcode() == websocketpp::frame::opcode::text) {
				handleServerSide(message->get_payload());
			}
		});
		sharedServer.listen(preferences.socket->port);
		sharedServer.start_accept();
		sharedServer.run();
	}

	void becomeClient() {
		sharedClient.init_asio();
		sharedClient.set_open_handler([](WSConnection connection) {
			thread([]() {
				while(true) {
					sendToServer({
						{"action", "heartbeat"},
						{"processID", (int)getpid()}
					});

					this_thread::sleep_for(10s);
				}
			}).detach();
		});
		sharedClient.set_message_handler([](WSConnection, WSClientMessageSP message) {
			if(message->get_opcode() == websocketpp::frame::opcode::text) {
				handleClientSide(message->get_payload());
			}
		});

		websocketpp::lib::error_code EC;
		WSClient::connection_ptr connection = sharedClient.get_connection("ws://"+preferences.socket->host+":"+to_string(preferences.socket->port), EC);

		if(!EC) {
			serverConnection = connection->get_handle();

			sharedClient.connect(connection);
			sharedClient.run();
		}
	}
}