#pragma once

#include "Parser.cpp"

namespace Interpreter {
	using Location = Lexer::Location;
	using Token = Lexer::Token;
	using Report = Parser::Report;

	deque<Token> tokens;
	NodeRef tree;
	int position;
	struct Preferences {
		int callStackSize,
			allowedReportLevel,
			metaprogrammingLevel;
		bool arbitaryPrecisionArithmetics;
	} preferences;
	deque<Report> reports;

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

	using TypeRef = shared_ptr<Type>;

	using ParenthesizedTypeRef = shared_ptr<ParenthesizedType>;
	using NillableTypeRef = shared_ptr<NillableType>;
	using DefaultTypeRef = shared_ptr<DefaultType>;
	using UnionTypeRef = shared_ptr<UnionType>;
	using IntersectionTypeRef = shared_ptr<IntersectionType>;

	using PredefinedTypeRef = shared_ptr<PredefinedType>;
	using PrimitiveTypeRef = shared_ptr<PrimitiveType>;
	using DictionaryTypeRef = shared_ptr<DictionaryType>;
	using CompositeTypeRef = shared_ptr<CompositeType>;
	using ReferenceTypeRef = shared_ptr<ReferenceType>;

	using FunctionTypeRef = shared_ptr<FunctionType>;
	using InoutTypeRef = shared_ptr<InoutType>;
	using VariadicTypeRef = shared_ptr<VariadicType>;

	// ----------------------------------------------------------------

	enum class TypeID : uint8_t {
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

	enum class PredefinedTypeID : uint8_t {
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

	enum class PrimitiveTypeID : uint8_t {
		Boolean,
		Float,
		Integer,
		String,
		Type
	};

	enum class CompositeTypeID : uint8_t {
		Class,
		Enumeration,
		Function,
		Namespace,
		Object,
		Protocol,
		Structure
	};

	// ----------------------------------------------------------------

	extern const TypeRef PredefinedEVoidTypeRef;
	extern const TypeRef PredefinedEAnyTypeRef;

	extern const TypeRef PredefinedPAnyTypeRef;
	extern const TypeRef PredefinedPBooleanTypeRef;
	extern const TypeRef PredefinedPFloatTypeRef;
	extern const TypeRef PredefinedPIntegerTypeRef;
	extern const TypeRef PredefinedPStringTypeRef;
	extern const TypeRef PredefinedPTypeTypeRef;

	extern const TypeRef PredefinedCAnyTypeRef;
	extern const TypeRef PredefinedCClassTypeRef;
	extern const TypeRef PredefinedCEnumerationTypeRef;
	extern const TypeRef PredefinedCFunctionTypeRef;
	extern const TypeRef PredefinedCNamespaceTypeRef;
	extern const TypeRef PredefinedCObjectTypeRef;
	extern const TypeRef PredefinedCProtocolTypeRef;
	extern const TypeRef PredefinedCStructureTypeRef;

	// ----------------------------------------------------------------

	deque<CompositeTypeRef> composites;  // Global composite storage is allowed, as it decided by design to have only one Interpreter instance in a single process memory
	deque<CompositeTypeRef> scopes;

	CompositeTypeRef getComposite(NodeValue ID) {
		return ID.type() == 2 && (int)ID < composites.size() ? composites[(int)ID] : nullptr;
	}

	CompositeTypeRef scope();
	CompositeTypeRef getValueComposite(TypeRef value);
	unordered_set<CompositeTypeRef> getValueComposites(TypeRef value);

	// ----------------------------------------------------------------

	struct ControlTransfer {
		TypeRef value;
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

	// ----------------------------------------------------------------

	struct Type : enable_shared_from_this<Type> {
		const TypeID ID;
		const bool concrete;

		Type(TypeID ID = TypeID::Undefined, bool concrete = false) : ID(ID), concrete(concrete) { /*cout << "Type created\n";*/ }
		virtual ~Type() { /*cout << "Type destroyed\n";*/ }

		virtual bool acceptsA(const TypeRef& type) { return false; }
		virtual bool conformsTo(const TypeRef& type) { return type->acceptsA(shared_from_this()); }
		virtual TypeRef normalized() { return shared_from_this(); }
		virtual string toString() const { return string(); }  // User-friendly representation

		virtual operator bool() const { return bool(); }
		virtual operator double() const { return double(); }
		virtual operator int() const { return operator double(); }
		virtual operator string() const { return string(); }  // Machine-friendly representation
		virtual TypeRef operator=(TypeRef type) { return shared_from_this(); }
		virtual TypeRef operator+() const { return Ref<Type>(); }
		virtual TypeRef operator-() const { return Ref<Type>(); }
		virtual TypeRef operator+(TypeRef type) const { return Ref<Type>(); }
		virtual TypeRef operator-(TypeRef type) const { return operator+(static_pointer_cast<Type>(type)->operator-()); }
		virtual TypeRef operator++() { return shared_from_this(); }  // Prefix
		virtual TypeRef operator--() { return shared_from_this(); }  // Prefix
		virtual TypeRef operator++(int) { return Ref<Type>(); }  // Postfix
		virtual TypeRef operator--(int) { return Ref<Type>(); }  // Postfix
		virtual TypeRef operator*(TypeRef type) const { return Ref<Type>(); }
		virtual TypeRef operator/(TypeRef type) const { return Ref<Type>(); }
		virtual bool operator!() const { return !operator bool(); }
		virtual bool operator==(const TypeRef& type) { return acceptsA(type) && conformsTo(type); }
		virtual bool operator!=(const TypeRef& type) { return !operator==(type); }


		// Now we are planning to normalize all types for one single time before subsequent comparisons, or using this/similar to this function at worst case, as this is performant-costly operation in C++
		// One thing that also should be mentioned here is that types can't be normalized without losing their initial string representation at the moment
		static bool acceptsANormalized(const TypeRef& left, const TypeRef& right) {
			return left && left->acceptsA(right ? right->normalized() : PredefinedEVoidTypeRef);
		}

		static bool acceptsAConcrete(const TypeRef& left, const TypeRef& right) {
			return left && right && right->concrete && left->acceptsA(right);
		}
	};

	static string to_string(const TypeRef& type, bool raw = false) {
		return type ? (raw ? type->operator string() : type->toString()) : "nil";  // std::to_string(type.use_count());
	}

