#include <string>
#include <iostream>
#include <vector>
#include <map>

using namespace std;

struct Type;
struct ParenthesizedType;

struct Composite {
	string title;
	Type* type;
};

struct Type {
	virtual string toString() {
		return "";
	}

	virtual bool accepts(Type* t) {
		return false;
	}
};

struct TypeGenericParameters : Type {
	map<string, Type*> list;

	TypeGenericParameters(map<string, Type*> gp) : list(gp) {}

	string toString() {
		string result;

		for(int i = 0; i < list.size(); i++) {
			if(i > 0) {
				result += ", ";
			}

			auto it = list.begin();
			advance(it, i);
			result += it->first+": "+it->second->toString();
		}

		return "<"+result+">";
	}
};

struct TypeGenericArguments : Type {
	vector<Type*> list;

	TypeGenericArguments(vector<Type*> ga) : list(ga) {}

	string toString() {
		string result;

		for(int i = 0; i < list.size(); i++) {
			if(i > 0) {
				result += ", ";
			}

			result += list[i]->toString();
		}

		return "<"+result+">";
	}
};

struct PredefinedType : Type {
	string name;

	PredefinedType(string n) : name(move(n)) {}

	string toString() {
		return name;
	}

	virtual bool accepts(PredefinedType* t) {
		return name == t->name;
	}
};

struct OptionalType : Type {
	Type* type;

	OptionalType(Type* t) : type(t) {}

	string toString() {
		return type->toString()+"?";
	}

	virtual bool accepts(Type* t) {
		return type->accepts(t);
	}
};

struct DefaultType : Type {
	Type* type;

	DefaultType(Type* t) : type(t) {}

	string toString() {
		return type->toString()+"!";
	}

	virtual bool accepts(Type* t) {
		return type->accepts(t);
	}
};

struct ParenthesizedType : Type {
	Type* type;

	ParenthesizedType(Type* t) : type(t) {}

	string toString() {
		return "("+type->toString()+")";
	}

	virtual bool accepts(Type* t) {
		return type->accepts(t);
	}
};

struct UnionType : Type {
	vector<Type*> alternatives;

	UnionType(vector<Type*> a) : alternatives(move(a)) {}

	string toString() {
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
	vector<Type*> alternatives;

	IntersectionType(vector<Type*> a) : alternatives(move(a)) {}

	string toString() {
		string result;

		for(int i = 0; i < alternatives.size(); i++) {
			if(i > 0) {
				result += " & ";
			}

			result += alternatives[i]->toString();
		}

		return result;
	}

	virtual bool accepts(IntersectionType* t) {
		for(auto alternative : alternatives) {
			if(!alternative->accepts(t)) {
				return false;
			}
		}

		return true;
	}
};

struct ReferenceType : Type {
	Composite* composite;
	optional<TypeGenericArguments> genericArguments;

	ReferenceType(Composite* c, optional<TypeGenericArguments> ga = nullopt) : composite(c), genericArguments(ga) {}

	string toString() {
		string result = composite->title;

		if(genericArguments) {
			result += genericArguments->toString();
		}

		return result;
	}

	virtual bool accepts(Type* t) {


		return false;
	}
};

int main() {
	Type* t = new ReferenceType(new Composite("A"), TypeGenericArguments(vector<Type*> { new OptionalType(new PredefinedType("bool")) }));

	cout << t->toString() << endl;

	cout << (new PredefinedType("bool"))->accepts(new PredefinedType("bool")) << endl;
}