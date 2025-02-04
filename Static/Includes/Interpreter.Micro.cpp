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

static string tolower(string_view string) {
	auto result = ::string(string);

	transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return std::tolower(c); });

	return result;
}

template <typename T>
optional<T> any_optcast(const any& value) {
	if(const optional<T>* v = any_cast<optional<T>>(&value)) {
		return *v;
	} else
	if(const T* v = any_cast<T>(&value)) {
		return optional<T>(*v);
	}

	return nullopt;
}

template <typename T>
shared_ptr<T> any_refcast(const any& value) {
	if(const shared_ptr<T>* v = any_cast<shared_ptr<T>>(&value)) {
		return *v;
	} else
	if(const T* v = any_cast<T>(&value)) {
		return make_shared<T>(*v);
	}

	return nullptr;
}

// ----------------------------------------------------------------

class Interpreter {
public:
	Interpreter() {};

	struct Composite {
		string title;
		Node IDs;
		int life;  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
		Node type;
	};

	using CompositeRef = shared_ptr<Composite>;

	struct ControlTransfer {
		PrimitiveRef value;
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
	deque<CompositeRef> composites;
	deque<CompositeRef> scopes;
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
			throw invalid_argument("The rule type ("+type+") is not in exceptions list for corresponding node to be present but null pointer is passed");
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

				if(value_) {
					value.get<PrimitiveDictionary>().emplace(Ref<Primitive>(i), value_);  // TODO: Copy or link value in accordance to type
				}
			}