	// ----------------------------------------------------------------

	struct ParenthesizedType : Type {
		TypeRef innerType;

		ParenthesizedType(TypeRef type) : Type(TypeID::Parenthesized), innerType(move(type)) { cout << "(" << innerType->toString() << ") type created\n"; }
		~ParenthesizedType() { cout << "(" << innerType->toString() << ") type destroyed\n"; }

		bool acceptsA(const TypeRef& type) override {
			return innerType->acceptsA(type);
		}

		TypeRef normalized() override {
			return innerType->normalized();
		}

		string toString() const override {
			return "("+innerType->toString()+")";
		}
	};

	struct NillableType : Type {
		TypeRef innerType;

		NillableType(TypeRef type) : Type(TypeID::Nillable), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
		~NillableType() { cout << innerType->toString() << "? type destroyed\n"; }

		bool acceptsA(const TypeRef& type) override {
			return PredefinedEVoidTypeRef->acceptsA(type) ||
				   type->ID == TypeID::Nillable && innerType->acceptsA(static_pointer_cast<NillableType>(type)->innerType) ||
				   innerType->acceptsA(type);
		}

		TypeRef normalized() override {
			auto normInnerType = innerType->normalized();

			if(normInnerType->ID == TypeID::Nillable) {
				return normInnerType;
			}

			return Ref<NillableType>(normInnerType);
		}

		string toString() const override {
			return innerType->toString()+"?";
		}
	};

	struct DefaultType : Type {
		TypeRef innerType;

		DefaultType(TypeRef type) : Type(TypeID::Default), innerType(move(type)) { cout << innerType->toString() << "? type created\n"; }
		~DefaultType() { cout << innerType->toString() << "! type destroyed\n"; }

		bool acceptsA(const TypeRef& type) override {
			return PredefinedEVoidTypeRef->acceptsA(type) ||
				   type->ID == TypeID::Default && innerType->acceptsA(static_pointer_cast<DefaultType>(type)->innerType) ||
				   innerType->acceptsA(type);
		}

		TypeRef normalized() override {
			auto normInnerType = innerType->normalized();

			if(normInnerType->ID == TypeID::Default) {
				return normInnerType;
			}

			return Ref<DefaultType>(normInnerType);
		}

		string toString() const override {
			return innerType->toString()+"!";
		}
	};

	struct UnionType : Type {
		vector<TypeRef> alternatives;

		UnionType(const vector<TypeRef>& alternatives) : Type(TypeID::Union), alternatives(alternatives) {}

		bool acceptsA(const TypeRef& type) override {
			for(const TypeRef& alt : alternatives) {
				if(alt->acceptsA(type)) {
					return true;
				}
			}

			return false;
		}

