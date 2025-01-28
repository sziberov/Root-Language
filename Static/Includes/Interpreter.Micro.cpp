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

template <typename T>
optional<T> any_optcast(const any& value) {
	if(const optional<T>* opt = any_cast<optional<T>>(&value)) {
		return *opt;
	} else
	if(const T* v = any_cast<T>(&value)) {
		return optional<T>(*v);
	}

	return nullopt;
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
	any rules(const string& type, NodeRef n = nullptr, Args... args) {
		vector<any> arguments = {args...};

		if(
			type != "module" &&
			type != "nilLiteral" &&
			!n
		) {
			throw invalid_argument("The rule type is not in exceptions list for corresponding node to be present but null pointer is passed: "+type);
		}

		if(type == "argument") {
			auto value = executeNode(n->get("value"));

			if(!threw()) {
				return make_pair(
					!n->empty("label") ? n->get<Node&>("label").get<string>("value") : "",
					value
				);
			}
		} else
		if(type == "arrayLiteral") {
			Primitive value = PrimitiveDictionary();

			for(int i = 0; i < n->get<NodeArray&>("values").size(); i++) {
				auto value_ = executeNode(n->get<NodeArray&>("values")[i]);

				if(value_ != nullopt) {
					value.get<PrimitiveDictionary>().emplace(i, *value_);
				}
			}

			return getValueWrapper(value, "Array");
		} else
		if(type == "booleanLiteral") {
			Primitive value = n->get("value") == "true";

			return getValueWrapper(value, "Boolean");
		} else
		if(type == "breakStatement") {
			optional<Primitive> value;

			if(!n->empty("label")) {
				value = n->get<Node&>("label").get<string>("value");
			}

			setControlTransfer(value, "break");

			return value;
		} else
		if(type == "continueStatement") {
			optional<Primitive> value;

			if(!n->empty("label")) {
				value = n->get<Node&>("label").get<string>("value");
			}

			setControlTransfer(value, "continue");

			return value;
		} else
		if(type == "dictionaryLiteral") {
			Primitive value = PrimitiveDictionary();

			for(const NodeRef& entry : n->get<NodeArray&>("entries")) {
				auto entry_ = any_optcast<PrimitiveDictionary::Entry>(rules("entry", entry));

				if(entry_ != nullopt) {
					value.get<PrimitiveDictionary>().emplace(entry_->first, entry_->second);
				}
			}

			return getValueWrapper(value, "Dictionary");
		} else
		if(type == "entry") {
			return make_pair(
				executeNode(n->get("key")),
				executeNode(n->get("value"))
			);
		} else
		if(type == "floatLiteral") {
			Primitive value = n->get<double>("value");

			return getValueWrapper(value, "Float");
		} else
		if(type == "integerLiteral") {
			Primitive value = n->get<int>("value");

			return getValueWrapper(value, "Integer");
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
			auto value = executeNode(n->get("value"));

			if(threw()) {
				return nullptr;
			}

			setControlTransfer(value, "return");
			report(0, n, getValueString(value));

			return value;
		} else
		if(type == "stringLiteral") {
			::string string = "";

			for(const NodeRef& segment : n->get<NodeArray&>("segments")) {
				if(segment->get("type") == "stringSegment") {
					string += segment->get<::string>("value");
				} else {  // stringExpression
					string += getValueString(executeNode(segment->get("value")));
				}
			}

			auto value = Primitive(string);

			return getValueWrapper(value, "String");
		} else
		if(type == "throwStatement") {
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
		if(!node) {
			return nullopt;
		}

		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		optional<Primitive> value;

		position = NP;
		value = any_optcast<Primitive>(rules(node->get("type"), node, arguments...));
		position = OP;

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

	string getValueString(optional<Primitive> primitive, bool explicitStrings = false) {
		if(primitive == nullopt) {
			return "nil";
		}

		if(set<string> {"boolean", "float", "integer"}.contains(primitive->type())) {
			return *primitive;
		}
		if(primitive->type() == "string") {
			return explicitStrings ? "'"+(string)(*primitive)+"'" : (string)(*primitive);
		}

		/*
		CompositeRef composite = getValueComposite(value);

		if(composite != nullptr) {
			return getCompositeString(composite);
		}
		*/

		if(primitive->type() == "dictionary") {
			string result = "[";
			PrimitiveDictionary dictionary = *primitive;
			auto it = dictionary.begin();

			while(it != dictionary.end()) {
				auto& [k, v] = *it;

				result += getValueString(k, true)+": "+getValueString(v, true);

				if(next(it) != dictionary.end()) {
					result += ", ";
				}

				it++;
			}

			result += "]";

			return result;
		}

		return "";
	}

	/*
	 * Returns result of identifier(value) call or plain value if no appropriate function found in current scope.
	 */
	Primitive getValueWrapper(const Primitive& value, string identifier) {
		// TODO

		return value;
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