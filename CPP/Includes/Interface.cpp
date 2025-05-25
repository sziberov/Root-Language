#pragma once

#include "Node.cpp"
#include "Socket.cpp"

namespace Interface {
	struct Preferences {
		enum class Mode {
			Undefined,
			Interpret,
			Dashboard
		} mode = Mode::Undefined;

		optional<filesystem::path> scriptPath;
		vector<string> scriptArguments;

		optional<filesystem::path> socketPath;
		unordered_set<string> tokens;

		usize callStackSize = 128,
			  reportsLevel = 3,
			  metaprogrammingLevel = 3;
		bool preciseArithmetics = false;
	} preferences;

	void printUsage() {
		cout << "Usage:\n"
			 << "    RootServer (--interpret [PATH] | --dashboard)\n"
			 << "               [--socket [PATH]] [--token [TOKEN]]\n"
			 << "               [--callStackSize NUMBER] [--reportsLevel NUMBER] [--metaprogrammingLevel NUMBER] [--preciseArithmetics]\n"
			 << "               [--arguments [ARGUMENT]...]\n\n"

			 << "Modes:\n"
			 << "    (-i | --interpret) [PATH]                Interpret script on given path\n"
			 << "    (-d | --dashboard)                       Set up dashboard interface\n\n"

			 << "Debugging:\n"
			 << "    (-s | --socket) [PATH]                   Path of the socket (default - \"/tmp/RootServer.sock\" if enabled; always enabled for dashboard)\n"
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

		if(preferences.socketPath) {
			cout << "          Socket Mode: " << (preferences.mode == Preferences::Mode::Dashboard ? "Server" : "Client") << endl;
			cout << "          Socket Path: " << *preferences.socketPath << endl;
		}

		cout << "                Token: " << *preferences.tokens.begin() << endl;  // TODO: Should be hidden
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
				preferences.socketPath = "/tmp/RootServer.sock";

				return true;
			}},
			{"--socket", [&](int& i) {
				if(i+1 < argc && argv[i+1][0] != '-') {
					preferences.socketPath = argv[++i];
				} else {
					preferences.socketPath = "/tmp/RootServer.sock";
				}

				return true;
			}},
			{"--token", [&](int& i) {
				if(i+1 < argc && argv[i+1][0] != '-') {
					preferences.tokens.insert(argv[++i]);
				} else {
					preferences.tokens.insert("");
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

		if(preferences.socketPath && preferences.tokens.empty()) {
			string token;

			cout << "Token: ";

			getline(cin, token);

			if(cin.fail()) {
				cin.clear();
			}

			preferences.tokens.insert(token);
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

	void sendToClients(const string& output) {
		if(sharedServer) {
			sharedServer->send(output);
		}
	}

	void sendToServer(const string& input) {
		if(sharedClient) {
			sharedClient->send(input);
		}
	}

	void sendToClients(Node node) {
		node.get("receiverProcessID") = 0;
		node.get("processID") = (int)getpid();

		sendToClients(to_string(node));
	}

	void sendToServer(Node node) {
		node.get("receiverProcessID") = 0;
		node.get("processID") = (int)getpid();

		sendToServer(to_string(node));
	}
}