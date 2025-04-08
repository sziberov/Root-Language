from typing import List, Optional, Any, Callable, Dict


# --- Классы для кандидатов и результатов разрешения ---

class FunctionCandidate:
    def __init__(self, identifier: str, param_types: List[str],
                 func: Callable[..., None], is_virtual: bool = False):
        self.identifier = identifier
        self.param_types = param_types  # например, ["int"], ["bool"], [] для методов без параметров
        self.func = func
        self.is_virtual = is_virtual

    def matches(self, arg_types: List[str]) -> bool:
        # Простое сравнение списков типов
        return self.param_types == arg_types

    def __repr__(self):
        virt = "virtual " if self.is_virtual else ""
        return f"<{virt}{self.identifier}{self.param_types}>"


class ResolveSearch:
    def __init__(self):
        self.overloads: List[FunctionCandidate] = []
        self.observer: Optional[FunctionCandidate] = None

    def __repr__(self):
        return f"ResolveSearch(candidates={self.overloads}, observer={self.observer})"


# --- Класс для пространства имён (namespace) ---

class Namespace:
    def __init__(self, name: str):
        self.name = name
        # Перегрузки для каждого идентификатора: identifier -> List[FunctionCandidate]
        self.functions: Dict[str, List[FunctionCandidate]] = {}
        # Наблюдатель для данного пространства имён (если задан)
        self.observer: Optional[FunctionCandidate] = None
        # Связи для цепочки объектов и цепочки классов (а также суперконтекст и scope)
        self.sub: Optional['Namespace'] = None     # объектная цепочка – для методов экземпляра
        self.Sub: Optional['Namespace'] = None       # цепочка классов (downwards для виртуальных)
        self.super: Optional['Namespace'] = None     # объектная цепочка вверх
        self.Super: Optional['Namespace'] = None     # цепочка классов вверх (наследование)
        self.scope: Optional['Namespace'] = None       # область видимости

    def add_function(self, candidate: FunctionCandidate):
        self.functions.setdefault(candidate.identifier, []).append(candidate)

    def get_overloads(self, identifier: str) -> List[FunctionCandidate]:
        return self.functions.get(identifier, [])

    def set_observer(self, candidate: FunctionCandidate):
        self.observer = candidate

    def __repr__(self):
        return f"<Namespace {self.name}>"


# --- Функция разрешения имён (резолюция) ---

def resolve(namespace: Namespace, identifier: str, chain_mode: Optional[str] = 'scope', visited: Optional[Dict[Namespace, Dict[Optional[str], bool]]] = None) -> ResolveSearch:
    if visited is None:
        visited = {}

    if namespace in visited:
        if chain_mode in visited[namespace] and visited[namespace][chain_mode]:
            return ResolveSearch()
        else:
            visited[namespace][chain_mode] = True
    else:
        visited[namespace] = {
            chain_mode: True
        }

    search = ResolveSearch()
    local_overloads = namespace.get_overloads(identifier)
    virtual = [c for c in local_overloads if c.is_virtual]
    modes = [None]

    if chain_mode == 'scope':
        if virtual:
            modes = ['sub', 'Sub']+modes

        modes += ['self', 'super', 'Self', 'Super', 'scope']
    elif not chain_mode in ('sub', 'Sub'):
        modes += [chain_mode]
    elif virtual:  # Sub/sub descend can only be continued if own virtual overloads exist
        modes = [chain_mode]+modes

    for mode in modes:
        if mode is None:
            search.overloads += local_overloads

            if namespace.observer is not None:
                search.observer = namespace.observer

                return search
        else:
            chained_namespace = getattr(namespace, mode, None)
            if chained_namespace is not None:
                if chained_namespace == namespace:
                    visited[namespace][mode] = True

                    continue

                chained_search = resolve(chained_namespace, identifier, mode, visited)
                search.overloads += chained_search.overloads

                if chained_search.observer is not None:
                    search.observer = chained_search.observer

                    return search
    return search

def select_best_candidate(candidates: List[FunctionCandidate], arg_types: List[str]) -> Optional[FunctionCandidate]:
    # Выбираем первый кандидат, для которого типы аргументов точно совпадают.
    for cand in candidates:
        if cand.matches(arg_types):
            return cand
    return None


def resolve_call(namespace: Namespace, identifier: str, arg_types: List[str]) -> FunctionCandidate:
    search = resolve(namespace, identifier, "scope")
    print()
    for i, candidate in enumerate(search.overloads):
        print(repr(i)+': '+repr(candidate)+', ')
    candidate = select_best_candidate(search.overloads, arg_types)
    if candidate is not None:
        return candidate
    if search.observer is not None:
        return search.observer
    raise Exception("Перегрузки не найдены или ни один кандидат не прошёл ранжирование")


# --- Утилита для определения строкового представления типа аргумента ---

def type_str(arg: Any) -> str:
    if isinstance(arg, bool):
        return "bool"
    elif isinstance(arg, int):
        return "int"
    elif isinstance(arg, str):
        return "string"
    elif isinstance(arg, list):
        return "array"
    else:
        return type(arg).__name__


