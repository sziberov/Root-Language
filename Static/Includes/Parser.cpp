#pragma once

#include "Lexer.cpp"
#include "Node.cpp"

using Location = Lexer::Location;
using Token = Lexer::Token;

template<typename T, typename UnaryPredicate>
vector<T> filter(const vector<T>& container, UnaryPredicate predicate) {
    vector<T> result;
    copy_if(container.begin(), container.end(), back_inserter(result), predicate);
    return result;
}

// ----------------------------------------------------------------

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
					{"start" , position}
				}},
				{"label", rules("identifier")},
				{"value", nullptr}
			};

			if(!node.empty("label") && token()->type.starts_with("operator") && token()->value == ":") {
				position++;
			} else {
				position = node.get<NodeRef>("range")->get("start");
				node.set("label") = nullptr;
			}

			node.set("value") = rules("expressionsSequence");

			if(node.empty("label") && node.empty("value")) {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

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

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("values", helpers_sequentialNodes(
				vector<string> {"expressionsSequence"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			));

			if(token()->type != "bracketClosed") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "arrayType") {
			Node node = Node {
				{"type", "arrayType"},
				{"range", Node {}},
				{"value", nullptr}
			};

			if(token()->type != "bracketOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("type");

			if(token()->type != "bracketClosed") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

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

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("expression");

			if(node.empty("value")) {
				position--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

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

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("expression");

			if(node.empty("value")) {
				position--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "body") {
			string type = any_cast<string>(arguments[0]);
			Node node = Node {
				{"type", type+"Body"},
				{"range", Node {}},
				{"statements", NodeArray {}}
			};

			if(token()->type != "braceOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("statements") = rules(type+"Statements");

			if(node.get<NodeArrayRef>("statements")->empty()) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No statements.");
			}

			if(!tokensEnd()) {
				node.get<NodeRef>("range")->set("end") = position++;
			} else {
				node.get<NodeRef>("range")->set("end") = position-1;

				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Node doesn't have the closing brace and was decided to be autoclosed at the end of stream.");
			}

			return node;
		} else
		if(type == "booleanLiteral") {
			Node node = Node {
				{"type", "booleanLiteral"},
				{"range", Node {}},
				{"value", nullptr}
			};

			if(!set<string> {"keywordFalse", "keywordTrue"}.contains(token()->type)) {
				return nullptr;
			}

			node.set("value") = token()->value;
			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "breakStatement") {
			Node node = Node {
				{"type", "breakStatement"},
				{"range", Node {}},
				{"label", nullptr}
			};

			if(token()->type != "keywordBreak") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("label") = rules("identifier");
			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "callExpression") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = Node {
				{"type", "callExpression"},
				{"range", Node {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"callee", node_},
				{"genericArguments", NodeArray {}},
				{"arguments", NodeArray {}},
				{"closure", nullptr}
			};

			if(token()->type.starts_with("operator") && token()->value == "<") {
				position++;
				node.set("genericArguments", helpers_sequentialNodes(
					vector<string> {"type"},
					[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
				));

				if(token()->type.starts_with("operator") && token()->value == ">") {
					position++;
				} else {
					position = node_->get<NodeRef>("range")->get<int>("end")+1;
				}
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			if(token()->type == "parenthesisOpen") {
				position++;
				node.set("arguments", helpers_skippableNodes(
					vector<string> {"argument"},
					[this]() { return token()->type == "parenthesisOpen"; },
					[this]() { return token()->type == "parenthesisClosed"; },
					[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
				));

				if(token()->type == "parenthesisClosed") {
					position++;
				} else {
					node.get<NodeRef>("range")->set("end") = position-1;

					report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Node doesn't have the closing parenthesis and was decided to be autoclosed at the end of stream.");

					return node;
				}
			}

			node.set("closure") = rules("closureExpression");

			if(node.empty("closure") && node.get<NodeRef>("range")->get("end") == position-1) {
				position = node_->get<NodeRef>("range")->get<int>("end")+1;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "caseDeclaration") {
			Node node = Node {
				{"type", "caseDeclaration"},
				{"range", Node {}},
				{"identifiers", NodeArray {}}
			};

			if(token()->type != "keywordCase") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("identifiers", helpers_sequentialNodes(
				vector<string> {"identifier"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			));

			if(node.get<NodeArrayRef>("identifiers")->empty()) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No identifiers(s).");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "catchClause") {
			Node node = Node {
				{"type", "catchClause"},
				{"range", Node {}},
				{"typeIdentifiers", NodeArray {}},
				{"body", nullptr},
				{"catch", nullptr}
			};

			if(token()->type != "keywordCatch") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("typeIdentifiers", helpers_sequentialNodes(
				vector<string> {"typeIdentifier"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			));
			node.set("body") = rules("functionBody");
			node.set("catch") = rules("catchClause");

			if(node.get<NodeArrayRef>("typeIdentifiers")->empty()) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No type identifiers.");
			}
			if(node.empty("body")) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "chainDeclaration") {
			Node node = Node {
				{"type", "chainDeclaration"},
				{"range", Node {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"body", nullptr}
			};

			if(token()->type != "identifier" || token()->value != "chain") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("body") = rules("observersBody");

			if(some(node.get<NodeArrayRef>("modifiers"), [](auto& v) { return v != "static"; })) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Can only have specific modifier (static).");
			}
			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "chainExpression") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = Node {
				{"type", "chainExpression"},
				{"range", Node {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"composite", node_},
				{"member", nullptr}
			};

			if(!set<string> {"operator", "operatorInfix"}.contains(token()->type) || token()->value != ".") {
				return nullptr;
			}

			position++;
			node.set("member", rules("identifier") ?:
							   rules("stringLiteral"));

			if(node.empty("member")) {
				position = node_->get<NodeRef>("range")->get<int>("end")+1;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = node.get<NodeRef>("member")->get<NodeRef>("range")->get("end");

			return node;
		} else
		if(type == "chainIdentifier") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = Node {
				{"type", "chainIdentifier"},
				{"range", Node {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"supervalue", node_},
				{"value", nullptr}
			};

			if(!set<string> {"operatorPrefix", "operatorInfix"}.contains(token()->type) || token()->value != ".") {
				return nullptr;
			}

			position++;
			node.set("value") = rules("identifier");

			if(node.empty("value")) {
				position = node_->get<NodeRef>("range")->get<int>("end")+1;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = node.get<NodeRef>("value")->get<NodeRef>("range")->get("end");

			return node;
		} else
		if(type == "chainStatements") {
			return rules("statements", vector<string> {"observerDeclaration"});
		} else
		if(type == "classBody") {
			return rules("body", "class");
		} else
		if(type == "classDeclaration") {
			bool anonymous = !arguments.empty() && any_cast<bool>(arguments[0]);
			Node node = {
				{"type", string("class")+(!anonymous ? "Declaration" : "Expression")},
				{"range", Node {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"identifier", nullptr},
				{"genericParameters", nullptr},
				{"inheritedTypes", nullptr},
				{"body", nullptr}
			};

			if(token()->type != "keywordClass") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("identifier") = rules("identifier");

			if(anonymous) {
				if(!node.get<NodeArrayRef>("modifiers")->empty() || !node.empty("identifier")) {
					position = node.get<NodeRef>("range")->get("start");

					return nullptr;
				}

				node.remove("modifiers");
				node.remove("identifier");
			} else
			if(node.empty("identifier")) {
				report(2, node.get<NodeRef>("range")->get("start"), node.get("type"), "No identifier.");
			}

			node.set("genericParameters") = rules("genericParametersClause");
			node.set("inheritedTypes") = rules("inheritedTypesClause");
			node.set("body") = rules("classBody");

			if(!node.empty("modifiers") && some(node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"private", "protected", "public", "static", "final"}.contains(v); })) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Wrong modifier(s).");
			}
			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "classExpression") {
			return rules("classDeclaration", true);
		} else
		if(type == "classStatements") {
			return rules("statements", vector<string> {
				"chainDeclaration",
				"classDeclaration",
				"deinitializerDeclaration",
				"enumerationDeclaration",
				"functionDeclaration",
				"initializerDeclaration",
				"namespaceDeclaration",
				"protocolDeclaration",
				"structureDeclaration",
				"subscriptDeclaration",
				"variableDeclaration"
			});
		} else
		if(type == "closureExpression") {
			Node node = {
				{"type", "closureExpression"},
				{"range", Node {}},
				{"signature", nullptr},
				{"statements", NodeArray {}}
			};

			if(token()->type != "braceOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("signature") = rules("functionSignature");

			if(!node.empty("signature")) {
				if(token()->type != "keywordIn") {
					position = node.get<NodeRef>("range")->get("start");

					return nullptr;
				}

				position++;
			} else {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No signature.");
			}

			node.set("statements") = rules("functionStatements");

			if(node.get<NodeArrayRef>("statements")->empty()) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No statements.");
			}

			if(!tokensEnd()) {
				node.get<NodeRef>("range")->set("end") = position++;
			} else {
				node.get<NodeRef>("range")->set("end") = position-1;

				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Node doesn't have the closing brace and was decided to be autoclosed at the end of stream.");
			}

			return node;
		} else
		if(type == "conditionalOperator") {
			Node node = {
				{"type", "conditionalOperator"},
				{"range", Node {}},
				{"expression", nullptr}
			};

			if(!token()->type.starts_with("operator") || token()->value != "?") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("expression") = rules("expressionsSequence");

			if(!token()->type.starts_with("operator") || token()->value != ":") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "continueStatement") {
			Node node = {
				{"type", "continueStatement"},
				{"range", Node {}},
				{"label", nullptr}
			};

			if(token()->type != "keywordContinue") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("label") = rules("identifier");
			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "controlTransferStatement") {
			return (
				rules("breakStatement") ?:
				rules("continueStatement") ?:
				rules("fallthroughStatement") ?:
				rules("returnStatement") ?:
				rules("throwStatement")
			);
		} else
		if(type == "declaration") {
			return (
				rules("chainDeclaration") ?:
				rules("classDeclaration") ?:
				rules("deinitializerDeclaration") ?:
				rules("enumerationDeclaration") ?:
				rules("functionDeclaration") ?:
				rules("importDeclaration") ?:
				rules("initializerDeclaration") ?:
				rules("namespaceDeclaration") ?:
				rules("operatorDeclaration") ?:
				rules("protocolDeclaration") ?:
				rules("structureDeclaration") ?:
				rules("subscriptDeclaration") ?:
				rules("variableDeclaration")
			);
		} else
		if(type == "declarator") {
			NodeRef node = make_shared<Node>(Node {
				{"type", "declarator"},
				{"range", Node {
					{"start", position}
				}},
				{"identifier", rules("identifier")},
				{"type_", nullptr},
				{"value", nullptr},
				{"body", nullptr}
			});

			if(node->empty("identifier")) {
				return nullptr;
			}

			node->set("type_") = rules("typeClause");
			node->set("value") = rules("initializerClause");

			helpers_bodyTrailedValue(node, "value", "body", false, [this]() { return rules("observersBody", true) ?: rules("functionBody"); });

			node->get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "defaultExpression") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = Node {
				{"type", "defaultExpression"},
				{"range", Node {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"value", node_}
			};

			if(token()->type != "operatorPostfix" || token()->value != "!") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "defaultType") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = Node {
				{"type", "defaultType"},
				{"range", Node {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"value", node_}
			};

			if(token()->type != "operatorPostfix" || token()->value != "!") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "deinitializerDeclaration") {
			Node node = Node {
				{"type", "deinitializerDeclaration"},
				{"range", Node {}},
				{"body", nullptr}
			};

			if(token()->type != "identifier" || token()->value != "deinit") {
				return nullptr;
			}

			position++;
			node.set("body") = rules("functionBody");

			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "deleteExpression") {
			Node node = Node {
				{"type", "deleteExpression"},
				{"range", Node {}},
				{"value", nullptr}
			};

			if(token()->type != "identifier" || token()->value != "delete") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("expression");

			if(node.empty("body")) {
				position--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "dictionaryLiteral") {
			Node node = Node {
				{"type", "dictionaryLiteral"},
				{"range", Node {}},
				{"entries", NodeArray {}}
			};

			if(token()->type != "bracketOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("entries", helpers_sequentialNodes(
				vector<string> {"entry"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			));

			if(node.get<NodeArrayRef>("entries")->empty() && token()->type.starts_with("operator") && token()->value == ":") {
				position++;
			}

			if(token()->type != "bracketClosed") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "dictionaryType") {
			Node node = Node {
				{"type", "dictionaryType"},
				{"range", Node {}},
				{"key", nullptr},
				{"value", nullptr}
			};

			if(token()->type != "bracketOpen") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("key") = rules("type");

			if(!token()->type.starts_with("operator") || token()->value != ":") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("key") = rules("type");


			if(token()->type != "bracketClosed") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "doStatement") {
			Node node = Node {
				{"type", "doStatement"},
				{"range", Node {}},
				{"body", nullptr},
				{"catch", nullptr}
			};

			if(token()->type != "keywordDo") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("body") = rules("functionBody");
			node.set("catch") = rules("catchClause");

			if(node.empty("body")) {
				report(2, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}
			if(node.empty("catch")) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "No catch.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "elseClause") {
			NodeRef node;
			int start = position;

			if(token()->type != "keywordElse") {
				return nullptr;
			}

			position++;
			node =
				rules("functionBody") ?:
				rules("expressionsSequence") ?:
				rules("ifStatement");

			if(node == nullptr) {
				report(0, start, "elseClause", "No value.");
			}

			return node;
		} else
		if(type == "entry") {
			Node node = Node {
				{"type", "entry"},
				{"range", Node {
					{"start", position}
				}},
				{"key", rules("expressionsSequence")},
				{"value", nullptr}
			};

			if(node.empty("key") || !token()->type.starts_with("operator") || token()->value != ":") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("value") = rules("expressionsSequence");

			if(node.empty("value")) {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "enumerationBody") {
			return rules("body", "enumeration");
		} else
		if(type == "enumerationDeclaration") {
			bool anonymous = !arguments.empty() && any_cast<bool>(arguments[0]);
			Node node = {
				{"type", string("enumeration")+(!anonymous ? "Declaration" : "Expression")},
				{"range", Node {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"identifier", nullptr},
				{"inheritedTypes", nullptr},
				{"body", nullptr}
			};

			if(token()->type != "keywordEnum") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("identifier") = rules("identifier");

			if(anonymous) {
				if(!node.get<NodeArrayRef>("modifiers")->empty() || !node.empty("identifier")) {
					position = node.get<NodeRef>("range")->get("start");

					return nullptr;
				}

				node.remove("modifiers");
				node.remove("identifier");
			} else
			if(node.empty("identifier")) {
				report(2, node.get<NodeRef>("range")->get("start"), node.get("type"), "No identifier.");
			}

			node.set("inheritedTypes") = rules("inheritedTypesClause");
			node.set("body") = rules("enumerationBody");

			if(!node.empty("modifiers") && some(node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"private", "protected", "public", "static", "final"}.contains(v); })) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Wrong modifier(s).");
			}
			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "enumerationExpression") {
			return rules("enumerationDeclaration", true);
		} else
		if(type == "enumerationStatements") {
			return rules("statements", vector<string> {
				"caseDeclaration",
				"enumerationDeclaration"
			});
		} else
		if(type == "expression") {
			return (
				rules("asyncExpression") ?:
				rules("awaitExpression") ?:
				rules("deleteExpression") ?:
				rules("tryExpression") ?:
				rules("prefixExpression")
			);
		} else
		if(type == "expressionsSequence") {
			Node node = {
				{"type", "expressionsSequence"},
				{"range", Node {
					{"start", position}
				}},
				{"values", NodeArray {}}
			};

			set<string> subsequentialTypes = {"inOperator", "isOperator"};
			NodeArrayRef values = node.set("values") = helpers_sequentialNodes(vector<string> {"expression", "infixExpression"}, nullptr, subsequentialTypes);

			if(values->empty()) {
				return nullptr;
			}
			if(filter(*values, [&](const NodeRef& v) { return !subsequentialTypes.contains(v->get("type")); }).size()%2 == 0) {
				position = values->back().get<NodeRef>()->get<NodeRef>("range")->get("start");

				values->pop_back();
			}
			if(values->size() == 1) {
				return values->at(0);
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "fallthroughStatement") {
			Node node = {
				{"type", "fallthroughStatement"},
				{"range", Node {}},
				{"label", nullptr}
			};

			if(token()->type != "keywordFallthrough") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("label") = rules("identifier");
			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "floatLiteral") {
			Node node = {
				{"type", "floatLiteral"},
				{"range", Node {}},
				{"value", nullptr}
			};

			if(token()->type != "numberFloat") {
				return nullptr;
			}

			node.set("value") = token()->value;
			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else

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
	NodeRef helpers_bodyTrailedValue(NodeRef node, const string& valueKey, const string& bodyKey, bool expressionTrailed = true, function<NodeRef()> body = nullptr) {
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
				position = n->get<NodeRef>("closure")->get<NodeRef>("range")->get("start");
				n->set("closure") = nullptr;
				end = position-1;

				NodeRef lhs = n->get("callee") ?: n->get("composite");
				bool exportable = lhs->get<NodeRef>("range")->get("end") == end;

				if(exportable) {
					for(const auto& [k, v] : *n)	n->remove(k);
					for(const auto& [k, v] : *lhs)	n->set(k, v);
				}
			}

			if(end != -1) {
				n->get<NodeRef>("range")->set("end") = end;
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
					position++;
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
				{"start", position},
				{"end", position}
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
			node->get<NodeRef>("range")->set("end") = position++;
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
								{"start", position},
								{"end", position}
							}}
						});

						nodes.push_back(node);
					} else {
						node->get<NodeRef>("range")->set("end") = position;
					}
				}

				position++;
			}

			if(node != nullptr) {
				continue;
			}

			node = nodes.back();

			if(node != nullptr && node->get("type") != "unsupported") {
				node = make_shared<Node>(Node {
					{"type", "unsupported"},
					{"range", Node {
						{"start", position},
						{"end", position}
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
			node->get<NodeRef>("range")->set("end") = position++;

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

	struct Position {
	private:
		Parser& self;
		int value;

		void update(int previous) {
			if(value < previous) {  // Rollback global changes
				erase_if(self.reports, [&](auto v) { return v->position >= value; });
			}
		}

	public:
		Position(Parser& s, int v = 0) : self(s), value(v) {}

		template <typename T>
		operator T() const {
			return value;
		}

		operator int() const {
			return value;
		}

		template <typename T>
		Position& operator=(const T& v) {
			int previous = value; value = v; update(previous); return *this;
		}

		Position& operator+=(int v) {
			value += v; update(value-v); return *this;
		}

		Position& operator-=(int v) {
			value -= v; update(value+v); return *this;
		}

		Position& operator++() {  // Prefix
			value++; update(value-1); return *this;
		}

		Position& operator--() {  // Prefix
			value--; update(value+1); return *this;
		}

		Position operator++(int) {  // Postfix
			Position previous = *this; value++; update(previous); return previous;
		}

		Position operator--(int) {  // Postfix
			Position previous = *this; value--; update(previous); return previous;
		}
	};

	Position position;

	shared_ptr<Token> token() {
		return tokens.size() > position
			 ? tokens[position]
			 : make_shared<Token>(Token());
	}

	inline bool tokensEnd() {
		return position == tokens.size();
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
		position = 0;
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