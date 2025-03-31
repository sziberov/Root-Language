let i_tokens,
	i_tree,
	i_composites,
	i_scopes,
	i_controlTransfer,
	i_preferences,
	i_reports;

let i_rules = {
	argument: (node, scope) => {
		return {
			label: node.label?.value,
			value: i_rules[node.value.type]?.(node.value, scope)
		}
	},
	arrayLiteral: (node, scope) => {
		let result,
			values = []

		for(let value of node.values) {
			value = i_rules[value.type]?.(value, scope);

			if(value != null) {
				values.push(value);
			}
		}

		let composite = i_valueGetComposite(i_findMember(scope, 'Array')?.value);

		if(composite != null) {
			// TODO: Instantinate Array()
		} else {
			result = i_valueCreate('dictionary', values);
		}

		return result;
	},
	arrayType: (n, s, t, tp) => {
		i_rules.collectionType(n, s, t, tp, 'array');
	},
	booleanLiteral: (node) => {
		// TODO: Instantinate Boolean()

		return i_valueCreate('boolean', node.value === 'true');
	},
	callExpression: (node, scope) => {
		let arguments_ = []

		for(let argument of node.arguments) {
			argument = i_rules[argument.type]?.(argument, scope);

			if(argument != null) {
				arguments_.push(argument);
			}
		}

		let function_ = i_valueFindFunction(i_rules[node.callee.type]?.(node.callee, scope), arguments_);

		if(function_ == null) {
			i_report(2, node, 'Composite is not a function or wasn\'t found.');

			return;
		}

		i_report(0, node, 'ca: '+function_.title+', '+function_.addresses.self);
		let value = i_compositeCallFunction(function_, arguments_);
		i_report(0, node, 'ke: '+JSON.stringify(value));

		return value;
	},
	chainExpression: (node, scope) => {
		let composite = i_valueGetComposite(i_rules[node.composite?.type](node.composite, scope));

		if(composite == null) {
			i_report(2, node, 'Composite wasn\'t found.');

			return;
		}

		let identifier = node.member;

		if(identifier.type === 'stringExpression') {
			identifier = i_rules.stringExpression?.(node.member, scope);
		} else {
			identifier = identifier.value;
		}

		return i_helpers.findMemberValue(composite, identifier, true);
	},
	classDeclaration: (node, scope) => {
		let modifiers = node.modifiers,
			identifier = node.identifier?.value;

		if(identifier == null) {
			return;
		}

		let inheritedTypes = node.inheritedTypes,
			composite = i_compositeCreateClass(identifier, scope);

		if(node.inheritedTypes.length > 0) {
			composite.type[0].inheritedTypes = true;

			for(let node_ of node.inheritedTypes) {
				i_helpers.createTypePart(composite.type, composite.type[0], node_, scope, false);
			}
			for(let typePart of composite.type) {
				i_compositeRetain(i_compositeGet(typePart.reference), composite);
			}
		}

		let type = [i_typeCreatePart(undefined, undefined, { predefined: 'Class', self: true })],
			value = i_valueCreate('reference', composite.addresses.self),
			observers = []

		i_setMember(scope, modifiers, identifier, type, value, observers);
		i_nodesEvaluate(node.body?.statements, composite);

		// TODO: Protocol conformance checking (if not conforms, remove from list and report)
	},
	collectionType: (node, scope, type, typePart, title) => {
		let capitalizedTitle = title[0].toUpperCase()+title.slice(1),
			composite = i_valueGetComposite(i_findMember(scope, capitalizedTitle)?.value);

		if(composite != null) {
			typePart.reference = composite.addresses.self;
		}

		typePart = i_helpers.createOrSetCollectionTypePart(type, typePart, { [composite != null ? 'genericArguments' : title]: true });

		if(title === 'dictionary') {
			i_helpers.createTypePart(type, typePart, node.key, scope);
		}

		i_helpers.createTypePart(type, typePart, node.value, scope);
	},
	combiningType: (node, scope, type, typePart, title) => {
		if(node.subtypes.length === 0) {
			return;
		}

		typePart = i_helpers.createOrSetCollectionTypePart(type, typePart, { [title]: true });

		for(let subtype of node.subtypes) {
			i_helpers.createTypePart(type, typePart, subtype, scope);
		}
	},
	defaultType: (n, s, t, tp) => {
		i_rules.optionalType(n, s, t, tp, ['default', 'nillable']);
	},
	dictionaryLiteral: (node, scope) => {
		let result,
			entries = new Map();

		for(let entry of node.entries) {
			entry = i_rules.entry(entry, scope);

			if(entry != null) {
				entries.set(entry.key, entry.value);
			}
		}

		let composite = i_valueGetComposite(i_findMember(scope, 'Dictionary')?.value);

		if(composite != null) {
			// TODO: Instantinate Dictionary()
		} else {
			result = i_valueCreate('dictionary', entries);
		}

		return result;
	},
	dictionaryType: (n, s, t, tp) => {
		i_rules.collectionType(n, s, t, tp, 'dictionary');
	},
	entry: (node, scope) => {
		return {
			key: i_rules[node.key.type]?.(node.key, scope),
			value: i_rules[node.value?.type]?.(node.value, scope)
		}
	},
	enumerationDeclaration: (node, scope) => {
		let modifiers = node.modifiers,
			identifier = node.identifier.value,
			composite = i_compositeCreateEnumeration(identifier, scope),
			type = [i_typeCreatePart(undefined, undefined, { predefined: 'Enumeration', self: true })],
			value = i_valueCreate('reference', composite.addresses.self),
			observers = []

		i_setMember(scope, modifiers, identifier, type, value, observers);
		i_nodesEvaluate(node.body?.statements, composite);
	},
	expressionsSequence: (node, scope) => {
		if(node.values.length === 3 && node.values[1].type === 'infixOperator' && node.values[1].value === '=') {
			let lhs = node.values[0],
				lhsComposite,
				lhsMember;

			if(lhs.type === 'chainExpression') {
				lhsComposite = i_valueGetComposite(i_rules[lhs.composite.type]?.(lhs.composite, scope));
				lhsMember = lhs.member.type === 'identifier' ? lhs.member.value : i_rules.stringLiteral(lhs.member, scope, true).primitiveValue;

				if(lhsComposite == null || lhsMember == null) {
					return;
				}

				lhs = i_findMember(lhsComposite, lhsMember, true);
			}
			if(lhs.type === 'identifier') {
				lhsComposite = scope;
				lhsMember = lhs.value;

				lhs = i_findMember(lhsComposite, lhsMember);
			}

			// TODO: Create member with default type if not exists

			if(lhs == null) {
				return;
			}

			let rhs = i_rules[node.values[2].type]?.(node.values[2], scope);

			i_setMember(lhsComposite, lhs.modifiers, lhsMember, lhs.type, rhs, lhs.observers);

			return rhs;
		}
	},
	floatLiteral: (node) => {
		// TODO: Instantinate Float()

		return i_valueCreate('float', node.value*1);
	},
	functionDeclaration: (node, scope) => {
		let modifiers = node.modifiers,
			identifier = node.identifier?.value;

		if(identifier == null) {
			return;
		}

		let function_ = i_compositeCreateFunction(identifier, node.body?.statements, scope),
			signature = i_rules.functionSignature(node.signature, scope),
			type = [],
			value = i_valueCreate('reference', function_.addresses.self),
			observers = [],
			member = i_getMember(scope, identifier);

		function_.type = signature;

		if(member == null) {
			i_typeCreatePart(type, undefined, { predefined: 'Function' });
		} else {
			modifiers = member.modifiers;

			if(member.type[0]?.predefined !== 'dict') {
				i_typeCreatePart(type, undefined, { predefined: 'dict' });

				value = i_valueCreate('dictionary', [member.value, value]);
			} else {
				type = member.type;
				value = i_valueCreate('dictionary', [...member.value.primitiveValue, value]);
			}
		}

		i_setMember(scope, modifiers, identifier, type, value, observers);
	},
	functionExpression: (node, scope) => {
		let signature = i_rules.functionSignature(node.signature, scope),
			function_ = i_compositeCreateFunction(undefined, node.body?.statements, scope);

		function_.type = signature;

		return i_valueCreate('reference', function_.addresses.self);
	},
	functionSignature: (node, scope) => {
		let type = [],
			typePart = i_typeCreatePart(type, undefined, { predefined: 'Function' });

		typePart.awaits = node?.awaits ?? -1;
		typePart.throws = node?.throws ?? -1;

		for(let v of ['genericParameters', 'parameters']) {
			if(node?.[v].length > 0) {
				let typePart_ = i_typeCreatePart(type, typePart, { [v]: true });

				for(let node_ of node[v]) {
					i_rules[node_.type]?.(node_, scope, type, typePart_);
				}
			}
		}

		i_helpers.createTypePart(type, typePart, node?.returnType, scope);

		return type;
	},
	functionType: (node, scope, type, typePart) => {
		typePart = i_helpers.createOrSetCollectionTypePart(type, typePart, { predefined: 'Function' });

		typePart.awaits = node.awaits ?? 0;
		typePart.throws = node.throws ?? 0;

		for(let v of ['genericParameter', 'parameter']) {
			if(node[v+'Types'].length > 0) {
				let typePart_ = i_typeCreatePart(type, typePart, { [v+'s']: true });

				for(let node_ of node[v+'Types']) {
					i_helpers.createTypePart(type, typePart_, node_, scope);
				}
			}
		}

		i_helpers.createTypePart(type, typePart, node.returnType, scope);
	},
	genericParameter: (node, scope, type, typePart) => {
		typePart = i_helpers.createTypePart(type, typePart, node.type_, scope);

		typePart.identifier = node.identifier.value;
	},
	identifier: (node, scope) => {
		return i_helpers.findMemberValue(scope, node.value);
	},
	ifStatement: (node, scope) => {
		if(node.condition == null) {
			return;
		}

		let namespace = i_compositeCreateNamespace('Local<'+(scope.title ?? '#'+scope.addresses.self)+', If>', scope),
			condition;

		i_scopeAdd(namespace);

		condition = i_rules[node.condition.type]?.(node.condition, namespace);
		condition = condition?.primitiveType === 'boolean' ? condition.primitiveValue : condition != null;

		if(condition || node.else?.type !== 'ifStatement') {
			let branch = condition ? 'then' : 'else';

			if(node[branch]?.type === 'functionBody') {
				i_nodesEvaluate(node[branch].statements, namespace);
			} else {
				i_controlTransferSet(i_rules[node[branch]?.type]?.(node[branch], namespace));
			}
		}

		i_scopeRemove();

		if(i_controlTransfer?.type === 'returnStatement') {
			return i_controlTransfer.value;
		}

		if(!condition && node.else?.type === 'ifStatement') {
			i_rules.ifStatement(node.else, scope);
		}

		return i_controlTransfer?.value;
	},
	inoutExpression: (node, scope) => {
		let value = i_rules[node.value?.type]?.(node.value, scope);

		if(value?.primitiveType === 'pointer') {
			return value;
		}
		if(value?.primitiveType !== 'reference') {
			i_report(2, node, 'Non-reference value ("'+(value?.primitiveType ?? 'nil')+'") can\'t be used as a pointer.');

			return;
		}

		return i_valueCreate('pointer', value.primitiveValue);
	},
	inoutType: (n, s, t, tp) => {
		i_rules.optionalType(n, s, t, tp, ['inout']);
	},
	integerLiteral: (node) => {
		// TODO: Instantinate Integer()

		return i_valueCreate('integer', node.value*1);
	},
	intersectionType: (n, s, t, tp) => {
		i_rules.combiningType(n, s, t, tp, 'intersection');
	},
	module: () => {
		let namespace = i_compositeGet(0) ?? i_compositeCreateNamespace('Global');

		i_scopeAdd(namespace);
		i_nodesEvaluate(i_tree?.statements, namespace);
		i_scopeRemove();
		i_controlTransferReset();
	},
	namespaceDeclaration: (node, scope) => {
		let modifiers = node.modifiers,
			identifier = node.identifier.value,
			composite = i_compositeCreateNamespace(identifier, scope),
			type = [i_typeCreatePart(undefined, undefined, { predefined: 'Namespace', self: true })],
			value = i_valueCreate('reference', composite.addresses.self),
			observers = []

		i_setMember(scope, modifiers, identifier, type, value, observers);
		i_nodesEvaluate(node.body?.statements, composite);
	},
	nillableType: (n, s, t, tp) => {
		i_rules.optionalType(n, s, t, tp, ['default', 'nillable'], 1);
	},
	nilLiteral: () => {},
		optionalType: (node, scope, type, typePart, titles, mainTitle) => {
		if(!titles.some(v => v in typePart)) {
			typePart[titles[mainTitle ?? 0]] = true;
		}

		i_rules[node.value?.type]?.(node.value, scope, type, typePart);
	},
	parameter: (node, scope, type, typePart) => {
		typePart = i_helpers.createTypePart(type, typePart, node.type_, scope);

		if(node.label != null) {
			typePart.label = node.label.value;
		}

		typePart.identifier = node.identifier.value;

		if(node.value != null) {
			typePart.value = node.value;
		}
	},
	parenthesizedExpression: (node, scope) => {
		return i_rules[node.value?.type]?.(node.value, scope);
	},
	parenthesizedType: (node, scope, type, typePart) => {
		i_rules[node.value?.type]?.(node.value, scope, type, typePart);
	},
	postfixExpression: (node, scope) => {
		let value = i_rules[node.value?.type]?.(node.value, scope);

		if(value == null) {
			return;
		}

		if(node.operator?.value === '++' && value.primitiveType === 'integer') {
			value.primitiveValue++;

			return i_valueCreate('integer', value.primitiveValue-1);
		}
		if(node.operator?.value === '--' && value.primitiveType === 'integer') {
			value.primitiveValue--;

			return i_valueCreate('integer', value.primitiveValue+1);
		}

		//TODO: Dynamic operators lookup

		return value;
	},
	predefinedType: (node, scope, type, typePart) => {
		typePart.predefined = node.value;
	},
	prefixExpression: (node, scope) => {
		let value = i_rules[node.value?.type]?.(node.value, scope);

		if(value == null) {
			return;
		}

		if(node.operator?.value === '!' && value.primitiveType === 'boolean') {
			return i_valueCreate('boolean', !value.primitiveValue);
		}
		if(node.operator?.value === '++' && value.primitiveType === 'integer') {
			value.primitiveValue++;

			return i_valueCreate('integer', value.primitiveValue);
		}
		if(node.operator?.value === '--' && value.primitiveType === 'integer') {
			value.primitiveValue--;

			return i_valueCreate('integer', value.primitiveValue);
		}

		//TODO: Dynamic operators lookup

		return value;
	},
	protocolDeclaration: (node, scope) => {
		let modifiers = node.modifiers,
			identifier = node.identifier.value,
			composite = i_compositeCreateProtocol(identifier, scope),
			type = [i_typeCreatePart(undefined, undefined, { predefined: 'Protocol', self: true })],
			value = i_valueCreate('reference', composite.addresses.self),
			observers = []

		i_setMember(scope, modifiers, identifier, type, value, observers);
		i_nodesEvaluate(node.body?.statements, composite);
	},
	protocolType: (node, scope, type, typePart) => {

	},
	returnStatement: (node, scope) => {
		return i_rules[node.value?.type]?.(node.value, scope);
	},
	stringLiteral: (node, scope, primitive) => {
		let string = '';

		for(let segment of node.segments) {
			if(segment.type === 'stringSegment') {
				string += segment.value;
			} else {
				let value = i_rules[segment.value?.type]?.(segment.value, scope);

				if(['boolean', 'float', 'integer', 'string'].includes(value?.primitiveType)) {
					value = value.primitiveValue;
				} else
				if(value != null) {
					value = '[not implemented]';  // TODO: Primitives to string conversion
				}

				string += value ?? '';
			}
		}

		// TODO: Instantinate String()

		return i_valueCreate('string', string);
	},
	structureDeclaration: (node, scope) => {
		let modifiers = node.modifiers,
			identifier = node.identifier?.value;

		if(identifier == null) {
			return;
		}

		let composite = i_compositeCreateStructure(identifier, scope),
			type = [i_typeCreatePart(undefined, undefined, { predefined: 'Structure', self: true })],
			value = i_valueCreate('reference', composite.addresses.self),
			observers = []

		i_setMember(scope, modifiers, identifier, type, value, observers);
		i_nodesEvaluate(node.body?.statements, composite);
	},
	typeIdentifier: (node, scope, type, typePart) => {
		let composite = i_valueGetComposite(i_rules.identifier(node.identifier, scope));

		if(composite == null || composite.type[0]?.predefined === 'Object') {
			typePart.predefined = 'Any';

			i_report(2, node, 'Composite is an object or wasn\'t found.');
		} else {
			typePart.reference = composite?.addresses.self;
		}

		if(node.genericArguments.length === 0) {
			return;
		}

		typePart = i_helpers.createOrSetCollectionTypePart(type, typePart, { genericArguments: true });

		for(let genericArgument of node.genericArguments) {
			i_helpers.createTypePart(type, typePart, genericArgument, scope);
		}
	},
	unionType: (n, s, t, tp) => {
		i_rules.combiningType(n, s, t, tp, 'union');
	},
	variableDeclaration: (node, scope) => {
		let modifiers = node.modifiers;

		for(let declarator of node.declarators) {
			let identifier = declarator.identifier.value,
				type = [],
				typePart = i_helpers.createTypePart(type, undefined, declarator.type_, scope),
				value = i_rules[declarator.value?.type]?.(declarator.value, scope),
				observers = []

			// Type-related checks

			i_setMember(scope, modifiers, identifier, type, value, observers);
		}
	},
	variadicGenericParameter: (node, scope) => {
		return {
			identifier: undefined,
			type: [i_typeCreatePart(undefined, undefined, { variadic: true })]
		}
	},
	variadicType: (n, s, t, tp) => {
		i_rules.optionalType(n, s, t, tp, ['variadic']);
	}
}

