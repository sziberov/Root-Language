#pragma once

#include "Parser.cpp"

namespace Interpreter {
	using Location = Lexer::Location;
	using Token = Lexer::Token;
	using Report = Parser::Report;

	deque<Token> tokens;
	NodeSP tree;
	int position;
	struct Preferences {
		int callStackSize,
			allowedReportLevel,
			metaprogrammingLevel;
		bool arbitaryPrecisionArithmetics;
	} preferences;
	deque<Report> reports;

	void report(int level, NodeSP node, const string& string) {
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

	// ----------------------------------------------------------------

	struct Type;

	struct ParenthesizedType;
	struct NillableType;
	struct DefaultType;
	struct UnionType;
	struct IntersectionType;

	struct PredefinedType;
	struct PrimitiveType;
	struct DictionaryType;
	struct CompositeType;
	struct ReferenceType;

	struct FunctionType;
	struct InoutType;
	struct VariadicType;

	// ----------------------------------------------------------------

	using TypeSP = sp<Type>;

	using ParenthesizedTypeSP = sp<ParenthesizedType>;
	using NillableTypeSP = sp<NillableType>;
	using DefaultTypeSP = sp<DefaultType>;
	using UnionTypeSP = sp<UnionType>;
	using IntersectionTypeSP = sp<IntersectionType>;

	using PredefinedTypeSP = sp<PredefinedType>;
	using PrimitiveTypeSP = sp<PrimitiveType>;
	using DictionaryTypeSP = sp<DictionaryType>;
	using CompositeTypeSP = sp<CompositeType>;
	using ReferenceTypeSP = sp<ReferenceType>;

	using FunctionTypeSP = sp<FunctionType>;
	using InoutTypeSP = sp<InoutType>;
	using VariadicTypeSP = sp<VariadicType>;

	// ----------------------------------------------------------------

	enum class TypeID : u8 {
		Undefined,

		Parenthesized,
		Nillable,
		Default,
		Union,
		Intersection,

		Predefined,
		Primitive,
		Array,
		Dictionary,
		Composite,
		Reference,

		Function,
		Inout,
		Variadic
	};

	enum class PredefinedTypeID : u8 {
		// Extremes
		EVoid,
		EAny,

		// Primitives
		PAny,
		PBoolean,
		PDictionary,
		PFloat,
		PInteger,
		PString,
		PType,

		// Composites
		CAny,
		CClass,
		CEnumeration,
		CFunction,
		CNamespace,
		CObject,
		CProtocol,
		CStructure
	};

	enum class PrimitiveTypeID : u8 {
		Boolean,
		Float,
		Integer,
		String,
		Type
	};

	enum class CompositeTypeID : u8 {
		Class,
		Enumeration,
		Function,
		Namespace,
		Object,
		Protocol,
		Structure
	};

	// ----------------------------------------------------------------

	extern const TypeSP PredefinedEVoidTypeSP;
	extern const TypeSP PredefinedEAnyTypeSP;

	extern const TypeSP PredefinedPAnyTypeSP;
	extern const TypeSP PredefinedPBooleanTypeSP;
	extern const TypeSP PredefinedPFloatTypeSP;
	extern const TypeSP PredefinedPIntegerTypeSP;
	extern const TypeSP PredefinedPStringTypeSP;
	extern const TypeSP PredefinedPTypeTypeSP;

	extern const TypeSP PredefinedCAnyTypeSP;
	extern const TypeSP PredefinedCClassTypeSP;
	extern const TypeSP PredefinedCEnumerationTypeSP;
	extern const TypeSP PredefinedCFunctionTypeSP;
	extern const TypeSP PredefinedCNamespaceTypeSP;
	extern const TypeSP PredefinedCObjectTypeSP;
	extern const TypeSP PredefinedCProtocolTypeSP;
	extern const TypeSP PredefinedCStructureTypeSP;

	// ----------------------------------------------------------------

	deque<CompositeTypeSP> composites;  // Global composite storage is allowed, as it decided by design to have only one Interpreter instance in a single process memory
	deque<CompositeTypeSP> scopes;

	CompositeTypeSP getComposite(optional<int> ID) {
		return ID && *ID < composites.size() ? composites[*ID] : nullptr;
	}

	CompositeTypeSP scope();
	CompositeTypeSP getValueComposite(TypeSP value);
	unordered_set<CompositeTypeSP> getValueComposites(TypeSP value);

	// ----------------------------------------------------------------

	struct ControlTransfer {
		TypeSP value;
		optional<string> type;
	};

	deque<ControlTransfer> controlTransfers;

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

	/**
	 * Should be used for a return values before releasing its scopes,
	 * as ARC utilizes a control trasfer value at the moment.
	 *
	 * Specifying a type means explicit control transfer.
	 */
	void setControlTransfer(TypeSP value = nullptr, optional<string> type = nullopt) {
		ControlTransfer& CT = controlTransfer();

		CT.value = value;
		CT.type = type;
	}

	void resetControlTransfer() {
		ControlTransfer& CT = controlTransfer();

		CT.value = nullptr;
		CT.type = nullopt;
	}

	// ----------------------------------------------------------------

	struct Type : enable_shared_from_this<Type> {
		const TypeID ID;
		const bool concrete;

		Type(TypeID ID = TypeID::Undefined, bool concrete = false) : ID(ID), concrete(concrete) { /*cout << "Type created\n";*/ }
		virtual ~Type() { /*cout << "Type destroyed\n";*/ }

		virtual bool acceptsA(const TypeSP& type) { return false; }
		virtual bool conformsTo(const TypeSP& type) { return type->acceptsA(shared_from_this()); }
		virtual TypeSP normalized() { return shared_from_this(); }
		virtual string toString() const { return string(); }  // User-friendly representation

		// Cast
		virtual operator bool() const { return bool(); }
		virtual operator double() const { return double(); }
		virtual operator int() const { return operator double(); }
		virtual operator string() const { return string(); }  // Machine-friendly representation

		// Direct access
		virtual TypeSP get(TypeSP getType = nullptr) { return shared_from_this(); }
		virtual TypeSP call(vector<TypeSP> arguments, TypeSP getType = nullptr) { return nullptr; }
		virtual void set(TypeSP setValue = nullptr) {}
		virtual void delete_() {}

		// Subscript access
		virtual TypeSP get(vector<TypeSP> arguments, TypeSP getType = nullptr) { return nullptr; }
		virtual void set(vector<TypeSP> arguments, TypeSP setValue = nullptr) {}
		virtual void delete_(vector<TypeSP> arguments) {}

		// Member access (for InoutType internal use only)
		virtual TypeSP get(TypeSP key, optional<vector<TypeSP>> arguments, TypeSP getType = nullptr) { return nullptr; }
		virtual void set(TypeSP key, optional<vector<TypeSP>> arguments, TypeSP setValue = nullptr) {}
		virtual void delete_(TypeSP key, optional<vector<TypeSP>> arguments) {}

		// Operators
		virtual TypeSP positive() const { return SP<Type>(); }
		virtual TypeSP negative() const { return SP<Type>(); }
		virtual TypeSP plus(TypeSP type) const { return SP<Type>(); }
		virtual TypeSP minus(TypeSP type) const { return plus(static_pointer_cast<Type>(type)->negative()); }
		virtual TypeSP preIncrement() { return shared_from_this(); }
		virtual TypeSP preDecrement() { return shared_from_this(); }
		virtual TypeSP postIncrement(int) { return SP<Type>(); }
		virtual TypeSP postDecrement(int) { return SP<Type>(); }
		virtual TypeSP multiply(TypeSP type) const { return SP<Type>(); }
		virtual TypeSP divide(TypeSP type) const { return SP<Type>(); }
		virtual bool not_() const { return !operator bool(); }
		virtual bool equalsTo(const TypeSP& type) { return acceptsA(type) && conformsTo(type); }
		virtual bool notEqualsTo(const TypeSP& type) { return !equalsTo(type); }

		// Now we are planning to normalize all types for one single time before subsequent comparisons, or using this/similar to this function at worst case, as this is performant-costly operation in C++
		// One thing that also should be mentioned here is that types can't be normalized without losing their initial string representation at the moment
		static bool acceptsANormalized(const TypeSP& left, const TypeSP& right) {
			return left && left->acceptsA(right ? right->normalized() : PredefinedEVoidTypeSP);
		}

		static bool acceptsAConcrete(const TypeSP& left, const TypeSP& right) {
			return left && right && right->concrete && left->acceptsA(right);
		}
	};

	static string to_string(const TypeSP& type, bool raw = false) {
		return type ? (raw ? type->operator string() : type->toString()) : "nil";  // std::to_string(type.use_count());
	}

	// ----------------------------------------------------------------

	struct ParenthesizedType : Type {
		TypeSP innerType;

		ParenthesizedType(TypeSP type) : Type(TypeID::Parenthesized), innerType(move(type)) { cout << "(" << innerType->toString() << ") type created\n"; }
		~ParenthesizedType() { cout << "(" << innerType->toString() << ") type destroyed\n"; }

		bool acceptsA(const TypeSP& type) override {
			return innerType->acceptsA(type);
		}

		TypeSP normalized() override {
			return innerType->normalized();
		}

		string toString() const override {
			return "("+innerType->toString()+")";
		}
	};

	struct NillableType : Type {
		TypeSP innerType;

		NillableType(TypeSP type) : Type(TypeID::Nillable), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
		~NillableType() { cout << innerType->toString() << "? type destroyed\n"; }

		bool acceptsA(const TypeSP& type) override {
			return PredefinedEVoidTypeSP->acceptsA(type) ||
				   type->ID == TypeID::Nillable && innerType->acceptsA(static_pointer_cast<NillableType>(type)->innerType) ||
				   innerType->acceptsA(type);
		}

		TypeSP normalized() override {
			auto normInnerType = innerType->normalized();

			if(normInnerType->ID == TypeID::Nillable) {
				return normInnerType;
			}

			return SP<NillableType>(normInnerType);
		}

		string toString() const override {
			return innerType->toString()+"?";
		}
	};

	struct DefaultType : Type {
		TypeSP innerType;

		DefaultType(TypeSP type) : Type(TypeID::Default), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
		~DefaultType() { cout << innerType->toString() << "! type destroyed\n"; }

		bool acceptsA(const TypeSP& type) override {
			return PredefinedEVoidTypeSP->acceptsA(type) ||
				   type->ID == TypeID::Default && innerType->acceptsA(static_pointer_cast<DefaultType>(type)->innerType) ||
				   innerType->acceptsA(type);
		}

		TypeSP normalized() override {
			auto normInnerType = innerType->normalized();

			if(normInnerType->ID == TypeID::Default) {
				return normInnerType;
			}

			return SP<DefaultType>(normInnerType);
		}

		string toString() const override {
			return innerType->toString()+"!";
		}
	};

	struct UnionType : Type {
		vector<TypeSP> alternatives;

		UnionType(const vector<TypeSP>& alternatives) : Type(TypeID::Union), alternatives(alternatives) {}

		bool acceptsA(const TypeSP& type) override {
			for(const TypeSP& alt : alternatives) {
				if(alt->acceptsA(type)) {
					return true;
				}
			}

			return false;
		}

		TypeSP normalized() override {
			vector<TypeSP> normAlts;

			for(const TypeSP& alt : alternatives) {
				TypeSP normAlt = alt->normalized();

				if(normAlt->ID == TypeID::Union) {
					auto normUnionAlt = static_pointer_cast<UnionType>(normAlt);

					normAlts.insert(normAlts.end(), normUnionAlt->alternatives.begin(), normUnionAlt->alternatives.end());
				} else {
					normAlts.push_back(normAlt);
				}
			}

			if(normAlts.size() == 1) {
				return normAlts[0];
			}

			return SP<UnionType>(normAlts);
		}

		string toString() const override {
			string result;

			for(int i = 0; i < alternatives.size(); i++) {
				if(i > 0) {
					result += " | ";
				}

				result += alternatives[i]->toString();
			}

			return result;
		}
	};

	struct IntersectionType : Type {
		vector<TypeSP> alternatives;

		IntersectionType(const vector<TypeSP>& alternatives) : Type(TypeID::Intersection), alternatives(alternatives) {}

		bool acceptsA(const TypeSP& type) override {
			for(const TypeSP& alt : alternatives) {
				if(!alt->acceptsA(type)) {
					return false;
				}
			}

			return true;
		}

		TypeSP normalized() override {
			vector<TypeSP> normAlts;

			for(const TypeSP& alt : alternatives) {
				TypeSP normAlt = alt->normalized();

				if(normAlt->ID == TypeID::Intersection) {
					auto normUnionAlt = static_pointer_cast<IntersectionType>(normAlt);

					normAlts.insert(normAlts.end(), normUnionAlt->alternatives.begin(), normUnionAlt->alternatives.end());
				} else {
					normAlts.push_back(normAlt);
				}
			}

			if(normAlts.size() == 1) {
				return normAlts[0];
			}

			return SP<IntersectionType>(normAlts);
		}

		string toString() const override {
			string result;

			for(int i = 0; i < alternatives.size(); i++) {
				if(i > 0) {
					result += " & ";
				}

				result += alternatives[i]->toString();
			}

			return result;
		}
	};

	// ----------------------------------------------------------------

	struct PredefinedType : Type {
		const PredefinedTypeID subID;
		function<bool(const TypeSP&)> acceptsFn;

		PredefinedType(PredefinedTypeID subID, function<bool(const TypeSP&)> acceptsFn) : Type(TypeID::Predefined), subID(subID), acceptsFn(acceptsFn) {}

		bool acceptsA(const TypeSP& type) override {
			return acceptsFn(type);
		}

		string toString() const override {
			switch(subID) {
				case PredefinedTypeID::EVoid:			return "void";
				case PredefinedTypeID::EAny:			return "_";

				case PredefinedTypeID::PAny:			return "any";
				case PredefinedTypeID::PBoolean:		return "bool";
				case PredefinedTypeID::PDictionary:		return "dict";
				case PredefinedTypeID::PFloat:			return "float";
				case PredefinedTypeID::PInteger:		return "int";
				case PredefinedTypeID::PString:			return "string";
				case PredefinedTypeID::PType:			return "type";

				case PredefinedTypeID::CAny:			return "Any";
				case PredefinedTypeID::CClass:			return "Class";
				case PredefinedTypeID::CEnumeration:	return "Enumeration";
				case PredefinedTypeID::CFunction:		return "Function";
				case PredefinedTypeID::CNamespace:		return "Namespace";
				case PredefinedTypeID::CObject:			return "Object";
				case PredefinedTypeID::CProtocol:		return "Protocol";
				case PredefinedTypeID::CStructure:		return "Structure";
			}

			return string();
		}
	};

	struct PrimitiveType : Type {
		const PrimitiveTypeID subID;
		mutable any value;

		PrimitiveType(const bool& v) : Type(TypeID::Primitive, true), subID(PrimitiveTypeID::Boolean), value(v) { cout << toString() << " type created\n"; }
		PrimitiveType(const double& v) : Type(TypeID::Primitive, true), subID(v != static_cast<int>(v) ? PrimitiveTypeID::Float : PrimitiveTypeID::Integer),
																		value(v != static_cast<int>(v) ? any(v) : any(static_cast<int>(v))) { cout << toString() << " type created\n"; }
		PrimitiveType(const int& v) : Type(TypeID::Primitive, true), subID(PrimitiveTypeID::Integer), value(v) { cout << toString() << " type created\n"; }
		PrimitiveType(const char* v) : Type(TypeID::Primitive, true), subID(PrimitiveTypeID::String), value(string(v)) { cout << toString() << " type created\n"; }
		PrimitiveType(const string& v) : Type(TypeID::Primitive, true), subID(PrimitiveTypeID::String), value(v) { cout << toString() << " type created\n"; }
		PrimitiveType(const TypeSP& v) : Type(TypeID::Primitive, true), subID(PrimitiveTypeID::Type), value(v ?
																										   (!v->concrete ? v : throw invalid_argument("Primitive type isn't mean to store concrete types")) :
																															   throw invalid_argument("Primitive type cannot be represented by nil")) { cout << toString() << " type created\n"; }
		~PrimitiveType() { cout << toString() << " type destroyed\n"; }

		bool acceptsA(const TypeSP& type) override {
			if(type->ID == TypeID::Primitive) {
				auto primType = static_pointer_cast<PrimitiveType>(type);

				if(subID == primType->subID) {
					if(subID == PrimitiveTypeID::Type) {
						return any_cast<TypeSP>(value)->acceptsA(any_cast<TypeSP>(primType->value));
					}

					return true;
				}
			}

			return false;
		}

		string toString() const override {
			switch(subID) {
				case PrimitiveTypeID::Boolean:	return any_cast<bool>(value) ? "true" : "false";
				case PrimitiveTypeID::Float:	return format("{}", any_cast<double>(value));
				case PrimitiveTypeID::Integer:	return std::to_string(any_cast<int>(value));
				case PrimitiveTypeID::String:	return "'"+any_cast<string>(value)+"'";
				case PrimitiveTypeID::Type:		return "type "+any_cast<TypeSP>(value)->toString();
				default:						return string();
			}
		}

		operator bool() const override {
			return operator double() > 0;
		}

		operator double() const override {
			switch(subID) {
				case PrimitiveTypeID::Boolean:	return any_cast<bool>(value);
				case PrimitiveTypeID::Float:	return any_cast<double>(value);
				case PrimitiveTypeID::Integer:	return any_cast<int>(value);
				case PrimitiveTypeID::String:	return stod(any_cast<string>(value));
				case PrimitiveTypeID::Type:		return any_cast<TypeSP>(value)->operator double();
				default:						return double();
			}
		}

		operator int() const override {
			return operator double();
		}

		operator string() const override {
			switch(subID) {
				case PrimitiveTypeID::String:	return any_cast<string>(value);
				case PrimitiveTypeID::Type:		return any_cast<TypeSP>(value)->operator string();
				default:						return toString();
			}
		}

		TypeSP set(TypeSP type) override {
			if(type->ID == TypeID::Primitive) {
				auto primType = static_pointer_cast<PrimitiveType>(type);

				if(primType->subID == subID) {
					value = primType->value;
				}
			}

			return shared_from_this();
		}

		TypeSP positive() const override {
			return subID == PrimitiveTypeID::Type
				 ? SP<PrimitiveType>(any_cast<TypeSP>(value))
				 : SP<PrimitiveType>(operator double());
		}

		TypeSP negative() const override {
			return subID == PrimitiveTypeID::Type
				 ? SP<PrimitiveType>(any_cast<TypeSP>(value))
				 : SP<PrimitiveType>(-operator double());
		}

		TypeSP plus(TypeSP type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::positive();
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			if(subID == PrimitiveTypeID::String || primType->subID == PrimitiveTypeID::String) {
				return SP<PrimitiveType>(operator string()+primType->operator string());
			}

			return SP<PrimitiveType>(operator double()+primType->operator double());
		}

		TypeSP minus(TypeSP type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::negative();
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			return SP<PrimitiveType>(operator double()-primType->operator double());
		}

		// TODO:
		// Examine the idea of concrete types access through members (as a kind of proxy).
		// In theory, this will make whole observing mechanism straight-forward:
		// - Members can track if contained value is changed and call observers when needed.
		// - Also they can track if value's type is changed and give an error if doesn't conform to accepting type.
		// Calling concrete type operators may be somewhat transparent, where using "outer" operator will internally recall "inner" and observe for any changes simultaneously.
		// Direct calls are also possible in case of literals mutation, but should be prohibited when accessing members.
		// Although (!) this still does not provide solution for automatic member notification about composites deinitialization.
		// Now, deinitializing composite must look up for each member (or another type of retaining declaration) of each retainer to set them nil and notify (or remove completely).
		//
		// Somewhere in between there is idea of direct value access with every concrete type storing references to all its definitions (containing members).
		// It sounds more promising in regards to backward control, e.g. deinitializing composite can directly nillify members and call their observers without iterating each member of each retainer.
		// Although (!) it may be too memory-consuming without real excuse, as this is the only case.
		//
		// And to contrast that two, there is idea where no special mechanism exist. This is no go, as changes can't be observed at all.

		TypeSP preIncrement() override {  // Prefix
			return shared_from_this();
		}

		TypeSP preDecrement() override {  // Prefix
			return shared_from_this();
		}

		TypeSP postIncrement(int) override {  // Postfix
			return SP<Type>();
		}

		TypeSP postDecrement(int) override {  // Postfix
			return SP<Type>();
		}

		TypeSP multiply(TypeSP type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::multiply(type);
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			return SP<PrimitiveType>(operator double()*primType->operator double());
		}

		TypeSP divide(TypeSP type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::divide(type);
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			return SP<PrimitiveType>(operator double()/primType->operator double());
		}

		bool equalsTo(const TypeSP& type) override {
			if(type->ID != TypeID::Primitive) {
				return Type::equalsTo(type);
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			switch(subID) {
				case PrimitiveTypeID::Boolean:	return any_cast<bool>(value) == primType->operator bool();
				case PrimitiveTypeID::Float:	return any_cast<double>(value) == primType->operator double();
				case PrimitiveTypeID::Integer:	return any_cast<int>(value) == primType->operator int();
				case PrimitiveTypeID::String:	return any_cast<string>(value) == primType->operator string();
				case PrimitiveTypeID::Type:		return acceptsA(primType) && conformsTo(primType);
				default:						return Type::equalsTo(type);
			}
		}
	};

	struct DictionaryType : Type {
		struct Hasher {
			size_t operator()(const TypeSP& t) const {
				return 0;  // TODO
			}
		};

		struct Comparator {
			bool operator()(const TypeSP& LHS, const TypeSP& RHS) const {
				return LHS == RHS || !LHS && !RHS || LHS && RHS && LHS->equalsTo(RHS);
			}
		};

		using Entry = pair<TypeSP, TypeSP>;

		TypeSP keyType,
			   valueType;
		unordered_map<TypeSP, vector<size_t>, Hasher, Comparator> kIndexes;	// key -> indexes
		vector<Entry> iEntries;												// index -> entry

		DictionaryType(const TypeSP& keyType = PredefinedEAnyTypeSP, const TypeSP& valueType = PredefinedEAnyTypeSP, bool concrete = false) : Type(TypeID::Dictionary, concrete), keyType(keyType), valueType(valueType) {}

		bool acceptsA(const TypeSP& type) override {
			if(type->ID == TypeID::Dictionary) {
				auto dictType = static_pointer_cast<DictionaryType>(type);

				return keyType->acceptsA(dictType->keyType) && valueType->acceptsA(dictType->valueType);
			}

			return false;
		}

		TypeSP normalized() override {
			return SP<DictionaryType>(keyType->normalized(), valueType->normalized());
		}

		string toString() const override {
			if(!concrete) {
				return "["+keyType->toString()+": "+valueType->toString()+"]";
			}

			string result = "[";
			auto it = begin();

			while(it != end()) {
				auto& [k, v] = *it;

				result += to_string(k)+": "+to_string(v);

				if(next(it) != end()) {
					result += ", ";
				}

				it++;
			}

			result += "]";

			return result;
		}

		bool equalsTo(const TypeSP& type) override {
			return false;  // TODO
		}

		void emplace(const TypeSP& key, const TypeSP& value, bool replace = false) {
			if(!concrete) {
				throw invalid_argument("Abstract dictionary type cannot contain values");
			}
			if(
				key && !key->conformsTo(keyType) ||
				!key && !PredefinedEVoidTypeSP->conformsTo(keyType)
			) {
				throw invalid_argument("'"+to_string(key)+"' does not conform to accepting key type '"+keyType->toString()+"'");
			}
			if(
				value && !value->conformsTo(valueType) ||
				!value && !PredefinedEVoidTypeSP->conformsTo(valueType)
			) {
				throw invalid_argument("'"+to_string(value)+"' does not conform to accepting value type '"+valueType->toString()+"'");
			}

			if(!replace) {
				kIndexes[key].push_back(size());
				iEntries.push_back(make_pair(key, value));
			} else
			if(auto it = kIndexes.find(key); it != kIndexes.end() && !it->second.empty()) {
				size_t keepIndex = it->second[0];
				iEntries[keepIndex].second = value;

				for(int j = static_cast<int>(it->second.size())-1; j >= 1; j--) {
					removeAtIndex(it->second[j]);
				}

				it->second.resize(1);
			}
		}

		void replace(const TypeSP& key, const TypeSP& value) {
			emplace(key, value, true);
		}

		void removeAtIndex(size_t idxToRemove) {
			iEntries.erase(begin()+idxToRemove);

			for(auto& [key, indexes] : kIndexes) {
				for(size_t& index : indexes) {
					if(index > idxToRemove) {
						index--;
					}
				}
			}
		}

		void removeFirst(const TypeSP& key) {
			auto it = kIndexes.find(key);

			if(it == kIndexes.end() || it->second.empty()) {
				return;
			}

			size_t idxToRemove = it->second.front();
			it->second.erase(it->second.begin());
			removeAtIndex(idxToRemove);

			if(it->second.empty()) {
				kIndexes.erase(it);
			}
		}

		void removeLast(const TypeSP& key) {
			auto it = kIndexes.find(key);

			if(it == kIndexes.end() || it->second.empty()) {
				return;
			}

			size_t idxToRemove = it->second.back();
			it->second.pop_back();
			removeAtIndex(idxToRemove);

			if(it->second.empty()) {
				kIndexes.erase(it);
			}
		}

		TypeSP get(const TypeSP& key) const {
			if(concrete) {
				auto it = kIndexes.find(key);

				if(it != kIndexes.end() && !it->second.empty()) {
					return iEntries[it->second.back()].second;
				}
			}

			return nullptr;
		}

		vector<Entry>::const_iterator begin() const {
			return iEntries.begin();
		}

		vector<Entry>::const_iterator end() const {
			return iEntries.end();
		}

		size_t size() const {
			return iEntries.size();
		}
	};

	struct CompositeType : Type {
		struct Retention {
			bool retained;  // This retained by another
			int retaining;  // Another retained by this
		};

		struct Observers {
			enum class Type {
				Member,
				Chain,
				Subscript
			} type;
			CompositeTypeSP willGet,
							get,
							didGet,
							willSet,
							set,
							didSet,
							willDelete,
							delete_,
							didDelete;
		};

		struct Overload {
			struct Modifiers {
				bool infix,
					 postfix,
					 prefix,

					 private_,
					 protected_,
					 public_,

					 implicit,
					 final,
					 lazy,
					 static_,
					 virtual_;
			} modifiers;
			TypeSP type,
				   value;
			Observers observers; // External access
		};

		using OverloadSP = sp<Overload>;
		using Member = deque<OverloadSP>;

		const CompositeTypeID subID;
		string title;
		const int ownID = composites.size();  // Assume that we won't have ID collisions (any composite must be pushed into global array after creation)
		unordered_map<string, CompositeTypeSP> hierarchy = {
			// Defaults are needed to distinguish between "not set" and "intentionally unset" states
			{"super", nullptr},
			{"Super", nullptr},
			{"self", nullptr},
			{"Self", nullptr},
			{"sub", nullptr},
			{"Sub", nullptr},
			{"scope", nullptr}
		};
		unordered_map<int, Retention> retentions;  // Another ID : Retention
		int life = 1;  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
		std::set<TypeSP> inheritedTypes;  // May be composite (class, struct, protocol), reference to, or function
		vector<TypeSP> genericParametersTypes;
		NodeArraySP statements;
		unordered_map<string, CompositeTypeSP> imports;
		unordered_map<string, Member> members;
		Observers chainObservers;  // Internal access
	//	unordered_map<FunctionTypeSP, Observers> subscriptObservers;  // External-internal access

		CompositeType(CompositeTypeID subID,
					  const string& title,
					  const std::set<TypeSP>& inheritedTypes = {},
					  const vector<TypeSP>& genericParametersTypes = {}) : Type(TypeID::Composite, true),
																		   subID(subID),
																		   title(title),
																		   inheritedTypes(inheritedTypes),
																		   genericParametersTypes(genericParametersTypes)
		{
			// TODO: Statically retain inherited types
		}

		std::set<TypeSP> getFullInheritanceChain() const;

		static bool checkConformance(const CompositeTypeSP& base, const CompositeTypeSP& candidate, const optional<vector<TypeSP>>& candidateGenericArgumentsTypes = {}) {
			if(candidate != base && !candidate->getFullInheritanceChain().contains(base)) {
				return false;
			}

			if(candidateGenericArgumentsTypes) {
				if(candidate->genericParametersTypes.size() != candidateGenericArgumentsTypes->size()) {
					return false;
				}
				for(int i = 0; i < candidate->genericParametersTypes.size(); i++) {
					if(!candidate->genericParametersTypes[i]->acceptsA(candidateGenericArgumentsTypes->at(i))) {
						return false;
					}
				}
			}

			return true;
		}

		bool acceptsA(const TypeSP& type) override;

		string toString() const override {
			string result;

			switch(subID) {
				case CompositeTypeID::Class:		result += "class";		break;
				case CompositeTypeID::Enumeration:	result += "enum";		break;
				case CompositeTypeID::Function:		result += "func";		break;
				case CompositeTypeID::Namespace:	result += "namespace";	break;
				case CompositeTypeID::Object:		result += "object";		break;
				case CompositeTypeID::Protocol:		result += "protocol";	break;
				case CompositeTypeID::Structure:	result += "structure";	break;
			}

			result += " "+title+"#"+std::to_string(ownID);

			if(!genericParametersTypes.empty()) {
				result += "<";

				for(int i = 0; i < genericParametersTypes.size(); i++) {
					if(i > 0) {
						result += ", ";
					}

					result += genericParametersTypes[i]->toString();
				}

				result += ">";
			}

			if(!inheritedTypes.empty()) {
				std::set<TypeSP> chain = getFullInheritanceChain();
				bool first = true;

				result += ": ";

				for(auto& type : chain) {
					if(!first) {
						result += ", ";
					}

					first = false;
					result += type->toString();
				}
			}

			return result;
		}

		operator double() const override {
			return ownID;
		}

		void destroy() {
			cout << "destroyComposite("+getTitle()+")" << endl;
			if(life == 2) {
				return;
			}

			life = 2;

			unordered_set<int> retainedIDs = getRetainedIDs();

			for(int retainedID : retainedIDs) {
				if(CompositeTypeSP retainedComposite = getComposite(retainedID)) {
					release(retainedComposite);

					/*  Let the objects destroy no matter of errors
					if(threw()) {
						return;
					}
					*/
				}
			}

			TypeSP self = shared_from_this();

			composites[ownID].reset();

			unordered_set<int> retainersIDs = getRetainersIDs();
			int aliveRetainers = 0;

			for(int retainerID : retainersIDs) {
				if(CompositeTypeSP retainingComposite = getComposite(retainerID); retainingComposite->life < 2) {
					aliveRetainers++;

					// TODO: Notify retainers about destroy
				}
			}

			if(aliveRetainers > 0) {
				report(1, nullptr, "Composite "+getTitle()+" was destroyed with a non-empty["+std::to_string(aliveRetainers)+"] retainer list.");
			}
			if(self.use_count() > 1) {
				report(1, nullptr, "Composite "+getTitle()+" was destroyed with a non-single["+std::to_string(self.use_count())+"] low-level reference count.");
			}
		}

		/**
		 * Destroys contextually released composite.
		 */
		void destroyReleased() {
			if(!retained()) {
				destroy();
			}
		}

		/**
		 * Directly retains composite.
		 *
		 * - Retaining composite's retained list gets the retained composite's retention counter increased.
		 * - Retained composite's retainers list gets the retaining composite's retention state set to true.
		 *
		 * Each entry will be added if not exists.
		 */
		void retain(CompositeTypeSP retainedComposite) {
			if(!retainedComposite || retainedComposite == shared_from_this()) {
				return;
			}

			int retainingID = ownID,
				retainedID = retainedComposite->ownID;
			auto& retainingRetentions = retentions,
				  retainedRetentions = retainedComposite->retentions;

			retainingRetentions[retainedID].retaining++;
			retainedRetentions[retainingID].retained = true;
		}

		/**
		 * Directly releases composite.
		 *
		 * - Retaining composite's retained list gets the retained composite's retention counter decreased.
		 * - Directly released composite's retainers list gets the retaining composite's retention state set to false.
		 *   Contextually released composite will be destroyed.
		 *   (release -> !retains -> destroyReleased -> !retained -> destroy)
		 *
		 * Each entry will be removed if redundant.
		 */
		void release(CompositeTypeSP retainedComposite) {
			if(!retainedComposite || retainedComposite == shared_from_this()) {
				return;
			}

			int retainingID = ownID,
				retainedID = retainedComposite->ownID;
			auto& retainingRetentions = retentions,
				  retainedRetentions = retainedComposite->retentions;
			auto retainingIt = retainingRetentions.find(retainedID),
				 retainedIt = retainedRetentions.find(retainingID);

			if(retainingIt != retainingRetentions.end() && retainingIt->second.retaining) {
				retainingIt->second.retaining--;

				if(!retainingIt->second.retained && !retainingIt->second.retaining) {
					retainingRetentions.erase(retainingIt);
				}
			}
			if(retainedIt != retainedRetentions.end() && !retains(retainedComposite)) {
				retainedIt->second.retained = false;

				if(!retainedIt->second.retaining) {
					retainedRetentions.erase(retainedIt);
				}

				retainedComposite->destroyReleased();
			}
		}

		/**
		 * Directly retains or releases composites found in values, basing on:
		 * - Current retention state - does not change if present in both values.
		 * - Order - first released, second retained.
		 *
		 * Check `getValueComposites()` for details of search.
		 */
		void retainOrRelease(TypeSP oldRetainedValue, TypeSP newRetainedValue) {
			auto ORC = getValueComposites(oldRetainedValue),  // Old/new retained composites
				 NRC = getValueComposites(newRetainedValue);

			for(auto oldRetainedComposite : ORC) if(!NRC.contains(oldRetainedComposite)) release(oldRetainedComposite);
			for(auto newRetainedComposite : NRC) if(!ORC.contains(newRetainedComposite)) retain(newRetainedComposite);
		}

		/**
		 * Directly retains composites found in value.
		 *
		 * Check `getValueComposites()` for details of search.
		 */
		void retain(TypeSP retainedValue) {
			for(auto retainedComposite : getValueComposites(retainedValue)) {
				retain(retainedComposite);
			}
		}

		/**
		 * Directly releases composites found in value.
		 *
		 * Check `getValueComposites()` for details of search.
		 */
		void release(TypeSP retainedValue) {
			for(auto retainedComposite : getValueComposites(retainedValue)) {
				release(retainedComposite);
			}
		}

		/**
		 * Returns a state of direct retention.
		 *
		 * Composite A is considered directly retained if it presents in composite B's retained IDs list.
		 *
		 * Formally this is the case when composite B uses composite A in its hierarchy, types,
		 * imports, members or observers, but technically retain and release functions must be utilized
		 * in corresponding places for this to work.
		 */
		bool retains(CompositeTypeSP retainedComposite) {
			if(life == 2 || !retainedComposite) {
				return false;
			}

			int retainedID = retainedComposite->ownID;

			return retentions.count(retainedID) && retentions[retainedID].retaining;
		}

		/**
		 * Returns a state of transitive retention.
		 *
		 * Composite A is considered transitively retained by composite B if they
		 * are same composite or if composite B directly retains it (recursively).
		 *
		 * visitedIDs is used to exclude a retain cycles and redundant passes
		 * from the lookup and is meant to be set internally only.
		 */
		bool retainsDistant(CompositeTypeSP retainedComposite, sp<unordered_set<int>> visitedIDs = SP<unordered_set<int>>()) {
			if(life == 2 || !retainedComposite) {
				return false;
			}
			if(retainedComposite == shared_from_this() || retains(retainedComposite)) {
				return true;
			}
			for(int retainedID : getRetainedIDs()) {
				if(!visitedIDs->contains(retainedID)) {
					visitedIDs->insert(retainedID);

					if(auto retainingComposite = getComposite(retainedID); retainingComposite->retainsDistant(retainedComposite, visitedIDs)) {
						return true;
					}
				}
			}

			return false;
		}

		/**
		 * Returns a state of contextual retention.
		 *
		 * Composite is considered contextually retained if it is transitively retained by
		 * the global namespace, a current scope composite or a current control trasfer value.
		 */
		bool retained() {
			CompositeTypeSP retainingComposite,
							retainedComposite = static_pointer_cast<CompositeType>(shared_from_this());

			if(retainingComposite = getValueComposite(controlTransfer().value))	if(retainingComposite->retainsDistant(retainedComposite)) return true;
			if(retainingComposite = scope())									if(retainingComposite->retainsDistant(retainedComposite)) return true;
		//	for(auto&& retainingComposite : scopes)								if(retainingComposite->retainsDistant(retainedComposite)) return true;  // May be useful when "with" syntax construct will be added
			if(retainingComposite = getComposite(0))							if(retainingComposite->retainsDistant(retainedComposite)) return true;

			return false;
		}

		CompositeTypeSP getHierarchy(const string& branch) {
			return hierarchy.contains(branch) ? hierarchy[branch] : nullptr;
		}

		void setHierarchy(const string& key, CompositeTypeSP value) {
			CompositeTypeSP OV = hierarchy[key],  // Old/new value
							NV = hierarchy[key] = value;

			if(OV == NV) {
				return;
			}

			bool self = tolower(key) == "self";

			if(NV && (!self || NV->ownID != ownID)) {  // Chains should not be cyclic, empty values can't create cycles
				auto composite = static_pointer_cast<CompositeType>(shared_from_this());
				std::set<CompositeTypeSP> composites;

				while(composite) {
					if(!composites.insert(composite).second) {
						hierarchy[key] = OV;  // TODO: Avoid accidental key creation

						return;
					}

					if(self && composite->hierarchy[key] == composite) {
						break;
					}

					composite = composite->hierarchy[key];
				}
			}

			retain(NV);
			release(OV);
		}

		bool removeHierarchy(const string& key) {
			if(auto keyNode = hierarchy.extract(key)) {
				release(move(keyNode.mapped()));

				return true;
			}

			return false;
		}

		bool hierarchyRetains(CompositeTypeSP retainedComposite) {
			for(const auto& [key, value] : hierarchy) {
				if(value == retainedComposite) {
					return true;
				}
			}

			return false;
		}

		bool isFunction() {
			return subID == CompositeTypeID::Function;
		}

		bool isNamespace() {
			return subID == CompositeTypeID::Namespace;
		}

		bool isObject() {
			return subID == CompositeTypeID::Object;
		}

		bool isInstantiable() {
			return subID == CompositeTypeID::Class ||
				   subID == CompositeTypeID::Structure;
		}

		bool isCallable() {
			return isFunction() ||
				   isInstantiable();
		}

		string getTitle() {
			return title.length() ? title : "#"+std::to_string(ownID);
		}

		CompositeTypeSP getSelfComposite() {
			return !isObject() ? static_pointer_cast<CompositeType>(shared_from_this()) : getInheritedTypeComposite();
		}

		CompositeTypeSP getScope() {
			return hierarchy["scope"];
		}

		unordered_set<int> getRetentionsIDs(bool direction) const {
			unordered_set<int> result;

			for(const auto& [key, value] : retentions) {
				if(direction ? value.retaining : value.retained) {
					result.insert(key);
				}
			}

			return result;
		}

		unordered_set<int> getRetainersIDs() const {
			return getRetentionsIDs(false);
		}

		unordered_set<int> getRetainedIDs() const {
			return getRetentionsIDs(true);
		}

		void setSelfLevels(CompositeTypeSP selfComposite) {
			setHierarchy("self", selfComposite);
			setHierarchy("Self", selfComposite->getSelfComposite());
		}

		void inheritLevels(CompositeTypeSP inheritedComposite) {
			if(inheritedComposite == shared_from_this()) {
				return;
			}

			for(const auto& [key, value] : hierarchy) {
				if(key != "scope") {
					setHierarchy(key, inheritedComposite ? inheritedComposite->getHierarchy(key) : nullptr);
				}
			}
		}

		void removeLevels() {
			/*
			for(auto it = hierarchy.cbegin(), next_it = it; it != hierarchy.cend(); it = next_it) {
				++next_it;

				if(removeHierarchy(it->first)) {
					hierarchy.erase(it);
				}
			}
			*/

			removeHierarchy("super");
			removeHierarchy("Super");
			removeHierarchy("self");
			removeHierarchy("Self");
			removeHierarchy("sub");
			removeHierarchy("Sub");
		}

		void setScope(CompositeTypeSP scopeComposite) {
			setHierarchy("scope", scopeComposite);
		}

		CompositeTypeSP getInheritedTypeComposite() {
			return inheritedTypes.size() ? getValueComposite(*inheritedTypes.begin()) : nullptr;
		}

		optional<reference_wrapper<Member>> getMember(const string& identifier) {
			auto it = members.find(identifier);

			if(it != members.end()) {
				return it->second;
			}

			return nullopt;
		}

		Member& addMember(const string& identifier) {
			return members[identifier];
		}

		void removeMember(const string& identifier) {
			if(auto memberNode = members.extract(identifier)) {
				Member member = move(memberNode.mapped());

				for(auto& overload : member) {
					release(overload->value);
				}
			}
		}

		void removeMembers() {
			for(auto& [identifier, member] : members) {
				removeMember(identifier);
			}
		}

		struct OverloadCandidate {
			CompositeTypeSP composite;
			OverloadSP overload;
			usize rank;

			bool operator<(const OverloadCandidate& candidate) const {
				return rank < candidate.rank;
			}

			bool operator==(const OverloadCandidate& candidate) const {
				return composite == candidate.composite &&
						overload == candidate.overload;
			}
		};

		struct OverloadSearch {
			deque<OverloadCandidate> candidates;
			optional<Observers> observers;
		};

		enum class AccessMode : u8 {
			Get,
			Set,
			Delete
		};

		/**
		 * TODO:
		 * - Rework acceptsA/conformsTo() to support ranged response: -1 == false, 0+ == count of an actions to find a conforming type (lower is better).
		 * - Match by overloads values or own types if getting/deleting, and by own types if setting or observers present.
		 * - Replace overload with overloads list and predicate with required type:
		 *   This function should create a ranged list using values of overloads and return overload with lowest range (but not -1) and
		 *   lowest index in the list (first found, if range is equal).
		 */
		optional<OverloadCandidate> matchOverload(
			OverloadSearch& search,
			AccessMode mode,
			optional<vector<TypeSP>> arguments = nullopt,
			TypeSP getType = nullptr,
			TypeSP setValue = nullptr
		) {
			if(!search.candidates.size()) {
				return nullopt;
			}

			search.candidates.erase(unique(search.candidates.begin(), search.candidates.end()), search.candidates.end());

			for(OverloadCandidate& candidate : search.candidates) {
				OverloadSP& overload = candidate.overload;
				TypeSP accepting,
					   accepted;

				switch(mode) {
					case AccessMode::Get:
						accepting = getType ?: SP<NillableType>(PredefinedEAnyTypeSP);
						accepted = overload->observers.get
								 ? overload->type
								 : overload->value ?: overload->type;
					break;
					case AccessMode::Set:
						accepting = overload->type;
						accepted = setValue ?: PredefinedEVoidTypeSP;
					break;
					case AccessMode::Delete:
						if(!overload->observers.delete_) {
							candidate.rank = -1;

							continue;
						}

						if(!arguments) {
							accepting = getType ?: SP<NillableType>(PredefinedEAnyTypeSP);
							accepted = overload->value ?: overload->type;
						} else {
							accepting = overload->value ?: overload->type;
							accepted = SP<FunctionType>(vector<TypeSP>(), arguments, SP<NillableType>(PredefinedEAnyTypeSP), FunctionType::Modifiers());
						}
					break;
				}

				candidate.rank = accepting->acceptsA(accepted);	// candidate.overload->observers->[g/s]et->returnType
			}

			return *min_element(search.candidates.begin(), search.candidates.end());
		}

		/**
		 * Looking for member overloads in a composite itself.
		 *
		 * Used to find member overloads (primarily of functions and namespaces),
		 * pseudovariables (such as "Global" or "self"), and imports within namespaces.
		 *
		 * Functions and namespaces are slightly different from other types. They can inherit or miss levels and
		 * eventually aren't inheritable nor instantiable themself, but they are still used as members storage
		 * and can participate in plain scope chains.
		 */
		Member findLocalOverloads(const string& identifier) {
			Member overloads;

			// Member

			if(auto member = getMember(identifier)) {
				auto m = member->get();

				overloads.insert(overloads.end(), m.begin(), m.end());
			}

			// Hierarchy

			auto hierarchy = this->hierarchy;

			hierarchy.erase("scope");						// Only object and inheritance chains
			hierarchy.emplace("Global", getComposite(0));	// Global-type
		//	hierarchy.emplace("metaSelf", -1);				   Self-object or a type (descriptor) (probably not a simple ID but some kind of proxy)
		//	hierarchy.emplace("arguments", -1);				   Function arguments array (should be in callFunction() if needed)

			if(hierarchy.contains(identifier)) {
				overloads.push_back(SP<Overload>(
					Overload::Modifiers { .implicit = true, .final = true },
					PredefinedCAnyTypeSP,
					hierarchy[identifier]
				));
			}

			// Imports

			if(isNamespace() && imports.contains(identifier)) {
				overloads.push_back(SP<Overload>(
					Overload::Modifiers { .implicit = true, .final = true },
					PredefinedCAnyTypeSP,
					imports[identifier]
				));
			}

		//	TODO:
		//	Search *inside* of imports should be implemented as a secondary stage after a _complete_ fail of the primary one (itself, object chain, composite chain / scope chain).
		//	If search did not fail completely (that said, some overloads with 0+ range was found), then imported namespaces lookup should not be done.

			return overloads;
		}

		void findSubscriptOverloads(OverloadSearch& search, AccessMode mode) {
			if(!search.candidates.size()) {
				return;
			}

			OverloadSearch search_;

			for(OverloadCandidate& candidate : search.candidates) {
				if(candidate.overload->observers.get) {
					continue;
				}

				if(TypeSP& value = candidate.overload->value) {
					switch(value->ID) {
						case TypeID::Dictionary:
							search_.candidates.push_back(candidate);
						break;
						case TypeID::Composite:
							auto compValue = static_pointer_cast<CompositeType>(value);

							compValue->findOverloads("subscript", search_, "scope", mode, false, true);
						break;
					}
				}
			}

			search.candidates = search_.candidates;
		}

		using VisitedBranches = unordered_map<CompositeTypeSP, unordered_set<string>>;

		/**
		 * Looking for member overloads in a composite itself and its hierarchy.
		 *
		 * Each "scope" is searched in the full order:
		 * - Descent: sub, Sub – only when virtual overloads are present.
		 * - Current – internally accompanies any other branch.
		 * - Ascent: self, super, Self, Super, scope (only when search is not internal).
		 *
		 * Branches are processed with the rules:
		 * - The order is well-defined and preserved.
		 * - For the current, local overloads and observers are added to the results.
		 * - For other, an attempt is made to recursively process the associated composite as the corresponding branch.
		 *
		 * Search stops immediately if any of the conditions met:
		 * - Observers of specified access mode are found.
		 * - Overloads while shadowing is enabled are found.
		 *
		 * The function automatically marks composites that have already been processed
		 * and/or do not require further processing as specific branches.
		 *
		 * NOTE:
		 * When accessing subscripts, we should collect all named overloads first without differentiating by their subscript support.
		 * Checking for subscript support should be done at ranking stage, including additional "subscript" search for composites.
		 * First overload candidates list should be transformed into another, containing only dictionaries and "subscript" overload candidates.
		 */
		void findOverloads(
			const string& identifier,
			OverloadSearch& search,
			const string& branch = "scope",
			AccessMode mode = AccessMode::Get,
			bool shadow = false,
			bool internal = false,
			sp<VisitedBranches> visited = SP<VisitedBranches>()
		) {
			auto composite = static_pointer_cast<CompositeType>(shared_from_this());

			if(!visited->contains(composite)) {
				visited->emplace(composite, unordered_set<string> { branch });
			} else
			if(!visited->at(composite).insert(branch).second) {
				return;
			}

			Member overloads = findLocalOverloads(identifier);
			bool hasVirtual = some(overloads, [](auto& v) { return v->modifiers.virtual_; });
			vector<string> branches = {""};

			if(branch == "scope") {
				if(hasVirtual) {
					branches.insert(branches.begin(),   "sub");
					branches.insert(branches.begin()+1, "Sub");
				}

				branches.push_back("self");
				branches.push_back("super");
				branches.push_back("Self");
				branches.push_back("Super");

				if(!internal) {
					branches.push_back("scope");
				}
			} else
			if(branch != "sub" && branch != "Sub") {
				branches.push_back(branch);
			} else
			if(hasVirtual) {
				branches.insert(branches.begin(), branch);
			}

			for(const string& b : branches) {
				if(b == "") {
					for(auto& overload : overloads) {
						search.candidates.push_back(OverloadCandidate(composite, overload));
					}

					if(mode == AccessMode::Get    && composite->chainObservers.get ||
					   mode == AccessMode::Set    && composite->chainObservers.set ||
					   mode == AccessMode::Delete && composite->chainObservers.delete_) {
						search.observers = composite->chainObservers;
					}
				} else
				if(CompositeTypeSP chainedComposite = composite->getHierarchy(b)) {
					if(chainedComposite == composite) {
						visited->at(composite).insert(b);

						continue;
					}

					chainedComposite->findOverloads(identifier, search, b, mode, shadow, internal, visited);
				} else {
					continue;
				}

				if(shadow && !search.candidates.empty() || search.observers) {
					return;
				}
			}
		}

		OverloadSP addOverload(
			const string& identifier,
			Overload::Modifiers modifiers = Overload::Modifiers(),
			TypeSP type = SP<NillableType>(PredefinedEAnyTypeSP),
			Observers observers = Observers()
		) {
			if(!type) {
				throw invalid_argument("Accepting type is nil");
			}

			Member& member = addMember(identifier);
			auto overload = SP<Overload>(modifiers, type, nullptr, observers);

			// TODO: Warn if equal overloads ranks are detected

			member.push_back(overload);

			return overload;

			/*
			if(
				value && !value->conformsTo(type) ||
				!value && !PredefinedEVoidTypeSP->conformsTo(type)
			) {
				throw invalid_argument("Value '"+to_string(value)+"' does not conform to accepting type '"+type->toString()+"'");
			}

			auto composite = static_pointer_cast<CompositeType>(shared_from_this());

			if(overload->modifiers.final) {
				throw invalid_argument("Overload is a 'final' constant");
			}

			TypeSP OT = overload->type ?: SP<Type>(),  // Old/new type
				   NT = type ?: SP<Type>(),
				   OV = overload->value,  // Old/new value
				   NV = value;

			overload->modifiers = modifiers;
			overload->type = NT;
			overload->value = value;

			if(OV != NV) {
				composite->retainOrRelease(OV, NV);
			}
			*/
		}

		TypeSP call(vector<TypeSP> arguments, TypeSP getType = nullptr) override;
	};

	struct ReferenceType : Type {
		CompositeTypeSP compType;
		optional<vector<TypeSP>> typeArgs;

		ReferenceType(const CompositeTypeSP& compType, const optional<vector<TypeSP>>& typeArgs = nullopt) : Type(TypeID::Reference), compType(compType), typeArgs(typeArgs) {}

		bool acceptsA(const TypeSP& type) override {
			if(type->ID == TypeID::Composite) {
				auto compType = static_pointer_cast<CompositeType>(type);

				return CompositeType::checkConformance(compType, compType, typeArgs);
			}
			if(type->ID == TypeID::Reference) {
				auto refType = static_pointer_cast<ReferenceType>(type);

				return CompositeType::checkConformance(compType, refType->compType, refType->typeArgs);
			}

			return false;
		}

		TypeSP normalized() override {
			if(!typeArgs) {
				return shared_from_this();
			}

			vector<TypeSP> normArgs;

			for(const TypeSP& arg : *typeArgs) {
				normArgs.push_back(arg->normalized());
			}

			return SP<ReferenceType>(compType, normArgs);
		}

		string toString() const override {
			string argsStr = "";

			if(typeArgs) {
				argsStr += "<";

				for(int i = 0; i < typeArgs->size(); i++) {
					if(i > 0) {
						argsStr += ", ";
					}

					argsStr += (*typeArgs)[i]->toString();
				}

				argsStr += ">";
			}

			return "Reference("+compType->getTitle()+argsStr+")";
		}
	};

	std::set<TypeSP> CompositeType::getFullInheritanceChain() const {
		auto chain = inheritedTypes;

		for(const TypeSP& parentType : inheritedTypes) {
			CompositeTypeSP compType;

			if(parentType->ID == TypeID::Composite) {
				compType = static_pointer_cast<CompositeType>(parentType);
			} else
			if(parentType->ID == TypeID::Reference) {
				compType = static_pointer_cast<ReferenceType>(parentType)->compType;
			}

			if(compType) {
				std::set<TypeSP> parentChain = compType->getFullInheritanceChain();

				chain.insert(parentChain.begin(), parentChain.end());
			}
		}

		return chain;
	}

	bool CompositeType::acceptsA(const TypeSP& type) {
		if(type->ID == TypeID::Composite) {
			auto compThis = static_pointer_cast<CompositeType>(shared_from_this());
			auto compType = static_pointer_cast<CompositeType>(type);

			return checkConformance(compThis, compType);
		}
		if(type->ID == TypeID::Reference) {
			auto compThis = static_pointer_cast<CompositeType>(shared_from_this());
			auto refType = static_pointer_cast<ReferenceType>(type);

			return checkConformance(compThis, refType->compType, refType->typeArgs);
		}

		return false;
	}

	// ----------------------------------------------------------------

	struct FunctionType : Type {
		vector<TypeSP> genericParametersTypes,
					   parametersTypes;
		TypeSP returnType;
		struct Modifiers {
			optional<bool> inits,
						   deinits,
						   awaits,
						   throws;
		} modifiers;

		FunctionType(const vector<TypeSP>& genericParametersTypes,
					 const vector<TypeSP>& parametersTypes,
					 const TypeSP& returnType,
					 const Modifiers& modifiers) : genericParametersTypes(genericParametersTypes),
												   parametersTypes(parametersTypes),
												   returnType(returnType),
												   modifiers(modifiers) {}

		static bool matchTypeLists(const vector<TypeSP>& expectedList, const vector<TypeSP>& providedList);

		bool acceptsA(const TypeSP& type) override {
			if(type->ID == TypeID::Composite) {
				// TODO: Compare with a composite's inherited function type
			} else
			if(type->ID == TypeID::Function) {
				auto funcType = static_pointer_cast<FunctionType>(type);

				return FunctionType::matchTypeLists(genericParametersTypes, funcType->genericParametersTypes) &&
					   FunctionType::matchTypeLists(parametersTypes, funcType->parametersTypes) &&
					   (!modifiers.inits || funcType->modifiers.inits == modifiers.inits) &&
					   (!modifiers.deinits || funcType->modifiers.deinits == modifiers.deinits) &&
					   (!modifiers.awaits || funcType->modifiers.awaits == modifiers.awaits) &&
					   (!modifiers.throws || funcType->modifiers.throws == modifiers.throws) &&
					   returnType->acceptsA(funcType->returnType);
			}

			return false;
		}

		TypeSP normalized() override {
			return SP<FunctionType>(
				transform(genericParametersTypes, [](const TypeSP& v) { return v->normalized(); }),
				transform(parametersTypes, [](const TypeSP& v) { return v->normalized(); }),
				returnType->normalized(),
				modifiers
			);
		}

		string toString() const override {
			string result;

			if(!genericParametersTypes.empty()) {
				result += "<";

				for(int i = 0; i < genericParametersTypes.size(); i++) {
					if(i > 0) {
						result += ", ";
					}

					result += genericParametersTypes[i]->toString();
				}

				result += ">";
			}

			result += "(";

			for(int i = 0; i < parametersTypes.size(); i++) {
				if(i > 0) {
					result += ", ";
				}

				result += parametersTypes[i]->toString();
			}

			result += ")";

			result += !modifiers.inits ? " inits?" : *modifiers.inits ? " inits" : "";
			result += !modifiers.deinits ? " deinits?" : *modifiers.deinits ? " deinits" : "";
			result += !modifiers.awaits ? " awaits?" : *modifiers.awaits ? " awaits" : "";
			result += !modifiers.throws ? " throws?" : *modifiers.throws ? " throws" : "";

			result += " -> "+returnType->toString();

			return result;
		}
	};

	struct InoutType : Type {
		using Observers = CompositeType::Observers;
		using OverloadSP = CompositeType::OverloadSP;
		using OverloadCandidate = CompositeType::OverloadCandidate;
		using OverloadSearch = CompositeType::OverloadSearch;
		using AccessMode = CompositeType::AccessMode;

		bool implicit;
		TypeSP innerType,
			   LHS,  // Dictionary/Composite/Inout
			   RHS;  // Primitive(string) for Composite
		bool internal;

		InoutType(bool implicit,
				  TypeSP& innerType,
				  TypeSP& LHS,
				  TypeSP& RHS,
				  bool internal) : Type(TypeID::Inout),
								   implicit(implicit),
								   innerType(innerType),
								   LHS(LHS),
								   RHS(RHS),
								   internal(internal) {}

		bool acceptsA(const TypeSP& type) override {
			return type->ID == TypeID::Inout && innerType->acceptsA(static_pointer_cast<InoutType>(type)->innerType);
		}

		TypeSP normalized() override {
			auto normInner = innerType->normalized();

			if(normInner->ID == TypeID::Inout) {
				return normInner;
			}

			return SP<InoutType>(implicit, normInner, LHS, RHS, internal);
		}

		string toString() const override {
			return "inout "+innerType->toString();  // TODO: Prepend "inout" only when explicit
		}

		// Direct access

		TypeSP get(TypeSP getType = nullptr) override {
			return LHS->get(RHS, nullopt, getType);
		}

		TypeSP call(vector<TypeSP> arguments, TypeSP getType = nullptr) override {
			if(TypeSP function = LHS->get(RHS, arguments, getType)) {
				return function->call(arguments, getType);
			}

			return nullptr;
		}

		void set(TypeSP setValue = nullptr) override {
			LHS->set(RHS, nullopt, setValue);
		}

		void delete_() {
			LHS->delete_(RHS, nullopt);
		}

		// Subscript access

		TypeSP get(vector<TypeSP> arguments, TypeSP getType = nullptr) override {
			return LHS->get(RHS, arguments, getType);
		}

		void set(vector<TypeSP> arguments, TypeSP setValue = nullptr) override {
			LHS->set(RHS, arguments, setValue);
		}

		void delete_(vector<TypeSP> arguments) override {
			LHS->delete_(RHS, arguments);
		}

		// Member access (support for inner inouts)

		TypeSP get(TypeSP key, optional<vector<TypeSP>> arguments, TypeSP getType = nullptr) override {
			if(TypeSP normLHS = normalizedLHS()) {
				return normLHS->get(key, arguments, getType);
			}

			return nullptr;
		}

		void set(TypeSP key, optional<vector<TypeSP>> arguments, TypeSP setValue = nullptr) override {
			if(TypeSP normLHS = normalizedLHS()) {
				normLHS->set(key, arguments, setValue);
			}
		}

		void delete_(TypeSP key, optional<vector<TypeSP>> arguments) override {
			if(TypeSP normLHS = normalizedLHS()) {
				normLHS->delete_(key, arguments);
			}
		}

		TypeSP normalizedLHS() {
			if(LHS && LHS->ID == TypeID::Inout) {
				return static_pointer_cast<InoutType>(LHS)->get();
			}

			return LHS;
		}

		/*
		TypeSP get(TypeSP getType = nullptr) override {
			return accessOverload(AccessMode::Get, internal, nullopt, false, getType);
		}

		TypeSP get(vector<TypeSP> arguments, bool subscript, TypeSP getType) override {
			return accessOverload(AccessMode::Get, internal, arguments, subscript, getType);
		}

		TypeSP set(TypeSP setValue = nullptr) override {
			return accessOverload(AccessMode::Set, internal, nullopt, false, nullptr, setValue);
		}

		TypeSP set(vector<TypeSP> arguments, bool subscript, TypeSP setValue) override {
			return accessOverload(AccessMode::Set, internal, arguments, subscript, nullptr, setValue);
		}

		TypeSP delete_() override {
			return accessOverload(AccessMode::Delete, internal);
		}

		TypeSP delete_(vector<TypeSP> arguments, bool subscript) override {
			return accessOverload(AccessMode::Delete, internal, arguments, subscript);
		}

		TypeSP call(vector<TypeSP> arguments, bool subscript, TypeSP getType) override {
			if(TypeSP type = accessOverload(AccessMode::Get, internal, arguments, false, getType)) {
				return type->call(arguments, false, getType);
			}
		}
		*/

		/**
		 * var a: A {
		 *		willGet
		 *			get -> A
		 *		 didGet
		 *
		 *		willSet(A)
		 *			set(A)
		 *		 didSet(A)
		 *
		 *		willDelete
		 *			delete
		 *		 didDelete
		 * }
		 *
		 * chain {
		 *		willGet(string)
		 *			get(string) -> _?
		 *		 didGet(string)
		 *
		 *		willSet(string, _?)
		 *			set(string, _?)
		 *		 didSet(string, _?)
		 *
		 *		willDelete(string)
		 *			delete(string)
		 *		 didDelete(string)
		 * }
		 *
		 * subscript(A, B) -> C {
		 *		willGet(A, B)
		 *			get(A, B) -> C
		 *		 didGet(A, B)
		 *
		 *		willSet(A, B, C)
		 *			set(A, B, C)
		 *		 didSet(A, B, C)
		 *
		 *		willDelete(A, B)
		 *			delete(A, B)
		 *		 didDelete(A, B)
		 * }
		 */
		TypeSP callObservers(const Observers& observers, OverloadSP overload, AccessMode mode, vector<TypeSP> arguments, TypeSP setValue = nullptr) {
			TypeSP value;

			switch(mode) {
				case AccessMode::Get:
					if(observers.willGet) {
						observers.willGet->call(arguments);
					}
					if(observers.get) {
						value = observers.get->call(arguments);
					} else
					if(overload) {
						value = overload->value;
					}
					if(observers.didGet) {
						observers.didGet->call(arguments);
					}
				break;
				case AccessMode::Set:
					arguments.push_back(setValue);

					if(observers.willSet) {
						observers.willSet->call(arguments);
					}
					if(observers.set) {
						observers.set->call(arguments);
					} else
					if(overload) {
						overload->value = setValue;
					}
					if(observers.didSet) {
						observers.didSet->call(arguments);
					}
				break;
				case AccessMode::Delete:
					if(observers.willDelete) {
						observers.willDelete->call(arguments);
					}
					if(observers.delete_) {
						observers.delete_->call(arguments);
					}
					if(observers.didDelete) {
						observers.didDelete->call(arguments);
					}
				break;
			}

			return value;
		}

		TypeSP accessOverload(
			AccessMode mode,
			bool internal = false,
			optional<vector<TypeSP>> arguments = nullopt,
			bool subscript = false,
			TypeSP getType = nullptr,
			TypeSP setValue = nullptr
		) {
			OverloadSearch search;

			LHS->findOverloads(RHS, search, "scope", mode, !arguments && !subscript, internal);

			if(subscript) {
				LHS->findSubscriptOverloads(search, mode);
			}

			optional<OverloadCandidate> candidate = LHS->matchOverload(search, mode, arguments, getType, setValue);

			if(candidate) {
				return callObservers(candidate->overload->observers, candidate->overload, mode, arguments ? *arguments : vector<TypeSP>(), setValue);
			} else
			if(search.observers) {
				return callObservers(*search.observers, nullptr, mode, vector<TypeSP> { SP<PrimitiveType>(RHS) }, setValue);
			}

			return nullptr;
		}
	};

	TypeSP CompositeType::call(vector<TypeSP> arguments, TypeSP getType) {
		if(!isCallable()) {
			throw invalid_argument("Composite is not callable");
		}
		if(subID != CompositeTypeID::Function) {
			return SP<InoutType>(true, SP<NillableType>(PredefinedEAnyTypeSP), shared_from_this(), SP<PrimitiveType>("init"), true)->call(arguments, getType);
		}

		return nullptr;
	}

	struct VariadicType : Type {
		TypeSP innerType;

		VariadicType(const TypeSP& innerType = nullptr) : Type(TypeID::Variadic), innerType(innerType) {}

		bool acceptsA(const TypeSP& type) override {
			if(!innerType) {
				return true;
			}
			if(type->ID != TypeID::Variadic) {
				return innerType->acceptsA(type);
			}

			auto varType = static_pointer_cast<VariadicType>(type);

			if(!varType->innerType) {
				return innerType->acceptsA(PredefinedEVoidTypeSP);
			}

			return innerType->acceptsA(varType->innerType);
		}

		TypeSP normalized() override {
			if(!innerType) {
				return shared_from_this();
			}

			return SP<VariadicType>(innerType->normalized());
		}

		string toString() const override {
			return (innerType ? innerType->toString() : "")+"...";
		}
	};

	bool FunctionType::matchTypeLists(const vector<TypeSP>& expectedList, const vector<TypeSP>& providedList) {
		int expectedSize = expectedList.size(),
			providedSize = providedList.size();

		function<bool(int, int)> matchFrom = [&](int expectedIndex, int providedIndex) {
			if(expectedIndex == expectedSize) {
				return providedIndex == providedSize;
			}

			const TypeSP& expectedType = expectedList[expectedIndex];

			if(expectedType->ID == TypeID::Variadic) {
				auto varExpectedType = static_pointer_cast<VariadicType>(expectedType);

				if(expectedIndex == expectedSize-1) {
					return all_of(providedList.begin()+providedIndex, providedList.end(), [&](const TypeSP& t) {
						return varExpectedType->acceptsA(t);
					});
				}

				if(matchFrom(expectedIndex+1, providedIndex)) {
					return true;
				}

				for(int currentIndex = providedIndex; currentIndex < providedSize; currentIndex++) {
					if(!varExpectedType->acceptsA(providedList[currentIndex])) {
						break;
					}
					if(matchFrom(expectedIndex+1, currentIndex+1)) {
						return true;
					}
				}

				return false;
			}

			if(providedIndex < providedSize && expectedType->acceptsA(providedList[providedIndex])) {
				return matchFrom(expectedIndex+1, providedIndex+1);
			}

			return false;
		};

		return matchFrom(0, 0);
	}

	// ----------------------------------------------------------------

	const TypeSP PredefinedEVoidTypeSP = SP<PredefinedType>(PredefinedTypeID::EVoid, [](const TypeSP& type) {
		return type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::EVoid;
	});

	const TypeSP PredefinedEAnyTypeSP = SP<PredefinedType>(PredefinedTypeID::EAny, [](const TypeSP& type) {
		return true;
	});

	const TypeSP PredefinedPAnyTypeSP = SP<PredefinedType>(PredefinedTypeID::PAny, [](const TypeSP& type) {
		return type->ID == TypeID::Primitive;
	});

	const TypeSP PredefinedPBooleanTypeSP = SP<PredefinedType>(PredefinedTypeID::PBoolean, [](const TypeSP& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Boolean;
	});

	const TypeSP PredefinedPDictionaryTypeRef = SP<PredefinedType>(PredefinedTypeID::PDictionary, [](const TypeSP& type) {
		return type->ID == TypeID::Dictionary;
	});

	const TypeSP PredefinedPFloatTypeSP = SP<PredefinedType>(PredefinedTypeID::PFloat, [](const TypeSP& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Float;
	});

	const TypeSP PredefinedPIntegerTypeSP = SP<PredefinedType>(PredefinedTypeID::PInteger, [](const TypeSP& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Integer;
	});

	const TypeSP PredefinedPStringTypeSP = SP<PredefinedType>(PredefinedTypeID::PString, [](const TypeSP& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::String;
	});

	const TypeSP PredefinedPTypeTypeSP = SP<PredefinedType>(PredefinedTypeID::PType, [](const TypeSP& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Type;
	});

	const TypeSP PredefinedCAnyTypeSP = SP<PredefinedType>(PredefinedTypeID::CAny, [](const TypeSP& type) {
		return type->ID == TypeID::Composite ||
			   type->ID == TypeID::Reference;
	});

	const TypeSP PredefinedCClassTypeSP = SP<PredefinedType>(PredefinedTypeID::CClass, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Class ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Class;
	});

	const TypeSP PredefinedCEnumerationTypeSP = SP<PredefinedType>(PredefinedTypeID::CEnumeration, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Enumeration ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Enumeration;
	});

