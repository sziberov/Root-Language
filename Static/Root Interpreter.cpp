#include "Lexer.cpp"
#include "Parser.cpp"
//#include "Interpreter.cpp"

#include "crow_all.h"
#include "glaze/glaze.hpp"

int main() {
	Lexer lexer;
	Parser parser;
	//Interpreter interpreter;

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
		auto stop_0 = chrono::high_resolution_clock::now();
		string lexerResultString = glz::write_json(lexerResult).value_or("error");
		auto stop_1 = chrono::high_resolution_clock::now();
		auto duration_0 = chrono::duration_cast<chrono::milliseconds>(stop_0-start),
			 duration_1 = chrono::duration_cast<chrono::milliseconds>(stop_1-start);

		cout << "                      [Lexer   ] Taken " << duration_0.count() << " (" << duration_1.count() << " with serialization) ms by string(" << req.body.length() << ")" << endl;

		return lexerResultString;
	});
	CROW_ROUTE(app, "/lex_bm").methods("POST"_method)([&](const crow::request& req) {
		auto lock = lock_guard<mutex>(interpreterLock);
		int maxIterations = 16;
		chrono::milliseconds duration_0(0),
							 duration_1(0);
		string lexerResultString;

		for(int i = 0; i < maxIterations; i++) {
			auto start = chrono::high_resolution_clock::now();
			lexerResult = make_shared<Lexer::Result>(lexer.tokenize(req.body));
			auto stop_0 = chrono::high_resolution_clock::now();
			lexerResultString = glz::write_json(lexerResult).value_or("error");
			auto stop_1 = chrono::high_resolution_clock::now();

			duration_0 += chrono::duration_cast<chrono::milliseconds>(stop_0-start);
			duration_1 += chrono::duration_cast<chrono::milliseconds>(stop_1-start);
		}

		auto averageDuration_0 = duration_0.count()/maxIterations,
			 averageDuration_1 = duration_1.count()/maxIterations;

		cout << "                      [Lexer   ] Taken (average) " << averageDuration_0 << " (" << averageDuration_1 << " with serialization) ms by string(" << req.body.length() << ")[" << maxIterations << "]" << endl;

		return lexerResultString;
	});
	CROW_ROUTE(app, "/parse")([&]() {
		auto lock = lock_guard<mutex>(interpreterLock);
		if(lexerResult == nullptr) return string();
		auto start = chrono::high_resolution_clock::now();
		parserResult = make_shared<Parser::Result>(parser.parse(*lexerResult));
		auto stop_0 = chrono::high_resolution_clock::now();
		string parserResultString = glz::write_json(parserResult).value_or("error");
		auto stop_1 = chrono::high_resolution_clock::now();
		auto duration_0 = chrono::duration_cast<chrono::milliseconds>(stop_0-start),
			 duration_1 = chrono::duration_cast<chrono::milliseconds>(stop_1-start);

		cout << "                      [Parser  ] Taken " << duration_0.count() << " (" << duration_1.count() << " with serialization) ms by rawTokens(" << lexerResult->rawTokens.size() << ")" << endl;

		return parserResultString;
	});

	app.port(3007)/*.multithreaded()*/.run();

	return 0;
}