let i_helpers = {
	createOrSetCollectionTypePart: (type, typePart, collectionFlag) => {
		let collectionFlags = {
			array: true,
			dictionary: true,
			genericArguments: true,
			genericParameters: true,
			intersection: true,
			parameters: true,
			predefined: 'Function',
			union: true
		}

		for(let flag in typePart) {
			if(
				flag in collectionFlags &&
				typePart[flag] === collectionFlags[flag]
			) {
				return i_typeCreatePart(type, typePart, collectionFlag);
			}
		}

		return Object.assign(typePart, collectionFlag);
	},
	createTypePart: (type, typePart, node, scope, fallback = true) => {
		typePart = i_typeCreatePart(type, typePart);

		i_rules[node?.type]?.(node, scope, type, typePart);

		if(fallback) {
			for(let flag in typePart) {
				if(flag !== 'super') {
					return typePart;
				}
			}

			typePart.predefined = '_';
			typePart.nillable = true;
		}

		return typePart;
	},
	findMemberValue: (composite, identifier, internal) => {
		let address = {
			global: () => undefined,				 // Global-object is no thing
			Global: () => 0,						 // Global-type
			metaSelf: () => undefined,				 // Self-object or a type (descriptor)
			self: () => composite.addresses.self,	 // Self-object or a type
			Self: () => composite.addresses.Self,	 // Self-type
			super: () => composite.addresses.super,  // Super-object or a type
			Super: () => composite.addresses.Super,  // Super-type
			sub: () => composite.addresses.sub,		 // Sub-object
			Sub: () => composite.addresses.Sub,		 // Sub-type
			arguments: () => undefined				 // Function arguments array (needed?)
		}[identifier]?.();

		if(address != null) {
			return i_valueCreate('reference', address);
		}

		// TODO: Access-related checks

		return i_findMember(composite, identifier, internal)?.value;
	}
}

