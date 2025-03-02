#pragma once

#include "Lexer.cpp"
#include "Parser.cpp"
#include "Type.cpp"

using Report = Parser::Report;

// ----------------------------------------------------------------

namespace Interpreter_ {

struct ControlTransfer {
	TypeRef value;
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
deque<CompositeTypeRef> scopes;
deque<ControlTransfer> controlTransfers;
Preferences preferences;
deque<Report> reports;

struct Interpreter {
	Interpreter() {};

	any any_rules(const string& type, NodeRef n) {
		if(!n) {
			throw invalid_argument("The rule type ("+type+") is not in exceptions list for corresponding node to be present but null pointer is passed");
		}

		if(type == "argument") {
			auto key = !n->empty("label") ? n->get<Node&>("label").get<string>("value") : "";
			auto value = executeNode(n->get("value"));

			if(threw()) {
				return nullptr;
			}

			return make_pair(
				key,
				value
			);
		} else
		if(type == "entry") {
			auto key = executeNode(n->get("key"));

			if(threw()) {
				return nullptr;
			}

			auto value = executeNode(n->get("value"));

			if(threw()) {
				return nullptr;
			}

			return make_pair(
				key,
				value
			);
		}

		return nullptr;
	}

	template<typename... Args>
	TypeRef rules(const string& type, NodeRef n = nullptr, Args... args) {
		vector<any> arguments = {args...};

		if(
			type != "module" &&
			type != "nilLiteral" &&
			!n
		) {
			throw invalid_argument("The rule type ("+type+") is not in exceptions list for corresponding node to be present but null pointer is passed");
		}

		if(type == "arrayLiteral") {
			auto value = Ref<DictionaryType>(
				PredefinedPIntegerTypeRef,
				Ref<NillableType>(PredefinedEAnyTypeRef),
				true
			);

			for(int i = 0; i < n->get<NodeArray&>("values").size(); i++) {
				auto value_ = executeNode(n->get<NodeArray&>("values")[i]);

				if(value_) {
					value->emplace(Ref<PrimitiveType>(i), value_);  // TODO: Copy or link value in accordance to type
				}
			}

			return getValueWrapper(value, "Array");
		} else
		if(type == "arrayType") {
			TypeRef valueType = executeNode(n->get("value"), false) ?: Ref<NillableType>(PredefinedEAnyTypeRef);
			CompositeTypeRef composite = getValueComposite(nullptr/*findMemberOverload(scope, "Array")?.value*/);

			if(composite) {
				return Ref<ReferenceType>(composite, vector<TypeRef> { valueType });  // TODO: Check if type accepts passed generic argument
			} else {
				return Ref<DictionaryType>(PredefinedPIntegerTypeRef, valueType);
			}

			return nullptr;
		} else
		if(type == "booleanLiteral") {
			return getValueWrapper(n->get("value") == "true", "Boolean");
		} else
		if(type == "breakStatement") {
			TypeRef value;

			if(!n->empty("label")) {
				value = Ref<PrimitiveType>(n->get<Node&>("label").get<string>("value"));
			}

			setControlTransfer(value, "break");

			return value;
		} else
		if(type == "continueStatement") {
			TypeRef value;

			if(!n->empty("label")) {
				value = Ref<PrimitiveType>(n->get<Node&>("label").get<string>("value"));
			}

			setControlTransfer(value, "continue");

			return value;
		} else
		if(type == "dictionaryLiteral") {
			auto value = Ref<DictionaryType>(
				Ref<NillableType>(PredefinedEAnyTypeRef),
				Ref<NillableType>(PredefinedEAnyTypeRef),
				true
			);

			for(const NodeRef& entry : n->get<NodeArray&>("entries")) {
				auto entry_ = any_optcast<DictionaryType::Entry>(any_rules("entry", entry));

				if(entry_) {
					value->emplace(entry_->first, entry_->second);  // TODO: Copy or link value in accordance to type
				}
			}

			return getValueWrapper(value, "Dictionary");
		} else
		if(type == "dictionaryType") {
			TypeRef keyType = executeNode(n->get("key"), false) ?: Ref<NillableType>(PredefinedEAnyTypeRef),
					valueType = executeNode(n->get("value"), false) ?: Ref<NillableType>(PredefinedEAnyTypeRef);
			CompositeTypeRef composite = getValueComposite(nullptr/*findMemberOverload(scope, "Dictionary")?.value*/);

			if(composite) {
				return Ref<ReferenceType>(composite, vector<TypeRef> { keyType, valueType });  // TODO: Check if type accepts passed generic arguments
			} else {
				return Ref<DictionaryType>(keyType, valueType);
			}

			return nullptr;
		} else
		if(type == "floatLiteral") {
			return getValueWrapper(n->get<double>("value"), "Float");
		} else
		if(type == "ifStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
			CompositeTypeRef namespace_ = createNamespace("Local<"+getTitle(scope())+", If>", scope(), nullopt);

			addScope(namespace_);

			auto conditionValue = executeNode(n->get("condition"));
			bool condition = conditionValue && *conditionValue,
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
			return getValueWrapper(n->get<int>("value"), "Integer");
		} else
		if(type == "module") {
			addControlTransfer();
			addScope(getComposite(0) ?: createNamespace("Global"));
		//	addDefaultMembers(scope());
			executeNodes(tree ? tree->get<NodeArrayRef>("statements") : nullptr/*, (t) => t !== 'throw' ? 0 : -1*/);

			if(threw()) {
				report(2, nullptr, to_string(controlTransfer().value));
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

			if(value->ID == TypeID::Primitive && !n->empty("operator")) {
				if(n->get<Node&>("operator").get("value") == "++") {
					*value = value->operator+()->operator+(Ref<PrimitiveType>(1));

					return value->operator-(Ref<PrimitiveType>(1));
				}
				if(n->get<Node&>("operator").get("value") == "--") {
					*value = value->operator+()->operator-(Ref<PrimitiveType>(1));

					return value->operator+(Ref<PrimitiveType>(1));
				}
			}

			// TODO: Dynamic operators lookup, check for values mutability, observers notification

			return value;
		} else
		if(type == "predefinedType") {
			string predefinedTypeTitle = n->get("value");

			if(predefinedTypeTitle == "void")			return PredefinedEVoidTypeRef;
			if(predefinedTypeTitle == "_")				return PredefinedEAnyTypeRef;

			if(predefinedTypeTitle == "any")			return PredefinedPAnyTypeRef;
			if(predefinedTypeTitle == "bool")			return PredefinedPBooleanTypeRef;
			if(predefinedTypeTitle == "dict")			return PredefinedPDictionaryTypeRef;
			if(predefinedTypeTitle == "float")			return PredefinedPFloatTypeRef;
			if(predefinedTypeTitle == "int")			return PredefinedPIntegerTypeRef;
			if(predefinedTypeTitle == "string")			return PredefinedPStringTypeRef;
			if(predefinedTypeTitle == "type")			return PredefinedPTypeTypeRef;

			if(predefinedTypeTitle == "Any")			return PredefinedCAnyTypeRef;
			if(predefinedTypeTitle == "Class")			return PredefinedCClassTypeRef;
			if(predefinedTypeTitle == "Enumeration")	return PredefinedCEnumerationTypeRef;
			if(predefinedTypeTitle == "Function")		return PredefinedCFunctionTypeRef;
			if(predefinedTypeTitle == "Namespace")		return PredefinedCNamespaceTypeRef;
			if(predefinedTypeTitle == "Object")			return PredefinedCObjectTypeRef;
			if(predefinedTypeTitle == "Protocol")		return PredefinedCProtocolTypeRef;
			if(predefinedTypeTitle == "Structure")		return PredefinedCStructureTypeRef;

			return PredefinedEAnyTypeRef;
		} else
		if(type == "prefixExpression") {
			auto value = executeNode(n->get("value"));

			if(!value) {
				return nullptr;
			}

			if(value->ID == TypeID::Primitive && !n->empty("operator") ) {
				if(n->get<Node&>("operator").get("value") == "!") {
					return Ref<PrimitiveType>(value->operator!());
				}
				if(n->get<Node&>("operator").get("value") == "-") {
					return value->operator-();
				}
				if(n->get<Node&>("operator").get("value") == "++") {
					*value = value->operator+()->operator+(Ref<PrimitiveType>(1));

					return value;
				}
				if(n->get<Node&>("operator").get("value") == "--") {
					*value = value->operator+()->operator-(Ref<PrimitiveType>(1));

					return value;
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
			report(0, n, to_string(value));

			return value;
		} else
		if(type == "stringLiteral") {
			::string string = "";

			for(const NodeRef& segment : n->get<NodeArray&>("segments")) {
				if(segment->get("type") == "stringSegment") {
					string += segment->get<::string>("value");
				} else {  // stringExpression
					auto value = executeNode(segment->get("value"));

					if(threw()) {
						return nullptr;
					}

					string += to_string(value, true);
				}
			}

			return getValueWrapper(string, "String");
		} else
		if(type == "throwStatement") {
			auto value = executeNode(n->get("value"));

			setControlTransfer(value, "throw");

			return value;
		} else
		if(type == "typeExpression") {
			auto type = executeNode(n->get("type_"), false);

			if(!type) {
				return nullptr;
			}

			return Ref<PrimitiveType>(type);
		} else
		if(type == "typeIdentifier") {
			CompositeTypeRef composite = getValueComposite(executeNode(n->get("identifier")));

			if(!composite || compositeIsObject(composite)) {
				report(1, n, "Composite is an object or wasn\'t found.");

				return PredefinedCAnyTypeRef;
			} else {
				vector<TypeRef> genericArguments;

				for(const NodeRef& type : n->get<NodeArray&>("genericArguments")) {
					auto type_ = executeNode(type, false);

					if(type_) {
						genericArguments.push_back(type_);
					}
				}

				return Ref<ReferenceType>(composite, genericArguments);  // TODO: Check if type accepts passed generic arguments
			}
		} else
		if(type == "whileStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
			CompositeTypeRef namespace_ = createNamespace("Local<"+getTitle(scope())+", While>", scope(), nullopt);

			addScope(namespace_);

			while(true) {
			//	removeMembers(namespace);

				auto conditionValue = executeNode(n->get("condition"));
				bool condition = conditionValue && *conditionValue;

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

	CompositeTypeRef getComposite(NodeValue ID) {
		return ID.type() == 2 && (int)ID < composites.size() ? composites[(int)ID] : nullptr;
	}

	CompositeTypeRef createComposite(const string& title, CompositeTypeID type, CompositeTypeRef scope = nullptr) {
		cout << "createComposite("+title+")" << endl;
		auto composite = Ref<CompositeType>(type, title);

		composites.push_back(composite);

		if(scope) {
			setScopeID(composite, scope);
		}

		return composite;
	}

	void destroyComposite(CompositeTypeRef composite) {
		cout << "destroyComposite("+getTitle(composite)+")" << endl;
		if(composite->life == 2) {
			return;
		}

		composite->life = 2;

		int ID = getOwnID(composite);
		NodeArrayRef retainersIDs = getRetainersIDs(composite);

		for(const CompositeTypeRef& composite_ : composites) {
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
			CompositeTypeRef composite_ = getComposite(retainerID);

			if(composite_ && composite_->life < 2) {
				aliveRetainers++;

				// TODO: Notify retainers about destroy
			}
		}

		if(aliveRetainers > 0) {
			report(1, nullptr, "Composite #"+(string)getOwnID(composite)+" was destroyed with a non-empty retainer list.");
		}
	}

	void destroyReleasedComposite(CompositeTypeRef composite) {
		if(composite && !compositeRetained(composite)) {
			destroyComposite(composite);
		}
	}

	void retainComposite(CompositeTypeRef retainingComposite, CompositeTypeRef retainedComposite) {
		if(retainedComposite == nullptr || retainedComposite == retainingComposite) {
			return;
		}

		NodeArrayRef retainersIDs = getRetainersIDs(retainedComposite);
		int retainingID = getOwnID(retainingComposite);

		if(!contains(*retainersIDs, retainingID)) {
			retainersIDs->push_back(retainingID);
		}
	}

	void releaseComposite(CompositeTypeRef retainingComposite, CompositeTypeRef retainedComposite) {
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
	void retainOrReleaseComposite(CompositeTypeRef retainingComposite, CompositeTypeRef retainedComposite) {
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
	bool compositeRetains(CompositeTypeRef retainingComposite, CompositeTypeRef retainedComposite) {
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
	bool compositeRetainsDistant(CompositeTypeRef retainingComposite, CompositeTypeRef retainedComposite, shared_ptr<vector<int>> retainersIDs = Ref<vector<int>>()) {
		/*
		if(Array.isArray(retainingComposite)) {
			for(const CompositeTypeRef& retainingComposite_ : retainingComposite) {
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
	bool compositeRetained(CompositeTypeRef composite) {
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
	CompositeTypeRef createNamespace(const string& title, CompositeTypeRef scope = nullptr, optional<CompositeTypeRef> levels = nullptr) {
		CompositeTypeRef namespace_ = createComposite(title, CompositeTypeID::Namespace, scope);

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

	bool compositeIsFunction(const CompositeTypeRef& composite) {
		return composite && PredefinedCFunctionTypeRef->acceptsA(composite);
	}

	bool compositeIsNamespace(const CompositeTypeRef& composite) {
		return composite && PredefinedCNamespaceTypeRef->acceptsA(composite);
	}

	bool compositeIsObject(const CompositeTypeRef& composite) {
		return composite && PredefinedCObjectTypeRef->acceptsA(composite);
	}

	bool compositeIsInstantiable(const CompositeTypeRef& composite) {
		return composite && (
			PredefinedCClassTypeRef->acceptsA(composite) ||
			PredefinedCStructureTypeRef->acceptsA(composite)
		);
	}

	bool compositeIsCallable(const CompositeTypeRef& composite) {
		return (
			compositeIsFunction(composite) ||
			compositeIsInstantiable(composite)
		);
	}

	CompositeTypeRef scope() {
		return getScope();
	}

	CompositeTypeRef getScope(int offset = 0) {
		return scopes.size() > scopes.size()-1+offset
			 ? scopes[scopes.size()-1+offset]
			 : nullptr;
	}

	/*
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	void addScope(CompositeTypeRef composite) {
		scopes.push_back(composite);
	}

	/*
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
		CompositeTypeRef composite = scopes.back();

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
	void setControlTransfer(TypeRef value = nullptr, optional<string> type = nullopt) {
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
	TypeRef executeNode(NodeRef node, bool concrete = true, Args... arguments) {
		if(!node) {
			return nullptr;
		}

		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		TypeRef value;

		position = NP;
		value = rules(node->get("type"), node, arguments...);

		if(value && value->concrete != concrete) {  // Anonymous protocols are _now_ concrete, but this may change in the future
			value = nullptr;
		}

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
			TypeRef value = executeNode(node);

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

	string getTitle(CompositeTypeRef composite) {
		return composite && composite->title.length() ? composite->title : "#"+(string)getOwnID(composite);
	}

	void setID(CompositeTypeRef composite, const string& key, const NodeValue& value) {
		if(set<string> {"own", "retainers"}.contains(key)) {
			return;
		}

		NodeValue OV, NV;  // Old/new value

		OV = composite->IDs.get(key);
		NV = composite->IDs.get(key) = value;

		if(OV != NV) {
			if(tolower(key) != "self" && NV != -1) {  // ID chains should not be cyclic, intentionally missed IDs can't create cycles
				set<CompositeTypeRef> composites;
				CompositeTypeRef composite_ = composite;

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

	NodeValue getOwnID(CompositeTypeRef composite, bool nillable = false) {
		return composite ? composite->IDs.get("own") :
			   nillable ? NodeValue() :
			   throw invalid_argument("Composite reference shouldn't be nil");
	}

	NodeArrayRef getRetainersIDs(CompositeTypeRef composite) {
		return composite ? composite->IDs.get<NodeArrayRef>("retainers") : nullptr;
	}

	void setSelfID(CompositeTypeRef composite, CompositeTypeRef selfComposite) {
		setID(composite, "self", getOwnID(selfComposite));
	//	setID(composite, "Self", getSelfCompositeID(selfComposite));
	}

	void setInheritedLevelIDs(CompositeTypeRef inheritingComposite, CompositeTypeRef inheritedComposite) {
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

	void setMissedLevelIDs(CompositeTypeRef missingComposite) {
		static set<string> excluded = {"own", "scope", "retainers"};

		for(const auto& [key, value] : missingComposite->IDs) {
			if(!excluded.contains(key)) {
				setID(missingComposite, key, -1);
			}
		}
	}

	void setScopeID(CompositeTypeRef composite, CompositeTypeRef scopeComposite) {
		setID(composite, "scope", getOwnID(scopeComposite, true));
	}

	bool IDsRetain(CompositeTypeRef retainingComposite, CompositeTypeRef retainedComposite) {
		static set<string> excluded = {"own", "retainers"};

		for(const auto& [key, value] : retainingComposite->IDs) {
			if(!excluded.contains(key) && retainingComposite->IDs.get(key) == getOwnID(retainedComposite)) {
				return true;
			}
		}

		return false;
	}

	/*
	 * Returns result of identifier(value) call or plain value if no appropriate function found in current scope.
	 */
	TypeRef getValueWrapper(const TypeRef& value, string identifier) {
		// TODO

		return value;
	}

	TypeRef getValueWrapper(const PrimitiveType& value, string identifier) {
		return getValueWrapper(Ref(value), identifier);
	}

	CompositeTypeRef getValueComposite(TypeRef value) {
		if(value) {
			switch(value->ID) {
				case TypeID::Composite: return static_pointer_cast<CompositeType>(value);
				case TypeID::Reference: return static_pointer_cast<ReferenceType>(value)->compType;
			}
		}

		return nullptr;
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

};  // namespace Interpreter_