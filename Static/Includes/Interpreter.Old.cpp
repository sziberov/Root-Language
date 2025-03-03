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
	struct Context {
		Node flags;
		unordered_set<string> tags;
	};

	struct Composite {
		struct OperatorOverload {
			unordered_set<string> modifiers;
			string associativity,
				   precedence;
		};

		struct MemberOverload {
			unordered_set<string> modifiers;
			NodeRef type;
			NodeRef value;
			Node observers/* = {
				{"willGet", nullptr},
				{"get", nullptr},
				{"didGet", nullptr},
				{"willSet", nullptr},
				{"set", nullptr},
				{"didSet", nullptr}
			}*/;
		};

		using Operator = vector<OperatorOverload>;
		using Member = vector<MemberOverload>;

		string title;
		Node IDs;
		int life;  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
		NodeArrayRef type;
		vector<NodeRef> statements;
		unordered_map<string, int> imports;
		unordered_map<string, Operator> operators;
		unordered_map<string, Member> members;
		Node observers;
	};

	using CompositeRef = shared_ptr<Composite>;

	struct Call {
		CompositeRef function;
		optional<Location> location;
	};

	struct ControlTransfer {
		NodeRef value;
		optional<string> type;
	};

	Interpreter() {};

	deque<Token> tokens;
	NodeRef tree;
	int position;
	deque<Context> contexts;
	deque<CompositeRef> composites;
	deque<Call> calls;
	deque<CompositeRef> scopes;
	deque<ControlTransfer> controlTransfers;
	Node preferences;
	deque<Report> reports;

	template<typename... Args>
	NodeValue rules(const string& type, Args... args) {
		vector<any> arguments = {args...};

		if(type == "module") {
			addControlTransfer();
			addScope(getComposite(0) ?: createNamespace("Global"));
			addDefaultMembers(scope);
			executeNodes(tree ? tree->get<NodeArrayRef>("statements") : nullptr, [](optional<string> t, NodeRef v) { return t != "throw" ? 0 : -1; });

			if(threw()) {
				report(2, nullptr, getValueString(controlTransfer().value));
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
		contexts.push_back({
			flags,
			tags
		});
	}

	void removeContext() {
		contexts.pop_back();
	}

	CompositeRef getComposite(int ID) {
		return ID < composites.size() ? composites[ID] : nullptr;
	}

	CompositeRef createComposite(const string& title, NodeArrayRef type, CompositeRef scope = nullptr) {
		if(!typeIsComposite(type)) {
			return nullptr;
		}

		CompositeRef composite = make_shared(Composite {
			title,
			{
				{"own", (int)composites.size()},
				{"super", nullptr},
				{"Super", nullptr},
				{"self", nullptr},
				{"Self", nullptr},
				{"sub", nullptr},
				{"Sub", nullptr},
				{"scope", nullptr},
				{"retainers", NodeArray {}}
			},
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

	void destroyComposite(CompositeRef composite);

	void destroyReleasedComposite(CompositeRef composite) {
		if(composite != nullptr && !compositeRetained(composite)) {
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
		if(retainingComposite && retainingComposite->life < 2) return (
			IDsRetain(retainingComposite, retainedComposite) ||
			typeRetains(retainingComposite->type, retainedComposite) ||
			importsRetain(retainingComposite, retainedComposite) ||
			membersRetain(retainingComposite, retainedComposite) ||
			observersRetain(retainingComposite->observers, retainedComposite)
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
	bool compositeRetainsDistant(CompositeRef retainingComposite, CompositeRef retainedComposite, NodeArrayRef retainersIDs = make_shared(NodeArray())) {
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

		for(int retainerID : *getRetainersIDs(retainedComposite)) {
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
	bool compositeRetained(CompositeRef composite) {
		return (
			compositeRetainsDistant(getComposite(0), composite) ||
			compositeRetainsDistant(scope(), composite) ||
		//	compositeRetainsDistant(scopes, composite) ||  // May be useful when "with" syntax construct will be added
			compositeRetainsDistant(getValueComposite(controlTransfer().value), composite)
		);
	}

	CompositeRef createClass(const string& title, CompositeRef scope) {
		CompositeRef class_ = createComposite(title, make_shared(NodeArray {{ "predefined", "Class" }}), scope);

		setSelfID(class_, class_);

		return class_;
	}

	CompositeRef createEnumeration(const string& title, CompositeRef scope) {
		CompositeRef enumeration = createComposite(title, make_shared(NodeArray {{ "predefined", "Enumeration" }}), scope);

		setSelfID(enumeration, enumeration);

		return enumeration;
	}

	CompositeRef createFunction(const string& title, NodeRef statements, CompositeRef scope) {
		CompositeRef function = createComposite(title, make_shared(NodeArray {{ "predefined", "Function" }}), scope);

		setInheritedLevelIDs(function, scope);
		setStatements(function, statements);

		return function;
	}

	/*
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels are intentionally missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	CompositeRef createNamespace(const string& title, CompositeRef scope = nullptr, optional<CompositeRef> levels = nullptr) {
		CompositeRef namespace_ = createComposite(title, make_shared(NodeArray {{ "predefined", "Namespace" }}), scope);

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

	CompositeRef createObject(CompositeRef superObject, CompositeRef selfComposite, CompositeRef subObject) {
		string title = "Object<"+(!selfComposite->title.empty() ? selfComposite->title : "#"+getOwnID(selfComposite))+">";
		NodeArray type = {Node {{"predefined", "Object"}, {"inheritedTypes", true}}, Node {{"super", 0}, {"reference", getOwnID(selfComposite)}}};
		CompositeRef object = createComposite(title, make_shared(type), selfComposite);

		object->life = 0;

		if(superObject != nullptr) {
			setSuperID(object, superObject);
		}

		setSelfID(object, object);

		if(subObject != nullptr) {
			setSubID(object, subObject);
		}

		return object;
	}

	CompositeRef createProtocol(const string& title, CompositeRef scope) {
		CompositeRef protocol = createComposite(title, make_shared(NodeArray {{ "predefined", "Protocol" }}), scope);

		setSelfID(protocol, protocol);

		return protocol;
	}

	CompositeRef createStructure(const string& title, CompositeRef scope) {
		CompositeRef structure = createComposite(title, make_shared(NodeArray {{ "predefined", "Structure" }}), scope);

		setSelfID(structure, structure);

		return structure;
	}

	bool compositeIsFunction(CompositeRef composite) {
		return composite && typeIsComposite(composite->type, "Function");
	}

	bool compositeIsNamespace(CompositeRef composite) {
		return composite && typeIsComposite(composite->type, "Namespace");
	}

	bool compositeIsObject(CompositeRef composite) {
		return composite && typeIsComposite(composite->type, "Object");
	}

	bool compositeIsInstantiable(CompositeRef composite) {
		return composite && (
			typeIsComposite(composite->type, "Class") ||
			typeIsComposite(composite->type, "Structure")
		);
	}

	bool compositeIsCallable(CompositeRef composite) {
		return (
			compositeIsFunction(composite) ||
			compositeIsInstantiable(composite)
		);
	}

	void addCall(CompositeRef function) {
		calls.push_back({
			function,
			tokens.size() > position ? make_optional(tokens[position].location) : nullopt
		});
	}

	void removeCall() {
		calls.pop_back();
	}

	string getCallsString() {
		string result = "";

		for(int i = calls.size()-1, j = 0; i >= 0 && j < 8; i--) {
			Call& call = calls[i];
			CompositeRef function = call.function;
			optional<Location> location = call.location;

			if(j > 0) {
				result += "\n";
			}

			result += j+": ";

			CompositeRef composite = getComposite(function->IDs.get("Self"));

			if(!function->title.empty() && composite != nullptr) {
				result += (!composite->title.empty() ? composite->title : "#"+getOwnID(composite))+".";
			}

			result += !function->title.empty() ? function->title : "#"+getOwnID(function);

			if(location != nullopt) {
				result += ":"+(location->line+1);
				result += ":"+(location->column+1);
			}

			j++;
		}

		return result;
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
	//	print("crs: "+composite.title+", "+getOwnID(composite));
	}

	/*
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
	//	print("dss: "+scope.title+", "+getOwnID(scope));
		CompositeRef composite = scopes.back();

		scopes.pop_back();

		if(destroy) {
			destroyReleasedComposite(composite);
		}
	//	print("des: "+composite.title+", "+getOwnID(composite));
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

	//	print("rct: "+JSON.stringify(controlTransfer?.value)+", "+JSON.stringify(controlTransfer?.type));
	}

	/*
	 * Should be used for a return values before releasing its scopes,
	 * as ARC utilizes a control trasfer value at the moment.
	 *
	 * Specifying a type means explicit control transfer.
	 */
	void setControlTransfer(const NodeValue& value, optional<string> type = nullopt) {
		ControlTransfer& CT = controlTransfer();

		CT.value = value;
		CT.type = type;

	//	print("cts: "+JSON.stringify(value)+", "+JSON.stringify(type));
	}

	void resetControlTransfer() {
		ControlTransfer& CT = controlTransfer();

		CT.value = nullptr;
		CT.type = nullopt;

	//	print("ctr");
	}

	template<typename... Args>
	NodeValue executeNode(NodeRef node, Args... arguments) {
		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		NodeValue value;

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
	void executeNodes(NodeArrayRef nodes, optional<function<bool(optional<string>, NodeRef)>> CTDiscarded = nullopt) {
		int OP = position;  // Old position

		for(const NodeRef& node : nodes ? *nodes : NodeArray()) {
			int NP = node->get<Node&>("range").get("start"),  // New position
				start = composites.size();
			NodeValue value = executeNode(node);
			int end = composites.size();

			if(node == nodes->at(-1) && controlTransfer().type == nullopt) {  // Implicit control transfer
				setControlTransfer(value);
			}

			int CTD = CTDiscarded.has_value() ? (*CTDiscarded)(controlTransfer().type, controlTransfer().value) : -1;

			switch(CTD) {
				case 0: setControlTransfer(nullptr, controlTransfer().type); break;
				case 1: resetControlTransfer();								 break;
			}

			position = NP;  // Consider deinitializers

			for(start; start < end; start++) {
				destroyReleasedComposite(getComposite(start));

				if(threw()) {
					break;
				}
			}

			if(controlTransfer().type != nullopt) {  // Explicit control transfer is done
				break;
			}
		}

		position = OP;
	}

	void setCompositeType(CompositeRef composite, NodeArrayRef type) {
		if(!typeIsComposite(type)) {
			return;
		}

		composite->type = type;
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

				while(composite_ != nullptr) {
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

	optional<int> getOwnID(CompositeRef composite, bool nillable = false) {
		if(!nillable || composite) return composite->IDs.get("own");
	}

	NodeValue getSelfCompositeID(CompositeRef composite) {
		return !compositeIsObject(composite) ? getOwnID(composite) : getTypeInheritedID(*composite->type);
	}

	NodeValue getScopeID(CompositeRef composite) {
		return composite->IDs.get("scope");
	}

	NodeArrayRef getRetainersIDs(CompositeRef composite) {
		return composite ? composite->IDs.get<NodeArrayRef>("retainers") : nullptr;
	}

	void setSelfID(CompositeRef composite, CompositeRef selfComposite) {
		setID(composite, "self", getOwnID(selfComposite));
		setID(composite, "Self", getSelfCompositeID(selfComposite));
	}

	void setSubID(CompositeRef object, CompositeRef subObject) {
		if(!compositeIsObject(object)) {
			return;
		}

		if(compositeIsObject(subObject)) {
			setID(object, "sub", getOwnID(subObject));
			setID(object, "Sub", getTypeInheritedID(*subObject->type));
		}
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
			if(!excluded.contains(key) && retainingComposite->IDs.get(key) == *getOwnID(retainedComposite)) {
				return true;
			}
		}

		return false;
	}

	NodeRef createTypePart(NodeArrayRef type, NodeRef superpart, const Node& flags = Node()) {
		Node part = flags;

		if(type != nullptr) {
			if(superpart != nullptr) {
				part.get("super") = index_of(*type, superpart);
			}

			type->push_back(part);
		}

		return make_shared(part);
	}

	NodeRef createOrSetCollectionTypePart(NodeArrayRef type, NodeRef part, const Node& collectionFlag) {
		static Node collectionFlags = {
			{"array", true},
			{"dictionary", true},
			{"genericArguments", true},
			{"genericParameters", true},
			{"intersection", true},
			{"parameters", true},
			{"predefined", "Function"},
			{"union", true}
		};

		for(const auto& [flag, v] : *part) {
			if(
				collectionFlags.contains(flag) &&
				part->get(flag) == collectionFlags.get(flag)
			) {
				return createTypePart(type, part, collectionFlag);
			}
		}

		for(const auto& [k, v] : collectionFlag) {
			part->get(k) = v;
		}

		return part;
	}

	optional<int> getTypeInheritedID(const NodeArray& type) {
		int index = find_index(type, [](const NodeRef& v) { return v->get("inheritedTypes"); });

		if(index > -1) {
			NodeRef part = find(type, [&](const NodeRef& v) { return v->get("super") == index; });

			if(part) {
				return part->get("reference");
			}
		}
	}

	Node createValue(const string& primitiveType, const NodeValue& primitiveValue) {
		return {
			{"primitiveType", primitiveType},	// 'boolean', 'dictionary', 'float', 'integer', 'pointer', 'reference', 'string', 'type'
			{"primitiveValue", primitiveValue}	// boolean, float, integer, map (object), string, type
		};
	}

	string getValueString(NodeRef value) {
		if(value == nullptr) {
			return "nil";
		}

		if(set<string> {"boolean", "float", "integer", "string"}.contains(value->get("primitiveType"))) {
			return value->get("primitiveValue");
		}

		CompositeRef composite = getValueComposite(value);

		if(composite != nullptr) {
		//	return JSON.stringify(composite);
		}

		/*
		return JSON.stringify(value, (k, v) => {
			if(v instanceof Map) {
				return {
					__TYPE__: 'Map',
					__VALUE__: Array.from(v.entries())
				}
			} else
			if(v == null) {
				return null;
			} else {
				return v;
			}
		});
		*/
	}

	optional<Composite::Member> getMember(CompositeRef composite, const string& identifier) {
		return composite->members.count(identifier) > 0
			 ? make_optional(composite->members[identifier])
			 : nullopt;
	}

	Composite::Member addMember(CompositeRef composite, const string& identifier) {
		return getMember(composite, identifier)
			 ? *getMember(composite, identifier)
			 : composite->members[identifier];
	}

	void removeMember(CompositeRef composite, const string& identifier) {
		optional<Composite::Member> member = getMember(composite, identifier);

		if(member == nullopt) {
			return;
		}

		composite->members.erase(identifier);

		for(const Composite::MemberOverload& overload : *member) {
			retainOrReleaseValueComposites(composite, overload.value);
		}
	}

	void removeMembers(CompositeRef composite) {
		for(const auto& [identifier, v] : composite->members) {
			removeMember(composite, identifier);
		}
	}

	optional<Composite::MemberOverload> findInitializingMemberOverload(CompositeRef composite, const string& identifier, const Node& args) {
		if(compositeIsObject(composite) && set<string> {"super", "self", "sub"}.contains(identifier)) {
			composite = getComposite(composite->IDs.get("Self"));
		}
		if(compositeIsObject(composite)) {
			return getMemberOverload(composite, "init", [&](optional<Composite::MemberOverload> v) {
				v = findValueFunction(v->value, {});

				if(v != nullopt && getTypeFunctionProperties(v->type)->get("inits") == 1) {
					return v;
				}
			});
		}
	}

	optional<Composite::MemberOverload> findDeinitializingMemberOverload(CompositeRef object) {
		if(compositeIsObject(object)) {
			return getMemberOverload(object, "deinit", [&](optional<Composite::MemberOverload> v) {
				v = findValueFunction(v->value, {});

				if(v != nullopt && getTypeFunctionProperties(v->type)->get("deinits") == 1) {
					return v;
				}
			});
		}
	}

	optional<Composite::MemberOverload> findSubscriptedMemberOverload(CompositeRef composite, const string& identifier, bool internal, const NodeArray& args) {}

	/*
	 * Looking for member overload in Scope chain (scope).
	 *
	 * If search is internal, only first composite in chain will be checked.
	 */
	optional<Composite::MemberOverload> findMemberOverload(CompositeRef composite, const string& identifier, function<> matching, bool internal) {
		while(composite != nullptr) {
			Composite::MemberOverload overload =
				findMemberOverloadInComposite(composite, identifier, matching) ?:
				findMemberOverloadInObjectChain(composite, identifier, matching) ?:
				findMemberOverloadInInheritanceChain(composite, identifier, matching);

			if(overload != nullopt) {
				return overload;
			}

			if(internal) {
				break;
			}

			composite = getComposite(getScopeID(composite));
		}

		return nullopt;
	}

	/*
	 * Looking for member overload in Object chain (super, self, sub).
	 *
	 * When a "virtual" overload is found, search oncely displaces to a lowest sub-object.
	 */
	optional<MemberOverloadProxy> findMemberOverloadInObjectChain(CompositeRef object, const string& identifier, function<> matching) {
		if(!compositeIsObject(object)) {
			CompositeRef object_ = getComposite(object->IDs.get("self"));

			if(object_ == object || !compositeIsObject(object_)) {
				return nullopt;
			}

			object = object_;
		}

		bool virtual_;

		while(object != nullptr) {  // Higher
			auto overload = getMemberOverload(object, identifier, matching);

			if(overload != nullopt) {
				if(!virtual_ && overload.modifiers.contains("virtual")) {
					virtual_ = true;

					CompositeRef object_;

					while((object_ = getComposite(object->IDs.get("sub"))) != nullptr) {  // Lowest
						object = object_;
					}

					continue;
				}

				return getMemberOverloadProxy(overload, {{"owner", object}});
			}

			object = getComposite(object->IDs.get("super"));
		}
	}

	/*
	 * Looking for member overload in Inheritance chain (Super, Self).
	 */
	optional<MemberOverloadProxy> findMemberOverloadInInheritanceChain(CompositeRef composite, const string& identifier, function<> matching) {
		if(compositeIsObject(composite)) {
			composite = getComposite(composite->IDs.get("Self"));
		}

		while(composite != nullptr) {
			optional<Composite::MemberOverload> overload = getMemberOverload(composite, identifier, matching);

			if(overload != nullopt) {
				return getMemberOverloadProxy(*overload, {{"owner", composite}});
			}

			composite = getComposite(composite->IDs.get("Super"));
		}

		return nullopt;
	}

	bool membersRetain(CompositeRef retainingComposite, CompositeRef retainedComposite) {
		for(const auto& [k, member] : retainingComposite->members) {
			for(const auto& overload : member) {
				if(
					typeRetains(overload.type, retainedComposite) ||
					valueRetains(overload.value, retainedComposite) ||
					observersRetain(overload.observers, retainedComposite)
				) {
					return true;
				}
			}
		}

		return false;
	}

	void getObserver() {}

	void findObserver() {}

	void setObserver(CompositeRef composite, const string& identifier, CompositeRef function) {
		if(!composite->type->empty() && !set<string> {"Class", "Function", "Namespace", "Structure"}.contains(composite->type->at(0).get<NodeRef>()->get("predefined"))) {
			return;
		}

		composite->observers.get(identifier) = *getOwnID(function);
	}

	void deleteObserver(CompositeRef composite, const string& identifier) {
		int ID = composite->observers.get(identifier);

		composite->observers.remove(identifier);

		retainOrReleaseComposite(composite, getComposite(ID));
	}

	bool observersRetain(const Node& retainingObservers, CompositeRef retainedComposite) {
		int ID = *getOwnID(retainedComposite);

		for(const auto& [k, ID_] : retainingObservers) {
			if(ID_ == ID) {
				return true;
			}
		}

		return false;
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

	void reset(deque<CompositeRef> composites_ = {}, const Node& preferences_ = {}) {
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
		deque<CompositeRef> composites;
		deque<Report> reports;
	};

	Result interpret(Lexer::Result lexerResult, Parser::Result parserResult, deque<CompositeRef> composites_ = {}, const Node& preferences_ = {}) {
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

	Result interpretRaw(string_view code, deque<CompositeRef> composites_ = {}, const Node& preferences_ = {}) {
		Lexer::Result lexerResult = Lexer().tokenize(code);
		Parser::Result parserResult = Parser().parse(lexerResult);
		Result interpreterResult = interpret(lexerResult, parserResult, composites_, preferences_);

		return interpreterResult;
	}
};