function i_serializeMemory() /*getSave()*/ {
	// TODO: Should support references to AST nodes (many references - one definition)

	/*
	return {
		tokens: JSON.stringify(i_tokens),
		tree: JSON.stringify(i_tree),
		composites: JSON.stringify(i_composites),
		calls: JSON.stringify(i_scopes),
		preferences: JSON.stringify(i_preferences),
		reports: JSON.stringify(i_reports)
	}
	*/

	return JSON.stringify(i_composites, (k, v) => {
		if(v instanceof Map) {
			return {
				__TYPE__: 'Map',
				__VALUE__: Array.from(v.entries())
			};
		} else
		if(v == null) {
			return null;
		} else {
			return v;
		}
	});
}

function i_deserializeMemory(serializedMemory) /*restoreSave()*/ {}

function i_compositeCreate(title, type, scope) {
	let composite = {
		title: title,
		addresses: {
			self: undefined,
			Self: undefined,
			super: undefined,
			Super: undefined,
			sub: undefined,
			Sub: undefined,
			scope: undefined,
			retainers: []
		},
		type: type,
		statements: [],
		imports: {},
		operators: {},
		members: {},
		observers: []
	}

	i_composites.push(composite);

	composite.addresses.self =
	composite.addresses.Self = i_composites.length-1;

	if(type != null && type[0].predefined !== 'Object') {  // TODO: Setter functions for addresses
		composite.addresses.super =
		composite.addresses.Super = type[1]?.reference;

		i_compositeRetain(i_compositeGet(composite.addresses.Super), composite);
	}

	composite.addresses.scope = scope?.addresses.self;

	i_compositeRetain(scope, composite);
	i_report(0, undefined, 'cr: '+composite.title+', '+composite.addresses.self);

	return composite;
}