def call_function(namespace: Namespace, identifier: str, *args):
    arg_types = [type_str(arg) for arg in args]
    try:
        candidate = resolve_call(namespace, identifier, arg_types)
        # Вызываем функцию кандидата (печатаем то, что печатается в теле)
        candidate.func(*args)
    except Exception as e:
        print("Ошибка:", e)


# namespace Global {
#     func test(a: bool) { print('zero') }
#
#     namespace A {
#         chain {
#             get { print('third') }
#         }
#
#         func test(a: array) { print('fifth') }
#
#         namespace B {
#             func test(a: int) { print('first') }
#             func test(a: string) { print('second') }
#         }
#     }
#
#     namespace C: A {
#         func test(a: int) { print('forth') }
#     }
# }
#
# // Global.scope == nil
# // A.scope == Global
# // B.scope == A
# // C.scope == Global
#
# // Global.Super == nil
# // A.Super == nil
# // B.Super == nil
# // C.Super == A
#
# Global.test(123) // ошибка - единственная перегрузка провалилась в ранжировании
# Global.test('abc') // ошибка - единственная перегрузка провалилась в ранжировании
# Global.test(true) // zero - победила в ранжировании
# Global.test([]) // ошибка - единственная перегрузка провалилась в ранжировании
# Global.test2 // ошибка - перегрузки не найдены
#
# A.test(123) // third - единственная перегрузка провалилась в ранжировании, наблюдатель перехватил запрос
# A.test('abc') // third - единственная перегрузка провалилась в ранжировании, наблюдатель перехватил запрос
# A.test(true) // third - единственная перегрузка провалилась в ранжировании, наблюдатель перехватил запрос
# A.test([]) // fifth - победила в ранжировании
# A.test2 // third - перегрузки не найдены, наблюдатель перехватил запрос
#
# B.test(123) // first - победила в ранжировании
# B.test('abc') // second - победила в ранжировании
# B.test(true) // third - три перегрузки провалились в ранжировании, наблюдатель перехватил запрос
# B.test([]) // fifth - победила в ранжировании
# B.test2 // third - перегрузки не найдены, наблюдатель перехватил запрос
#
# С.test(123) // forth - победила в ранжировании
# C.test('abc') // third - обе перегрузки провалились в ранжировании, наблюдатель перехватил запрос
# С.test(true) // third - обе перегрузки провалились в ранжировании, наблюдатель перехватил запрос
# C.test([]) // fifth - победила в ранжировании
# С.test2 // third - перегрузки не найдены, наблюдатель перехватил запрос

# Создаем пространства имён
Global = Namespace("Global")
A_ns = Namespace("A")
B_ns = Namespace("B")
C_ns = Namespace("C")

# Связываем области видимости и наследования для структуры A:
# Global.scope == None
A_ns.scope = Global
B_ns.scope = A_ns
C_ns.scope = Global

# Super (наследование)
Global.Super = None
A_ns.Super = None
B_ns.Super = None
C_ns.Super = A_ns

# Для простоты не задаем object chain: sub и super остаются None.
# Но для цепочки классов (downwards для виртуальных) можно задать Sub, если потребуется.
# Здесь для структуры A виртуальных методов нет.

# Добавляем функции в Global
Global.add_function(FunctionCandidate("test", ["bool"], lambda a: print("zero")))
# Наблюдателей в Global нет.

# В A: устанавливаем наблюдатель (общий для пространства)
A_ns.set_observer(FunctionCandidate("test_observer", [], lambda: print("third")))
# Добавляем перегрузку test(a: array) => "fifth"
A_ns.add_function(FunctionCandidate("test", ["array"], lambda a: print("fifth")))

# В B (вложено в A)
B_ns.add_function(FunctionCandidate("test", ["int"], lambda a: print("first")))
B_ns.add_function(FunctionCandidate("test", ["string"], lambda a: print("second")))

# В C (наследует от A)
C_ns.add_function(FunctionCandidate("test", ["int"], lambda a: print("forth")))

print("=== Структура A ===")
print("Global.test(123):", end=" ")
call_function(Global, "test", 123)      # 123 -> "int" -> не проходит тест "bool" => ошибка
print("Global.test('abc'):", end=" ")
call_function(Global, "test", "abc")      # "abc" -> "string" => ошибка
print("Global.test(True):", end=" ")
call_function(Global, "test", True)       # True -> "bool" => zero
print("Global.test([]):", end=" ")
call_function(Global, "test", [])         # [] -> "array" => Global не содержит array, ошибка
print("Global.test2:", end=" ")
try:
    call_function(Global, "test2")         # нет перегрузок => ошибка
except Exception as e:
    print("Ошибка:", e)

