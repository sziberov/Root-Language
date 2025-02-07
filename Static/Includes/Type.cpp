#include <iostream>
#include <vector>
#include <string>
#include <optional>

using namespace std;

// Флаги характеристик узла
enum TypeFlags {
	Array           = 1 << 0,
	AwaitsOptional  = 1 << 1,
	Awaits          = 1 << 2,
	Default         = 1 << 3,
	GenericArguments= 1 << 5,
	GenericParameters   = 1 << 6,
	InheritedTypes  = 1 << 7,
	Inout           = 1 << 8,
	Intersection    = 1 << 9,
	Nillable        = 1 << 10,
	Parameters      = 1 << 11,
	ThrowsOptional  = 1 << 12,
	Throws          = 1 << 13,
	Tuple           = 1 << 14,
	Union           = 1 << 15,
	Variadic        = 1 << 16,
	Dictionary      = 1 << 17
};

// Узел типа
struct TypeNode {
	optional<int> super; // Индекс родителя
	string predefined;       // Имя типа или иное значение (например, "Function", "Class")
	int flags = 0;
	vector<int> children; // Индексы дочерних узлов

	// Дополнительные поля для поддержки tuple и композитов
	string identifier;      // Например, имя переменной или класса
	string label;           // Для tuple-элементов (метка перед идентификатором)

	TypeNode(optional<int> parent, string ref, int f = 0): super(parent), predefined(move(ref)), flags(f) {}
};

// Класс, инкапсулирующий дерево типов и функции для работы с ним
class TypeTree {
public:
	vector<TypeNode> tree;

	// Добавление узла в дерево; возвращается индекс добавленного узла.
	int addNode(optional<int> parent, string ref, int flags = 0) {
		int index = tree.size();

		tree.emplace_back(parent, move(ref), flags);

		if(parent.has_value()) {
			tree[*parent].children.push_back(index);
		}

		return index;
	}

	string toString(int index) {
		return toString(tree[index]);
	}

private:
	bool isCollection(const TypeNode& node) {
		switch(node.flags) {
			case Array:
			case Dictionary:
			case GenericArguments:
			case GenericParameters:
			case Intersection:
			case Parameters:
			case Union:
				return true;
		}
		if(node.predefined == "Function") {
			return true;
		}

		return false;
	}

	string unionIntersectionToString(const TypeNode& node) {
		string result = formatChildren(node, node.flags & Union ? " | " : " & ");

		int parent_flags = node.super.has_value() ? tree[*node.super].flags : 0;
		bool needs_parens = parent_flags & Union && node.flags & Union ||
							parent_flags & Intersection && (node.flags & (Intersection | Union));

		return needs_parens ? "("+result+")" : result;
	}

	string formatChildren(const TypeNode& node, const string& delimiter = ", ") {
		string result;

		for(int i = 0; i < node.children.size(); i++) {
			if(i > 0) {
				result += delimiter;
			}

			const TypeNode& child = tree[node.children[i]];
			result += toString(child);
		}

		return result;
	}

	string applyModifiers(const TypeNode& node, string result) {
		if(node.flags & Array) {
			result = "["+result+"]";
		}
		if(node.flags & Inout) {
			result = "inout "+result;
		}
		if(node.flags & (Nillable | Default)) {
			if(result.contains(' ')) {
				result = "("+result+")";
			}
			if(node.flags & Default) {
				result += "!";
			} else
			if(node.flags & Nillable) {
				result += "?";
			}
		}
		if(node.flags & Variadic) {
			result += "...";
		}

		return result;
	}

	string basicToString(const TypeNode& node) {
		string result = node.predefined;
		return result;
	}

	string childToString(const TypeNode& node, string result) {
		if(!node.children.empty()) {
			if(node.flags & GenericArguments) {
				result += "<"+formatChildren(node)+">";
			} else {
				result += formatChildren(node);
			}
		}

		return result;
	}