function i_compositeDestroy(composite) {
	i_report(0, undefined, 'de: '+JSON.stringify(composite));

	// TODO:
	// - Notify all retainers about destroying
	// - Call deinitializer

	delete i_composites[composite.addresses.self]

	for(let composite_ of i_composites) {
		if(composite_?.addresses.retainers.includes(composite.addresses.self)) {
			i_compositeRelease(composite_, composite);
		}
	}
}

function i_compositeDestroyReleased(composite) {
	if(composite != null && !i_compositeRetained(composite)) {
		i_compositeDestroy(composite);
	}
}

function i_compositeGet(address) {
	return i_composites[address]
}

function i_compositeRetain(retainedComposite, retainingComposite) {
	if(retainedComposite == null) {
		return;
	}

	if(!retainedComposite.addresses.retainers.includes(retainingComposite.addresses.self)) {
		retainedComposite.addresses.retainers.push(retainingComposite.addresses.self);
	}
}

function i_compositeRelease(retainedComposite, retainingComposite) {
	if(retainedComposite == null) {
		return;
	}

	retainedComposite.addresses.retainers = retainedComposite.addresses.retainers.filter(v => v !== retainingComposite.addresses.self);

	i_compositeDestroyReleased(retainedComposite);
}

function i_compositeRetainOrRelease(retainedComposite, retainingComposite) {
	if(retainedComposite == null) {
		return;
	}

	if(i_compositeRetains(retainedComposite, retainingComposite)) {
		i_compositeRetain(retainedComposite, retainingComposite);
	} else {
		i_compositeRelease(retainedComposite, retainingComposite);
	}
}

