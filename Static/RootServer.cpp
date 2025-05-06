#include "Interpreter.cpp"

int main(int argc, char* argv[]) {
	if(Interface::parseArguments(argc, argv)) {
		Interface::printPreferences();
	} else {
		return 1;
	}

	optional<deque<Lexer::Token>> tokens;
	optional<NodeSP> tree;
	optional<Interpreter::TypeSP> value;

	switch(Interface::preferences.mode) {
		case Interface::Preferences::Mode::Interpret: {
			if(Interface::preferences.socket) {
				// TODO: Open long-living debug socket
			}

			optional<string> code = read_file(Interface::preferences.scriptPath);

			if(code) {
				tokens = sharedLexer.tokenize(*code);
			}
			if(tokens) {
				tree = sharedParser.parse(*tokens);
			}
			if(tree) {
				value = sharedInterpreter.interpret(*tokens, *tree);
			}

			break;
		}
		case Interface::Preferences::Mode::Dashboard: {
			crow::SimpleApp app;
			mutex interpreterLock;

			CROW_ROUTE(app, "/")([](const crow::request&, crow::response& response) {
				response.set_static_file_info("Resources/Interface.html");
				response.end();
			});
			CROW_WEBSOCKET_ROUTE(app, "/interface")
			.onopen([&](crow::websocket::connection& conn) {
				Interface::register_connection(&conn);
			})
			.onclose([&](crow::websocket::connection& conn, const string& reason, uint16_t) {
				Interface::unregister_connection(&conn);
			})
			.onmessage([&](crow::websocket::connection& conn, const string& data, bool isBinary) {
				if(!isBinary) {
					Interface::handle_message(data);
				}
			});
			CROW_ROUTE(app, "/lex").methods("POST"_method)([&](const crow::request& req) {
				auto lock = lock_guard<mutex>(interpreterLock);

				tokens = sharedLexer.tokenize(req.body);

				return string();
			});
			CROW_ROUTE(app, "/parse")([&]() {
				auto lock = lock_guard<mutex>(interpreterLock);

				if(tokens) {
					tree = sharedParser.parse(*tokens);
				}

				return string();
			});
			CROW_ROUTE(app, "/interpret")([&]() {
				auto lock = lock_guard<mutex>(interpreterLock);

				if(tokens && tree) {
					value = sharedInterpreter.interpret(*tokens, *tree);
				}

				return string();
			});

			app.port(Interface::preferences.socketPort)/*.multithreaded()*/.run();

			break;
		}
	}

	return 0;
}