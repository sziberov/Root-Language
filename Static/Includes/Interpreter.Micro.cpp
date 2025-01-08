#pragma once

#include "Lexer.cpp"
#include "Parser.cpp"

using Report = Parser::Report;

template<typename Container, typename T>
int index_of(Container container, const T& value) {
	auto it = find(container.begin(), container.end(), value);

	return it != container.end() ? it-container.begin() : -1;
}

template<typename Container, typename Predicate>
int find_index(Container container, Predicate predicate) {
	auto it = find_if(container.begin(), container.end(), predicate);

	return it != container.end() ? it-container.begin() : -1;
}

string tolower(string_view string) {
	auto result = ::string(string);

	transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });

	return result;
}

// ----------------------------------------------------------------

class Interpreter {
public:
	Interpreter() {};

	deque<Token> tokens;
	NodeRef tree;

	template<typename... Args>
	NodeValue rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "module") {
			cout << "Hello World" << endl;

			executeNodes(tree ? tree->get<NodeArrayRef>("statements") : nullptr);
		}

		return nullptr;
	}

	template<typename... Args>
	NodeValue executeNode(NodeRef node, Args... arguments) {
		NodeValue value;

		value = rules(node ? node->get("type") : "", node, arguments...);

		return value;
	}

	/*
	 * Last statement in a body will be treated like a local return (overwritable by subsequent
	 * outer statements) if there was no explicit control transfer previously. ECT should
	 * be implemented by rules.
	 *
	 * Control transfer result can be discarded (fully - 1, value only - 0).
	 */
	void executeNodes(NodeArrayRef nodes) {
		for(const NodeRef& node : nodes ? *nodes : NodeArray()) {
			NodeValue value = executeNode(node);
		}
	}

	void reset() {
		tokens = {};
		tree = nullptr;
	}

	NodeValue interpret(Lexer::Result lexerResult, Parser::Result parserResult) {
		reset();

		tokens = lexerResult.tokens;
		tree = parserResult.tree;

		NodeValue result = rules("module");

		reset();

		return result;
	}
};