/*
 * Returns a real state of retainment.
 *
 * Composite considered really retained if it is used by an at least one of
 * retainer's addresses (excluding "self" and retainers list), type, import, member or observer.
 */
function i_compositeRetains(retainedComposite, retainingComposite) {
	if(
		retainedComposite?.addresses.self === retainingComposite?.addresses.self ||
		i_composites[retainingComposite?.addresses.self] == null
	) {
		return;
	}

	return (
		i_compositeAddressesRetain(retainedComposite, retainingComposite) ||
		i_typeRetains(retainedComposite, retainingComposite.type) ||
		i_importRetains(retainedComposite, retainingComposite) ||
		i_memberRetains(retainedComposite, retainingComposite) ||
		i_observerRetains(retainedComposite, retainingComposite)
	);
}

/*
 * Returns a significant state of retainment.
 *
 * Composite considered significantly retained if it is reachable from
 * the global namespace, a current scope namespace or a return value.
 */
function i_compositeRetained(composite) {
	return (
		i_compositeReachable(composite, i_compositeGet(0)) ||
		i_compositeReachable(composite, i_scopeGet().namespace) ||
		i_compositeReachable(composite, i_valueGetComposite(i_controlTransfer?.value))
	);
}

/*
 * Returns true if the retainedComposite is reachable from the retainingComposite,
 * recursively checking its retainers addresses.
 *
 * retainChain is used to exclude a retain cycles and meant to be set internally only.
 */
function i_compositeReachable(retainedComposite, retainingComposite, retainChain = []) {
	if(retainedComposite == null || retainingComposite == null) {
		return;
	}
	if(retainedComposite.addresses.self === retainingComposite.addresses.self) {
		return true;
	}

	for(let retainerAddress of retainedComposite.addresses.retainers) {
		if(retainChain.includes(retainerAddress)) {
			continue;
		}

		retainChain.push(retainerAddress);

		if(i_compositeReachable(i_compositeGet(retainerAddress), retainingComposite, retainChain)) {
			return true;
		}
	}
}

function i_compositeCreateClass(title, scope) {
	return i_compositeCreate(title, [{ predefined: 'Class' }], scope);
}

function i_compositeCreateEnumeration(title, scope) {
	return i_compositeCreate(title, [{ predefined: 'Enumeration' }], scope);
}

function i_compositeCreateFunction(title, statements, scope) {
	let function_ = i_compositeCreate(title, [{ predefined: 'Function' }], scope);

	i_compositeSetStatements(function_, statements);

	return function_;
}

function i_compositeCreateNamespace(title, scope) {
	return i_compositeCreate(title, [{ predefined: 'Namespace' }], scope);
}

