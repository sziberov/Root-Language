#pragma once

#include "Node.cpp"

// ----------------------------------------------------------------

namespace Interface {
	struct Preferences {
		enum class Mode {
			Undefined,
			Interpret,
			Dashboard
		} mode = Mode::Undefined;
		filesystem::path scriptPath;
		bool //standardIO = false,
			 socket = false;
		string socketHost = "localhost";
		usize socketPort = 3007;
		string token;

		usize callStackSize = 128,
			  reportsLevel = 3,
			  metaprogrammingLevel = 3;
		bool preciseArithmetics = false;
	} preferences;

	// TODO: Allow process to listen multiple [socket: tocken]'s. Use case:
	// User wants to create a process with debug connection, but Opium Kernel (Poppy DE)
	// must create process with its own debugging socket to control the process.
	void printUsage() {
		cout << "Usage:\n"
			 << "    (--interpret <Path> | --dashboard)\n"
		//	 << "    [--standardIO]\n"
			 << "    [--socket [<Host> | <Port>]]\n"
			 << "    [--token <Token>]\n"  // TODO: Should be passed securely (as a password)
			 << "    [--callStackSize <Number>]\n"
			 << "    [--reportsLevel <Number>]\n"
			 << "    [--metaprogrammingLevel <Number>]\n"
			 << "    [--preciseArithmetics]\n\n"

			 << "Modes:\n"
			 << "    (-i | --interpret) <Path>                  Interpret script on given path\n"
			 << "    (-d | --dashboard)                         Set up dashboard interface\n\n"

			 << "Debugging:\n"
		//	 << "    (-sio | --standardIO)                      Use standard input/output for debugging purposes (default - disabled)\n"
			 << "    (-s | --socket) [<Host> | <Port>]          Host or port of the socket (default - localhost:3007 if enabled; always enabled for dashboard)\n"
			 << "    (-t | --token) <Token>                     Security token (default - empty string)\n\n"

			 << "Execution:\n"
			 << "    (-css | --callStackSize) <Number>          Call stack size (default - 128)\n"
			 << "    (-rl | --reportsLevel) <Number>            Reports level (default - 2): 0 - information, 1 - warning, 2 - error\n"
			 << "    (-ml | --metaprogrammingLevel) <Number>    Metaprogramming level (default - 2): 0 - disabled, 1 - read, 2 - write\n"
			 << "    (-pa | --preciseArithmetics)               Precise string-based arithmetic (default - disabled)\n\n"

			 << "Help:\n"
			 << "    (-h | --help)                              Show this help message and exit\n";
	}

	void printPreferences() {
		cout << "Mode: " << (preferences.mode == Preferences::Mode::Interpret ? "Interpret" : "Dashboard") << "\n";
		if(preferences.mode == Preferences::Mode::Interpret) {
			cout << "Script Path: " << preferences.scriptPath << "\n";
		}
	//	cout << "Standard IO: " << (preferences.standardIO ? "Enabled" : "Disabled") << "\n";
		cout << "Socket: " << (preferences.socket ? "Enabled" : "Disabled") << "\n";
		cout << "Socket Host: " << preferences.socketHost << "\n";
		cout << "Socket Port: " << preferences.socketPort << "\n";
		cout << "Token: " << preferences.token << "\n";  // TODO: Should be hidden
		cout << "Call Stack Size: " << preferences.callStackSize << "\n";
		cout << "Reports Level: " << preferences.reportsLevel << "\n";
		cout << "Metaprogramming Level: " << preferences.metaprogrammingLevel << "\n";
		cout << "Precise Arithmetics: " << (preferences.preciseArithmetics ? "Enabled" : "Disabled") << "\n";
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
		//	{"-sio", "--standardIO"},
			{"-s", "--socket"},
			{"-t", "--token"},
			{"-css", "--callStackSize"},
			{"-rl", "--reportsLevel"},
			{"-ml", "--metaprogrammingLevel"},
			{"-pa", "--preciseArithmetics"},
			{"-h", "--help"}
		};

