#pragma once

#include "Lexer.cpp"
#include "Parser.cpp"

using Report = Parser::Report;

class Interpreter {
public:
	Interpreter() {};

	deque<shared_ptr<Token>> tokens;
	NodeRef tree;
	int position;
	NodeArray contexts,
			  composites,
			  calls,
			  scopes,
			  controlTransfers;
	Node preferences;
	deque<shared_ptr<Report>> reports;

	template<typename... Args>
	NodeValue rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "argument") {}

		return nullptr;
	}

	NodeRef getContext(const string& tag) {
		Node flags;

		for(const Node& context : contexts) {
			if(context.empty("tags") || contains(context.get<NodeArray&>("tags"), tag)) {
				for(const auto& [k, v] : context.get<Node&>("flags")) {
					flags.get(k) = v;
				}
			}
		}

		return make_shared<Node>(flags);
	}

	/*
	 * Should be used to pass local state between super-rule and ambiguous sub-rules.
	 *
	 * Local means everything that does not belong to global execution process
	 * (calls, scopes, controlTransfers, etc), but rather to concrete rules.
	 *
	 * Rule can be considered ambiguous if it is e.g. subrule of a subrule and cannot be recognized directly.
	 *
	 * Specifying tags allows to filter current context(s).
	 */
	void addContext(const Node& flags, const NodeArray& tags) {
		contexts.push_back(Node {
			{"flags", flags},
			{"tags", tags}
		});
	}

	void removeContext() {
		contexts.pop_back();
	}

	void report(int level, NodeRef node, const string& string) {
		auto location = position < tokens.size()
					  ? tokens[position]->location
					  : make_shared<Location>(Location());

		reports.push_back(make_shared<Report>(Report {
			level,
			position,
			location,
			(node != nullptr ? node->get<::string>("type")+" -> " : "")+string
		}));
	}

	void print(const string& string) {
		reports.push_back(make_shared<Report>(Report {
			0,
			0,
			nullptr,
			string
		}));
	}

	void reset(const NodeArray& composites_ = {}, const Node& preferences_ = {}) {
		tokens = {};
		tree = nullptr;
		position = 0;
		contexts = {};
		composites = composites_;
		calls = {};
		scopes = {};
		controlTransfers = {};
		preferences = {
			{"callStackSize", 128},
			{"allowedReportLevel", 2},
			{"metaprogrammingLevel", 3},
			{"arbitaryPrecisionArithmetics", true}
		};
		for(const auto& [k, v] : preferences_) {
			preferences.get(k) = v;
		}
		reports = {};
	}

	struct Result {
		NodeArray composites;
		deque<shared_ptr<Report>> reports;
	};

	Result interpret(Lexer::Result lexerResult, Parser::Result parserResult, const NodeArray& composites_ = {}, const Node& preferences_ = {}) {
		reset(composites_, preferences_);

		tokens = lexerResult.tokens;
		tree = parserResult.tree;
	//	tree = structuredClone(parserResult.tree);

		rules("module");

		Result result = {
			composites,
			reports
		};

		reset();

		return result;
	}

	Result interpretRaw(string_view code, const NodeArray& composites_ = {}, const Node& preferences_ = {}) {
 		Lexer::Result lexerResult = Lexer().tokenize(code);
	 	Parser::Result parserResult = Parser().parse(lexerResult);
	 	Result interpreterResult = interpret(lexerResult, parserResult, composites_, preferences_);

	 	return interpreterResult;
	}
};