function i_compositeCreateObject(scope) {
	let title = (scope.title ?? '#'+scope.addresses.self)+'()';

	return i_compositeCreate(title, [{ predefined: 'Object' }], scope);
}

function i_compositeCreateProtocol(title, scope) {
	return i_compositeCreate(title, [{ predefined: 'Protocol' }], scope);
}

function i_compositeCreateStructure(title, scope) {
	return i_compositeCreate(title, [{ predefined: 'Structure' }], scope);
}

function i_compositeBindFunction(function_, scope) {}

/*
 * Forwarding allows a function's statements to be evaluated in its scope directly.
 *
 * If no scope is specified, default function's scope is used.
 */
function i_compositeCallFunction(function_, arguments_, forwarded, scope) {
	if(typeof function_.statements === 'function') {
		return i_compositeCallNativeFunction(function_, arguments_);
	}

	scope ??= i_compositeGet(function_.addresses.scope);

	let namespaceTitle = 'Call<'+(function_.title ?? '#'+function_.addresses.self)+'>',
		namespace = !forwarded ? i_compositeCreateNamespace(namespaceTitle, scope) : scope,
		parameters = i_typeGetFunctionParameters(function_.type);

	for(let i = 0; i < (arguments_ ?? []).length; i++) {
		let argument = arguments_[i],
			parameterType = i_typeGetSubtype(parameters, parameters[i]),
			identifier = parameterType[0]?.identifier ?? '$'+i;

		i_setMember(namespace, [], identifier, parameterType, argument.value, []);
	}

	i_scopeAdd(namespace, function_);
	i_nodesEvaluate(function_.statements, namespace);
	i_scopeRemove();

	let returnValue = i_controlTransfer?.value;

	i_controlTransferReset();

	return returnValue;
}

function i_compositeCallNativeFunction(function_, arguments_) {}

function i_typeGetFunctionParameters(functionType) {
	let parameters = i_typeGetSubtype(functionType, functionType.find(v => v.parameters && v.super === 0));

	parameters.shift();

	return parameters;
}

function i_scopeGet(offset = 0) {
	return i_scopes[i_scopes.length-1+offset]
}

/*
 * Should be used for function bodies before evaluating its contents,
 * so ARC can correctly detect any references.
 *
 * Additionally function itself can be specified for debugging purposes.
 */
function i_scopeAdd(namespace, function_) {
	i_scopes.push({
		namespace: namespace,
		function: function_
	});
}

/*
 * Removes from the stack and automatically destroys its last scope.
 */
function i_scopeRemove() {
	let namespace = i_scopes.at(-1)?.namespace;

	i_scopes.pop();
	i_compositeDestroyReleased(namespace);
}

/*
 * Should be used for statements inside of a bodies.
 *
 * Last statement in a body will be treated like a returning one even if it's not an explicit return.
 * Manual control transfer is supported but should be also implemented by rules.
 */
function i_nodesEvaluate(nodes, scope) {
	let globalScope = i_scopeGet(-1) == null,
		controlTransferTypes = [
			'breakStatement',
			'continueStatement',
			'returnStatement'
		]

	for(let node of nodes ?? []) {
		let start = i_composites.length,
			value = i_rules[node.type]?.(node, scope),
			end = i_composites.length,
			returned = i_controlTransfer != null && i_controlTransfer.value === value,
			returning = controlTransferTypes.includes(node.type) || node === nodes.at(-1);

		if(!returned && returning) {
			i_controlTransferSet(!globalScope ? value : undefined, node.type);
		}

		for(start; start < end; start++) {
			i_compositeDestroyReleased(i_compositeGet(start));
		}

		if(controlTransferTypes.includes(i_controlTransfer?.type)) {
			break;
		}
	}
}

function i_controlTransferSet(value, type) {
	i_controlTransfer = {
		value: value,
		type: type
	}
}

function i_controlTransferReset() {
	i_controlTransfer = undefined;
}

function i_compositeAddressesRetain(retainedComposite, retainingComposite) {
	for(let key in retainingComposite.addresses) {
		if(
			key !== 'Self' && key !== 'retainers' &&
			retainingComposite.addresses[key] === retainedComposite.addresses.self
		) {
			return true;
		}
	}
}

function i_typeGetParentAddress(type) {
	return type.find(v => v.super === 0)?.reference;
}

function i_typeCreatePart(type, superTypePart, flags) {
	let typePart = { ...flags }

	if(type != null) {
		if(superTypePart != null) {
			typePart.super = type.indexOf(superTypePart);
		}

		type.push(typePart);
	}

	return typePart;
}

function i_typeGetSubtype(type, typePart, offset) {
	offset ??= type.indexOf(typePart);

	if(offset < 0) {
		return []
	}
	if(offset === 0) {
		return structuredClone(type);
	}

	let subtype = [],
		typePart_ = structuredClone(typePart);

	typePart_.super -= offset;

	if(typePart_.super < 0) {
		delete typePart_.super;
	}

	subtype.push(typePart_);

	for(let typePart_ of type) {
		if(typePart_.super === type.indexOf(typePart)) {
			subtype.push(...i_typeGetSubtype(type, typePart_, offset));
		}
	}

	return subtype;
}

function i_typeAccepts(acceptedType, acceptingType) {
	// TODO: Type equality check

	return true;
}

