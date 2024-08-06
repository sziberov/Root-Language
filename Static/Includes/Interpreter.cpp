#pragma once

#include <unordered_set>

#include "Lexer.cpp"
#include "Parser.cpp"

using Report = Parser::Report;

class Interpreter {
public:
	struct Context {};
	struct Call {};
	struct Scope {};
	struct ControlTransfer {};

	struct Value {
		NodeValue primitiveValue;
		string primitiveType;
	};

	struct Composite {
		struct IDs {
			int own;
			optional<int> super,
						  Super,
						  self,
						  Self,
						  sub,
						  Sub,
						  scope;
			unordered_set<int> retainers;
		};

		struct Observers {
			optional<int> willGet,
						  get,
						  didGet,
						  willSet,
						  set,
						  didSet;
		};

		struct OperatorOverload {
			struct {
				bool prefix,
					 infix,
					 postfix;
			} modifiers;
			string associativity,
				   pecedence;
		};

		struct MemberOverload {
			struct {
				bool private_,
					 protected_,
					 public_,
					 static_,
					 final;
			} modifiers;
			NodeRef type;
			Value value;
			Observers observers;
		};

		using Operator = vector<OperatorOverload>;
		using Member = vector<MemberOverload>;

		string title;
		IDs IDs;
		enum {
			creation,
			idle,
			destruction
		} life;
		NodeRef type;
		vector<NodeRef> statements;
		unordered_map<string, int> imports;
		unordered_map<string, Operator> operators;
		unordered_map<string, Member> members;
		Observers observers;
	};

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
			//	return make_shared(context.get<Node>("flags"));