		unordered_map<string, function<bool(int&)>> rules = {
			{"--interpret", [&](int& i) {
				if(i+1 >= argc) return false;
				preferences.mode = Preferences::Mode::Interpret;
				preferences.scriptPath = argv[++i];
				return true;
			}},
			{"--dashboard", [&](int&) {
				preferences.mode = Preferences::Mode::Dashboard;
				preferences.socket = true;
				return true;
			}},
			/*
			{"--standardIO", [&](int&) {
				preferences.standardIO = true;
				return true;
			}},
			*/
			{"--socket", [&](int& i) {
				preferences.socket = true;

				if(i + 1 < argc && argv[i + 1][0] != '-') {
					string next = argv[++i];

					auto colonPos = next.find(':');
					if(colonPos != string::npos) {
						string hostPart = next.substr(0, colonPos);
						string portPart = next.substr(colonPos + 1);

						if(!isPositiveInteger(portPart)) {
							cerr << "Invalid port in --socket: " << portPart << "\n";
							return false;
						}

						preferences.socketHost = hostPart;
						preferences.socketPort = stoi(portPart);
					} else
					if (isPositiveInteger(next)) {
						preferences.socketPort = stoi(next);
					} else {
						preferences.socketHost = next;
					}
				}

				return true;
			}},
			{"--token", [&](int& i) {
				if(i+1 >= argc) return false;
				preferences.token = argv[++i];
				return true;
			}},
			{"--callStackSize", [&](int& i) {
				if(i+1 >= argc || !isPositiveInteger(argv[i+1])) return false;
				preferences.callStackSize = stoi(argv[++i]);
				return true;
			}},
			{"--reportsLevel", [&](int& i) {
				if(i+1 >= argc || !isPositiveInteger(argv[i+1])) return false;
				preferences.reportsLevel = stoi(argv[++i]);
				return true;
			}},
			{"--metaprogrammingLevel", [&](int& i) {
				if(i+1 >= argc || !isPositiveInteger(argv[i+1])) return false;
				preferences.metaprogrammingLevel = stoi(argv[++i]);
				return true;
			}},
			{"--preciseArithmetics", [&](int&) {
				preferences.preciseArithmetics = true;
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
					cerr << "Invalid or missing argument for: " << raw << "\n";
					return false;
				}
			} else {
				cerr << "Unknown argument: " << raw << "\n";
				return false;
			}
		}

		if(preferences.mode == Preferences::Mode::Undefined) {
			cerr << "Error: --interpret or --dashboard must be specified\n";
			return false;
		}

		return true;
	}

	// ----------------------------------------------------------------

	mutex mutex_;
	unordered_set<crow::websocket::connection*> connections;
	vector<string> messages; // JSON

	void send(const Node& output) {
		lock_guard<mutex> lock(mutex_);
		string outputString = to_string(output);

		messages.push_back(outputString);

		/*
		if(preferences.standardIO) {
			cout << outputString << endl;
		}
		*/
		for(auto* connection : connections) {
			connection->send_text(outputString);
		}
	}

	void register_connection(crow::websocket::connection* connection) {
		lock_guard<mutex> lock(mutex_);

		connections.insert(connection);

		/* TODO: Send important only
		for(const auto& message : messages) {
			connection->send_text(message);
		}
		*/
	}

	void unregister_connection(crow::websocket::connection* connection) {
		lock_guard<mutex> lock(mutex_);

		connections.erase(connection);
	}

	void handle_message(const string& input) {
		auto cmd = crow::json::load(input);
		if (!cmd) return;

		lock_guard<mutex> lock(mutex_);

		string action = cmd["action"].s();
		if (action == "clear") {
			messages.clear();
		} else
		if (action == "removeLast") {
			if (!messages.empty()) {
				messages.pop_back();
			}
		}
	}
}