			return getValueWrapper(value, "Array");
		} else
		if(type == "booleanLiteral") {
			Primitive value = n->get("value") == "true";

			return getValueWrapper(value, "Boolean");
		} else
		if(type == "breakStatement") {
			PrimitiveRef value;

			if(!n->empty("label")) {
				value = Ref<Primitive>(n->get<Node&>("label").get<string>("value"));
			}

			setControlTransfer(value, "break");

			return value;
		} else
		if(type == "continueStatement") {
			PrimitiveRef value;

			if(!n->empty("label")) {
				value = Ref<Primitive>(n->get<Node&>("label").get<string>("value"));
			}

			setControlTransfer(value, "continue");

			return value;
		} else
		if(type == "dictionaryLiteral") {
			Primitive value = PrimitiveDictionary();

			for(const NodeRef& entry : n->get<NodeArray&>("entries")) {
				auto entry_ = any_optcast<PrimitiveDictionary::Entry>(rules("entry", entry));

				if(entry_) {
					value.get<PrimitiveDictionary>().emplace(entry_->first, entry_->second);  // TODO: Copy or link value in accordance to type
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
		if(type == "ifStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
			CompositeRef namespace_ = createNamespace("Local<"+getTitle(scope())+", If>", scope(), nullopt);

			addScope(namespace_);

			auto conditionValue = executeNode(n->get("condition"));
			bool condition = conditionValue && (conditionValue->type() != "boolean" || conditionValue->get<bool>()),
				 elseif = !n->empty("else") && n->get<Node&>("else").get("type") == "ifStatement";

			if(condition || !elseif) {
				NodeRef branch = n->get(condition ? "then" : "else");

				if(branch && branch->get("type") == "functionBody") {
					executeNodes(branch->get("statements"));
				} else {
					setControlTransfer(executeNode(branch), controlTransfer().type);
				}
			}

			removeScope();

			if(!condition && elseif) {
				rules("ifStatement", n->get("else"));
			}

			return controlTransfer().value;
		} else
		if(type == "integerLiteral") {
			Primitive value = n->get<int>("value");

			return getValueWrapper(value, "Integer");
		} else
		if(type == "module") {
			addControlTransfer();
			addScope(getComposite(0) ?: createNamespace("Global"));
		//	addDefaultMembers(scope());
			executeNodes(tree ? tree->get<NodeArrayRef>("statements") : nullptr/*, (t) => t !== 'throw' ? 0 : -1*/);

			if(threw()) {
				report(2, nullptr, getValueString(controlTransfer().value));
			}

			removeScope();
			removeControlTransfer();
		} else
		if(type == "nilLiteral") {
		} else
		if(type == "parenthesizedExpression") {
			return executeNode(n->get("value"));
		} else
		if(type == "postfixExpression") {
			auto value = executeNode(n->get("value"));

			if(!value) {
				return nullptr;
			}

			if(set<string> {"float", "integer"}.contains(value->type())) {
				if(!n->empty("operator") && n->get<Node&>("operator").get("value") == "++") {
					*value = *value*1+1;

					return Ref(*value-1);
				}
				if(!n->empty("operator") && n->get<Node&>("operator").get("value") == "--") {
					*value = *value*1-1;

					return Ref(*value+1);
				}
			}

			// TODO: Dynamic operators lookup, check for values mutability, observers notification

			return value;
		} else
		if(type == "prefixExpression") {
			auto value = executeNode(n->get("value"));

			if(!value) {
				return nullptr;
			}

			if(!n->empty("operator") && n->get<Node&>("operator").get("value") == "!" && value->type() == "boolean") {
				return Ref(!*value);
			}
			if(set<string> {"float", "integer"}.contains(value->type())) {
				if(!n->empty("operator") && n->get<Node&>("operator").get("value") == "-") {
					return Ref(-*value);
				}
				if(!n->empty("operator") && n->get<Node&>("operator").get("value") == "++") {
					*value = *value*1+1;

					return Ref(*value);
				}
				if(!n->empty("operator") && n->get<Node&>("operator").get("value") == "--") {
					*value = *value*1-1;

					return Ref(*value);
				}
			}

			// TODO: Dynamic operators lookup, check for values mutability, observers notification

			return value;
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

			Primitive value = string;

			return getValueWrapper(value, "String");
		} else
		if(type == "throwStatement") {
			auto value = executeNode(n->get("value"));

			setControlTransfer(value, "throw");

			return value;
		} else
		if(type == "whileStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
			CompositeRef namespace_ = createNamespace("Local<"+getTitle(scope())+", While>", scope(), nullopt);

			addScope(namespace_);

			while(true) {
			//	removeMembers(namespace);

				auto conditionValue = executeNode(n->get("condition"));
				bool condition = conditionValue && (conditionValue->type() != "boolean" || conditionValue->get<bool>());

				if(!condition) {
					break;
				}

				if(!n->empty("value") && n->get<Node&>("value").get("type") == "functionBody") {
					executeNodes(n->get<Node&>("value").get("statements"));
				} else {
					setControlTransfer(executeNode(n->get("value")), controlTransfer().type);
				}

				string CTT = controlTransfer().type.value_or("");

				if(set<string> {"break", "continue"}.contains(CTT)) {
					resetControlTransfer();
				}
				if(set<string> {"break", "return"}.contains(CTT)) {
					break;
				}
			}

			removeScope();

			return controlTransfer().value;
		}

		return nullptr;
	}

	CompositeRef getComposite(NodeValue ID) {
		return ID.type() == 2 && (int)ID < composites.size() ? composites[(int)ID] : nullptr;
	}

	CompositeRef createComposite(const string& title/*, const Node& type*/, CompositeRef scope = nullptr) {
		cout << "createComposite("+title+")" << endl;
	//	if(!typeIsComposite(type)) {
	//		return nullptr;
	//	}

		auto composite = Ref<Composite>(
			title,
			Node {
				{"own", (int)composites.size()},
				{"scope", nullptr},
				{"retainers", NodeArray {}}
			},
			1//,
		//	type
		);

		composites.push_back(composite);

		if(scope) {
			setScopeID(composite, scope);
		}

		return composite;
	}

	void destroyComposite(CompositeRef composite) {
		cout << "destroyComposite("+getTitle(composite)+")" << endl;
		if(composite->life == 2) {
			return;
		}

		composite->life = 2;

		int ID = getOwnID(composite);
		NodeArrayRef retainersIDs = getRetainersIDs(composite);

		for(const CompositeRef& composite_ : composites) {
		//	if(getRetainersIDs(composite_) && contains(*getRetainersIDs(composite_), ID)) {
				releaseComposite(composite, composite_);

				/*  Let the objects destroy no matter of errors
				if(threw()) {
					return;
				}
				*/
		//	}
		}

		composites[ID] = nullptr;

		int aliveRetainers = 0;

		for(int retainerID : *retainersIDs) {
			CompositeRef composite_ = getComposite(retainerID);

			if(composite_ && composite_->life < 2) {
				aliveRetainers++;

				// TODO: Notify retainers about destroy
			}
		}

		if(aliveRetainers > 0) {
			report(1, nullptr, "Composite #"+(string)getOwnID(composite)+" was destroyed with a non-empty retainer list.");
		}
	}

	void destroyReleasedComposite(CompositeRef composite) {
		if(composite && !compositeRetained(composite)) {
			destroyComposite(composite);
		}
	}

	void retainComposite(CompositeRef retainingComposite, CompositeRef retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		NodeArrayRef retainersIDs = getRetainersIDs(retainedComposite);
		int retainingID = getOwnID(retainingComposite);

		if(!contains(*retainersIDs, retainingID)) {
			retainersIDs->push_back(retainingID);
		}
	}

	void releaseComposite(CompositeRef retainingComposite, CompositeRef retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		NodeArrayRef retainersIDs = getRetainersIDs(retainedComposite);
		int retainingID = getOwnID(retainingComposite);

		if(contains(*retainersIDs, retainingID)) {
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
	void retainOrReleaseComposite(CompositeRef retainingComposite, CompositeRef retainedComposite) {
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
	bool compositeRetains(CompositeRef retainingComposite, CompositeRef retainedComposite) {
		return retainingComposite && retainingComposite->life < 2 && (
			IDsRetain(retainingComposite, retainedComposite) /*||
			typeRetains(retainingComposite->type, retainedComposite) ||
			importsRetain(retainingComposite, retainedComposite) ||
			membersRetain(retainingComposite, retainedComposite) ||
			observersRetain(retainingComposite->observers, retainedComposite)*/
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
	bool compositeRetainsDistant(CompositeRef retainingComposite, CompositeRef retainedComposite, shared_ptr<vector<int>> retainersIDs = Ref<vector<int>>()) {
		/*
		if(Array.isArray(retainingComposite)) {
			for(const CompositeRef& retainingComposite_ : retainingComposite) {
				if(compositeRetainsDistant(retainingComposite_, retainedComposite)) {
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

		for(int retainerID : *getRetainersIDs(retainedComposite)) {
			if(contains(*retainersIDs, retainerID)) {
				continue;
			}

			retainersIDs->push_back(retainerID);

			if(compositeRetainsDistant(retainingComposite, getComposite(retainerID), retainersIDs)) {
				return true;
			}
		}

		return false;
	}

	/*
	 * Returns a significant state of general retainment.
	 *
	 * Composite considered significantly retained if it is formally retained by
	 * the global namespace, a current scope composite or a current control trasfer value.
	 */
	bool compositeRetained(CompositeRef composite) {
		return (
			compositeRetainsDistant(getComposite(0), composite) ||
			compositeRetainsDistant(scope(), composite) ||
		//	compositeRetainsDistant(scopes, composite) ||  // May be useful when "with" syntax construct will be added
			compositeRetainsDistant(getValueComposite(controlTransfer().value), composite)
		);
	}

	/*
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels are intentionally missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	CompositeRef createNamespace(const string& title, CompositeRef scope = nullptr, optional<CompositeRef> levels = nullptr) {
		CompositeRef namespace_ = createComposite(title/*, make_shared(NodeArray {{ "predefined", "Namespace" }})*/, scope);

		if(levels && *levels) {
			setInheritedLevelIDs(namespace_, *levels);
		} else
		if(!levels) {
			setMissedLevelIDs(namespace_);
		} else {
			setSelfID(namespace_, namespace_);
		}

		return namespace_;
	}

	CompositeRef scope() {
		return getScope();
	}

	CompositeRef getScope(int offset = 0) {
		return scopes.size() > scopes.size()-1+offset
			 ? scopes[scopes.size()-1+offset]
			 : nullptr;
	}

	/*
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	void addScope(CompositeRef composite) {
		scopes.push_back(composite);
	}

	/*
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
		CompositeRef composite = scopes.back();

		scopes.pop_back();

		if(destroy) {
			destroyReleasedComposite(composite);
		}
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
	}

	void removeControlTransfer() {
		controlTransfers.pop_back();
	}

	/*
	 * Should be used for a return values before releasing its scopes,
	 * as ARC utilizes a control trasfer value at the moment.
	 *
	 * Specifying a type means explicit control transfer.
	 */
	void setControlTransfer(PrimitiveRef value = nullptr, optional<string> type = nullopt) {
		ControlTransfer& CT = controlTransfer();

		CT.value = value;
		CT.type = type;
	}

	void resetControlTransfer() {
		ControlTransfer& CT = controlTransfer();

		CT.value = nullptr;
		CT.type = nullopt;
	}

	template<typename... Args>
	PrimitiveRef executeNode(NodeRef node, Args... arguments) {
		if(!node) {
			return nullptr;
		}

		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		PrimitiveRef value;

		position = NP;
		value = any_refcast<Primitive>(rules(node->get("type"), node, arguments...));
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
			PrimitiveRef value = executeNode(node);

			if(node == nodes->back().get<NodeRef>() && !controlTransfer().type) {  // Implicit control transfer
				setControlTransfer(value);
			}

			position = NP;  // Consider deinitializers

			if(controlTransfer().type) {  // Explicit control transfer is done
				break;
			}
		}

		position = OP;
	}

	string getTitle(CompositeRef composite) {
		return composite && composite->title.length() ? composite->title : "#"+(string)getOwnID(composite);
	}

	void setID(CompositeRef composite, const string& key, const NodeValue& value) {
		if(set<string> {"own", "retainers"}.contains(key)) {
			return;
		}

		NodeValue OV, NV;  // Old/new value

		OV = composite->IDs.get(key);
		NV = composite->IDs.get(key) = value;

		if(OV != NV) {
			if(tolower(key) != "self" && NV != -1) {  // ID chains should not be cyclic, intentionally missed IDs can't create cycles
				set<CompositeRef> composites;
				CompositeRef composite_ = composite;

				while(composite_) {
					if(composites.contains(composite_)) {
						composite->IDs.get(key) = OV;

						return;
					}

					composites.insert(composite_);

					composite_ = getComposite(composite_->IDs.get(key));
				}
			}

			retainOrReleaseComposite(composite, getComposite(OV));
			retainComposite(composite, getComposite(NV));
		}
	}

	NodeValue getOwnID(CompositeRef composite, bool nillable = false) {
		return composite ? composite->IDs.get("own") :
			   nillable ? NodeValue() :
			   throw invalid_argument("Composite reference shouldn't be nil");
	}

	NodeArrayRef getRetainersIDs(CompositeRef composite) {
		return composite ? composite->IDs.get<NodeArrayRef>("retainers") : nullptr;
	}

	void setSelfID(CompositeRef composite, CompositeRef selfComposite) {
		setID(composite, "self", getOwnID(selfComposite));
	//	setID(composite, "Self", getSelfCompositeID(selfComposite));
	}

	void setInheritedLevelIDs(CompositeRef inheritingComposite, CompositeRef inheritedComposite) {
		if(inheritedComposite == inheritingComposite) {
			return;
		}

		static set<string> excluded = {"own", "scope", "retainers"};

		for(const auto& [key, value] : inheritingComposite->IDs) {
			if(!excluded.contains(key)) {
				setID(inheritingComposite, key, inheritedComposite ? inheritedComposite->IDs.get(key) : NodeValue());
			}
		}
	}

	void setMissedLevelIDs(CompositeRef missingComposite) {
		static set<string> excluded = {"own", "scope", "retainers"};

		for(const auto& [key, value] : missingComposite->IDs) {
			if(!excluded.contains(key)) {
				setID(missingComposite, key, -1);
			}
		}
	}

	void setScopeID(CompositeRef composite, CompositeRef scopeComposite) {
		setID(composite, "scope", getOwnID(scopeComposite, true));
	}

	bool IDsRetain(CompositeRef retainingComposite, CompositeRef retainedComposite) {
		static set<string> excluded = {"own", "retainers"};

		for(const auto& [key, value] : retainingComposite->IDs) {
			if(!excluded.contains(key) && retainingComposite->IDs.get(key) == getOwnID(retainedComposite)) {
				return true;
			}
		}

		return false;
	}

	string getValueString(PrimitiveRef primitive, bool explicitStrings = false) {
		if(!primitive) {
			return "nil";
		}

		if(set<string> {"boolean", "float", "integer"}.contains(primitive->type())) {
			return *primitive;
		}
		if(primitive->type() == "string") {
			return explicitStrings ? "'"+(string)(*primitive)+"'" : (string)(*primitive);
		}

		CompositeRef composite = getValueComposite(primitive);

		if(composite) {
		//	return getCompositeString(composite);
		}

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

	CompositeRef getValueComposite(PrimitiveRef value) {
		if(!value || !set<string> {"pointer", "reference"}.contains(value->type())) {
			return nullptr;
		}

		return getComposite((int)*value);
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