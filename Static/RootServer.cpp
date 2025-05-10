#include "Interpreter.cpp"

int main(int argc, char* argv[]) {
	if(Interface::parseArguments(argc, argv)) {
		Interface::printPreferences();
	} else {
		return 1;
	}

	optional<string_view> code;
	optional<deque<Lexer::Token>> tokens;
	optional<NodeSP> tree;

	switch(Interface::preferences.mode) {
		case Interface::Preferences::Mode::Interpret: {
			if(Interface::preferences.socket) {
				// TODO: Open long-living debug socket
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

			break;
		}
		case Interface::Preferences::Mode::Dashboard: {
			crow::SimpleApp app;
			mutex interpreterLock;

			CROW_ROUTE(app, "/")
			([](const crow::request&, crow::response& response) {
				response.set_static_file_info("Resources/Dashboard.html");
				response.end();
			});

			CROW_WEBSOCKET_ROUTE(app, "/socket")
			.onopen([&](crow::websocket::connection& conn) {
				Interface::registerСonnection(&conn);
			})
			.onclose([&](crow::websocket::connection& conn, const string& reason, uint16_t) {
				Interface::unregisterСonnection(&conn);
			})
			.onmessage([&](crow::websocket::connection& conn, const string& data, bool isBinary) {
				if(!isBinary) {
					Interface::handleMessage(data);
				}
			});

			Interface::handlers.push_back([&](crow::json::rvalue& JSON) {
				string action = JSON["action"].s();

				if(action == "lex") {
					auto lock = lock_guard<mutex>(interpreterLock);

					code = (string_view)JSON["code"].s();
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
					string code = JSON["code"].s();
					deque<Lexer::Token> tokens = Lexer(code).tokenize();
					NodeSP tree = Parser(tokens).parse();

					SP<Interpreter>(sharedInterpreter, Interpreter::InheritedContext(true), code, tokens, tree)->interpret();
				}
			});

			app.port(Interface::preferences.socket->port).run();

			break;
		}
	}

	return 0;
}