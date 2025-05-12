#include "Interpreter.cpp"

int main(int argc, char* argv[]) {
	if(!Interface::parseArguments(argc, argv)) {
		return 1;
	}

	Interface::printPreferences();

	switch(Interface::preferences.mode) {
		case Interface::Preferences::Mode::Interpret: {
			optional<string_view> code;
			optional<deque<Lexer::Token>> tokens;
			optional<NodeSP> tree;
			optional<thread> clientThread;

			if(Interface::preferences.socket) {
				mutex interpreterLock;

				Interface::clientSideHandler = [&](Node node) {
					string action = node.get("action");

					if(action == "lex") {
						auto lock = lock_guard<mutex>(interpreterLock);

						code = node.get<string>("code");
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
							SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true, true, true, true), *code, *tokens, *tree)->interpret();
						}
					} else
					if(action == "evaluate") {
						string code = node.get("code");
						auto tokens = Lexer(code).tokenize();
						auto tree = Parser(tokens).parse();

						SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true), code, tokens, tree)->interpret();
					}
				};

				clientThread = thread(Interface::becomeClient);
			}

			if(Interface::preferences.scriptPath) {
				code = read_file(*Interface::preferences.scriptPath);

				if(code) {
					tokens = Lexer(*code).tokenize();
				}
				if(tokens) {
					tree = Parser(*tokens).parse();
				}
				if(code && tokens && tree) {
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
			Interface::becomeServer();

			break;
		}
	}

	return 0;
}