function i_typeRetains(retainedComposite, retainingType) {
	return retainingType.some(v => v.reference === retainedComposite.addresses.self);
}

function i_valueCreate(primitiveType, primitiveValue) {
	return {
		primitiveType: primitiveType,	// 'boolean', 'dictionary', 'float', 'integer', 'node', 'pointer', 'reference', 'string'
		primitiveValue: primitiveValue	// boolean, integer, map (object), string, AST node
	}
}

function i_valueGetType(value) {
	if(value == null) {
		return []
	}

	let typePart = i_typeCreatePart();

	if(!['pointer', 'reference'].includes(value.primitiveType)) {
		typePart.predefined = value.primitiveType;
	} else {
		if(value.primitiveType === 'pointer') {
			typePart.inout = true;
		}

		typePart.reference = value.primitiveValue;

		let composite = i_valueGetComposite(value);

		while(composite != null) {
			if(composite.type[0]?.predefined !== 'Object') {
				if(composite.addresses.self === value.primitiveValue) {
					typePart.self = true;
				} else {
					typePart.reference = composite.addresses.self;
				}

				composite = undefined;
			} else {
				composite = i_compositeGet(composite.addresses.scope);
			}
		}
	}

	return [typePart]
}

function i_valueGetComposite(value) {
	if(!['pointer', 'reference'].includes(value?.primitiveType)) {
		return;
	}

	return i_compositeGet(value.primitiveValue);
}

function i_valueGetFunction(value, arguments_) {
	let function_ = i_valueGetComposite(value);

	if(function_?.type[0]?.predefined !== 'Function') {
		return;
	}

	let parameters = i_typeGetFunctionParameters(function_.type);

	if(arguments_.length !== parameters.length) {
		// TODO: Variadic parameters support

		return;
	}

	for(let i = 0; i < (arguments_ ?? []).length; i++) {
		let argument = arguments_[i],
			parameter = parameters[i]

		if(parameter.label != null && argument.label !== parameter.label) {
			return;
		}
		if(!i_typeAccepts(i_valueGetType(argument.value), parameter)) {
			return;
		}
	}

	return function_;
}

function i_valueFindFunction(value, arguments_) {
	let function_;

	if(value?.primitiveType === 'dictionary') {
		for(let key in value.primitiveValue) {
			if((function_ = i_valueGetFunction(value.primitiveValue[key], arguments_)) != null) {
				break;
			}
		}
	} else {
		function_ = i_valueGetFunction(value, arguments_);
	}

	return function_;
}

function i_valueRetainOrReleaseComposites(retainingValue, retainingComposite) {
	if(retainingValue?.primitiveType === 'dictionary') {
		for(let key in retainingValue.primitiveValue) {
			i_valueRetainOrReleaseComposites(retainingValue.primitiveValue[key], retainingComposite);
		}
	} else {
		i_compositeRetainOrRelease(i_valueGetComposite(retainingValue), retainingComposite);
	}
}

function i_valueRetains(retainedComposite, retainingValue) {
	if(retainingValue?.primitiveType === 'dictionary') {
		for(let key in retainingValue.primitiveValue) {
			let retainingValue_ = retainingValue.primitiveValue[key]

			if(i_valueRetains(retainedComposite, retainingValue_)) {
				return true;
			}
		}
	} else {
		return i_valueGetComposite(retainingValue) === retainedComposite;
	}
}

function i_compositeSetStatements(composite, statements) {
	if(composite.type[0]?.predefined !== 'Function') {
		return;
	}

	composite.statements = statements;
}

function i_compositeFindImports(composite, identifier) {
	while(composite != null) {
		let import_ = i_getImport(composite, identifier);

		if(import_ != null) {
			return import_;
		}

		composite = i_compositeGet(composite.addresses.scope);
	}
}

function i_getImport(composite, identifier) {
	return composite.imports[identifier]
}

function i_setImport(composite, identifier, value) {
	if(composite.type[0]?.predefined !== 'Namespace') {
		return;
	}

	composite.imports[identifier] = value;
}

function i_deleteImport(composite, identifier) {
	let value = i_compositeGet(i_getImport(composite, identifier));

	delete composite.imports[identifier]

	i_compositeRetainOrRelease(value, composite);
}

function i_importRetains(retainedComposite, retainingComposite) {
	for(let identifier in retainingComposite.imports) {
		if(retainingComposite.imports[identifier] === retainedComposite.addresses.self) {
			return true;
		}
	}
}

function i_findOperator(composite, identifier) {
	while(composite != null) {
		let operator = i_getOperator(composite, identifier);

		if(operator != null) {
			return operator;
		}

		composite = i_compositeGet(composite.addresses.scope);
	}
}

function i_getOperator(composite, identifier) {
	return composite.operators[identifier]
}

function i_setOperator(composite, modifiers, identifier, associativity, precedence) {
	if(composite.type[0]?.predefined !== 'Namespace') {
		return;
	}

	let operator = i_getOperator(composite, identifier) ?? (composite.operators[identifier] = {});

	operator.modifiers = modifiers;
	operator.associativity = associativity;
	operator.precedence = precedence;
}

function i_deleteOperator(composite, identifier) {
	delete composite.operators[identifier]
}

