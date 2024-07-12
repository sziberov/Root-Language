#pragma once

#include "Lexer.cpp"

#include <any>

using Location = Lexer::Location;
using Token = Lexer::Token;

class Parser {
public:
	struct Report {
		int level,
			position;
		shared_ptr<Location> location;
		string string;
	};

	deque<shared_ptr<Token>> tokens;
	int _position;
	deque<shared_ptr<Report>> reports;

	using Node = unordered_map<string, any>;

	struct Range {
		int start = -1,
			end = -1;
	};

	template<typename... Args>
	any rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "argument") {
			auto node = make_shared<Node>(Node {
				{"type", "argument"},
				{"range", make_shared<Range>(Range {
					.start = position()
				})},
				{"label", rules("identifier")},
				{"value", nullopt}
			});

			if((*node)["label"].type() != typeid(nullopt_t) && token()->type.starts_with("operator") && token()->value == ":") {
				position()++;
			} else {
				position() = any_cast<shared_ptr<Range>>((*node)["range"])->start;
				(*node)["label"] = nullopt;
			}

			(*node)["value"] = rules("expressionsSequence");

			if((*node)["label"].type() == typeid(nullopt_t) && (*node)["value"].type() == typeid(nullopt_t)) {
				return nullopt;
			}

			any_cast<shared_ptr<Range>>((*node)["range"])->end = position()-1;

			return node;
		} else
		if(type == "arrayLiteral") {

		}

		return nullopt;
	}

	inline int& position() {
		return _position;
	}

	void setPosition(int value) {
		if(value < _position) {  // Rollback global changes
			erase_if(reports, [&](auto v) { return v->position >= value; });
		}

		_position = value;
	}

	shared_ptr<Token> token() {
		return tokens.size() > position()
			 ? tokens[position()]
			 : make_shared<Token>(Token());
	}

	inline bool tokensEnd() {
		return position() == tokens.size();
	}

	void report(int level, int position, const string& type, const string& string) {
		auto location = tokens[position]->location;

		for(auto v : reports) if(
			v->location->line == location->line &&
			v->location->column == location->column &&
			v->string == string
		) {
			return;
		}

		reports.push_back(make_shared<Report>(Report {
			level,
			position,
			location,
			type+" -> "+string
		}));
	}

	void reset() {
		tokens = {};
		_position = 0;
		reports = {};
	}

	struct Result {
		shared_ptr<Node> tree;
		deque<shared_ptr<Report>> reports;
	};

	Result parse(Lexer::Result lexerResult) {
		reset();

		tokens = lexerResult.tokens;

		Result result = {
			any_cast<shared_ptr<Node>>(rules("module")),
			reports
		};

		reset();

		return result;
	}
};