	string functionToString(const TypeNode& node) {
		string genParams,
			   parameters = "()",
			   retType;

		for(auto child : node.children) {
			const TypeNode& c = tree[child];

			if(c.flags & GenericParameters) {
				genParams = "<"+formatChildren(c)+">";
			} else
			if(c.flags & Parameters) {
				parameters = "("+formatChildren(c)+")";
			} else {
				retType = " -> "+toString(c);
			}
		}

		string result = genParams+parameters;
		if (node.flags & Awaits) { result += " awaits"; }
		else if (node.flags & AwaitsOptional) { result += " awaits?"; }
		if (node.flags & Throws) { result += " throws"; }
		else if (node.flags & ThrowsOptional) { result += " throws?"; }
		result += retType;
		if(node.flags & Nillable) {
			if(result.ends_with(')'))
				result += "?";
			else
				result = "("+result+")?";
		}

		return result;
	}

	string tupleToString(const TypeNode& node) {
		string result;

		for(int i = 0; i < node.children.size(); i++) {
			if (i > 0) result += ", ";
			const TypeNode& elem = tree[node.children[i]];
			string s;
			if (!elem.label.empty()) {
				s += elem.label + " ";
			}
			if (!elem.identifier.empty()) {
				s += elem.identifier;
			}
			if (!elem.predefined.empty()) {
				s += ": "+toString(elem);
			}
			result += s;
		}

		return "("+result+")";
	}

	// Композитный тип (например, класс).
	string compositeToString(const TypeNode& node) {
		string result,
			   identifier = node.identifier,
			   genParams,
			   inheritedTypes;

		for(auto child : node.children) {
			const TypeNode& c = tree[child];

			if(c.flags & GenericParameters) {
				genParams = "<"+formatChildren(c)+">";
			} else
			if(c.flags & InheritedTypes) {
				inheritedTypes = ": "+formatChildren(c);
			}
		}

		if(node.predefined == "Class") {
			result = "class ";
		}

		result += identifier+genParams+inheritedTypes;

		return result;
	}

	string toString(const TypeNode& node) {
		// Обработка специальных типов
		if(node.flags & (Union | Intersection)) {
			return unionIntersectionToString(node);
		}
		if(node.predefined == "Function") {
			return functionToString(node);
		}
		if(node.flags & Tuple) {
			return tupleToString(node);
		}
		if(node.predefined == "Class") {
			return compositeToString(node);
		}

		// Обработка обычного типа
		string result = basicToString(node);
		result = childToString(node, result);
		result = applyModifiers(node, result);

		return result;
   }
};

