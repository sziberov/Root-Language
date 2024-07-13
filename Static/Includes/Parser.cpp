#pragma once

#include "Lexer.cpp"
#include "Node.cpp"

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

	template<typename... Args>
	NodeValue rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "argument") {
			Node node = Node {
				{"type", "argument"},
				{"range", Node {
					{"start" , position()}
				}},
				{"label", rules("identifier")},
				{"value", nullptr}
			};

			if(!node.empty("label") && token()->type.starts_with("operator") && token()->value == ":") {
				position()++;
			} else {
				position() = node.get<NodeRef>("range")->get("start");
				node.set("label", nullptr);
			}

			node.set("value", rules("expressionsSequence"));

			if(node.empty("label") && node.empty("value")) {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("end", position()-1);

			return node;
		} else
		if(type == "arrayLiteral") {
			Node node = Node {
				{"type", "arrayLiteral"},
				{"range", Node {}},
				{"values", NodeArray {}}
			};

			if(token()->type != "bracketOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start", position()++);
			node.set("values", helpers_sequentialNodes(
				vector<string> {"expressionsSequence"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			));

			if(token()->type != "bracketClosed") {
				position() = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end", position()++);

			return node;
		} else
		if(type == "arrayType") {
			Node node = Node {
				{"type", "arrayType"},
				{"range", Node {}},
				{"value", NodeArray {}}
			};

			if(token()->type != "bracketOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start", position()++);
			node.set("value", rules("type"));

			if(token()->type != "bracketClosed") {
				position() = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end", position()++);

			return node;
		} else
		if(type == "asyncExpression") {
			Node node = Node {
				{"type", "asyncExpression"},
				{"range", Node {}},
				{"value", nullptr}
			};

			if(token()->type != "keywordAsync") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start", position()++);
			node.set("value", rules("expression"));

			if(node.empty("value")) {
				position()--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end", position()-1);

			return node;
		} else
		if(type == "awaitExpression") {
			Node node = Node {
				{"type", "awaitExpression"},
				{"range", Node {}},
				{"value", nullptr}
			};

			if(token()->type != "keywordAwait") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start", position()++);
			node.set("value", rules("expression"));

			if(node.empty("value")) {
				position()--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end", position()-1);

			return node;
		}

		return nullptr;
	}

	/*
	 * Trying to set the node's value and body basing on its value.
	 *
	 * Value that have unsignatured trailing closure will be divided to:
	 * - Value - a left-hand-side (exported, if closure goes right after it).
	 * - Body - the closure.
	 *
	 * Closures, nested into expressionsSequence, prefixExpression and inOperator, are also supported.
	 *
	 * Useful for unwrapping trailing bodies and completing preconditional statements, such as if or for.
	 */
	NodeRef bodyTrailedValue(NodeRef node, const string& valueKey, const string& bodyKey, bool expressionTrailed = true, function<NodeRef()> body = nullptr) {
		if(!body) {
			body = [this]() { return rules("functionBody"); };
		}

		node->set(bodyKey, body());

		if(node->empty(valueKey) || !node->empty(bodyKey)) {
			return nullptr;
		}

		int end = -1;

		function<void(NodeRef)> parse = [&](NodeRef n) {
			if(n == node) {
				parse(n->get(valueKey));

				if(end != -1) {
					n->set(bodyKey, body());
				}
			} else
			if(n->get("type") == "expressionsSequence") {
				parse(n->get<NodeArrayRef>("values")->back());
			} else
			if(n->get("type") == "prefixExpression") {
				parse(n->get("value"));
			} else
			if(n->get("type") == "inOperator") {
				parse(n->get("composite"));
			} else
			if(!n->empty("closure") && n->get<NodeRef>("closure")->empty("signature")) {
				position() = n->get<NodeRef>("closure")->get<NodeRef>("range")->get("start");
				n->set("closure", nullptr);
				end = position()-1;

				NodeRef lhs = n->get("callee") ?: n->get("composite");
				bool exportable = lhs->get<NodeRef>("range")->get("end") == end;

				if(exportable) {
					for(const auto& [k, v] : *n)	n->remove(k);
					for(const auto& [k, v] : *lhs)	n->set(k, v);
				}
			}

			if(end != -1) {
				n->get<NodeRef>("range")->set("end", end);
			}
		};

		parse(node);

		if(expressionTrailed && node->empty(bodyKey)) {
			node->set(bodyKey, rules("expressionsSequence"));
		}
	}

	/*
	 * Returns a list of nodes of the types in sequential order like [1, 2, 3, 1...].
	 *
	 * Additionally a separator between the nodes can be set.
	 * Also types can be marked subsequential and therefore have no impact on offset in iterations.
	 *
	 * Stops when an unsupported type occurs.
	 *
	 * Useful for precise sequences lookup.
	 */
	NodeArray helpers_sequentialNodes(const vector<string>& types, function<bool()> separating = nullptr, optional<set<string>> subsequentialTypes = nullopt) {
		NodeArray nodes;
		int offset = 0;

		while(!tokensEnd()) {
			string type = types[offset%types.size()];
			NodeRef node = rules(type);

			if(node == nullptr) {
				break;
			}

			nodes.push_back(node);

			if(subsequentialTypes != nullopt && !(*subsequentialTypes).contains(node->get("type"))) {
				offset++;
			}

			if(separating && offset > 0) {
				if(separating()) {
					position()++;
				} else {
					break;
				}
			}
		}

		return nodes;
	}

	/*
	 * Returns a node of the type or the "unsupported" node if not closed immediately.
	 *
	 * Stops if node found, at 0 (relatively to 1 at start) scope level or at the .tokensEnd.
	 *
	 * Warns about "unsupported" nodes.
	 *
	 * Useful for imprecise single enclosed(ing) nodes lookup.
	 */
	NodeRef helpers_skippableNode(const string& type, function<bool()> opening, function<bool()> closing) {
		NodeRef node = rules(type);
		int scopeLevel = 1;

		if(node != nullptr || closing() || tokensEnd()) {
			return node;
		}

		node = make_shared<Node>(Node {
			{"type", "unsupported"},
			{"range", Node {
				{"start", position()},
				{"end", position()}
			}},
			{"tokens", NodeArray {}}
		});

		while(!tokensEnd()) {
			scopeLevel += opening();
			scopeLevel -= closing();

			if(scopeLevel == 0) {
				break;
			}

			node->get<NodeArrayRef>("tokens")->push_back(make_any<shared_ptr<Token>>(token()));
			node->get<NodeRef>("range")->set("end", position()++);
		}

		NodeRef nodeRange = node->get("range");
		string type_ = node->get("type");
		int start = nodeRange->get("start"),
			end = nodeRange->get("end");
		bool range = start != end;
		string message = range ? "range of tokens ["+to_string(start)+":"+to_string(end)+"]" : "token ["+to_string(start)+"]";

		report(1, start, type_, "At "+message+".");

		return node;
	}

	/*
	 * Returns a list of nodes of the types, including the "unsupported" nodes if any occur.
	 *
	 * Additionally a (optional) separator between the nodes can be set.
	 *
	 * Stops at 0 (relatively to 1 at start) scope level or at the .tokensEnd.
	 *
	 * Warns about invalid separators and "unsupported" nodes.
	 *
	 * Useful for imprecise multiple enclosed(ing) nodes lookup.
	 */
	NodeArray helpers_skippableNodes(const vector<string>& types, function<bool()> opening, function<bool()> closing, function<bool()> separating = nullptr, bool optionalSeparator = false) {
		NodeArray nodes;
		int scopeLevel = 1;

		while(!tokensEnd()) {
			NodeRef node;

			if(!separating || nodes.empty() || nodes.back().get<NodeRef>()->get("type") == "separator" || optionalSeparator) {
				for(const string& type : types) {
					node = rules(type);

					if(node != nullptr) {
						nodes.push_back(node);

						break;
					}
				}
			}

			if(closing() && scopeLevel == 1 || tokensEnd()) {
				break;
			}

			if(separating && separating()) {
				node = nodes.back();

				if(node != nullptr) {
					if(node->get("type") != "separator") {
						node = make_shared<Node>(Node {
							{"type", "separator"},
							{"range", Node {
								{"start", position()},
								{"end", position()}
							}}
						});

						nodes.push_back(node);
					} else {
						node->get<NodeRef>("range")->set("end", position());
					}
				}

				position()++;
			}

			if(node != nullptr) {
				continue;
			}

			node = nodes.back();

			if(node != nullptr && node->get("type") != "unsupported") {
				node = make_shared<Node>(Node {
					{"type", "unsupported"},
					{"range", Node {
						{"start", position()},
						{"end", position()}
					}},
					{"tokens", NodeArray {}}
				});
			}

			scopeLevel += opening();
			scopeLevel -= closing();

			if(scopeLevel == 0) {
				break;
			}

			node->get<NodeArrayRef>("tokens")->push_back(make_any<shared_ptr<Token>>(token()));
			node->get<NodeRef>("range")->set("end", position()++);

			if(node != nodes.back()) {
				nodes.push_back(node);
			}
		}

		for(int key = 0; key < nodes.size(); key++) {
			NodeRef node = nodes[key];
			string type = node->get("type");
			NodeRef nodeRange = node->get("range");
			int start = nodeRange->get("start"),
				end = nodeRange->get("end");
			bool range = start != end;

			if(type == "separator") {
				if(key == 0) {
					report(0, start, type, "Met before any supported type at token ["+to_string(start)+"].");
				}
				if(range) {
					report(0, start, type, "Sequence at range of tokens ["+to_string(start)+":"+to_string(end)+"].");
				}
				if(key == nodes.size()-1) {
					report(0, start, type, "Excess at token ["+to_string(start)+"].");
				}
			}
			if(type == "unsupported") {
				string message = range ? "range of tokens ["+to_string(start)+":"+to_string(end)+"]" : "token ["+to_string(start)+"]";

				report(1, start, type, "At "+message+".");
			}
		}

		erase_if(nodes, [](NodeRef v) { return v->get("type") == "separator"; });

		return nodes;
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
		NodeRef tree;
		deque<shared_ptr<Report>> reports;
	};

	Result parse(Lexer::Result lexerResult) {
		reset();

		tokens = lexerResult.tokens;

		Result result = {
			rules("module"),
			reports
		};

		reset();

		return result;
	}
};