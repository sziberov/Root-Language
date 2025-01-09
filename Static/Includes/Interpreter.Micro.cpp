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

	struct Value {
		string primitiveType;
		NodeValue primitiveValue;
	};

	struct ControlTransfer {
		optional<Value> value;
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
	optional<Value> rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "module") {
			cout << "Terminal Hello World" << endl;
			report(0, nullptr /*any_cast<NodeRef>(arguments[0])*/, "Console Hello World");

			addControlTransfer();
			executeNodes(tree ? tree->get<NodeArrayRef>("statements") : nullptr);

			if(threw()) {
				report(2, nullptr, getValueString(controlTransfer().value));
			}

			removeControlTransfer();
		}

		return nullopt;
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
	void setControlTransfer(optional<Value> value = nullopt, optional<string> type = nullopt) {
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
	optional<Value> executeNode(NodeRef node, Args... arguments) {
		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		optional<Value> value;

		position = NP;
		value = rules(node ? node->get("type") : "", node, arguments...);
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
			optional<Value> value = executeNode(node);

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

	Value createValue(const string& primitiveType, const NodeValue& primitiveValue) {
		return {
			primitiveType,	// 'boolean', 'dictionary', 'float', 'integer', 'pointer', 'reference', 'string', 'type'
			primitiveValue	// boolean, float, integer, map (object), string, type
		};
	}

	string getValueString(optional<Value> value) {
		if(value == nullopt) {
			return "nil";
		}

		if(set<string> {"boolean", "float", "integer", "string"}.contains(value->primitiveType)) {
			return value->primitiveValue;
		}

		return "";  // TODO
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
		NodeValue value;
		deque<Report> reports;
	};

	Result interpret(Lexer::Result lexerResult, Parser::Result parserResult, optional<Preferences> preferences_ = nullopt) {
		Result result;

		reset(preferences_);

		tokens = lexerResult.tokens;
		tree = parserResult.tree;

		result.value = rules("module");
		result.reports = move(reports);

		reset();

		return result;
	}
};