//
// Пример использования
//
int main() {
	TypeTree type;

	{
		//    inout A! | _? & Global<B>...
		// 0: inout    |               ...
		int root = type.addNode(nullopt, "", Inout | Union | Variadic);
		// 1:       A!
		int a = type.addNode(root, "A", Default);
		// 2:               &
		int andNode = type.addNode(root, "", Intersection);
		// 3:            _?
		int nullable_ = type.addNode(andNode, "_", Nillable);
		// 4:                 Global< >
		int global = type.addNode(andNode, "Global", GenericArguments);
		// 5:                        B
		int b = type.addNode(global, "B");

		cout << "Type representation: " << type.toString(root) << endl;
	}

	// Пример: A | (B | C)
	{
		int memberType1 = type.addNode(nullopt, "", Union);
		int A = type.addNode(memberType1, "A");
		int unionChild = type.addNode(memberType1, "", Union);
		int B = type.addNode(unionChild, "B");
		int C = type.addNode(unionChild, "C");
		// Ожидаем: A | (B | C)
		cout << "Type: " << type.toString(memberType1) << endl;
	}

	// Пример более сложного выражения: A | (B | C & D & (E | F))
	{
		// Построим дерево вручную:
		// 0: Корневой узел Union
		int root = type.addNode(nullopt, "", Union);
		int A = type.addNode(root, "A");
		// 1: дочерний узел Union, который будет обёрнут в скобки, т.к. родитель — Union
		int union1 = type.addNode(root, "", Union);
		int B = type.addNode(union1, "B");
		// 2: узел Intersection внутри union1
		int inter1 = type.addNode(union1, "", Intersection);
		int C = type.addNode(inter1, "C");
		int D = type.addNode(inter1, "D");
		// 3: еще один узел Intersection внутри union1
		int inter2 = type.addNode(union1, "", Intersection);
		int union2 = type.addNode(inter2, "", Union);
		int E = type.addNode(union2, "E");
		int F = type.addNode(union2, "F");

		cout << "Type: " << type.toString(root) << endl;
	}

	// --- Функциональные типы ---

	//    () / Function
	// 0: () / Function
	int funcType1 = type.addNode(nullopt, "Function");

	//    <...>(...) awaits? throws? -> _?
	// 0:            awaits? throws?
	int funcType2 = type.addNode(nullopt, "Function", AwaitsOptional | ThrowsOptional);
	// 1: <   >
	int genParams2 = type.addNode(funcType2, "", GenericParameters);
	// 2:  ...
	int genParamChild = type.addNode(genParams2, "", Variadic);
	// 3:      (   )
	int params2 = type.addNode(funcType2, "", Parameters);
	// 4:       ...
	int paramChild = type.addNode(params2, "", Variadic);
	// 5:                            -> _?
	int retType2 = type.addNode(funcType2, "_", Nillable);

	//    ([]..., ...) awaits throws -> _
	// 0:              awaits throws
	int funcType3 = type.addNode(nullopt, "Function", Awaits | Throws);
	// 1: (     ,    )
	int params3 = type.addNode(funcType3, "", Parameters);
	// 2:  []...
	int arrayParam = type.addNode(params3, "", Array | Variadic);
	// 3:         ...
	int param3 = type.addNode(params3, "", Variadic);
	// 4:                            -> _
	int retType3 = type.addNode(funcType3, "_", 0);

	//    (() -> _)?
	// 0: (()     )?
	int funcType4 = type.addNode(nullopt, "Function", Nillable);
	// 1:     -> _
	int retType4 = type.addNode(funcType4, "_", 0);

	//    <T>()
	// 0:    ()
	int funcType5 = type.addNode(nullopt, "Function");
	// 1: < >
	int genParams5 = type.addNode(funcType5, "", GenericParameters);
	// 2:  T
	int genParamT = type.addNode(genParams5, "T", 0);

	cout << "Type 1: " << type.toString(funcType1) << endl;
	cout << "Type 2: " << type.toString(funcType2) << endl;
	cout << "Type 3: " << type.toString(funcType3) << endl;
	cout << "Type 4: " << type.toString(funcType4) << endl;
	cout << "Type 5: " << type.toString(funcType5) << endl;

	// --- Массивные типы ---

	//    [] / [_?]
	// 0: [] / [  ]
	int memberType6 = type.addNode(nullopt, "", Array);
	// 1:    /  _?
	int fallbackChild = type.addNode(memberType6, "_", Nillable);

	cout << "Fallback array type: " << type.toString(memberType6) << endl;

	//    Array<_?>
	// 0: Array<  >
	int memberType7 = type.addNode(nullopt, "Array", GenericArguments);
	// 1:       _?
	int genericChild = type.addNode(memberType7, "_", Nillable);

	cout << "Preferred array type: " << type.toString(memberType7) << endl;

	// --- Кортежные типы ---

	//    (a: A, b c: B)
	// 0: (    ,       )
	int tupleType0 = type.addNode(nullopt, "", Tuple);
	// 1:  a: A
	int tupleElem1 = type.addNode(tupleType0, "A");
	type.tree[tupleElem1].identifier = "a";
	// 2:        b c: B
	int tupleElem2 = type.addNode(tupleType0, "B");
	type.tree[tupleElem2].label = "b";
	type.tree[tupleElem2].identifier = "c";

	cout << "Tuple type: " << type.toString(tupleType0) << endl;

	// --- Композитные типы ---

	//    class A<T: _?>: B<C?, D>
	// 0: class A<     >
	int compositeType0 = type.addNode(nullopt, "Class", GenericArguments);
	type.tree[compositeType0].identifier = "A";
	// 1:         T: _?
	int compositeParameter = type.addNode(compositeType0, "_", Nillable);
	type.tree[compositeParameter].identifier = "T";
	// 2:               :
	int inheritedTypes = type.addNode(compositeType0, "", InheritedTypes);
	// 3:                 B<  ,  >
	int inheritedType = type.addNode(inheritedTypes, "B", GenericArguments);
	// 4:                   C?
	int genericArg = type.addNode(inheritedType, "C", Nillable);
	// 5:                       D
	int genericArg2 = type.addNode(inheritedType, "D");

	cout << "Composite type: " << type.toString(compositeType0) << endl;

	return 0;
}