		TypeRef normalized() override {
			vector<TypeRef> normAlts;

			for(const TypeRef& alt : alternatives) {
				TypeRef normAlt = alt->normalized();

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

			return Ref<UnionType>(normAlts);
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
		vector<TypeRef> alternatives;

		IntersectionType(const vector<TypeRef>& alternatives) : Type(TypeID::Intersection), alternatives(alternatives) {}

		bool acceptsA(const TypeRef& type) override {
			for(const TypeRef& alt : alternatives) {
				if(!alt->acceptsA(type)) {
					return false;
				}
			}

			return true;
		}

		TypeRef normalized() override {
			vector<TypeRef> normAlts;

			for(const TypeRef& alt : alternatives) {
				TypeRef normAlt = alt->normalized();

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

			return Ref<IntersectionType>(normAlts);
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
		function<bool(const TypeRef&)> acceptsFn;

		PredefinedType(PredefinedTypeID subID, function<bool(const TypeRef&)> acceptsFn) : Type(TypeID::Predefined), subID(subID), acceptsFn(acceptsFn) {}

		bool acceptsA(const TypeRef& type) override {
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
		PrimitiveType(const TypeRef& v) : Type(TypeID::Primitive, true), subID(PrimitiveTypeID::Type), value(v ?
																										   (!v->concrete ? v : throw invalid_argument("Primitive type isn't mean to store concrete types")) :
																															   throw invalid_argument("Primitive type cannot be represented by nil")) { cout << toString() << " type created\n"; }
		~PrimitiveType() { cout << toString() << " type destroyed\n"; }

		bool acceptsA(const TypeRef& type) override {
			if(type->ID == TypeID::Primitive) {
				auto primType = static_pointer_cast<PrimitiveType>(type);

				if(subID == primType->subID) {
					if(subID == PrimitiveTypeID::Type) {
						return any_cast<TypeRef>(value)->acceptsA(any_cast<TypeRef>(primType->value));
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
				case PrimitiveTypeID::Type:		return "type "+any_cast<TypeRef>(value)->toString();
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
				case PrimitiveTypeID::Type:		return any_cast<TypeRef>(value)->operator double();
				default:						return double();
			}
		}

		operator int() const override {
			return operator double();
		}

		operator string() const override {
			switch(subID) {
				case PrimitiveTypeID::String:	return any_cast<string>(value);
				case PrimitiveTypeID::Type:		return any_cast<TypeRef>(value)->operator string();
				default:						return toString();
			}
		}

		TypeRef operator=(TypeRef type) override {
			if(type->ID == TypeID::Primitive) {
				auto primType = static_pointer_cast<PrimitiveType>(type);

				if(primType->subID == subID) {
					value = primType->value;
				}
			}

			return shared_from_this();
		}

		TypeRef operator+() const override {
			return subID == PrimitiveTypeID::Type
				 ? Ref<PrimitiveType>(any_cast<TypeRef>(value))
				 : Ref<PrimitiveType>(operator double());
		}

		TypeRef operator-() const override {
			return subID == PrimitiveTypeID::Type
				 ? Ref<PrimitiveType>(any_cast<TypeRef>(value))
				 : Ref<PrimitiveType>(-operator double());
		}

		TypeRef operator+(TypeRef type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::operator+();
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			if(subID == PrimitiveTypeID::String || primType->subID == PrimitiveTypeID::String) {
				return Ref<PrimitiveType>(operator string()+primType->operator string());
			}

			return Ref<PrimitiveType>(operator double()+primType->operator double());
		}

		TypeRef operator-(TypeRef type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::operator-();
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			return Ref<PrimitiveType>(operator double()-primType->operator double());
		}

		// TODO:
		// Examine the idea of concrete types access through members (as a kind of proxy).
		// In theory, this will make whole observing mechanism straight-forward:
		// - Members can track if contained value is changed and call observers when needed.
		// - Also they can track if value's type is changed and give an error if doesn't conform to expected type.
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

		TypeRef operator++() override {  // Prefix
			return shared_from_this();
		}

		TypeRef operator--() override {  // Prefix
			return shared_from_this();
		}

		TypeRef operator++(int) override {  // Postfix
			return Ref<Type>();
		}

		TypeRef operator--(int) override {  // Postfix
			return Ref<Type>();
		}

		TypeRef operator*(TypeRef type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::operator*(type);
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			return Ref<PrimitiveType>(operator double()*primType->operator double());
		}

		TypeRef operator/(TypeRef type) const override {
			if(type->ID != TypeID::Primitive) {
				return Type::operator/(type);
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			return Ref<PrimitiveType>(operator double()/primType->operator double());
		}

		bool operator==(const TypeRef& type) override {
			if(type->ID != TypeID::Primitive) {
				return Type::operator==(type);
			}

			auto primType = static_pointer_cast<PrimitiveType>(type);

			switch(subID) {
				case PrimitiveTypeID::Boolean:	return any_cast<bool>(value) == primType->operator bool();
				case PrimitiveTypeID::Float:	return any_cast<double>(value) == primType->operator double();
				case PrimitiveTypeID::Integer:	return any_cast<int>(value) == primType->operator int();
				case PrimitiveTypeID::String:	return any_cast<string>(value) == primType->operator string();
				case PrimitiveTypeID::Type:		return acceptsA(primType) && conformsTo(primType);
				default:						return Type::operator==(type);
			}
		}
	};

	struct DictionaryType : Type {
		struct Hasher {
			size_t operator()(const TypeRef& t) const {
				return 0;  // TODO
			}
		};

		struct Comparator {
			bool operator()(const TypeRef& lhs, const TypeRef& rhs) const {
				return lhs == rhs || !lhs && !rhs || lhs && rhs && lhs->operator==(rhs);
			}
		};

		using Entry = pair<TypeRef, TypeRef>;

		TypeRef keyType,
				valueType;
		unordered_map<TypeRef, vector<size_t>, Hasher, Comparator> kIndexes;	// key -> indexes
		vector<Entry> iEntries;													// index -> entry

		DictionaryType(const TypeRef& keyType = PredefinedEAnyTypeRef, const TypeRef& valueType = PredefinedEAnyTypeRef, bool concrete = false) : Type(TypeID::Dictionary, concrete), keyType(keyType), valueType(valueType) {}

		bool acceptsA(const TypeRef& type) override {
			if(type->ID == TypeID::Dictionary) {
				auto dictType = static_pointer_cast<DictionaryType>(type);

				return keyType->acceptsA(dictType->keyType) && valueType->acceptsA(dictType->valueType);
			}

			return false;
		}

		TypeRef normalized() override {
			return Ref<DictionaryType>(keyType->normalized(), valueType->normalized());
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

		bool operator==(const TypeRef& type) override {
			return false;  // TODO
		}

		void emplace(const TypeRef& key, const TypeRef& value, bool replace = false) {
			if(!concrete) {
				throw invalid_argument("Abstract dictionary type cannot contain values");
			}
			if(
				key && !key->conformsTo(keyType) ||
				!key && !PredefinedEVoidTypeRef->conformsTo(keyType)
			) {
				throw invalid_argument("'"+to_string(key)+"' does not conform to expected key type '"+keyType->toString()+"'");
			}
			if(
				value && !value->conformsTo(valueType) ||
				!value && !PredefinedEVoidTypeRef->conformsTo(valueType)
			) {
				throw invalid_argument("'"+to_string(value)+"' does not conform to expected value type '"+valueType->toString()+"'");
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

		void replace(const TypeRef& key, const TypeRef& value) {
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

		void removeFirst(const TypeRef& key) {
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

		void removeLast(const TypeRef& key) {
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

		TypeRef get(const TypeRef& key) const {
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
		struct MemberOverload {
			struct Modifiers {
				bool infix,
					 postfix,
					 prefix,

					 private_,
					 protected_,
					 public_,

					 final,
					 lazy,
					 static_,
					 virtual_;
			} modifiers;
			TypeRef type,
					value;
		};

		using MemberOverloadRef = shared_ptr<MemberOverload>;
		using Member = vector<MemberOverloadRef>;

		const CompositeTypeID subID;
		string title;
		Node IDs = {
			{"own", (int)composites.size()},  // Assume that we won't have ID collisions (any composite must be pushed into global array after creation)
			{"scope", nullptr}
		};
		unordered_map<int, pair<bool, int>> retainsCounts;  // Another ID : (This retained by another [bit] / Another retained by this [count])
		int life = 1;  // 0 - Creation (, Initialization?), 1 - Idle (, Deinitialization?), 2 - Destruction
		vector<int> inheritedTypes;  // May be reference, another composite (protocol), or function
		vector<TypeRef> genericParameterTypes;
		NodeArrayRef statements;
		unordered_map<string, int> imports;
		unordered_map<string, Member> members;

		CompositeType(CompositeTypeID subID,
					  const string& title,
					  const vector<int>& inheritedTypes = {},
					  const vector<TypeRef>& genericParameterTypes = {}) : Type(TypeID::Composite, true),
																		   subID(subID),
																		   title(title),
																		   inheritedTypes(inheritedTypes),
																		   genericParameterTypes(genericParameterTypes)
		{
			// TODO: Statically retain inherited types
		}

		set<int> getFullInheritanceChain() const {
			auto chain = set<int>(inheritedTypes.begin(), inheritedTypes.end());

			for(int parentIndex : inheritedTypes) {
				set<int> parentChain = composites[parentIndex]->getFullInheritanceChain();

				chain.insert(parentChain.begin(), parentChain.end());
			}

			return chain;
		}

		static bool checkConformance(const CompositeTypeRef& base, const CompositeTypeRef& candidate, const optional<vector<TypeRef>>& candidateGenericArgumentTypes = {}) {
			if(candidate->getOwnID() != base->getOwnID() && !candidate->getFullInheritanceChain().contains(base->getOwnID())) {
				return false;
			}

			if(candidateGenericArgumentTypes) {
				if(candidate->genericParameterTypes.size() != candidateGenericArgumentTypes->size()) {
					return false;
				}
				for(int i = 0; i < candidate->genericParameterTypes.size(); i++) {
					if(!candidate->genericParameterTypes[i]->acceptsA(candidateGenericArgumentTypes->at(i))) {
						return false;
					}
				}
			}

			return true;
		}

		bool acceptsA(const TypeRef& type) override;

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

			result += " "+title+"#"+to_string(IDs.get("own"));

			if(!genericParameterTypes.empty()) {
				result += "<";

				for(int i = 0; i < genericParameterTypes.size(); i++) {
					if(i > 0) {
						result += ", ";
					}

					result += genericParameterTypes[i]->toString();
				}

				result += ">";
			}

			if(!inheritedTypes.empty()) {
				set<int> chain = getFullInheritanceChain();
				bool first = true;

				result += " : [";

				for(int index : chain) {
					if(!first) {
						result += ", ";
					}

					first = false;
					result += index;
				}

				result += "]";
			}

			return result;
		}

		operator double() const override {
			return IDs.get("own");
		}

		void destroy() {
			cout << "destroyComposite("+getTitle()+")" << endl;
			if(life == 2) {
				return;
			}

			life = 2;

			unordered_set<int> retainedIDs = getRetainedIDs();

			for(int retainedID : retainedIDs) {
				if(CompositeTypeRef retainedComposite = getComposite(retainedID)) {
					release(retainedComposite);

					/*  Let the objects destroy no matter of errors
					if(threw()) {
						return;
					}
					*/
				}
			}

			int ID = getOwnID();
			TypeRef self = shared_from_this();

			composites[ID].reset();

			unordered_set<int> retainersIDs = getRetainersIDs();
			int aliveRetainers = 0;

			for(int retainerID : retainersIDs) {
				if(CompositeTypeRef retainingComposite = getComposite(retainerID); retainingComposite->life < 2) {
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
		void retain(CompositeTypeRef retainedComposite) {
			if(!retainedComposite || retainedComposite == shared_from_this()) {
				return;
			}

			int retainingID = getOwnID(),
				retainedID = retainedComposite->getOwnID();
			auto& retainingRC = retainsCounts,
				  retainedRC = retainedComposite->retainsCounts;

			retainingRC[retainedID].second++;
			retainedRC[retainingID].first = true;
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
		void release(CompositeTypeRef retainedComposite) {
			if(!retainedComposite || retainedComposite == shared_from_this()) {
				return;
			}

			int retainingID = getOwnID(),
				retainedID = retainedComposite->getOwnID();
			auto& retainingRC = retainsCounts,
				  retainedRC = retainedComposite->retainsCounts;
			auto retainingIt = retainingRC.find(retainedID),
				 retainedIt = retainedRC.find(retainingID);

			if(retainingIt != retainingRC.end() && retainingIt->second.second) {
				retainingIt->second.second--;

				if(!retainingIt->second.first && !retainingIt->second.second) {
					retainingRC.erase(retainingIt);
				}
			}
			if(retainedIt != retainedRC.end() && !retains(retainedComposite)) {
				retainedIt->second.first = false;

				if(!retainedIt->second.second) {
					retainedRC.erase(retainedIt);
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
		void retainOrRelease(TypeRef oldRetainedValue, TypeRef newRetainedValue) {
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
		void retain(TypeRef retainedValue) {
			for(auto retainedComposite : getValueComposites(retainedValue)) {
				retain(retainedComposite);
			}
		}

		/**
		 * Directly releases composites found in value.
		 *
		 * Check `getValueComposites()` for details of search.
		 */
		void release(TypeRef retainedValue) {
			for(auto retainedComposite : getValueComposites(retainedValue)) {
				release(retainedComposite);
			}
		}

		/**
		 * Returns a state of direct retention.
		 *
		 * Composite A is considered directly retained if it presents in composite B's retained IDs list.
		 *
		 * Formally this is the case when composite B uses composite A in its IDs (excluding own and retainers list),
		 * types, imports, members or observers, but technically retain and release functions must be utilized
		 * in corresponding places for this to work.
		 */
		bool retains(CompositeTypeRef retainedComposite) {
			if(life == 2 || !retainedComposite) {
				return false;
			}

			int retainedID = retainedComposite->getOwnID();

			return retainsCounts.count(retainedID) && retainsCounts[retainedID].second > 0;
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
		bool retainsDistant(CompositeTypeRef retainedComposite, shared_ptr<unordered_set<int>> visitedIDs = Ref<unordered_set<int>>()) {
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
			CompositeTypeRef retainingComposite,
							 retainedComposite = static_pointer_cast<CompositeType>(shared_from_this());

			if(retainingComposite = getValueComposite(controlTransfer().value))	if(retainingComposite->retainsDistant(retainedComposite)) return true;
			if(retainingComposite = scope())									if(retainingComposite->retainsDistant(retainedComposite)) return true;
		//	for(auto&& retainingComposite : scopes)								if(retainingComposite->retainsDistant(retainedComposite)) return true;  // May be useful when "with" syntax construct will be added
			if(retainingComposite = getComposite(0))							if(retainingComposite->retainsDistant(retainedComposite)) return true;

			return false;
		}

		void setID(const string& key, const NodeValue& value) {
			if(key == "own") {
				return;
			}

			NodeValue OV, NV;  // Old/new value

			OV = IDs.get(key);
			NV = IDs.get(key) = value;

			if(OV != NV) {
				if(tolower(key) != "self" && NV != -1) {  // ID chains should not be cyclic, intentionally missed IDs can't create cycles
					set<CompositeTypeRef> composites;
					CompositeTypeRef composite_ = static_pointer_cast<CompositeType>(shared_from_this());

					while(composite_) {
						if(composites.contains(composite_)) {
							IDs.get(key) = OV;

							return;
						}

						composites.insert(composite_);

						composite_ = getComposite(composite_->IDs.get(key));
					}
				}

				retain(getComposite(NV));
				release(getComposite(OV));
			}
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
			return title.length() ? title : "#"+(string)getOwnID();
		}

		NodeValue getOwnID() {
			return IDs.get("own");
		}

		NodeValue getSelfCompositeID() {
			return !isObject() ? getOwnID() : getTypeInheritedID();
		}

		NodeValue getScopeID() {
			return IDs.get("scope");
		}

		unordered_set<int> getRetainIDs(bool direction) const {
			unordered_set<int> result;

			for(const auto& [key, value] : retainsCounts) {
				if(direction ? value.second : value.first) {
					result.insert(key);
				}
			}

			return result;
		}

		unordered_set<int> getRetainersIDs() {
			return getRetainIDs(false);
		}

		unordered_set<int> getRetainedIDs() {
			return getRetainIDs(true);
		}

		void setSelfID(CompositeTypeRef selfComposite) {
			setID("self", selfComposite->getOwnID());
			setID("Self", selfComposite->getSelfCompositeID());
		}

		void setInheritedLevelIDs(CompositeTypeRef inheritedComposite) {
			if(inheritedComposite == shared_from_this()) {
				return;
			}

			static set<string> excluded = {"own", "scope"};

			for(const auto& [key, value] : IDs) {
				if(!excluded.contains(key)) {
					setID(key, inheritedComposite ? inheritedComposite->IDs.get(key) : NodeValue());
				}
			}
		}

		void setMissedLevelIDs() {
			static set<string> excluded = {"own", "scope"};

			for(const auto& [key, value] : IDs) {
				if(!excluded.contains(key)) {
					setID(key, -1);
				}
			}
		}

		void setScopeID(CompositeTypeRef scopeComposite) {
			setID("scope", scopeComposite ? scopeComposite->getOwnID() : NodeValue());
		}

		bool IDsRetain(CompositeTypeRef retainedComposite) {
			static set<string> excluded = {"own"};

			for(const auto& [key, value] : IDs) {
				if(!excluded.contains(key) && IDs.get(key) == retainedComposite->getOwnID()) {
					return true;
				}
			}

			return false;
		}

		NodeValue getTypeInheritedID() {
			return inheritedTypes.size() ? NodeValue(inheritedTypes.front()) : NodeValue();
		}

		optional<reference_wrapper<Member>> getMember(const string& identifier) {
			auto it = members.find(identifier);

			if(it != members.end()) {
				return it->second;
			}

			return nullopt;
		}

		Member& addMember(const string& identifier) {
			if(auto member = getMember(identifier)) {
				return *member;
			} else {
				return members[identifier] = Member();
			}
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

		MemberOverloadRef getMemberOverload(const string& identifier, optional<function<bool(MemberOverloadRef)>> matching = nullopt) {
			if(auto member = getMember(identifier)) {
				for(auto& overload : member->get()) {
					if(auto match = getMemberOverloadMatch(overload, matching)) {
						return match;
					}
				}
			}

			return nullptr;
		}

		MemberOverloadRef getMemberOverloadMatch(const MemberOverloadRef& overload, optional<function<bool(MemberOverloadRef)>> matching = nullopt) {
			if(!matching || (*matching)(overload)) {
				return overload;
			}

			return nullptr;
		}

		struct OverloadSearch {
			CompositeTypeRef owner;		// An exact composite where an overload was found, defined if found real (and nil if not found or found virtual)
			MemberOverloadRef overload;	// Reference to a found overload, defined if found (and nil if not found)
		};

		/**
		 * Looking for member overload in a composite itself.
		 *
		 * Used to find member overloads (primarily of functions and namespaces),
		 * pseudovariables (such as "Global" or "self"), and imports within namespaces.
		 *
		 * Functions and namespaces are slightly different from other types. They can inherit or miss levels and
		 * eventually aren't inheritable nor instantiable themself, but they are still used as members storage
		 * and can participate in plain scope chains.
		 */
		OverloadSearch findMemberOverloadInComposite(const string& identifier, optional<function<bool(MemberOverloadRef)>> matching = nullopt) {
			auto overload = getMemberOverload(identifier, matching);
			MemberOverloadRef backingStore;

			if(!overload) {
				unordered_map<string, NodeValue> IDs = {
				//	{"global", -1},						   Global-object is no thing
					{"Global", 0},						// Global-type
					{"super", this->IDs.get("super")},  // Super-object or a type
					{"Super", this->IDs.get("Super")},  // Super-type
					{"self", this->IDs.get("self")},	// Self-object or a type
					{"Self", this->IDs.get("Self")},	// Self-type
					{"sub", this->IDs.get("sub")},		// Sub-object
					{"Sub", this->IDs.get("Sub")}		// Sub-type
				//	{"metaSelf", -1},					   Self-object or a type (descriptor) (probably not a simple ID but some kind of proxy)
				//	{"arguments", -1}					   Function arguments array (should be in callFunction() if needed)
				};

				if(IDs.count(identifier)) {
					auto ID = IDs[identifier];

					if(ID != -1) {  // Intentionally missed IDs should be treated like non-existent
						backingStore = Ref<MemberOverload>(
							MemberOverload::Modifiers { .final = true },
							Ref<NillableType>(PredefinedCAnyTypeRef),
							getComposite(ID)
						);

						if(!getMemberOverloadMatch(backingStore, matching)) {
							backingStore = nullptr;
						}
					}
				}
			}

			if(!overload && !backingStore && isNamespace()) {
				unordered_map<string, int> IDs = {};  // imports;

				if(IDs.count(identifier)) {
					backingStore = Ref<MemberOverload>(
						MemberOverload::Modifiers { .final = true },
						PredefinedCAnyTypeRef,
						getComposite(IDs[identifier])
					);

					if(!getMemberOverloadMatch(backingStore, matching)) {
						backingStore = nullptr;
					}
				} else
				for(auto& [identifier, ID] : IDs) {
					auto composite = getComposite(IDs[identifier]);

					if(!composite) {
						continue;
					}

					overload = composite->getMemberOverload(identifier, matching);  // TODO: Investigate if this should be replaced with a search function

					if(overload) {
						break;
					}
				}
			}

			if(backingStore) {
				return OverloadSearch(nullptr, backingStore);
			} else
			if(overload) {
				return OverloadSearch(static_pointer_cast<CompositeType>(shared_from_this()), overload);
			}

			return OverloadSearch();
		}

		/**
		 * Looking for member overload in Object chain (super, self, sub).
		 *
		 * When a "virtual" overload is found, search oncely descends to a lowest sub-object.
		 */
		OverloadSearch findMemberOverloadInObjectChain(const string& identifier, optional<function<bool(MemberOverloadRef)>> matching = nullopt) {
			auto object = isObject()
						? static_pointer_cast<CompositeType>(shared_from_this())
						: getComposite(IDs.get("self"));

			if(!object || object == shared_from_this() || !object->isObject()) {
				return OverloadSearch();
			}

			bool descended;

			while(object) {  // Higher
				auto overload = object->getMemberOverload(identifier, matching);

				if(overload) {
					if(!descended && overload->modifiers.virtual_) {
						descended = true;

						while(CompositeTypeRef object_ = getComposite(object->IDs.get("sub"))) {  // Lowest
							object = object_;
						}

						continue;
					}

					return OverloadSearch(object, overload);
				}

				object = getComposite(object->IDs.get("super"));
			}

			return OverloadSearch();
		}

		/**
		 * Looking for member overload in Inheritance chain (Super, Self).
		 */
		OverloadSearch findMemberOverloadInInheritanceChain(const string& identifier, optional<function<bool(MemberOverloadRef)>> matching = nullopt) {
			auto composite = !isObject()
						   ? static_pointer_cast<CompositeType>(shared_from_this())
						   : getComposite(IDs.get("Self"));

			while(composite) {
				auto overload = composite->getMemberOverload(identifier, matching);

				if(overload) {
					return OverloadSearch(composite, overload);
				}

				composite = getComposite(composite->IDs.get("Super"));
			}

			return OverloadSearch();
		}

		/**
		 * Looking for member overload in Scope chain (scope).
		 *
		 * If search is internal, only first composite in chain will be checked.
		 */
		OverloadSearch findMemberOverload(const string& identifier, optional<function<bool(MemberOverloadRef)>> matching = nullopt, bool internal = false) {
			auto composite = static_pointer_cast<CompositeType>(shared_from_this());

			while(composite) {
				OverloadSearch search;

				if(search = composite->findMemberOverloadInComposite(identifier, matching);			search.overload) return search;
				if(search = composite->findMemberOverloadInObjectChain(identifier, matching);		search.overload) return search;
				if(search = composite->findMemberOverloadInInheritanceChain(identifier, matching);	search.overload) return search;

				if(internal) {
					break;
				}

				composite = getComposite(composite->getScopeID());
			}

			return OverloadSearch();
		}

		/**
		 * Not specifying "internal" means use of direct declaration without search.
		 */
		void setMemberOverload(const string& identifier, MemberOverload::Modifiers modifiers, TypeRef type, TypeRef value, optional<function<bool(MemberOverloadRef)>> matching = nullopt, optional<bool> internal = nullopt) {
			type = type ?: Ref<NillableType>(PredefinedEAnyTypeRef);

			if(
				value && !value->conformsTo(type) ||
				!value && !PredefinedEVoidTypeRef->conformsTo(type)
			) {
				throw invalid_argument("'"+to_string(value)+"' does not conform to expected value type '"+type->toString()+"'");
			}

			auto composite = static_pointer_cast<CompositeType>(shared_from_this());
			MemberOverloadRef overload;

			if(!internal) {
				overload = getMemberOverload(identifier, matching);
			} else
			if(auto search = findMemberOverload(identifier, matching, *internal); search.owner) {
				composite = search.owner;
				overload = search.overload;
			}

			if(!overload) {
				Member& member = addMember(identifier);
				member.push_back(Ref<MemberOverload>());
				overload = member.back();
			}

			TypeRef OT = overload->type ?: Ref<Type>(),  // Old/new type
					NT = type ?: Ref<Type>(),
					OV = overload->value,  // Old/new value
					NV = value;

			overload->modifiers = modifiers;
			overload->type = NT;
			overload->value = value;

			if(OV != NV) {
				composite->retainOrRelease(OV, NV);
			}
		}
	};

	struct ReferenceType : Type {
		CompositeTypeRef compType;
		optional<vector<TypeRef>> typeArgs;

		ReferenceType(const CompositeTypeRef& compType, const optional<vector<TypeRef>>& typeArgs = nullopt) : Type(TypeID::Reference), compType(compType), typeArgs(typeArgs) {}

		bool acceptsA(const TypeRef& type) override {
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

		TypeRef normalized() override {
			if(!typeArgs) {
				return shared_from_this();
			}

			vector<TypeRef> normArgs;

			for(const TypeRef& arg : *typeArgs) {
				normArgs.push_back(arg->normalized());
			}

			return Ref<ReferenceType>(compType, normArgs);
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

	bool CompositeType::acceptsA(const TypeRef& type) {
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
		vector<TypeRef> genericParameterTypes,
						parameterTypes;
		TypeRef returnType;
		struct Modifiers {
			optional<bool> inits,
						   deinits,
						   awaits,
						   throws;
		} modifiers;

		FunctionType(const vector<TypeRef>& genericParameterTypes,
					 const vector<TypeRef>& parameterTypes,
					 const TypeRef& returnType,
					 const Modifiers& modifiers) : genericParameterTypes(genericParameterTypes),
												   parameterTypes(parameterTypes),
												   returnType(returnType),
												   modifiers(modifiers) {}

		static bool matchTypeLists(const vector<TypeRef>& expectedList, const vector<TypeRef>& providedList);

		bool acceptsA(const TypeRef& type) override {
			if(type->ID == TypeID::Function) {
				auto funcType = static_pointer_cast<FunctionType>(type);

				return FunctionType::matchTypeLists(genericParameterTypes, funcType->genericParameterTypes) &&
					   FunctionType::matchTypeLists(parameterTypes, funcType->parameterTypes) &&
					   (!modifiers.inits || funcType->modifiers.inits == modifiers.inits) &&
					   (!modifiers.deinits || funcType->modifiers.deinits == modifiers.deinits) &&
					   (!modifiers.awaits || funcType->modifiers.awaits == modifiers.awaits) &&
					   (!modifiers.throws || funcType->modifiers.throws == modifiers.throws) &&
					   returnType->acceptsA(funcType->returnType);
			}

			return false;
		}

		TypeRef normalized() override {
			return Ref<FunctionType>(
				transform(genericParameterTypes, [](const TypeRef& v) { return v->normalized(); }),
				transform(parameterTypes, [](const TypeRef& v) { return v->normalized(); }),
				returnType->normalized(),
				modifiers
			);
		}

		string toString() const override {
			string result;

			if(!genericParameterTypes.empty()) {
				result += "<";

				for(int i = 0; i < genericParameterTypes.size(); i++) {
					if(i > 0) {
						result += ", ";
					}

					result += genericParameterTypes[i]->toString();
				}

				result += ">";
			}

			result += "(";

			for(int i = 0; i < parameterTypes.size(); i++) {
				if(i > 0) {
					result += ", ";
				}

				result += parameterTypes[i]->toString();
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
		TypeRef innerType;

		InoutType(const TypeRef& innerType) : Type(TypeID::Inout), innerType(innerType) {}

		bool acceptsA(const TypeRef& type) override {
			return type->ID == TypeID::Inout && innerType->acceptsA(static_pointer_cast<InoutType>(type)->innerType);
		}

		TypeRef normalized() override {
			auto normInner = innerType->normalized();

			if(normInner->ID == TypeID::Inout) {
				return normInner;
			}

			return Ref<InoutType>(normInner);
		}

		string toString() const override {
			return "inout "+innerType->toString();
		}
	};

	struct VariadicType : Type {
		TypeRef innerType;

		VariadicType(const TypeRef& innerType = nullptr) : Type(TypeID::Variadic), innerType(innerType) {}

		bool acceptsA(const TypeRef& type) override {
			if(!innerType) {
				return true;
			}
			if(type->ID != TypeID::Variadic) {
				return innerType->acceptsA(type);
			}

			auto varType = static_pointer_cast<VariadicType>(type);

			if(!varType->innerType) {
				return innerType->acceptsA(PredefinedEVoidTypeRef);
			}

			return innerType->acceptsA(varType->innerType);
		}

		TypeRef normalized() override {
			if(!innerType) {
				return shared_from_this();
			}

			return Ref<VariadicType>(innerType->normalized());
		}

		string toString() const override {
			return (innerType ? innerType->toString() : "")+"...";
		}
	};

	bool FunctionType::matchTypeLists(const vector<TypeRef>& expectedList, const vector<TypeRef>& providedList) {
		int expectedSize = expectedList.size(),
			providedSize = providedList.size();

		function<bool(int, int)> matchFrom = [&](int expectedIndex, int providedIndex) {
			if(expectedIndex == expectedSize) {
				return providedIndex == providedSize;
			}

			const TypeRef& expectedType = expectedList[expectedIndex];

			if(expectedType->ID == TypeID::Variadic) {
				auto varExpectedType = static_pointer_cast<VariadicType>(expectedType);

				if(expectedIndex == expectedSize-1) {
					return all_of(providedList.begin()+providedIndex, providedList.end(), [&](const TypeRef& t) {
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

	const TypeRef PredefinedEVoidTypeRef = Ref<PredefinedType>(PredefinedTypeID::EVoid, [](const TypeRef& type) {
		return type->ID == TypeID::Predefined && static_pointer_cast<PredefinedType>(type)->subID == PredefinedTypeID::EVoid;
	});

	const TypeRef PredefinedEAnyTypeRef = Ref<PredefinedType>(PredefinedTypeID::EAny, [](const TypeRef& type) {
		return true;
	});

	const TypeRef PredefinedPAnyTypeRef = Ref<PredefinedType>(PredefinedTypeID::PAny, [](const TypeRef& type) {
		return type->ID == TypeID::Primitive;
	});

	const TypeRef PredefinedPBooleanTypeRef = Ref<PredefinedType>(PredefinedTypeID::PBoolean, [](const TypeRef& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Boolean;
	});

	const TypeRef PredefinedPDictionaryTypeRef = Ref<PredefinedType>(PredefinedTypeID::PDictionary, [](const TypeRef& type) {
		return type->ID == TypeID::Dictionary;
	});

	const TypeRef PredefinedPFloatTypeRef = Ref<PredefinedType>(PredefinedTypeID::PFloat, [](const TypeRef& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Float;
	});

	const TypeRef PredefinedPIntegerTypeRef = Ref<PredefinedType>(PredefinedTypeID::PInteger, [](const TypeRef& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Integer;
	});

	const TypeRef PredefinedPStringTypeRef = Ref<PredefinedType>(PredefinedTypeID::PString, [](const TypeRef& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::String;
	});

	const TypeRef PredefinedPTypeTypeRef = Ref<PredefinedType>(PredefinedTypeID::PType, [](const TypeRef& type) {
		return type->ID == TypeID::Primitive && static_pointer_cast<PrimitiveType>(type)->subID == PrimitiveTypeID::Type;
	});

	const TypeRef PredefinedCAnyTypeRef = Ref<PredefinedType>(PredefinedTypeID::CAny, [](const TypeRef& type) {
		return type->ID == TypeID::Composite ||
			   type->ID == TypeID::Reference;
	});

	const TypeRef PredefinedCClassTypeRef = Ref<PredefinedType>(PredefinedTypeID::CClass, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Class ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Class;
	});

	const TypeRef PredefinedCEnumerationTypeRef = Ref<PredefinedType>(PredefinedTypeID::CEnumeration, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Enumeration ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Enumeration;
	});

	const TypeRef PredefinedCFunctionTypeRef = Ref<PredefinedType>(PredefinedTypeID::CFunction, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Function ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Function;
	});

	const TypeRef PredefinedCNamespaceTypeRef = Ref<PredefinedType>(PredefinedTypeID::CNamespace, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Namespace ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Namespace;
	});

	const TypeRef PredefinedCObjectTypeRef = Ref<PredefinedType>(PredefinedTypeID::CObject, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Object ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Object;
	});

	const TypeRef PredefinedCProtocolTypeRef = Ref<PredefinedType>(PredefinedTypeID::CProtocol, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Protocol ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Protocol;
	});

	const TypeRef PredefinedCStructureTypeRef = Ref<PredefinedType>(PredefinedTypeID::CStructure, [](const TypeRef& type) {
		return type->ID == TypeID::Composite && static_pointer_cast<CompositeType>(type)->subID == CompositeTypeID::Structure ||
			   type->ID == TypeID::Reference && static_pointer_cast<ReferenceType>(type)->compType->subID == CompositeTypeID::Structure;
	});

	// ----------------------------------------------------------------

	CompositeTypeRef createComposite(const string& title, CompositeTypeID type, CompositeTypeRef scope = nullptr) {
		cout << "createComposite("+title+")" << endl;
		auto composite = Ref<CompositeType>(type, title);

		composites.push_back(composite);

		if(scope) {
			composite->setScopeID(scope);
		}

		return composite;
	}

	/**
	 * Levels such as super, self or sub, can be self-defined (self only), inherited from another composite or missed.
	 * If levels are intentionally missed, Scope will prevail over Object and Inheritance chains at member overload search.
	 */
	CompositeTypeRef createNamespace(const string& title, CompositeTypeRef scope = nullptr, optional<CompositeTypeRef> levels = nullptr) {
		CompositeTypeRef namespace_ = createComposite(title, CompositeTypeID::Namespace, scope);

		if(levels && *levels) {
			namespace_->setInheritedLevelIDs(*levels);
		} else
		if(!levels) {
			namespace_->setMissedLevelIDs();
		} else {
			namespace_->setSelfID(namespace_);
		}

		return namespace_;
	}

	string getTitle(CompositeTypeRef composite) {
		return composite ? composite->getTitle() : "";
	}

	/*
	bool compositeIsFunction(const CompositeTypeRef& composite) {
		return composite && composite->isFunction();
	}

	bool compositeIsNamespace(const CompositeTypeRef& composite) {
		return composite && composite->isNamespace();
	}

	bool compositeIsObject(const CompositeTypeRef& composite) {
		return composite && composite->isObject();
	}

	bool compositeIsInstantiable(const CompositeTypeRef& composite) {
		return composite && composite->isInstantiable();
	}

	bool compositeIsCallable(const CompositeTypeRef& composite) {
		return composite && composite->isCallable();
	}
	*/

	CompositeTypeRef getScope(int offset = 0) {
		return scopes.size() > scopes.size()-1+offset
			 ? scopes[scopes.size()-1+offset]
			 : nullptr;
	}

	CompositeTypeRef scope() {
		return getScope();
	}

	/**
	 * Should be used for a bodies before executing its contents,
	 * as ARC utilizes a current scope at the moment.
	 */
	void addScope(CompositeTypeRef composite) {
		scopes.push_back(composite);
	}

	/**
	 * Removes from the stack and optionally automatically destroys a last scope.
	 */
	void removeScope(bool destroy = true) {
		CompositeTypeRef composite = scopes.back();

		scopes.pop_back();

		if(destroy && composite) {
			composite->destroyReleased();
		}
	}

	/**
	 * Returns result of identifier(value) call or plain value if no appropriate function found in current scope.
	 */
	TypeRef getValueWrapper(const TypeRef& value, string identifier) {
		// TODO

		return value;
	}

	TypeRef getValueWrapper(const PrimitiveType& value, string identifier) {
		return getValueWrapper(Ref(value), identifier);
	}

	/**
	 * Returns a live composite found in value. Works with `CompositeType`.
	 */
	CompositeTypeRef getValueComposite(TypeRef value) {
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
	unordered_set<CompositeTypeRef> getValueComposites(TypeRef value) {
		unordered_set<CompositeTypeRef> result;

		if(value && value->ID == TypeID::Dictionary) {
			for(auto& [key, value_] : *static_pointer_cast<DictionaryType>(value)) {
				auto keyComposites = getValueComposites(key),
					 valueComposites = getValueComposites(value_);

				result.insert(keyComposites.begin(), keyComposites.end());
				result.insert(valueComposites.begin(), valueComposites.end());
			}
		} else
		if(CompositeTypeRef composite = getValueComposite(value)) {
			result.insert(composite);
		}

		return result;
	}

	// ----------------------------------------------------------------

	template<typename... Args>
	TypeRef rules(const string& type, NodeRef n = nullptr, Args... args);

	template<typename... Args>
	TypeRef executeNode(NodeRef node, bool concrete = true, Args... arguments) {
		if(!node) {
			return nullptr;
		}

		int OP = position,  // Old/new position
			NP = node ? node->get<Node&>("range").get<int>("start") : 0;
		TypeRef value;

		position = NP;

		try {
			value = rules(node->get("type"), node, arguments...);
		} catch(exception& e) {
			value = Ref<PrimitiveType>(e.what());

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
	TypeRef rules(const string& type, NodeRef n, Args... args) {
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
		} else
		if(type == "floatLiteral") {
			return getValueWrapper(n->get<double>("value"), "Float");
		} else
		if(type == "identifier") {
			string identifier = n->get("value");
			auto overload = scope()->findMemberOverload(identifier).overload;

			if(!overload) {
				report(1, n, "Member overload wasn't found (accessing '"+identifier+"').");

				return nullptr;
			}

			return overload->value;
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
			executeNodes(n->get<NodeArrayRef>("statements")/*, (t) => t !== 'throw' ? 0 : -1*/);

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

			if(!composite || composite->isObject()) {
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
		if(type == "variableDeclaration") {
			NodeArrayRef modifiers = n->get("modifiers");

			for(Node& declarator : n->get<NodeArray&>("declarators")) {
				string identifier = declarator.get<Node&>("identifier").get("value");
				TypeRef type = executeNode(declarator.get("type_"), false),
						value;

			//	addContext({ type: type }, ['implicitChainExpression']);

				value = executeNode(declarator.get("value"));

			//	removeContext();

				scope()->setMemberOverload(identifier, CompositeType::MemberOverload::Modifiers(), type, value);
			}
		} else
		if(type == "whileStatement") {
			if(n->empty("condition")) {
				return nullptr;
			}

			// TODO: Align namespace/scope creation/destroy close to functionBody execution (it will allow single statements to affect current scope)
			// Edit: What did I mean by "single statements", inline declarations like "while var a = b() {}"? That isn't even implemented in the parser right now
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

	// ----------------------------------------------------------------

	struct Place {
		NodeRef node,
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