	const TypeSP PredefinedCFunctionTypeSP = SP<PredefinedType>(PredefinedTypeID::CFunction, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Function ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Function;
	});

	const TypeSP PredefinedCNamespaceTypeSP = SP<PredefinedType>(PredefinedTypeID::CNamespace, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Namespace ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Namespace;
	});

	const TypeSP PredefinedCObjectTypeSP = SP<PredefinedType>(PredefinedTypeID::CObject, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Object ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Object;
	});

	const TypeSP PredefinedCProtocolTypeSP = SP<PredefinedType>(PredefinedTypeID::CProtocol, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Protocol ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Protocol;
	});

	const TypeSP PredefinedCStructureTypeSP = SP<PredefinedType>(PredefinedTypeID::CStructure, [](const TypeSP& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Structure ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Structure;
	});

	// ----------------------------------------------------------------

	CompositeTypeSP createComposite(const string& title, CompositeTypeID type, CompositeTypeSP scope = nullptr) {
		cout << "createComposite("+title+")" << endl;
		auto composite = SP<CompositeType>(type, title);

		composites.push_back(composite);

		if(scope) {
			composite->setScope(scope);
		}

		return composite;
	}

	/**
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or unset.
	 * If levels are intentionally unset, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	CompositeTypeSP createNamespace(const string& title, CompositeTypeSP scope = nullptr, optional<CompositeTypeSP> levels = nullptr) {
		CompositeTypeSP namespace_ = createComposite(title, CompositeTypeID::Namespace, scope);

		if(levels && *levels) {
			namespace_->inheritLevels(*levels);
		} else
		if(!levels) {
			namespace_->removeLevels();
		} else {
			namespace_->setSelfLevels(namespace_);
		}

		return namespace_;
	}

	string getTitle(CompositeTypeSP composite) {
		return composite ? composite->getTitle() : "";
	}

	/*
	bool compositeIsFunction(const CompositeTypeSP& composite) {
		return composite && composite->isFunction();
	}

	bool compositeIsNamespace(const CompositeTypeSP& composite) {
		return composite && composite->isNamespace();
	}

	bool compositeIsObject(const CompositeTypeSP& composite) {
		return composite && composite->isObject();
	}

	bool compositeIsInstantiable(const CompositeTypeSP& composite) {
		return composite && composite->isInstantiable();
	}

	bool compositeIsCallable(const CompositeTypeSP& composite) {
		return composite && composite->isCallable();
	}
	*/

	CompositeTypeSP getScope(int offset = 0) {
		return scopes.size() > scopes.size()-1+offset
			 ? scopes[scopes.size()-1+offset]
			 : nullptr;
	}

	CompositeTypeSP scope() {
		return getScope();
	}

	/**
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	void addScope(CompositeTypeSP composite) {
		scopes.push_back(composite);
	}

	/**
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
		CompositeTypeSP composite = scopes.back();

		scopes.pop_back();

		if(destroy && composite) {
			composite->destroyReleased();
		}
	}

	/**
	 * Returns result of identifier(value) call or plain value if no appropriate function found in current scope.
	 */
	TypeSP getValueWrapper(const TypeSP& value, string identifier) {
		// TODO

		return value;
	}

	TypeSP getValueWrapper(const PrimitiveType& value, string identifier) {
		return getValueWrapper(SP(value), identifier);
	}

	/**
	 * Returns a live composite found in value. Works with `CompositeType`.
	 */
	CompositeTypeSP getValueComposite(TypeSP value) {
		if(value && value->ID == TypeID::Composite) {
			auto compValue = static_pointer_cast<CompositeType>(value);

			if(compValue->life < 2) {
				return compValue;
			} else {
				report(1, nullptr, "Cannot (and shouldn't) access composite in deinitialization state");
			}
		}

		return nullptr;
	}

	/**
	 * Returns a set of live composites found in value (recursively). Works with `CompositeType` and `DictionaryType`.
	 */
	unordered_set<CompositeTypeSP> getValueComposites(TypeSP value) {
		unordered_set<CompositeTypeSP> result;

		if(value && value->ID == TypeID::Dictionary) {
			for(auto& [key, value_] : *static_pointer_cast<DictionaryType>(value)) {
				auto keyComposites = getValueComposites(key),
					 valueComposites = getValueComposites(value_);

				result.insert(keyComposites.begin(), keyComposites.end());
				result.insert(valueComposites.begin(), valueComposites.end());
			}
		} else
		if(CompositeTypeSP composite = getValueComposite(value)) {
			result.insert(composite);
		}

		return result;
	}

	// ----------------------------------------------------------------

	template<typename... Args>
	TypeSP rules(const string& type, NodeSP n = nullptr, Args... args);

	template<typename... Args>
	TypeSP executeNode(NodeSP node, bool concrete = true, Args... arguments) {
		if(!node) {
			return nullptr;
		}

		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		TypeSP value;

		position = NP;

		try {
			value = rules(node->get("type"), node, arguments...);
		} catch(exception& e) {
			value = SP<PrimitiveType>(e.what());

			setControlTransfer(value, "throw");
		//	report(2, node, to_string(value));
		}

		if(value && value->concrete != concrete) {  // Anonymous protocols are _now_ concrete, but this may change in the future
			value = nullptr;
		}

		position = OP;

		return value;
	}

	/**
	 * Last statement in a body will be treated like a local return (overwritable by subsequent
	 * outer statements) if there was no explicit control transfer previously. ECT should
	 * be implemented by rules.
	 *
	 * Control transfer result can be discarded (fully - 1, value only - 0).
	 */
	void executeNodes(NodeArraySP nodes) {
		int OP = position;  // Old position

		for(const NodeSP& node : nodes ? *nodes : NodeArray()) {
			int NP = node->get<Node&>("range").get("start");  // New position
			TypeSP value = executeNode(node);

			if(node == nodes->back().get<NodeSP>() && !controlTransfer().type) {  // Implicit control transfer
				setControlTransfer(value);
			}

			position = NP;  // Consider deinitializers

			if(controlTransfer().type) {  // Explicit control transfer is done
				break;
			}
		}

		position = OP;
	}

	any any_rules(const string& type, NodeSP n) {
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
	TypeSP rules(const string& type, NodeSP n, Args... args) {
		vector<any> arguments = {args...};

		if(
			type != "module" &&
			type != "nilLiteral" &&
			!n
		) {
			throw invalid_argument("The rule type ("+type+") is not in exceptions list for corresponding node to be present but null pointer is passed");
		}

		if(type == "arrayLiteral") {
			auto value = SP<DictionaryType>(
				PredefinedPIntegerTypeSP,
				SP<NillableType>(PredefinedEAnyTypeSP),
				true
			);

			for(int i = 0; i < n->get<NodeArray&>("values").size(); i++) {
				auto value_ = executeNode(n->get<NodeArray&>("values")[i]);

				if(value_) {
					value->emplace(SP<PrimitiveType>(i), value_);  // TODO: Copy or link value in accordance to type
				}
			}

			return getValueWrapper(value, "Array");
		} else
		if(type == "arrayType") {
			TypeSP valueType = executeNode(n->get("value"), false) ?: SP<NillableType>(PredefinedEAnyTypeSP);
			CompositeTypeSP composite = getValueComposite(nullptr/*findOverload(scope, "Array")?.value*/);

			if(composite) {
				return SP<ReferenceType>(composite, vector<TypeSP> { valueType });  // TODO: Check if type accepts passed generic argument
			} else {
				return SP<DictionaryType>(PredefinedPIntegerTypeSP, valueType);
			}
		} else
		if(type == "booleanLiteral") {
			return getValueWrapper(n->get("value") == "true", "Boolean");
		} else
		if(type == "breakStatement") {
			TypeSP value;

			if(!n->empty("label")) {
				value = SP<PrimitiveType>(n->get<Node&>("label").get<string>("value"));
			}

			setControlTransfer(value, "break");

			return value;
		} else
		if(type == "continueStatement") {
			TypeSP value;

			if(!n->empty("label")) {
				value = SP<PrimitiveType>(n->get<Node&>("label").get<string>("value"));
			}

			setControlTransfer(value, "continue");

			return value;
		} else
		if(type == "dictionaryLiteral") {
			auto value = SP<DictionaryType>(
				SP<NillableType>(PredefinedEAnyTypeSP),
				SP<NillableType>(PredefinedEAnyTypeSP),
				true
			);

			for(const NodeSP& entry : n->get<NodeArray&>("entries")) {
				auto entry_ = any_optcast<DictionaryType::Entry>(any_rules("entry", entry));

				if(entry_) {
					value->emplace(entry_->first, entry_->second);  // TODO: Copy or link value in accordance to type
				}
			}

			return getValueWrapper(value, "Dictionary");
		} else
		if(type == "dictionaryType") {
			TypeSP keyType = executeNode(n->get("key"), false) ?: SP<NillableType>(PredefinedEAnyTypeSP),
					valueType = executeNode(n->get("value"), false) ?: SP<NillableType>(PredefinedEAnyTypeSP);
			CompositeTypeSP composite = getValueComposite(nullptr/*findOverload(scope, "Dictionary")?.value*/);

			if(composite) {
				return SP<ReferenceType>(composite, vector<TypeSP> { keyType, valueType });  // TODO: Check if type accepts passed generic arguments
			} else {
				return SP<DictionaryType>(keyType, valueType);
			}
		} else
		if(type == "floatLiteral") {
			return getValueWrapper(n->get<double>("value"), "Float");
		} else
		if(type == "identifier") {
			/*
			string identifier = n->get("value");
			auto overload = scope()->findOverload(identifier).overload;

			if(!overload) {
				report(1, n, "Member overload wasn't found (accessing '"+identifier+"').");

				return nullptr;
			}

			return overload->value;
			*/

			return SP<InoutType>(true, SP<NillableType>(PredefinedEAnyTypeSP), scope(), n->get("value"), false);
		} else
		if(type == "ifStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "if var a = b() {}"? That isn't even implemented in the parser right now
			CompositeTypeSP namespace_ = createNamespace("Local<"+getTitle(scope())+", If>", scope(), nullopt);

			addScope(namespace_);

			auto conditionValue = executeNode(n->get("condition"));
			bool condition = conditionValue && *conditionValue,
				 elseif = !n->empty("else") && n->get<Node&>("else").get("type") == "ifStatement";

			if(condition || !elseif) {
				NodeSP branch = n->get(condition ? "then" : "else");

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
			executeNodes(n->get<NodeArraySP>("statements")/*, (t) => t !== 'throw' ? 0 : -1*/);

			ControlTransfer& CT = controlTransfer();

			if(CT.type || CT.value) {
				report(threw() ? 2 : 0, n, to_string(controlTransfer().value));
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
					value->set(value->positive()->plus(SP<PrimitiveType>(1)));

					return value->minus(SP<PrimitiveType>(1));
				}
				if(n->get<Node&>("operator").get("value") == "--") {
					value->set(value->positive()->minus(SP<PrimitiveType>(1)));

					return value->plus(SP<PrimitiveType>(1));
				}
			}

			// TODO: Dynamic operators lookup, check for values mutability, observers notification

			return value;
		} else
		if(type == "predefinedType") {
			string predefinedTypeTitle = n->get("value");

			if(predefinedTypeTitle == "void")			return PredefinedEVoidTypeSP;
			if(predefinedTypeTitle == "_")				return PredefinedEAnyTypeSP;

			if(predefinedTypeTitle == "any")			return PredefinedPAnyTypeSP;
			if(predefinedTypeTitle == "bool")			return PredefinedPBooleanTypeSP;
			if(predefinedTypeTitle == "dict")			return PredefinedPDictionaryTypeRef;
			if(predefinedTypeTitle == "float")			return PredefinedPFloatTypeSP;
			if(predefinedTypeTitle == "int")			return PredefinedPIntegerTypeSP;
			if(predefinedTypeTitle == "string")			return PredefinedPStringTypeSP;
			if(predefinedTypeTitle == "type")			return PredefinedPTypeTypeSP;

			if(predefinedTypeTitle == "Any")			return PredefinedCAnyTypeSP;
			if(predefinedTypeTitle == "Class")			return PredefinedCClassTypeSP;
			if(predefinedTypeTitle == "Enumeration")	return PredefinedCEnumerationTypeSP;
			if(predefinedTypeTitle == "Function")		return PredefinedCFunctionTypeSP;
			if(predefinedTypeTitle == "Namespace")		return PredefinedCNamespaceTypeSP;
			if(predefinedTypeTitle == "Object")			return PredefinedCObjectTypeSP;
			if(predefinedTypeTitle == "Protocol")		return PredefinedCProtocolTypeSP;
			if(predefinedTypeTitle == "Structure")		return PredefinedCStructureTypeSP;

			return PredefinedEAnyTypeSP;
		} else
		if(type == "prefixExpression") {
			auto value = executeNode(n->get("value"));

			if(!value) {
				return nullptr;
			}

			if(value->ID == TypeID::Primitive && !n->empty("operator") ) {
				if(n->get<Node&>("operator").get("value") == "!") {
					return SP<PrimitiveType>(value->not_());
				}
				if(n->get<Node&>("operator").get("value") == "-") {
					return value->negative();
				}
				if(n->get<Node&>("operator").get("value") == "++") {
					value->set(value->positive()->plus(SP<PrimitiveType>(1)));

					return value;
				}
				if(n->get<Node&>("operator").get("value") == "--") {
					value->set(value->positive()->minus(SP<PrimitiveType>(1)));

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

			return value;
		} else
		if(type == "stringLiteral") {
			::string string = "";

			for(const NodeSP& segment : n->get<NodeArray&>("segments")) {
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

			return SP<PrimitiveType>(type);
		} else
		if(type == "typeIdentifier") {
			CompositeTypeSP composite = getValueComposite(executeNode(n->get("identifier")));

			if(!composite || composite->isObject()) {
				report(1, n, "Composite is an object or wasn\'t found.");

				return PredefinedCAnyTypeSP;
			} else {
				vector<TypeSP> genericArguments;

				for(const NodeSP& type : n->get<NodeArray&>("genericArguments")) {
					auto type_ = executeNode(type, false);

					if(type_) {
						genericArguments.push_back(type_);
					}
				}

				return SP<ReferenceType>(composite, genericArguments);  // TODO: Check if type accepts passed generic arguments
			}
		} else
		if(type == "variableDeclaration") {
			NodeArraySP modifiers = n->get("modifiers");

			for(Node& declarator : n->get<NodeArray&>("declarators")) {
				string identifier = declarator.get<Node&>("identifier").get("value");
				TypeSP type = executeNode(declarator.get("type_"), false),
					   value;

			//	addContext({ type: type }, ['implicitChainExpression']);

				value = executeNode(declarator.get("value"));

			//	removeContext();

				scope()->addOverload(identifier, CompositeType::Overload::Modifiers(), type);
			//	scope()->accessOverload(identifier, AccessMode::Set, true, nullopt, false, nullptr, value);
			}
		} else
		if(type == "whileStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "while var a = b() {}"? That isn't even implemented in the parser right now
			CompositeTypeSP namespace_ = createNamespace("Local<"+getTitle(scope())+", While>", scope(), nullopt);

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

	// ----------------------------------------------------------------

	struct Place {
		NodeSP node,
				location;
	};

	void reset(optional<Preferences> preferences_ = nullopt) {
		tokens = {};
		tree = nullptr;
		position = 0;
		composites = {};
		scopes = {};
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

		executeNode(tree);

		result.reports = move(reports);

		reset();

		return result;
	}
};