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

template<typename Container, typename T>
bool contains(const Container& container, const T& value) {
    return find(container.begin(), container.end(), value) != container.end();
}

// ----------------------------------------------------------------

class Parser {
public:
	Parser() {};

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
			Node node = {
				{"type", "argument"},
				{"range", {
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
			Node node = {
				{"type", "arrayLiteral"},
				{"range", {}},
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
			Node node = {
				{"type", "arrayType"},
				{"range", {}},
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
			Node node = {
				{"type", "asyncExpression"},
				{"range", {}},
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
			Node node = {
				{"type", "awaitExpression"},
				{"range", {}},
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
			Node node = {
				{"type", type+"Body"},
				{"range", {}},
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
			Node node = {
				{"type", "booleanLiteral"},
				{"range", {}},
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
			Node node = {
				{"type", "breakStatement"},
				{"range", {}},
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
			Node node = {
				{"type", "callExpression"},
				{"range", {
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
			Node node = {
				{"type", "caseDeclaration"},
				{"range", {}},
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
			Node node = {
				{"type", "catchClause"},
				{"range", {}},
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
			Node node = {
				{"type", "chainDeclaration"},
				{"range", {
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

			if(some(*node.get<NodeArrayRef>("modifiers"), [](auto& v) { return v != "static"; })) {
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
			Node node = {
				{"type", "chainExpression"},
				{"range", {
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
			Node node = {
				{"type", "chainIdentifier"},
				{"range", {
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
				{"range", {
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

			if(!node.empty("modifiers") && some(*node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"private", "protected", "public", "static", "final"}.contains(v); })) {
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
				{"range", {}},
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
				{"range", {}},
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
				{"range", {}},
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
				{"range", {
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
			Node node = {
				{"type", "defaultExpression"},
				{"range", {
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
			Node node = {
				{"type", "defaultType"},
				{"range", {
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
			Node node = {
				{"type", "deinitializerDeclaration"},
				{"range", {}},
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
			Node node = {
				{"type", "deleteExpression"},
				{"range", {}},
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
			Node node = {
				{"type", "dictionaryLiteral"},
				{"range", {}},
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
			Node node = {
				{"type", "dictionaryType"},
				{"range", {}},
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
			Node node = {
				{"type", "doStatement"},
				{"range", {}},
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
			Node node = {
				{"type", "entry"},
				{"range", {
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
				{"range", {
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

			if(!node.empty("modifiers") && some(*node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"private", "protected", "public", "static", "final"}.contains(v); })) {
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
				{"range", {
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
				{"range", {}},
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
				{"range", {}},
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
		if(type == "forStatement") {
			NodeRef node = make_shared<Node>(Node {
				{"type", "forStatement"},
				{"range", {}},
				{"identifier", nullptr},
				{"in", nullptr},
				{"where", nullptr},
				{"value", nullptr}
			});

			if(token()->type != "keywordFor") {
				return nullptr;
			}

			node->get<NodeRef>("range")->set("start") = position++;
			node->set("identifier") = rules("identifier");

			if(token()->type == "keywordIn") {
				position++;
				node->set("in") = rules("expressionsSequence");
			}
			if(token()->type == "keywordWhere") {
				position++;
				node->set("where") = rules("expressionsSequence");
			}

			helpers_bodyTrailedValue(node, node->empty("where") ? "where" : "in", "value");

			if(node->empty("identifier")) {
				report(1, node->get<NodeRef>("range")->get("start"), node->get("type"), "No identifier.");
			}
			if(node->empty("in")) {
				report(2, node->get<NodeRef>("range")->get("start"), node->get("type"), "No in.");
			}
			if(node->empty("value")) {
				report(0, node->get<NodeRef>("range")->get("start"), node->get("type"), "No value.");
			}

			node->get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "functionBody") {
			return rules("body", "function");
		} else
		if(type == "functionDeclaration") {
			bool anonymous = !arguments.empty() && any_cast<bool>(arguments[0]);
			Node node = {
				{"type", string("function")+(!anonymous ? "Declaration" : "Expression")},
				{"range", {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"identifier", nullptr},
				{"signature", nullptr},
				{"body", nullptr}
			};

			if(token()->type != "keywordFunc") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("identifier") =
				rules("identifier") ?:
				rules("operator");

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

			node.set("signature") = rules("functionSignature");
			node.set("body") = rules("functionBody");

			if(node.empty("signature")) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "No signature.");
			}
			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "functionExpression") {
			return rules("functionDeclaration", true);
		} else
		if(type == "functionSignature") {
			Node node = {
				{"type", "functionSignature"},
				{"range", {
					{"start", position}
				}},
				{"genericParameters", rules("genericParametersClause")},
				{"parameters", NodeArray {}},
				{"inits", -1},
				{"deinits", -1},
				{"awaits", -1},
				{"throws", -1},
				{"returnType", nullptr}
			};

			if(token()->type == "parenthesisOpen") {
				position++;
				node.set("arguments", helpers_skippableNodes(
					vector<string> {"parameter"},
					[this]() { return token()->type == "parenthesisOpen"; },
					[this]() { return token()->type == "parenthesisClosed"; },
					[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
				));

				if(token()->type == "parenthesisClosed") {
					position++;
				} else {
					node.get<NodeRef>("range")->set("end") = position-1;

					report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Parameters don't have the closing parenthesis and were decided to be autoclosed at the end of stream.");

					return node;
				}
			}

			while(set<string> {"keywordAwaits", "keywordThrows"}.contains(token()->type)) {
				if(token()->type == "keywordAwaits") {
					position++;
					node.set("awaits") = 1;
				}
				if(token()->type == "keywordThrows") {
					position++;
					node.set("throws") = 1;
				}
			}

			if(token()->type.starts_with("operator") && token()->value == "->") {
				position++;
				node.set("returnType") = rules("type");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			if(node.get<NodeRef>("range")->get<int>("end") < node.get<NodeRef>("range")->get<int>("start")) {
				return nullptr;
			}

			return node;
		} else
		if(type == "functionStatement") {
			NodeArrayRef types = rules("functionStatements", true);

			for(const string& type : *types) {
				NodeRef node = rules(type);

				if(node != nullptr) {
					return node;
				}
			}
		} else
		if(type == "functionStatements") {
			bool list = !arguments.empty() && any_cast<bool>(arguments[0]);
			NodeValue types = {
				"expressionsSequence",  // Expressions must be parsed first as they may include (anonymous) declarations
				"declaration",
				"controlTransferStatement",
				"doStatement",
				"forStatement",
				"ifStatement",
				"whileStatement"
			};

			return !list ? rules("statements", types) : types;
		} else
		if(type == "functionType") {
			Node node = {
				{"type", "functionType"},
				{"range", {
					{"start", position},
					{"end", position}
				}},
				{"genericParameterTypes", rules("genericParametersClause")},
				{"parameterTypes", NodeArray {}},
				{"awaits", -1},
				{"throws", -1},
				{"returnType", nullptr}
			};

			if(token()->type.starts_with("operator") && token()->value == "<") {
				position++;
				node.set("genericParameterTypes", helpers_sequentialNodes(
					vector<string> {"type"},
					[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
				));

				if(token()->type.starts_with("operator") && token()->value == ">") {
					node.get<NodeRef>("range")->set("end") = position++;
				} else {
					position = node.get<NodeRef>("range")->get("start");

					return nullptr;
				}
			}

			if(token()->type != "parenthesisOpen") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("parameterTypes", helpers_sequentialNodes(
				vector<string> {"type"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			));

			if(token()->type != "parenthesisClosed") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;

			while(set<string> {"keywordAwaits", "keywordThrows"}.contains(token()->type)) {
				if(token()->type == "keywordAwaits") {
					position++;
					node.set("awaits") = 1;

					if(token()->type == "operatorPostfix" && token()->value == "?") {
						position++;
						node.set("awaits") = 0;
					}
				}
				if(token()->type == "keywordThrows") {
					position++;
					node.set("throws") = 1;

					if(token()->type == "operatorPostfix" && token()->value == "?") {
						position++;
						node.set("throws") = 0;
					}
				}
			}

			if(!token()->type.starts_with("operator") || token()->value != "->") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("returnType") = rules("type");

			if(node.empty("returnType")) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "No return type.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "genericParameter") {
			Node node = {
				{"type", "genericParameter"},
				{"range", {
					{"start", position}
				}},
				{"identifier", rules("identifier")},
				{"type_", nullptr}
			};

			if(node.empty("identifier")) {
				return nullptr;
			}

			node.set("type_") = rules("typeClause");
			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "genericParametersClause") {
			NodeArray nodes;
			int start = position;

			if(!token()->type.starts_with("operator") || token()->value != "<") {
				return nodes;
			}

			position++;
			nodes = helpers_skippableNodes(
				vector<string> {"genericParameter"},
				[this]() { return token()->type.starts_with("operator") && token()->value == "<"; },
				[this]() { return token()->type.starts_with("operator") && token()->value == ">"; },
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			);

			if(!token()->type.starts_with("operator") || token()->value != ">") {
				report(1, start, "genericParametersClause", "Node doesn't have the closing angle and was decided to be autoclosed at the end of stream.");

				return nodes;
			}

			position++;

			return nodes;
		} else
		if(type == "identifier") {
			Node node = {
				{"type", "identifier"},
				{"range", {}},
				{"value", nullptr}
			};

			if(token()->type != "identifier") {
				return nullptr;
			}

			node.set("value") = token()->value;
			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "ifStatement") {
			NodeRef node = make_shared<Node>(Node {
				{"type", "ifStatement"},
				{"range", {}},
				{"condition", nullptr},
				{"then", nullptr},
				{"else", nullptr}
			});

			if(token()->type != "keywordIf") {
				return nullptr;
			}

			node->get<NodeRef>("range")->set("start") = position++;
			node->set("condition") = rules("expressionsSequence");

			helpers_bodyTrailedValue(node, "condition", "then");

			node->set("else") = rules("elseClause");

			if(node->empty("condition")) {
				report(2, node->get<NodeRef>("range")->get("start"), node->get("type"), "No condition.");
			}
			if(node->empty("then")) {
				report(0, node->get<NodeRef>("range")->get("start"), node->get("type"), "No value.");
			}

			node->get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "implicitChainExpression") {
			Node node = {
				{"type", "implicitChainExpression"},
				{"range", {}},
				{"member", nullptr}
			};

			if(!token()->type.starts_with("operator") || token()->type.ends_with("Postfix") || token()->value != ".") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("member") =
				rules("identifier") ?:
				rules("stringLiteral");

			if(node.empty("member")) {
				position--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = node.get<NodeRef>("member")->get<NodeRef>("range")->get("end");

			return node;
		} else
		if(type == "implicitChainIdentifier") {
			Node node = {
				{"type", "implicitChainIdentifier"},
				{"range", {}},
				{"value", nullptr}
			};

			if(!token()->type.starts_with("operator") || token()->type.ends_with("Postfix") || token()->value != ".") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("identifier");

			if(node.empty("value")) {
				position--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = node.get<NodeRef>("value")->get<NodeRef>("range")->get("end");

			return node;
		} else
		if(type == "importDeclaration") {
			Node node = {
				{"type", "importDeclaration"},
				{"range", {}},
				{"value", nullptr}
			};

			if(token()->type != "keywordImport") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;
			node.set("value") = rules("identifier");

			if(!node.empty("value")) {
				while(!tokensEnd()) {
					NodeRef node_ = rules("chainIdentifier", node.get<NodeRef>("value"));

					if(node_ == nullptr) {
						break;
					}

					node.set("value") = node_;
				}
			} else {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "No value.");
			}

			if(!node.empty("value")) {
				node.get<NodeRef>("range")->set("end") = node.get<NodeRef>("value")->get<NodeRef>("range")->get("end");
			}

			return node;
		} else
		if(type == "infixExpression") {
			return (
				rules("conditionalOperator") ?:
				rules("inOperator") ?:
				rules("isOperator") ?:
				rules("infixOperator")
			);
		} else
		if(type == "infixOperator") {
			Node node = {
				{"type", "infixOperator"},
				{"range", {}},
				{"value", nullptr}
			};

			set<string> exceptions = {",", ":"};  // Enclosing nodes can use operators from the list as delimiters

			if(!set<string> {"operator", "operatorInfix"}.contains(token()->type) || exceptions.contains(token()->value)) {
				return nullptr;
			}

			node.set("value") = token()->value;
			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "inheritedTypesClause") {
			NodeArray nodes;
			int start = position;

			if(!token()->type.starts_with("operator") || token()->value != ":") {
				return nodes;
			}

			position++;
			nodes = helpers_sequentialNodes(
				vector<string> {"typeIdentifier"},
				[this]() { return token()->type.starts_with("operator") && token()->value == ","; }
			);

			if(nodes.empty()) {
				report(0, start, "inheritedTypesClause", "No type identifiers.");
			}

			return nodes;
		} else
		if(type == "initializerClause") {
			NodeRef node;
			int start = position;

			if(!token()->type.starts_with("operator") || token()->value != "=") {
				return nullptr;
			}

			position++;
			node = rules("expressionsSequence");

			if(node == nullptr) {
				report(0, start, "initializerClause", "No value.");
			}

			return node;
		} else
		if(type == "initializerDeclaration") {
			Node node = {
				{"type", "initializerDeclaration"},
				{"range", {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"nillable", false},
				{"signature", nullptr},
				{"body", nullptr}
			};

			if(token()->type != "identifier" || token()->value != "init") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;

			if(token()->type.starts_with("operator") && token()->value == "?") {
				position++;
				node.set("nillable") = true;
			}

			node.set("signature") = rules("functionSignature");
			node.set("body") = rules("functionBody");

			if(some(*node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"private", "protected", "public"}.contains(v); })) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Wrong modifier(s).");
			}
			if(node.empty("signature")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No signature.");
			} else
			if(!node.get<NodeRef>("signature")->empty("returnType")) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Signature shouldn't have a return type.");
			}
			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "inOperator") {
			Node node = {
				{"type", "inOperator"},
				{"range", {
					{"start", position}
				}},
				{"inverted", false},
				{"composite", nullptr}
			};

			if(token()->type == "operatorPrefix" && token()->value == "!") {
				position++;
				node.set("inverted") = true;
			}

			if(token()->type != "keywordIn") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("composite") = rules("expressionsSequence");

			if(node.empty("composite")) {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "inoutExpression") {
			Node node = {
				{"type", "inoutExpression"},
				{"range", {}},
				{"value", nullptr}
			};

			if(token()->type != "operatorPrefix" || token()->value != "&") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("postfixExpression");

			if(node.empty("value")) {
				position--;

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "inoutType") {
			Node node = {
				{"type", "inoutType"},
				{"range", {}},
				{"value", nullptr}
			};

			if(token()->type != "keywordInout") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") = position++;
			node.set("value") = rules("unionType");
			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "integerLiteral") {
			Node node = {
				{"type", "integerLiteral"},
				{"range", {}},
				{"value", nullptr}
			};

			if(token()->type != "numberInteger") {
				return nullptr;
			}

			node.set("value") = token()->value;
			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "intersectionType") {
			Node node = {
				{"type", "intersectionType"},
				{"range", {
					{"start", position}
				}},
				{"subtypes", helpers_sequentialNodes(
					vector<string> {"postfixType"},
					[this]() { return token()->type.starts_with("operator") && token()->value == "&"; }
				)}
			};

			NodeArrayRef subtypes = node.get("subtypes");

			if(subtypes->empty()) {
				return nullptr;
			}
			if(subtypes->size() == 1) {
				return subtypes->at(0);
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "isOperator") {
			Node node = {
				{"type", "isOperator"},
				{"range", {
					{"start", position}
				}},
				{"type_", nullptr},
				{"inverted", false}
			};

			if(token()->type == "operatorPrefix" && token()->value == "!") {
				position++;
				node.set("inverted") = true;
			}

			if(token()->type != "keywordIs") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("type_") = rules("type");

			if(node.empty("type_")) {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "literalExpression") {
			return (
				rules("arrayLiteral") ?:
				rules("booleanLiteral") ?:
				rules("dictionaryLiteral") ?:
				rules("floatLiteral") ?:
				rules("integerLiteral") ?:
				rules("nilLiteral") ?:
				rules("stringLiteral")
			);
		} else
		if(type == "modifiers") {
			NodeArray values;
			int start = position;
			set<string> keywords = {
				"keywordInfix",
				"keywordPostfix",
				"keywordPrefix",

				"keywordPrivate",
				"keywordProtected",
				"keywordPublic",

				"keywordFinal",
				"keywordLazy",
				"keywordStatic",
				"keywordVirtual"
			};

			while(keywords.contains(token()->type)) {
				values.push_back(token()->value);
				position++;
			}

			set<string> operator_ = {"infix", "postfix", "prefix"},
						access = {"private", "protected", "public"};

			if(
				filter(values, [&](const string& v) { return operator_.contains(v); }).size() > 1 ||
				filter(values, [&](const string& v) { return access.contains(v); }).size() > 1 ||
				contains(values, "final") && contains(values, "virtual")
			) {
				report(1, start, "modifiers", "Mutual exclusion.");
			}

			return values;
		} else
		if(type == "module") {
			Node node = {
				{"type", "module"},
				{"range", {
					{"start", position}
				}},
				{"statements", rules("functionStatements")}
			};

			node.get<NodeRef>("range")->set("end") = !node.get<NodeArrayRef>("statements")->empty() ? position-1 : 0;

			return node;
		} else
		if(type == "namespaceBody") {
			return rules("body", "namespace");
		} else
		if(type == "namespaceDeclaration") {
			bool anonymous = !arguments.empty() && any_cast<bool>(arguments[0]);
			Node node = {
				{"type", string("namespace")+(!anonymous ? "Declaration" : "Expression")},
				{"range", {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"identifier", nullptr},
				{"body", nullptr}
			};

			if(token()->type != "keywordNamespace") {
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

			node.set("body") = rules("enumerationBody");

			if(!node.empty("modifiers") && some(*node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"private", "protected", "public", "static", "final"}.contains(v); })) {
				report(1, node.get<NodeRef>("range")->get("start"), node.get("type"), "Wrong modifier(s).");
			}
			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "namespaceExpression") {
			return rules("namespaceDeclaration", true);
		} else
		if(type == "namespaceStatements") {
			return rules("statements", vector<string> {"declaration"});
		} else
		if(type == "nillableExpression") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = {
				{"type", "nillableExpression"},
				{"range", {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"value", node_}
			};

			if(!set<string> {"operatorInfix", "operatorPostfix"}.contains(token()->type) || token()->value != "?") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "nillableType") {
			NodeRef node_ = any_cast<NodeRef>(arguments[0]);
			Node node = {
				{"type", "nillableType"},
				{"range", {
					{"start", node_->get<NodeRef>("range")->get("start")}
				}},
				{"value", node_}
			};

			if(token()->type != "operatorPostfix" || token()->value != "?") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "nilLiteral") {
			Node node = {
				{"type", "nilLiteral"},
				{"range", {}}
			};

			if(token()->type != "keywordNil") {
				return nullptr;
			}

			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "observerDeclaration") {
			Node node = {
				{"type", "observerDeclaration"},
				{"range", {
					{"start", position}
				}},
				{"identifier", rules("identifier")},
				{"body", nullptr}
			};

			NodeRef identifier = node.get("identifier");

			if(!set<string> {
				"willGet",
				"get",
				"didGet",
				"willSet",
				"set",
				"didSet",
				"willDelete",
				"delete",
				"didDelete"
			}.contains(identifier != nullptr ? identifier->get("value") : "")) {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			node.set("body") = rules("functionBody");

			if(node.empty("body")) {
				report(0, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			}

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "observersBody") {
			bool strict = !arguments.empty() && any_cast<bool>(arguments[0]);
			NodeRef node = rules("body", "observers");

			if(node != nullptr && strict && !some(*node->get<NodeArrayRef>("statements"), [](const NodeRef& v) { return v->get("type") != "unsupported"; })) {
				position = node->get<NodeRef>("range")->get("start");

				return nullptr;
			}

			return node;
		} else
		if(type == "observersStatements") {
			return rules("statements", vector<string> {"observerDeclaration"});
		} else
		if(type == "operator") {
			Node node = {
				{"type", "operator"},
				{"range", {}},
				{"value", nullptr}
			};

			if(!token()->type.starts_with("operator")) {
				return nullptr;
			}

			node.set("value") = token()->value;
			node.get<NodeRef>("range")->set("start") =
			node.get<NodeRef>("range")->set("end") = position++;

			return node;
		} else
		if(type == "operatorBody") {
			return rules("body", "operator");
		} else
		if(type == "operatorDeclaration") {
			Node node = {
				{"type", "operatorDeclaration"},
				{"range", {
					{"start", position}
				}},
				{"modifiers", rules("modifiers")},
				{"operator", nullptr},
				{"body", nullptr}
			};

			if(token()->type != "keywordOperator") {
				position = node.get<NodeRef>("range")->get("start");

				return nullptr;
			}

			position++;
			node.set("operator") = rules("operator");
			node.set("body") = rules("operatorBody");

			/*
			if(!some(*node.get<NodeArrayRef>("modifiers"), [](auto& v) { return !set<string> {"infix", "postfix", "prefix"}.contains(v); })) {
				report(2, node.get<NodeRef>("range")->get("start"), node.get("type"), "Should have specific modifier (infix, postfix, prefix).");
			}
			if(node.empty("operator")) {
				report(2, node.get<NodeRef>("range")->get("start"), node.get("type"), "No operator.");
			}
			if(node.empty("body")) {
				report(2, node.get<NodeRef>("range")->get("start"), node.get("type"), "No body.");
			} else {
				
			}
			*/

			node.get<NodeRef>("range")->set("end") = position-1;

			return node;
		} else
		if(type == "statements") {
			vector<string> types = any_cast<vector<string>>(arguments[0]);

			return helpers_skippableNodes(
				types,
				[this]() { return token()->type == "braceOpen"; },
				[this]() { return token()->type == "braceClosed"; },
				[this]() { return token()->type == "delimiter"; },
				true
			);
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
	void helpers_bodyTrailedValue(NodeRef node, const string& valueKey, const string& bodyKey, bool statementTrailed = true, function<NodeRef()> body = nullptr) {
		if(!body) {
			body = [this]() { return rules("functionBody"); };
		}

		node->set(bodyKey, body());

		if(node->empty(valueKey) || !node->empty(bodyKey)) {
			return;
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

		if(statementTrailed && node->empty(bodyKey)) {
			node->set(bodyKey, rules("functionStatement"));
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
			{"range", {
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
							{"range", {
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
					{"range", {
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

	Position position = Position(*this, 0);

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