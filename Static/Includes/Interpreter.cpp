#pragma once

#include <unordered_set>

#include "Lexer.cpp"
#include "Parser.cpp"

using Report = Parser::Report;

class Interpreter {
public:
	struct Context {
		Node flags;
		unordered_set<string> tags;
	};

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
		int life;  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
		NodeArrayRef type;
		vector<NodeRef> statements;
		unordered_map<string, int> imports;
		unordered_map<string, Operator> operators;
		unordered_map<string, Member> members;
		Observers observers;
	};

	struct Call {
		shared_ptr<Composite> function;
		shared_ptr<Location> location;
	};

	struct ControlTransfer {
		optional<Value> value;
		optional<string> type;
	};

	Interpreter() {};

	deque<Token> tokens;
	NodeRef tree;
	int position;
	deque<Context> contexts;
	deque<shared_ptr<Composite>> composites;
	deque<Call> calls;
	deque<shared_ptr<Composite>> scopes;
	deque<shared_ptr<ControlTransfer>> controlTransfers;
	Node preferences;
	deque<Report> reports;

	template<typename... Args>
	NodeValue rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "module") {
			addControlTransfer();
			addScope(getComposite(0) ?: createNamespace("Global"));
			addDefaultMembers(scope);
			executeNodes(tree?.statements, (t) => t != "throw" ? 0 : -1);

			if(threw()) {
				report(2, undefined, getValueString(controlTransfer.value));
			}

			removeScope();
			removeControlTransfer();
		}

		return nullptr;
	}

	NodeRef getContext(const string& tag) {
		Node flags;

		for(const Context& context : contexts) {
			if(context.tags.empty() || context.tags.contains(tag)) {
				for(const auto& [k, v] : context.flags) {
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
	void addContext(const Node& flags, const unordered_set<string>& tags) {
		contexts.push_back(Context {
			flags,
			tags
		});
	}

	void removeContext() {
		contexts.pop_back();
	}

	shared_ptr<Composite> getComposite(int ID) {
		return ID < composites.size() ? composites[ID] : nullptr;
	}

	shared_ptr<Composite> createComposite(const string& title, NodeArrayRef type, shared_ptr<Composite> scope = nullptr) {
		if(!typeIsComposite(type)) {
			return nullptr;
		}

		shared_ptr<Composite> composite = make_shared(Composite {
			title,
			{(int)composites.size()},
			0,
			type
		});

		composites.push_back(composite);

		if(scope != nullptr) {
			setScopeID(composite, scope);
		}

	//	print("cr: "+composite->get<string>("title")+", "+getOwnID(composite));

		return composite;
	}

	void destroyReleasedComposite(shared_ptr<Composite> composite) {
		if(composite != nullptr && !compositeRetained(composite)) {
			destroyComposite(composite);
		}
	}

	void retainComposite(shared_ptr<Composite> retainingComposite, shared_ptr<Composite> retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		NodeArrayRef retainersIDs = getRetainersIDs(retainedComposite);
		int retainingID = getOwnID(retainingComposite);

		if(!contains(*retainersIDs, retainingID)) {
			retainersIDs->push_back(retainingID);
		}
	}

	void releaseComposite(shared_ptr<Composite> retainingComposite, shared_ptr<Composite> retainedComposite) {
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
	void retainOrReleaseComposite(shared_ptr<Composite> retainingComposite, shared_ptr<Composite> retainedComposite) {
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
	bool compositeRetains(shared_ptr<Composite> retainingComposite, shared_ptr<Composite> retainedComposite) {
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
	bool compositeRetainsDistant(shared_ptr<Composite> retainingComposite, shared_ptr<Composite> retainedComposite, NodeArrayRef retainersIDs = make_shared(NodeArray())) {
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
	bool compositeRetained(shared_ptr<Composite> composite) {
		return (
			compositeRetainsDistant(getComposite(0), composite) ||
			compositeRetainsDistant(scope(), composite) ||
		//	compositeRetainsDistant(scopes, composite) ||  // May be useful when "with" syntax construct will be added
			compositeRetainsDistant(getValueComposite(controlTransfer() ? controlTransfer()->get("value") : NodeValue()), composite)
		);
	}

	shared_ptr<Composite> createClass(const string& title, shared_ptr<Composite> scope) {
		shared_ptr<Composite> class_ = createComposite(title, make_shared(NodeArray {{ "predefined", "Class" }}), scope);

		setSelfID(class_, class_);

		return class_;
	}

	shared_ptr<Composite> createEnumeration(const string& title, shared_ptr<Composite> scope) {
		shared_ptr<Composite> enumeration = createComposite(title, make_shared(NodeArray {{ "predefined", "Enumeration" }}), scope);

		setSelfID(enumeration, enumeration);

		return enumeration;
	}

	shared_ptr<Composite> createFunction(const string& title, NodeRef statements, shared_ptr<Composite> scope) {
		shared_ptr<Composite> function = createComposite(title, make_shared(NodeArray {{ "predefined", "Function" }}), scope);

		setInheritedLevelIDs(function, scope);
		setStatements(function, statements);

		return function;
	}

	/*
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels are intentionally missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	shared_ptr<Composite> createNamespace(const string& title, shared_ptr<Composite> scope = nullptr, optional<shared_ptr<Composite>> levels = nullptr) {
		shared_ptr<Composite> namespace_ = createComposite(title, make_shared(NodeArray {{ "predefined", "Namespace" }}), scope);

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

	shared_ptr<Composite> createObject(shared_ptr<Composite> superObject, shared_ptr<Composite> selfComposite, shared_ptr<Composite> subObject) {
		string title = "Object<"+selfComposite->get("title", "#"+getOwnID(selfComposite))+">";
		NodeArray type = {Node {{"predefined", "Object"}, {"inheritedTypes", true}}, Node {{"super", 0}, {"reference", getOwnID(selfComposite)}}};
		shared_ptr<Composite> object = createComposite(title, make_shared(type), selfComposite);

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

	shared_ptr<Composite> createProtocol(const string& title, shared_ptr<Composite> scope) {
		shared_ptr<Composite> protocol = createComposite(title, make_shared(NodeArray {{ "predefined", "Protocol" }}), scope);

		setSelfID(protocol, protocol);

		return protocol;
	}

	shared_ptr<Composite> createStructure(const string& title, shared_ptr<Composite> scope) {
		shared_ptr<Composite> structure = createComposite(title, make_shared(NodeArray {{ "predefined", "Structure" }}), scope);

		setSelfID(structure, structure);

		return structure;
	}

	bool compositeIsFunction(shared_ptr<Composite> composite) {
		return typeIsComposite(composite ? composite.get("type") : nullptr, "Function");
	}

	bool compositeIsNamespace(shared_ptr<Composite> composite) {
		return typeIsComposite(composite ? composite.get("type") : nullptr, "Namespace");
	}

	bool compositeIsObject(shared_ptr<Composite> composite) {
		return typeIsComposite(composite ? composite.get("type") : nullptr, "Object");
	}

	bool compositeIsInstantiable(shared_ptr<Composite> composite) {
		return (
			typeIsComposite(composite ? composite.get("type") : nullptr, "Class") ||
			typeIsComposite(composite ? composite.get("type") : nullptr, "Structure")
		);
	}

	bool compositeIsCallable(shared_ptr<Composite> composite) {
		return (
			compositeIsFunction(composite) ||
			compositeIsInstantiable(composite)
		);
	}

	void addCall(shared_ptr<Composite> function) {
		calls.push_back({
			function,
			tokens.size() > position ? tokens[position]->location : nullptr
		});
	}

	void removeCall() {
		calls.pop_back();
	}

	string getCallsString() {
		string result = "";

		for(int i = calls.size()-1, j = 0; i >= 0 && j < 8; i--) {
			Call& call = calls[i];
			shared_ptr<Composite> function = call.function;
			shared_ptr<Location> location = call.location;

			if(j > 0) {
				result += "\n";
			}

			result += j+": ";

			shared_ptr<Composite> composite = getComposite(*function->IDs.Self);

			if(!function->title.empty() && composite != nullptr) {
				result += (!composite->title.empty() ? composite->title : "#"+getOwnID(composite))+".";
			}

			result += !function->title.empty() ? function->title : "#"+getOwnID(function);

			if(location != nullptr) {
				result += ":"+(location->line+1);
				result += ":"+(location->column+1);
			}

			j++;
		}

		return result;
	}

	shared_ptr<Composite> scope() {
		return getScope();
	}

	shared_ptr<Composite> getScope(int offset = 0) {
		return scopes.size() > scopes.size()-1+offset
			 ? scopes[scopes.size()-1+offset]
			 : nullptr;
	}

	/*
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	void addScope(shared_ptr<Composite> composite) {
		scopes.push_back(composite);
	//	print("crs: "+composite.title+", "+getOwnID(composite));
	}

	/*
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
	//	print("dss: "+scope.title+", "+getOwnID(scope));
		shared_ptr<Composite> composite = scopes.back();

		scopes.pop_back();

		if(destroy) {
			destroyReleasedComposite(composite);
		}
	//	print("des: "+composite.title+", "+getOwnID(composite));
	}

	shared_ptr<ControlTransfer> controlTransfer() {
		return controlTransfers.size() > 0
			 ? controlTransfers.back()
			 : nullptr;
	}

	bool threw() {
		return controlTransfer() && controlTransfer()->type == "throw";
	}

	void addControlTransfer() {
		controlTransfers.push_back(make_shared(ControlTransfer()));

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
		shared_ptr<ControlTransfer> CT = controlTransfer();

		if(CT != nullptr) {
			CT->value = value;
			CT->type = type;
		}

	//	print("cts: "+JSON.stringify(value)+", "+JSON.stringify(type));
	}

	void resetControlTransfer() {
		shared_ptr<ControlTransfer> CT = controlTransfer();

		if(CT != nullptr) {
			CT->value = nullopt;
			CT->type = nullopt;
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

	void setCompositeType(shared_ptr<Composite> composite, NodeArrayRef type) {
		if(!typeIsComposite(type)) {
			return;
		}

		composite->get("type") = type;
	}

	NodeValue getOwnID(shared_ptr<Composite> composite, bool nillable = false) {
		return !nillable || composite ? composite->IDs.own : NodeValue();
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

	void reset(deque<shared_ptr<Composite>> composites_ = {}, const Node& preferences_ = {}) {
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
		deque<shared_ptr<Composite>> composites;
		deque<Report> reports;
	};

	Result interpret(Lexer::Result lexerResult, Parser::Result parserResult, deque<shared_ptr<Composite>> composites_ = {}, const Node& preferences_ = {}) {
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

	Result interpretRaw(string_view code, deque<shared_ptr<Composite>> composites_ = {}, const Node& preferences_ = {}) {
		Lexer::Result lexerResult = Lexer().tokenize(code);
		Parser::Result parserResult = Parser().parse(lexerResult);
		Result interpreterResult = interpret(lexerResult, parserResult, composites_, preferences_);

		return interpreterResult;
	}
};