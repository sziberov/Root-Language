#include "Lexer.cpp"
#include "Parser.cpp"
#include "Interpreter.cpp"

#include "crow_all.h"
#include "glaze/glaze.hpp"

int main() {
	Lexer lexer;
	Parser parser;
//	Interpreter interpreter;

	shared_ptr<Lexer::Result> lexerResult;
	shared_ptr<Parser::Result> parserResult;

	crow::SimpleApp app;
	mutex interpreterLock;

	CROW_ROUTE(app, "/")([](const crow::request&, crow::response& response) {
		response.set_static_file_info("Resources/Interface.html");
		response.end();
	});
	CROW_ROUTE(app, "/lex").methods("POST"_method)([&](const crow::request& req) {
		auto lock = lock_guard<mutex>(interpreterLock);
		auto start = chrono::high_resolution_clock::now();
		lexerResult = make_shared<Lexer::Result>(lexer.tokenize(req.body));
		string lexerResultString = glz::write_json(lexerResult).value_or("error");
		auto stop = chrono::high_resolution_clock::now();
		auto duration = chrono::duration_cast<chrono::milliseconds>(stop-start);

		cout << "                      [Lexer   ] Taken " << duration.count() << "ms by string(" << req.body.length() << ")" << endl;

		return lexerResultString;
	});
	CROW_ROUTE(app, "/parse")([&]() {
		auto lock = lock_guard<mutex>(interpreterLock);
		if(lexerResult == nullptr) return string();
		auto start = chrono::high_resolution_clock::now();
		string parserResultString = glz::write_json(parser.parse(*lexerResult)).value_or("error");
		auto stop = chrono::high_resolution_clock::now();
		auto duration = chrono::duration_cast<chrono::milliseconds>(stop-start);

		cout << "                      [Parser  ] Taken " << duration.count() << "ms by rawTokens(" << lexerResult->rawTokens.size() << ")" << endl;

		return parserResultString;
	});

	app.port(3007).multithreaded().run();

	return 0;
}