/*
 * Search order:
 *
 * 1. Current.(Lowest).Higher.Higher...  Object chain (parentheses - member is virtual)
 * 2. Current.Parent.Parent...           Inheritance chain
 * 3. Current.Scope.Scope...             Scope chain (search is not internal)
 */
function i_findMember(composite, identifier, internal) {
	return (
		i_findMemberInObjectChain(composite, identifier) ??
		i_findMemberInInheritanceChain(composite, identifier) ??
		!internal ? i_findMemberInScopeChain(composite, identifier) : undefined
	);
}

function i_findMemberInObject(object, identifier) {
	if(object.type[0]?.predefined !== 'Object') {
		return;
	}

	let member = i_getMember(object, identifier);

	if(member != null) {
		let member_ = i_findMemberInInheritanceChain(object, identifier);

		if(member_ == null) {
			return;
		}

		member = {
			modifiers: member_.modifiers,
			identifier: identifier,
			type: member_.type,
			value: member.value,
			observers: member_.observers
		}
	}

	return member;
}

function i_findMemberInObjectChain(object, identifier) {
	let member,
		virtual;

	while(object != null) {  // Higher
		member = i_findMemberInObject(object, identifier);

		if(member != null) {
			if(!virtual && member.modifiers.includes('virtual')) {
				virtual = true;

				let object_;

				while((object_ = i_compositeGet(object.addresses.sub)) != null) {  // Lowest
					object = object_;
				}

				continue;
			}

			break;
		}

		object = i_compositeGet(object.addresses.super);
	}

	return member;
}

function i_findMemberInInheritanceChain(composite, identifier) {
	let member;

	while(composite != null) {
		member = i_getMember(composite, identifier);

		if(member != null) {
			break;
		}

		composite = i_compositeGet(composite.addresses.Super);
	}

	return member;
}

function i_findMemberInScopeChain(composite, identifier) {
	let member;

	while(composite != null) {
		member = i_getMember(composite, identifier);

		if(member != null) {
			break;
		}

		// TODO: In-imports lookup

		composite = i_compositeGet(composite.addresses.scope);
	}

	return member;
}

function i_getMember(composite, identifier) {
	return composite.members[identifier]
}

function i_setMember(composite, modifiers, identifier, type, value, observers) {
	let member = i_getMember(composite, identifier) ?? (composite.members[identifier] = {}),
		ot = member.type ?? [],  // Old/new type
		nt = type ?? [],
		ov = member.value,  // Old/new value
		nv = value;

	member.modifiers = modifiers;
	member.type = type;
	member.value = value;
	member.observers = observers;

	if(ot != nt) {
		let addresses = new Set([...ot, ...nt].map(v => v.reference));

		for(let address of addresses) {
			i_compositeRetainOrRelease(i_compositeGet(address), composite);
		}
	}
	if(ov !== nv) {
		i_valueRetainOrReleaseComposites(ov, composite);
		i_valueRetainOrReleaseComposites(nv, composite);
	}
}

function i_deleteMember(composite, identifier) {
	let member = i_getMember(composite, identifier);

	if(member == null) {
		return;
	}

	delete composite.members[identifier]

	i_valueRetainOrReleaseComposites(member.value, composite);
}

function i_memberRetains(retainedComposite, retainingComposite) {
	for(let identifier in retainingComposite.members) {
		let member = retainingComposite.members[identifier]

		if(
			i_typeRetains(retainedComposite, member.type) ||
			i_valueRetains(retainedComposite, member.value) ||
			member.observers.some(v => v.value === retainedComposite.addresses.self)
		) {
			return true;
		}
	}
}

function i_findObserver() {}

function i_getObserver() {}

function i_setObserver(composite) {
	if(!['Class', 'Function', 'Namespace', 'Structure'].includes(composite.type[0]?.predefined)) {
		return;
	}
}

function i_deleteObserver(composite, observer) {
	composite.observers = composite.observers.filter(v => v !== observer);

	i_compositeRetainOrRelease(i_compositeGet(observer.value), composite);
}

function i_observerRetains(retainedComposite, retainingComposite) {
	return retainingComposite.observers.some(v => v.value === retainedComposite.addresses.self);
}

function i_report(level, node, string) {
	let position = node?.range.start ?? 0,
		location = i_tokens[position]?.location ?? {
			line: 0,
			column: 0
		}

	if(i_reports.find(v =>
		v.location.line === location.line &&
		v.location.column === location.column &&
		v.string === string) != null
	) {
		return;
	}

	i_reports.push({
		level: level,
		position: position,
		location: location,
		string: (node?.type ?? '?')+' -> '+string
	});
}

function i_reset(composites, preferences) {
	i_tokens = []
	i_tree = undefined;
	i_composites = composites ?? []
	i_scopes = []
	i_controlTransfer = undefined;
	i_preferences = {
		allowedReportLevel: 2,
		metaprogrammingLevel: 3,
		arbitaryPrecisionArithmetics: true,
		...(preferences ?? {})
	}
	i_reports = []
}

function i_interpret(lexerResult, parserResult, composites = [], preferences = {}) {
	i_reset(composites, preferences);

	i_tokens = lexerResult.tokens;
	i_tree = structuredClone(parserResult.tree);

	i_rules.module();

	let result = {
		composites: i_composites,
		reports: i_reports
	}

	i_reset();

	return result;
}