				for(const auto& [k, v] : context.get<Node&>("flags")) {
					flags.get(k) = v;
				}
			}
		}

		return make_shared(flags);
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

	NodeRef getComposite(int ID) {
		return ID < composites.size() ? (NodeRef)composites[ID] : nullptr;
	}

	NodeRef createComposite(const string& title, NodeArrayRef type, NodeRef scope) {
		if(!typeIsComposite(type)) {
			return nullptr;
		}

		NodeRef composite = make_shared(Node {
			{"title", title},
			{"IDs", {
				{"own", (int)composites.size()},
				{"super", nullptr},
				{"Super", nullptr},
				{"self", nullptr},
				{"Self", nullptr},
				{"sub", nullptr},
				{"Sub", nullptr},
				{"scope", nullptr},
				{"retainers", NodeArray {}},
			}},
			{"life", 1},  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
			{"type", type},
			{"statements", NodeArray {}},
			{"imports", Node {}},
			{"operators", Node {}},
			{"members", Node {}},
			{"observers", Node {}}
		});

		composites.push_back(composite);

		if(scope != nullptr) {
			setScopeID(composite, scope);
		}

	//	print("cr: "+composite->get<string>("title")+", "+getOwnID(composite));

		return composite;
	}

	void destroyReleasedComposite(NodeRef composite) {
		if(composite != nullptr && !compositeRetained(composite)) {
			destroyComposite(composite);
		}
	}

	void retainComposite(NodeRef retainingComposite, NodeRef retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		NodeArrayRef retainersIDs = getRetainersIDs(retainedComposite);
		int retainingID = getOwnID(retainingComposite);

		if(!contains(*retainersIDs, retainingID)) {
			retainersIDs->push_back(retainingID);
		}
	}

	void releaseComposite(NodeRef retainingComposite, NodeRef retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		NodeArrayRef retainersIDs = getRetainersIDs(retainedComposite);
		int retainingID = getOwnID(retainingComposite);

		if(contains(*retainersIDs, retainingID)) {
//			print("rl: "+getOwnID(retainingComposite)+", "+getOwnID(retainedComposite)+", "+getRetainersIDs(retainedComposite));

			erase(*retainersIDs, retainingID);

			destroyReleasedComposite(retainedComposite);
		}
	}

	/*
	 * Automatically retains or releases composite.
	 *
	 * Use of this function is highly recommended when real retainment state is unknown
	 * at the moment, otherwise basic functions can be used in favour of performance.
	 */
	void retainOrReleaseComposite(NodeRef retainingComposite, NodeRef retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		if(compositeRetains(retainingComposite, retainedComposite)) {
			retainComposite(retainingComposite, retainedComposite);
		} else {
			releaseComposite(retainingComposite, retainedComposite);
		}
	}

	/*
	 * Returns a real state of direct retainment.
	 *
	 * Composite considered really retained if it is used by an at least one of
	 * the retainer's IDs (excluding own and retainers list), type, import, member or observer.
	 */
	bool compositeRetains(NodeRef retainingComposite, NodeRef retainedComposite) {
		if(retainingComposite && retainingComposite->get<int>("life") < 2) return (
			IDsRetain(retainingComposite, retainedComposite) ||
			typeRetains(retainingComposite->get<NodeArrayRef>("type"), retainedComposite) ||
			importsRetain(retainingComposite, retainedComposite) ||
			membersRetain(retainingComposite, retainedComposite) ||
			observersRetain(retainingComposite->get<NodeRef>("observers"), retainedComposite)
		);
	}

	/*
	 * Returns a formal state of direct or indirect retainment.
	 *
	 * Composite considered formally retained if it or at least one of
	 * its retainers list members (recursively) can be as recognized as the retainer.
	 *
	 * retainersIDs is used to exclude a retain cycles and redundant passes
	 * from the lookup and meant to be set internally only.
	 */
	bool compositeRetainsDistant(NodeRef retainingComposite, NodeRef retainedComposite, NodeArrayRef retainersIDs = make_shared(NodeArray())) {
		/*
		if(Array.isArray(retainingComposite)) {
			for(let retainingComposite_ of retainingComposite) {
				if(this.compositeRetainsDistant(retainingComposite_, retainedComposite)) {
					return true;
				}
			}

			return false;
		}
		*/

		if(retainedComposite == nullptr || retainingComposite == nullptr) {
			return false;
		}
		if(retainedComposite == retainingComposite) {
			return true;
		}

		for(int retainerID : getRetainersIDs(retainedComposite)) {
			if(contains(*retainersIDs, retainerID)) {
				continue;
			}

			retainersIDs->push_back(retainerID);

			if(compositeRetainsDistant(retainingComposite, getComposite(retainerID), retainersIDs)) {
				return true;
			}
		}
	}

	/*
	 * Returns a significant state of general retainment.
	 *
	 * Composite considered significantly retained if it is formally retained by
	 * the global namespace, a current scope composite or a current control trasfer value.
	 */
	bool compositeRetained(NodeRef composite) {
		return (
			compositeRetainsDistant(getComposite(0), composite) ||
			compositeRetainsDistant(scope(), composite) ||
		//	compositeRetainsDistant(scopes, composite) ||  // May be useful when "with" syntax construct will be added
			compositeRetainsDistant(getValueComposite(controlTransfer() ? controlTransfer()->get("value") : NodeValue()), composite)
		);
	}

	NodeRef createClass(const string& title, NodeRef scope) {
		NodeRef class_ = createComposite(title, make_shared(NodeArray {{ "predefined", "Class" }}), scope);

		setSelfID(class_, class_);

		return class_;
	}

	NodeRef createEnumeration(const string& title, NodeRef scope) {
		NodeRef enumeration = createComposite(title, make_shared(NodeArray {{ "predefined", "Enumeration" }}), scope);

		setSelfID(enumeration, enumeration);

		return enumeration;
	}

	NodeRef createFunction(const string& title, NodeRef statements, NodeRef scope) {
		NodeRef function = createComposite(title, make_shared(NodeArray {{ "predefined", "Function" }}), scope);

		setInheritedLevelIDs(function, scope);
		setStatements(function, statements);

		return function;
	}

	/*
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels are intentionally missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	NodeRef createNamespace(const string& title, NodeRef scope, optional<NodeRef> levels = nullptr) {
		NodeRef namespace_ = createComposite(title, make_shared(NodeArray {{ "predefined", "Namespace" }}), scope);

		if(levels != nullopt && *levels != nullptr) {
			setInheritedLevelIDs(namespace_, *levels);
		} else
		if(levels == nullopt) {
			setMissedLevelIDs(namespace_);
		} else {
			setSelfID(namespace_, namespace_);
		}

		return namespace_;
	}

	NodeRef createObject(NodeRef superObject, NodeRef selfComposite, NodeRef subObject) {
		string title = "Object<"+selfComposite->get("title", "#"+getOwnID(selfComposite))+">";
		NodeArray type = {Node {{"predefined", "Object"}, {"inheritedTypes", true}}, Node {{"super", 0}, {"reference", getOwnID(selfComposite)}}};
		NodeRef object = createComposite(title, make_shared(type), selfComposite);

		object->get("life") = 0;

		if(superObject != nullptr) {
			setSuperID(object, superObject);
		}

		setSelfID(object, object);

		if(subObject != nullptr) {
			setSubID(object, subObject);
		}

		return object;
	}

	NodeRef createProtocol(const string& title, NodeRef scope) {
		NodeRef protocol = createComposite(title, make_shared(NodeArray {{ "predefined", "Protocol" }}), scope);

		setSelfID(protocol, protocol);

		return protocol;
	}

	NodeRef createStructure(const string& title, NodeRef scope) {
		NodeRef structure = createComposite(title, make_shared(NodeArray {{ "predefined", "Structure" }}), scope);

		setSelfID(structure, structure);

		return structure;
	}

	bool compositeIsFunction(NodeRef composite) {
		return typeIsComposite(composite ? composite.get("type") : nullptr, "Function");
	}

	bool compositeIsNamespace(NodeRef composite) {
		return typeIsComposite(composite ? composite.get("type") : nullptr, "Namespace");
	}

	bool compositeIsObject(NodeRef composite) {
		return typeIsComposite(composite ? composite.get("type") : nullptr, "Object");
	}

	bool compositeIsInstantiable(NodeRef composite) {
		return (
			typeIsComposite(composite ? composite.get("type") : nullptr, "Class") ||
			typeIsComposite(composite ? composite.get("type") : nullptr, "Structure")
		);
	}

	bool compositeIsCallable(NodeRef composite) {
		return (
			compositeIsFunction(composite) ||
			compositeIsInstantiable(composite)
		);
	}

	void addCall(NodeRef function_) {
		calls.push_back({
			{"function", function_},
			{"location", tokens.size() > position ? make_any<Location>(tokens[position]->location) : nullptr}
		});
	}

	void removeCall() {
		calls.pop_back();
	}

	string getCallsString() {
		string result = "";

		for(int i = calls.size()-1, j = 0; i >= 0 && j < 8; i--) {
			NodeRef call = calls[i],
					function = call->get("function");
			Location location = any_cast<Location>(call->get("location"));

			if(j > 0) {
				result += "\n";
			}

			result += j+": ";

			NodeRef composite = getComposite(function->get<NodeRef>("IDs")->get("Self"));

			if(!function->empty("title") && composite != nullptr) {
				result += composite.get("title", "#"+getOwnID(composite))+".";
			}

			result += function.get("title", "#"+getOwnID(function));

			if(location != nullptr) {
				result += ":"+(location.line+1)+":"+(location.column+1);
			}

			j++;
		}

		return result;
	}

	NodeRef scope() {
		return getScope();
	}

	NodeRef getScope(int offset = 0) {
		return scopes.size() > scopes.size()-1+offset
			 ? (NodeRef)scopes[scopes.size()-1+offset]
			 : nullptr;
	}

	/*
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	void addScope(NodeRef composite) {
		scopes.push_back(composite);
	//	print("crs: "+composite.title+", "+getOwnID(composite));
	}

	/*
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
	//	print("dss: "+scope.title+", "+getOwnID(scope));
		NodeRef composite = scopes.back();

		scopes.pop_back();

		if(destroy) {
			destroyReleasedComposite(composite);
		}
	//	print("des: "+composite.title+", "+getOwnID(composite));
	}

	NodeRef controlTransfer() {
		return controlTransfers.size() > 0
			 ? (NodeRef)controlTransfers.back()
			 : nullptr;
	}

	bool threw() {
		return controlTransfer() && controlTransfer()->get("type") == "throw";
	}

	void addControlTransfer() {
		controlTransfers.push_back({
			{"value", nullptr},
			{"type", nullptr}
		});

	//	print("act");
	}

	void removeControlTransfer() {
		controlTransfers.pop_back();

	//	print("rct: "+JSON.stringify(controlTransfer?.value)+", "+JSON.stringify(controlTransfer?.type));
	}

	/*
	 * Should be used for a return values before releasing its scopes,
	 * as ARC utilizes a control trasfer value at the moment.
	 *
	 * Specifying a type means explicit control transfer.
	 */
	void setControlTransfer(const NodeValue& value, const string& type) {
		NodeRef CT = controlTransfer();

		if(CT != nullptr) {
			CT->get("value") = value;
			CT->get("type") = type;
		}

	//	print("cts: "+JSON.stringify(value)+", "+JSON.stringify(type));
	}

	void resetControlTransfer() {
		NodeRef CT = controlTransfer();

		if(CT != nullptr) {
			CT->get("value") =
			CT->get("type") = nullptr;
		}

	//	print("ctr");
	}

	template<typename... Args>
	NodeValue executeNode(NodeRef node, Args... arguments) {
		int OP = position,  // Old/new position
			NP = node ? node->get<NodeRef>("range")->get<int>("start") : 0;
		NodeValue value;

		position = NP;
		value = rules(node ? node->get("type") : "", node, arguments...);
		position = OP;

		return value;
	}

	void setCompositeType(NodeRef composite, NodeArrayRef type) {
		if(!typeIsComposite(type)) {
			return;
		}

		composite->get("type") = type;
	}

	NodeValue getOwnID(NodeRef composite, bool nillable = false) {
		return !nillable || composite ? composite->get<NodeRef>("IDs")->get("own") : NodeValue();
	}

	void report(int level, NodeRef node, const string& string) {
		auto location = position < tokens.size()
					  ? tokens[position]->location
					  : make_shared(Location());

		reports.push_back(make_shared(Report {
			level,
			position,
			location,
			(node != nullptr ? node->get<::string>("type")+" -> " : "")+string
		}));
	}

	void print(const string& string) {
		reports.push_back(make_shared(Report {
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