/*
class ASTNode;

using NodeValue = variant<int, double, string, shared_ptr<ASTNode>, vector<shared_ptr<ASTNode>>, vector<shared_ptr<Token>>>;

class ASTNode {
public:
	unordered_map<string, NodeValue> properties;

	void setProperty(const string& key, NodeValue value) {
		properties[key] = value;
	}

	template<typename T>
	T getProperty(const string& key) const {
		return get<T>(properties.at(key));
	}
};

class Parser {
public:
	Parser(const vector<Token>& tokens) : tokens(tokens), current(0) {}

	vector<shared_ptr<ASTNode>> skippableNodes(
		const vector<string>& types,
		function<bool()> opening,
		function<bool()> closing,
		function<bool()> separating,
		bool optionalSeparator = false
	) {
		vector<shared_ptr<ASTNode>> nodes;
		int scopeLevel = 1;

		while (!tokensEnd()) {
			shared_ptr<ASTNode> node;

			if (separating == nullptr || nodes.empty() || nodes.back()->getProperty<string>("type") == "separator" || optionalSeparator) {
				for (const auto& type : types) {
					node = parseNode(type);
					if (node != nullptr) {
						nodes.push_back(node);
						break;
					}
				}
			}

			if (closing() && scopeLevel == 1 || tokensEnd()) {
				break;
			}

			if (separating()) {
				node = nodes.back();
				if (node->getProperty<string>("type") != "separator") {
					node = make_shared<ASTNode>();
					node->setProperty("type", string("separator"));
					node->setProperty("rangeStart", current);
					node->setProperty("rangeEnd", current);
					nodes.push_back(node);
				} else {
					node->setProperty("rangeEnd", current);
				}
				current++;
			}

			if (node != nullptr) {
				continue;
			}

			node = nodes.back();
			if (node->getProperty<string>("type") != "unsupported") {
				node = make_shared<ASTNode>();
				node->setProperty("type", string("unsupported"));
				node->setProperty("rangeStart", current);
				node->setProperty("rangeEnd", current);
				node->setProperty("tokens", vector<shared_ptr<Token>>{});
			}

			scopeLevel += opening() ? 1 : 0;
			scopeLevel -= closing() ? 1 : 0;

			if (scopeLevel == 0) {
				break;
			}

			auto tokens = node->getProperty<vector<Token>>("tokens");
			tokens.push_back(this->tokens[current]);
			node->setProperty("tokens", tokens);
			node->setProperty("rangeEnd", current++);

			if (node != nodes.back()) {
				nodes.push_back(node);
			}
		}

		// Process the nodes for reporting
		for (size_t i = 0; i < nodes.size(); ++i) {
			auto node = nodes[i];
			bool range = node->getProperty<int>("rangeStart") != node->getProperty<int>("rangeEnd");

			if (node->getProperty<string>("type") == "separator") {
				if (i == 0) {
					report(0, node->getProperty<int>("rangeStart"), node->getProperty<string>("type"), "Met before any supported type at token [" + to_string(node->getProperty<int>("rangeStart")) + "].");
				}
				if (range) {
					report(0, node->getProperty<int>("rangeStart"), node->getProperty<string>("type"), "Sequence at range of tokens [" + to_string(node->getProperty<int>("rangeStart")) + ":" + to_string(node->getProperty<int>("rangeEnd")) + "].");
				}
				if (i == nodes.size() - 1) {
					report(0, node->getProperty<int>("rangeStart"), node->getProperty<string>("type"), "Excess at token [" + to_string(node->getProperty<int>("rangeStart")) + "].");
				}
			}
			if (node->getProperty<string>("type") == "unsupported") {
				string message = range ? "range of tokens [" + to_string(node->getProperty<int>("rangeStart")) + ":" + to_string(node->getProperty<int>("rangeEnd")) + "]" : "token [" + to_string(node->getProperty<int>("rangeStart")) + "]";
				report(1, node->getProperty<int>("rangeStart"), node->getProperty<string>("type"), "At " + message + ".");
			}
		}

		// Remove separator nodes from the list
		nodes.erase(remove_if(nodes.begin(), nodes.end(), [](const shared_ptr<ASTNode>& node) {
			return node->getProperty<string>("type") == "separator";
		}), nodes.end());

		return nodes;
	}

private:
	vector<Token> tokens;
	size_t current;

	shared_ptr<ASTNode> parseNode(const string& type, const unordered_map<string, NodeValue>& params = {}) {
		auto node = make_shared<ASTNode>();
		node->setProperty("type", type);

		if (type == "chainDeclaration") {
			// Process tokens specific to ChainDeclarationNode
			current++;
		} else if (type == "classDeclaration") {
			// Process tokens specific to ClassDeclarationNode
			current++;
		} else if (type == "deinitializerDeclaration") {
			// Process tokens specific to DeinitializerDeclarationNode
			current++;
		} else if (type == "arrayLiteral") {
			// Process tokens specific to ArrayLiteralNode
			current++;
			while (current < tokens.size() && tokens[current].type != "RBRACKET") {
				current++;
			}
			current++;
		} else if (type == "dictionaryLiteral") {
			// Process tokens specific to DictionaryLiteralNode
			current++;
			while (current < tokens.size() && tokens[current].type != "RBRACKET") {
				current++;
			}
			current++;
		} else {
			return nullptr;
		}

		for (const auto& param : params) {
			node->setProperty(param.first, param.second);
		}

		return node;
	}

	bool tokensEnd() const {
		return current >= tokens.size();
	}

	void report(int level, int position, const string& type, const string& message) {
		cerr << "Level " << level << " [" << position << "]: " << type << " - " << message << endl;
	}
};

// Example usage
int main() {
	// Example tokens
	vector<Token> tokens = {
		{"CHAIN_DECLARATION", "chain1"},
		{"UNKNOWN", "unknown1"},
		{"CLASS_DECLARATION", "class1"},
		{"LBRACKET", "["},
		{"RBRACKET", "]"},
		{"UNKNOWN", "unknown2"},
		{"LBRACKET", "["},
		{"COLON", ":"},
		{"RBRACKET", "]"},
		{"DEINITIALIZER_DECLARATION", "deinit1"},
		{"UNKNOWN", "unknown3"}
	};

	// Create the parser and parse the tokens into an AST
	Parser parser(tokens);
	vector<shared_ptr<ASTNode>> ast = parser.skippableNodes(
		{"chainDeclaration", "classDeclaration", "deinitializerDeclaration"},
		[]() { return false; },  // Example opening condition
		[]() { return false; },  // Example closing condition
		[]() { return false; }   // Example separating condition
	);

	// Print the AST node types
	for (const auto& node : ast) {
		cout << "Node type: " << node->getProperty<string>("type") << endl;
	}

	return 0;
}
*/