#include "Lexer.cpp"
#include "Parser.cpp"
#include "Interpreter.cpp"

#include "crow_all.h"

int main() {
	Lexer lexer = Lexer();
//	Parser parser;
//	Interpreter interpreter;

	crow::SimpleApp app;

	CROW_ROUTE(app, "/")([](const crow::request&, crow::response& response) {
		response.set_static_file_info("Resources/Interface.html");
		response.end();
	});
	CROW_ROUTE(app, "/lex").methods("POST"_method)([&lexer](const crow::request& req) {
		json lexerResult = lexer.tokenize(req.body);

		return lexerResult.dump(4);
	});

	app.port(3007).multithreaded().run();

	return 0;
}