print("A.test(123):", end=" ")
call_function(A_ns, "test", 123)          # int не проходит array, поэтому срабатывает наблюдатель => third
print("A.test('abc'):", end=" ")
call_function(A_ns, "test", "abc")          # наблюдатель => third
print("A.test(True):", end=" ")
call_function(A_ns, "test", True)           # наблюдатель => third
print("A.test([]):", end=" ")
call_function(A_ns, "test", [])            # array проходит перегрузку => fifth
print("A.test2:", end=" ")
call_function(A_ns, "test2")               # нет перегрузок, наблюдатель срабатывает => third

print("B.test(123):", end=" ")
call_function(B_ns, "test", 123)           # int => first
print("B.test('abc'):", end=" ")
call_function(B_ns, "test", "abc")         # string => second
print("B.test(True):", end=" ")
call_function(B_ns, "test", True)          # ни один не проходит => перейдет наблюдатель из A (scope B.scope == A) => third
print("B.test([]):", end=" ")
call_function(B_ns, "test", [])            # array => fifth (найден в A)
print("B.test2:", end=" ")
call_function(B_ns, "test2")               # перегрузок нет => наблюдатель A => third

print("C.test(123):", end=" ")
call_function(C_ns, "test", 123)           # int => forth (локальная перегрузка в C)
print("C.test('abc'):", end=" ")
call_function(C_ns, "test", "abc")         # string => ни одна не проходит, наблюдатель из A (потому что C.Super==A) => third
print("C.test(True):", end=" ")
call_function(C_ns, "test", True)          # наблюдатель => third
print("C.test([]):", end=" ")
call_function(C_ns, "test", [])            # array => fifth (из A)
print("C.test2:", end=" ")
call_function(C_ns, "test2")               # перегрузок нет => наблюдатель => third

# namespace A {
#     virtual test() { print('first') }
# }
#
# namespace B: A {
#     virtual test() { print('second') }
# }
#
# namespace C: B {
#     test() { print('third') }
# }
#
# namespace D: C {
#     test() { print('forth') }
# }
#
# A.test() // third
# B.test() // third
# C.test() // third
# D.test() // forth

# Здесь мы моделируем цепочку наследования с виртуальными методами.
# Для упрощения создадим новые пространства имён.
A_v = Namespace("A_v")
B_v = Namespace("B_v")
C_v = Namespace("C_v")
D_v = Namespace("D_v")

# Устанавливаем область видимости – здесь можно не задавать scope, т.к. вызовы идут непосредственно по цепочке классов.
# Устанавливаем цепочку наследования:
A_v.Sub = B_v   # A_v → B_v
B_v.Sub = C_v   # B_v → C_v
C_v.Sub = D_v   # C_v → D_v
D_v.Sub = None

# Для наследования вверх (Super) можно задать обратную связь, если потребуется:
B_v.Super = A_v
C_v.Super = B_v
D_v.Super = C_v

# В A_v добавляем виртуальный test() => "first"
A_v.add_function(FunctionCandidate("test", [], lambda: print("first"), is_virtual=True))
# В B_v – виртуальный test() => "second"
B_v.add_function(FunctionCandidate("test", [], lambda: print("second"), is_virtual=True))
# В C_v – обычный (не виртуальный) test() => "third"
C_v.add_function(FunctionCandidate("test", [], lambda: print("third")))
# В D_v – обычный test() => "forth"
D_v.add_function(FunctionCandidate("test", [], lambda: print("forth")))

print("\n=== Структура B (виртуальные перегрузки) ===")
print("A_v.test():", end=" ")
call_function(A_v, "test")   # Ожидается "third"
print("B_v.test():", end=" ")
call_function(B_v, "test")   # Ожидается "third"
print("C_v.test():", end=" ")
call_function(C_v, "test")   # Ожидается "third"
print("D_v.test():", end=" ")
call_function(D_v, "test")   # Ожидается "forth"

# namespace Global {
#     func test(a: bool) { print("first") }
#
#     namespace A {
#         func test(a: int) { print("second") }
#
#         namespace C {}
#
#         namespace B: C {
#             func test(a: string) { print("third") }
#         }
#     }
# }
#
# B.test(true) // first
# B.test(123) // second
# B.test('hello') // third
# B.test([]) // ошибка

Global = Namespace('Global')
A = Namespace('A')
B = Namespace('B')
C = Namespace('C')

Global.Self = Global
A.Self = A
B.Self = B
C.Self = C

B.Super = C

A.scope = Global
B.scope = A
C.scope = A

# Добавление функций
Global.add_function(FunctionCandidate('test', ['bool'], lambda _: print("first")))
A.add_function(FunctionCandidate('test', ['int'], lambda _: print("second")))
B.add_function(FunctionCandidate('test', ['string'], lambda _: print("third")))

print("\n=== Структура C ===")
print("B.test(true) →", end=' ')
call_function(B, 'test', True)  # Должно быть "first"
print("B.test(123) →", end=' ')
call_function(B, 'test', 123)  # Должно быть "second"
print("B.test('hello') →", end=' ')
call_function(B, 'test', 'hello')  # Должно быть "third"
print("B.test([]) →", end=' ')
call_function(B, 'test', [])  # Должно быть "ошибка"