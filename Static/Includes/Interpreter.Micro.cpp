#pragma once

#include "Lexer.cpp"
#include "Parser.cpp"
#include "Primitive.cpp"

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

	struct ControlTransfer {
		optional<Primitive> value;
		optional<string> type;
	};

	struct Preferences {
		int callStackSize,
			allowedReportLevel,
			metaprogrammingLevel;
		bool arbitaryPrecisionArithmetics;
	};

	deque<Token> tokens;
	NodeRef tree;
	int position;
	deque<ControlTransfer> controlTransfers;
	Preferences preferences;
	deque<Report> reports;

	template<typename... Args>
	variant<optional<Primitive>, NodeValue> rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "argument") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = executeNode(n->get("value"));

			if(!threw()) {
				return Node {
					{"label", !n->empty("label") ? n->get<Node&>("label").get("value") : NodeValue()},
					{"value", make_any<optional<Primitive>>(value)}
				};
			}
		} else
		if(type == "arrayLiteral") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = Primitive(PrimitiveDictionary());

			for(int i = 0; i < n->get<NodeArray&>("values").size(); i++) {
				auto value_ = executeNode(n->get<NodeArray&>("values")[i]);

				if(value_ != nullopt) {
					get<PrimitiveDictionary>(value.value).emplace(i, *value_);
				}
			}

		//	return getValueWrapper(value, "Array");
			return value;
		} else
		if(type == "booleanLiteral") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = Primitive(n->get("value") == "true");

		//	return getValueWrapper(value, "Boolean");
			return value;
		} else
		if(type == "breakStatement") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = !n->empty("label") ? optional(Primitive(n->get<Node&>("label").get<string>("value"))) : nullopt;

			setControlTransfer(value, "break");

			return value;
		} else
		if(type == "continueStatement") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = !n->empty("label") ? optional(Primitive(n->get<Node&>("label").get<string>("value"))) : nullopt;

			setControlTransfer(value, "continue");

			return value;
		} else
		if(type == "dictionaryLiteral") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = Primitive(PrimitiveDictionary());

			for(const NodeRef& entry : n->get<NodeArray&>("entries")) {
				NodeRef entry_ = (NodeValue)rules("entry", entry);

				get<PrimitiveDictionary>(value.value).emplace(
					*any_cast<optional<Primitive>>(entry_->get<any>("key")),
					*any_cast<optional<Primitive>>(entry_->get<any>("value"))
				);
			}

		//	return getValueWrapper(value, "Dictionary");
			return value;
		} else
		if(type == "entry") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);

			return Node {
				{"key", rules(n->get("type"), n->get<NodeRef>("key"))},
				{"value", rules()},
			};
		} else
		if(type == "floatLiteral") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = Primitive(n->get<double>("value"));

		//	return getValueWrapper(value, "Float");
			return value;
		} else
		if(type == "integerLiteral") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = Primitive(n->get<int>("value"));

		//	return getValueWrapper(value, "Integer");
			return value;
		} else
		if(type == "module") {
			addControlTransfer();
			executeNodes(tree ? tree->get<NodeArrayRef>("statements") : nullptr);

			if(threw()) {
				report(2, nullptr, getValueString(controlTransfer().value));
			}

			removeControlTransfer();
		} else
		if(type == "nilLiteral") {
		} else
		if(type == "returnStatement") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = executeNode(n->get("value"));

			if(threw()) {
				return nullptr;
			}

			setControlTransfer(value, "return");
			report(0, n, getValueString(value));

			return value;
		} else
		if(type == "stringLiteral") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			::string string = "";

			for(const NodeRef& segment : n->get<NodeArray&>("segments")) {
				if(segment->get("type") == "stringSegment") {
					string += segment->get<::string>("value");
				} else {  // stringExpression
					string += getValueString(executeNode(segment->get("value")));
				}
			}

			auto value = Primitive(string);

		//	return getValueWrapper(value, "String");
			return value;
		} else
		if(type == "throwStatement") {
			NodeRef n = any_cast<NodeRef>(arguments[0]);
			auto value = executeNode(n->get("value"));

			setControlTransfer(value, "throw");

			return value;
		}

		return nullptr;
	}

	ControlTransfer& controlTransfer() {
		static ControlTransfer dummy;

		return !controlTransfers.empty()
			 ? controlTransfers.back()
			 : dummy = ControlTransfer();
	}

	bool threw() {
		return controlTransfer().type == "throw";
	}

	void addControlTransfer() {
		controlTransfers.push_back(ControlTransfer());

	//	print("act");
	}

	void removeControlTransfer() {
		controlTransfers.pop_back();

	//	print("rct: "/*+JSON.stringify(controlTransfer?.value)+", "+JSON.stringify(controlTransfer?.type)*/);
	}

	/*
	 * Should be used for a return values before releasing its scopes,
	 * as ARC utilizes a control trasfer value at the moment.
	 *
	 * Specifying a type means explicit control transfer.
	 */
	void setControlTransfer(optional<Primitive> value = nullopt, optional<string> type = nullopt) {
		ControlTransfer& CT = controlTransfer();

		CT.value = value;
		CT.type = type;

	//	print("cts: "/*+JSON.stringify(value)+", "+JSON.stringify(type)*/);
	}

	void resetControlTransfer() {
		ControlTransfer& CT = controlTransfer();

		CT.value = nullopt;
		CT.type = nullopt;

	//	print("ctr");
	}

	template<typename... Args>
	optional<Primitive> executeNode(NodeRef node, Args... arguments) {
		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		variant<optional<Primitive>, NodeValue> value;

		position = NP;
		value = rules(node ? node->get("type") : "", node, arguments...);
		position = OP;

		return holds_alternative<optional<Primitive>>(value) ?
							 get<optional<Primitive>>(value) : nullopt;
	}

	/*
	 * Last statement in a body will be treated like a local return (overwritable by subsequent
	 * outer statements) if there was no explicit control transfer previously. ECT should
	 * be implemented by rules.
	 *
	 * Control transfer result can be discarded (fully - 1, value only - 0).
	 */
	void executeNodes(NodeArrayRef nodes) {
		int OP = position;  // Old position

		for(const NodeRef& node : nodes ? *nodes : NodeArray()) {
			int NP = node->get<Node&>("range").get("start");  // New position
			optional<Primitive> value = executeNode(node);

			if(node == nodes->back().get<NodeRef>() && controlTransfer().type == nullopt) {  // Implicit control transfer
				setControlTransfer(value);
			}

			position = NP;  // Consider deinitializers

			if(controlTransfer().type != nullopt) {  // Explicit control transfer is done
				break;
			}
		}

		position = OP;
	}

	string getValueString(optional<Primitive> primitive) {
		if(primitive == nullopt) {
			return "nil";
		}

		if(set<string> {"boolean", "float", "integer"}.contains(primitive->type())) {
			return *primitive;
		}
		if(primitive->type() == "string") {
			return "\""+(string)(*primitive)+"\"";
		}

		/*
		CompositeRef composite = getValueComposite(value);

		if(composite != nullptr) {
			return getCompositeString(composite);
		}
		*/

		if(primitive->type() == "dictionary") {
			string result = "{";
			PrimitiveDictionary dictionary = *primitive;
			auto it = dictionary.begin();

			while(it != dictionary.end()) {
				auto& [k, v] = *it;

				result += getValueString(k)+": "+getValueString(v);

				if(next(it) != dictionary.end()) {
					result += ", ";
				}

				it++;
			}

			result += "}";

			return result;
		}

		return "";
	}

	void report(int level, NodeRef node, const string& string) {
		Location location = position < tokens.size()
						  ? tokens[position].location
						  : Location();

		reports.push_back(Report {
			level,
			position,
			location,
			(node != nullptr ? node->get<::string>("type")+" -> " : "")+string
		});
	}

	void print(const string& string) {
		reports.push_back(Report {
			0,
			0,
			Location(),
			string
		});
	}

	void reset(optional<Preferences> preferences_ = nullopt) {
		tokens = {};
		tree = nullptr;
		position = 0;
		controlTransfers = {};
		preferences = preferences_.value_or(Preferences {
			128,
			2,
			3,
			true
		});
		reports = {};
	}

	struct Result {
		deque<Report> reports;
	};

	Result interpret(Lexer::Result lexerResult, Parser::Result parserResult, optional<Preferences> preferences_ = nullopt) {
		Result result;

		reset(preferences_);

		tokens = lexerResult.tokens;
		tree = parserResult.tree;

		rules("module");

		result.reports = move(reports);

		reset();

		return result;
	}
};