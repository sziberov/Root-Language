/*
a, b и c существуют:
a.b.c - ok
a?.b.c - ok
a.b?.c - ok
a.b.c? - ok
a?.b?.c - ok
a.b?.c? - ok
a?.b?.c? - ok

a и b существуют, c не существует:
a.b.c - MemberDoesntExist('c')
a?.b.c - MemberDoesntExist('c')
a.b?.c - MemberDoesntExist('c')
a.b.c? - nil
a?.b?.c - MemberDoesntExist('c')
a.b?.c? - nil
a?.b?.c? - nil

a существует, b и c не существуют:
a.b.c - MemberDoesntExist('b')
a?.b.c - MemberDoesntExist('b')
a.b?.c - MemberDoesntExist('c')
a.b.c? - MemberDoesntExist('b')
a?.b?.c - MemberDoesntExist('c')
a.b?.c? - nil
a?.b?.c? - nil

a, b и c не существуют:
a.b.c - MemberDoesntExist('a')
a?.b.c - MemberDoesntExist('b')
a.b?.c - MemberDoesntExist('c')
a.b.c? - MemberDoesntExist('b')
a?.b?.c - MemberDoesntExist('c')
a.b?.c? - MemberDoesntExist('a')
a?.b?.c? - nil
*/

class MemberDoesntExist extends Error {
  constructor(member) {
    super(`MemberDoesntExist('${member}')`);
    this.name = "MemberDoesntExist";
    this.member = member;
  }

  toString() {
      return this.name+'('+this.member+')';
  }
}

class Context {
  constructor(vars) {
    this.vars = vars;
  }

  resolve(name) {
    if (!(name in this.vars)) {
      throw new MemberDoesntExist(name);
    }
    return this.vars[name];
  }
}

// ------------------ Интерпретатор AST ------------------

function evalExpr(expr, ctx) {
  switch (expr.type) {
    case 'Identifier':
      return ctx.resolve(expr.name);

    case 'MemberExpression':
      return evalMember(expr, ctx);

    case 'NillableExpression':
      return evalNillable(expr, ctx);

    default:
      throw new Error(`Unknown expression type: ${expr.type}`);
  }
}

function evalMember(expr, ctx) {
  const obj = evalExpr(expr.object, ctx);
  if (obj == null || typeof obj !== 'object') {
    throw new MemberDoesntExist(expr.property);
  }
  if (!(expr.property in obj)) {
    throw new MemberDoesntExist(expr.property);
  }
  return obj[expr.property];
}

function evalNillable(expr, ctx) {
  try {
    return evalExpr(expr.base, ctx);
  } catch (e) {
    if (e instanceof MemberDoesntExist) {
      return null;
    }
    throw e;
  }
}

// ------------------ Утилита построения AST ------------------

function id(name) {
  return { type: 'Identifier', name };
}

function member(obj, prop) {
  return { type: 'MemberExpression', object: obj, property: prop };
}

function maybe(base) {
  return { type: 'NillableExpression', base };
}

// ------------------ Примеры ------------------

function test(name, expr, ctx) {
  try {
    const result = evalExpr(expr, ctx);
    console.log(`${name} → `+result);
  } catch (e) {
    console.log(`${name} → `+e.toString());
  }
}

function buildExamples(vars) {
  const ctx = new Context(vars);

  const exprs = {
    "a.b.c": member(member(id("a"), "b"), "c"),
    "a?.b.c": member(member(maybe(id("a")), "b"), "c"),
    "a.b?.c": member(maybe(member(id("a"), "b")), "c"),
    "a.b.c?": maybe(member(member(id("a"), "b"), "c")),
    "a?.b?.c": member(maybe(member(maybe(id("a")), "b")), "c"),
    "a.b?.c?": maybe(member(maybe(member(id("a"), "b")), "c")),
    "a?.b?.c?": maybe(member(maybe(member(maybe(id("a")), "b")), "c")),
  };

  for (const [name, expr] of Object.entries(exprs)) {
    test(name, expr, ctx);
  }

  console.log('---');
}

// ------------------ Сценарии ------------------

// 1. a, b, c существуют
buildExamples({
  a: { b: { c: "ok" } }
});

// 2. a и b существуют, c не существует
buildExamples({
  a: { b: {} }
});

// 3. a существует, b и c не существуют
buildExamples({
  a: {}
});

// 4. ничего не существует
buildExamples({});
