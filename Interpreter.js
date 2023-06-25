class Interpreter {
	static tokens;
	static tree;
	static composites;
	static scopes;
	static controlTransfer;
	static preferences;
	static reports;

	static rules = {
		arrayType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'array');
		},
		argument: (node, scope) => {
			return {
				label: node.label?.value,
				value: this.rules[node.value.type]?.(node.value, scope)
			}
		},
		booleanLiteral: (node) => {
			// TODO: Instantinate Boolean()

			return this.createValue('boolean', node.value === 'true');
		},
		callExpression: (node, scope) => {
			let arguments_ = []

			for(let argument of node.arguments) {
				argument = this.rules[argument.type]?.(argument, scope);

				if(argument != null) {
					arguments_.push(argument);
				}
			}

			let function_ = this.findValueFunction(this.rules[node.callee.type]?.(node.callee, scope), arguments_);

			if(function_ == null) {
				this.report(2, node, 'Composite is not a function or wasn\'t found.');

				return;
			}

			this.report(0, node, 'ca: '+function_.title+', '+function_.addresses.self);
			let value = this.callFunction(function_, arguments_);
			this.report(0, node, 'ke: '+JSON.stringify(value));

			return value;
		},
		chainExpression: (node, scope) => {
			let composite = this.getValueComposite(this.rules[node.composite?.type](node.composite, scope));

			if(composite == null) {
				this.report(2, node, 'Composite wasn\'t found.');

				return;
			}

			let identifier = node.member;

			if(identifier.type === 'stringExpression') {
				identifier = this.rules.stringExpression?.(node.member, scope);
			} else {
				identifier = identifier.value;
			}

			return this.helpers.findMemberValue(composite, identifier, true);
		},
		classDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				inheritedTypes = node.inheritedTypes,
				composite = this.createClass(identifier, scope);

			if(node.inheritedTypes.length > 0) {
				composite.type[0].inheritedTypes = true;

				for(let node_ of node.inheritedTypes) {
					this.helpers.createTypePart(composite.type, composite.type[0], node_, scope, false);
				}
				for(let typePart of composite.type) {
					this.retainComposite(this.getComposite(typePart.reference), composite);
				}
			}

			let type = [this.createTypePart(undefined, undefined, { reference: composite.addresses.self, self: true })],
				value = this.createValue('reference', composite.addresses.self),
				observers = []

			this.setMember(scope, modifiers, identifier, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);

			// TODO:
			// - Protocol conformance checking (if not conforms, remove from list and report)
			// - Overloaded functions pre-parse (create empty dict members)
		},
		collectionType: (node, scope, type, typePart, title) => {
			let capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				composite = this.getValueComposite(this.findMember(scope, capitalizedTitle)?.value);

			if(composite != null) {
				typePart.reference = composite.addresses.self;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [composite != null ? 'genericArguments' : title]: true });

			if(title === 'dictionary') {
				this.helpers.createTypePart(type, typePart, node.key, scope);
			}

			this.helpers.createTypePart(type, typePart, node.value, scope);
		},
		combiningType: (node, scope, type, typePart, title) => {
			if(node.subtypes.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { [title]: true });

			for(let subtype of node.subtypes) {
				this.helpers.createTypePart(type, typePart, subtype, scope);
			}
		},
		defaultType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable']);
		},
		dictionaryType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'dictionary');
		},
		enumerationDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				composite = this.createEnumeration(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { reference: composite.addresses.self, self: true })],
				value = this.createValue('reference', composite.addresses.self),
				observers = []

			this.setMember(scope, modifiers, identifier, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		expressionsSequence: (node, scope) => {},
		floatLiteral: (node) => {
			// TODO: Instantinate Float()

			return this.createValue('float', node.value*1);
		},
		functionDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier?.value;

			if(identifier == null) {
				return;
			}

			let function_ = this.createFunction(identifier, node.body?.statements, scope),
				signature = this.rules.functionSignature(node.signature, scope),
				type = [],
				value = this.createValue('reference', function_.addresses.self),
				observers = [],
				member = this.getMember(scope, identifier);

			function_.type = signature;

			if(member == null) {
				this.createTypePart(type, undefined, { predefined: 'Function' });
			} else {
				modifiers = member.modifiers;

				if(member.type[0]?.predefined !== 'dict') {
					this.createTypePart(type, undefined, { predefined: 'dict' });

					value = this.createValue('dictionary', [member.value, value]);
				} else {
					type = member.type;
					value = this.createValue('dictionary', [...member.value.primitiveValue, value]);
				}
			}

			this.setMember(scope, modifiers, identifier, type, value, observers);
		},
		functionExpression: (node, scope) => {
			let signature = this.rules.functionSignature(node.signature, scope),
				function_ = this.createFunction(undefined, node.body?.statements, scope);

			function_.type = signature;

			return this.createValue('reference', function_.addresses.self);
		},
		functionSignature: (node, scope) => {
			let type = [],
				typePart = this.createTypePart(type, undefined, { predefined: 'Function' });

			typePart.awaits = node?.awaits ?? -1;
			typePart.throws = node?.throws ?? -1;

			for(let v of ['genericParameter', 'parameter']) {
				if(node?.[v+'s'].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v+'s']: true });

					for(let node_ of node[v+'s']) {
						this.rules[v](node_, scope, type, typePart_);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, node?.returnType, scope);

			return type;
		},
		functionType: (node, scope, type, typePart) => {
			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { predefined: 'Function' });

			typePart.awaits = node.awaits ?? 0;
			typePart.throws = node.throws ?? 0;

			for(let v of ['genericParameter', 'parameter']) {
				if(node[v+'Types'].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v+'s']: true });

					for(let node_ of node[v+'Types']) {
						this.helpers.createTypePart(type, typePart_, node_, scope);
					}
				}
			}

			this.helpers.createTypePart(type, typePart, node.returnType, scope);
		},
		genericParameter: (node, scope, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, node.type_, scope);

			typePart.identifier = node.identifier.value;
		},
		identifier: (node, scope) => {
			return this.helpers.findMemberValue(scope, node.value);
		},
		ifStatement: (node, scope) => {
			if(node.condition == null) {
				return;
			}

			let namespace = this.createNamespace('Local<'+(scope.title ?? '#'+scope.addresses.self)+', If>', scope),
				condition;

			this.addScope(namespace);

			condition = this.rules[node.condition.type]?.(node.condition, namespace);
			condition = condition?.primitiveType === 'boolean' ? condition.primitiveValue : condition != null;

			if(condition || node.else?.type !== 'ifStatement') {
				let branch = condition ? 'then' : 'else';

				if(node[branch]?.type === 'functionBody') {
					this.evaluateNodes(node[branch].statements, namespace);
				} else {
					this.setControlTransfer(this.rules[node[branch]?.type]?.(node[branch], namespace));
				}
			}

			this.removeScope();

			if(this.controlTransfer?.type === 'returnStatement') {
				return this.controlTransfer.value;
			}

			if(!condition && node.else?.type === 'ifStatement') {
				this.rules.ifStatement(node.else, scope);
			}

			return this.controlTransfer?.value;
		},
		inoutExpression: (node, scope) => {
			let value = this.rules[node.value?.type]?.(node.value, scope);

			if(value?.primitiveType === 'pointer') {
				return value;
			}
			if(value?.primitiveType !== 'reference') {
				this.report(2, node, 'Non-reference value ("'+(value?.primitiveType ?? 'nil')+'") can\'t be used as a pointer.');

				return;
			}

			return this.createValue('pointer', value.primitiveValue);
		},
		inoutType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['inout']);
		},
		integerLiteral: (node) => {
			// TODO: Instantinate Integer()

			return this.createValue('integer', node.value*1);
		},
		intersectionType: (n, s, t, tp) => {
			this.rules.combiningType(n, s, t, tp, 'intersection');
		},
		module: () => {
			let namespace = this.getComposite(0) ?? this.createNamespace('Global');

			this.addScope(namespace);
			this.evaluateNodes(this.tree?.statements, namespace);
			this.removeScope();
			this.resetControlTransfer();
		},
		namespaceDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				composite = this.createNamespace(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { reference: composite.addresses.self, self: true })],
				value = this.createValue('reference', composite.addresses.self),
				observers = []

			this.setMember(scope, modifiers, identifier, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		nillableType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable'], 1);
		},
		optionalType: (node, scope, type, typePart, titles, mainTitle) => {
			if(!titles.some(v => v in typePart)) {
				typePart[titles[mainTitle ?? 0]] = true;
			}

			this.rules[node.value?.type]?.(node.value, scope, type, typePart);
		},
		parameter: (node, scope, type, typePart) => {
			typePart = this.helpers.createTypePart(type, typePart, node.type_, scope);

			if(node.label != null) {
				typePart.label = node.label.value;
			}

			typePart.identifier = node.identifier.value;

			if(node.value != null) {
				typePart.value = node.value;
			}
		},
		parenthesizedExpression: (node, scope) => {
			return this.rules[node.value?.type]?.(node.value, scope);
		},
		parenthesizedType: (node, scope, type, typePart) => {
			this.rules[node.value?.type]?.(node.value, scope, type, typePart);
		},
		postfixExpression: (node, scope) => {
			let value = this.rules[node.value?.type]?.(node.value, scope);

			if(value == null) {
				return;
			}

			if(node.operator?.value === '++' && value.primitiveType === 'integer') {
				value.primitiveValue++;

				return this.createValue('integer', value.primitiveValue-1);
			}
			if(node.operator?.value === '--' && value.primitiveType === 'integer') {
				value.primitiveValue--;

				return this.createValue('integer', value.primitiveValue+1);
			}

			//TODO: Dynamic operators lookup

			return value;
		},
		predefinedType: (node, scope, type, typePart) => {
			typePart.predefined = node.value;
		},
		prefixExpression: (node, scope) => {
			let value = this.rules[node.value?.type]?.(node.value, scope);

			if(value == null) {
				return;
			}

			if(node.operator?.value === '!' && value.primitiveType === 'boolean') {
				return this.createValue('boolean', !value.primitiveValue);
			}
			if(node.operator?.value === '++' && value.primitiveType === 'integer') {
				value.primitiveValue++;

				return this.createValue('integer', value.primitiveValue);
			}
			if(node.operator?.value === '--' && value.primitiveType === 'integer') {
				value.primitiveValue--;

				return this.createValue('integer', value.primitiveValue);
			}

			//TODO: Dynamic operators lookup

			return value;
		},
		protocolDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				composite = this.createProtocol(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { reference: composite.addresses.self, self: true })],
				value = this.createValue('reference', composite.addresses.self),
				observers = []

			this.setMember(scope, modifiers, identifier, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		protocolType: (node, scope, type, typePart) => {

		},
		returnStatement: (node, scope) => {
			return this.rules[node.value?.type]?.(node.value, scope);
		},
		stringLiteral: (node, scope) => {
			let string = '';

			for(let segment of node.segments) {
				if(segment.type === 'stringSegment') {
					string += segment.value;
				} else {
					let value = this.rules[segment.value?.type]?.(segment.value, scope);

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

			return this.createValue('string', string);
		},
		structureDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				composite = this.createStructure(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { reference: composite.addresses.self, self: true })],
				value = this.createValue('reference', composite.addresses.self),
				observers = []

			this.setMember(scope, modifiers, identifier, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		typeIdentifier: (node, scope, type, typePart) => {
			let composite = this.getValueComposite(this.rules.identifier(node.identifier, scope));

			if(composite == null || composite.type[0]?.predefined === 'Object') {
				typePart.predefined = 'Any';

				this.report(2, node, 'Composite is an object or wasn\'t found.');
			} else {
				typePart.reference = composite?.addresses.self;
			}

			if(node.genericArguments.length === 0) {
				return;
			}

			typePart = this.helpers.createOrSetCollectionTypePart(type, typePart, { genericArguments: true });

			for(let genericArgument of node.genericArguments) {
				this.helpers.createTypePart(type, typePart, genericArgument, scope);
			}
		},
		unionType: (n, s, t, tp) => {
			this.rules.combiningType(n, s, t, tp, 'union');
		},
		variableDeclaration: (node, scope) => {
			let modifiers = node.modifiers;

			for(let declarator of node.declarators) {
				let identifier = declarator.identifier.value,
					type = [],
					typePart = this.helpers.createTypePart(type, undefined, declarator.type_, scope),
					value = this.rules[declarator.value?.type]?.(declarator.value, scope),
					observers = []

				// Type-related checks

				this.setMember(scope, modifiers, identifier, type, value, observers);
			}
		},
		variadicGenericParameter: (node, scope) => {
			return {
				identifier: undefined,
				type: [this.createTypePart(undefined, undefined, { variadic: true })]
			}
		},
		variadicType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['variadic']);
		}
	}

	static helpers = {
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
					return this.createTypePart(type, typePart, collectionFlag);
				}
			}

			return Object.assign(typePart, collectionFlag);
		},
		createTypePart: (type, typePart, node, scope, fallback = true) => {
			typePart = this.createTypePart(type, typePart);

			this.rules[node?.type]?.(node, scope, type, typePart);

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
				return this.createValue('reference', address);
			}

			// TODO: Access-related checks

			return this.findMember(composite, identifier, internal)?.value;
		}
	}

	static serializeMemory() /*getSave()*/ {
		// TODO: Should support references to AST nodes (many references - one definition)

		/*
		return {
			tokens: JSON.stringify(this.tokens),
			tree: JSON.stringify(this.tree),
			composites: JSON.stringify(this.composites),
			calls: JSON.stringify(this.scopes),
			preferences: JSON.stringify(this.preferences),
			reports: JSON.stringify(this.reports)
		}
		*/

		return JSON.stringify(this.composites/*, (k, v) => v === undefined ? null : v*/);
	}

	static deserializeMemory(serializedMemory) /*restoreSave()*/ {}

	static createComposite(title, type, scope) {
		let composite = {
			title: title,
			type: type,
			statements: [],
			imports: {},
			operators: {},
			members: {},
			observers: [],
			addresses: {
				self: undefined,
				Self: undefined,
				super: undefined,
				Super: undefined,
				sub: undefined,
				Sub: undefined,
				scope: undefined,
				retainers: []
			}
		}

		this.composites.push(composite);

		composite.addresses.self =
		composite.addresses.Self = this.composites.length-1;

		if(type != null && type[0].predefined !== 'Object') {  // TODO: Setter functions for addresses
			composite.addresses.super =
			composite.addresses.Super = type[1]?.reference;

			this.retainComposite(this.getComposite(composite.addresses.Super), composite);
		}

		composite.addresses.scope = scope?.addresses.self;

		this.retainComposite(scope, composite);
		this.report(1, undefined, 'cr: '+composite.title+', '+composite.addresses.self);

		return composite;
	}

	static destroyComposite(composite) {
		this.report(1, undefined, 'de: '+JSON.stringify(composite));

		// TODO:
		// - Notify all retainers about destroying
		// - Call deinitializer

		delete this.composites[composite.addresses.self]

		for(let composite_ of this.composites) {
			if(composite_?.addresses.retainers.includes(composite.addresses.self)) {
				this.releaseComposite(composite_, composite);
			}
		}
	}

	static destroyReleasedComposite(composite) {
		if(composite != null && !this.compositeRetained(composite)) {
			this.destroyComposite(composite);
		}
	}

	static getComposite(address) {
		return this.composites[address]
	}

	static retainComposite(retainedComposite, retainingComposite) {
		if(retainedComposite == null) {
			return;
		}

		if(!retainedComposite.addresses.retainers.includes(retainingComposite.addresses.self)) {
			retainedComposite.addresses.retainers.push(retainingComposite.addresses.self);
		}
	}

	static releaseComposite(retainedComposite, retainingComposite) {
		if(retainedComposite == null) {
			return;
		}

		retainedComposite.addresses.retainers = retainedComposite.addresses.retainers.filter(v => v !== retainingComposite.addresses.self);

		this.destroyReleasedComposite(retainedComposite);
	}

	static retainOrReleaseComposite(retainedComposite, retainingComposite) {
		if(retainedComposite == null) {
			return;
		}

		if(this.compositeRetains(retainedComposite, retainingComposite)) {
			this.retainComposite(retainedComposite, retainingComposite);
		} else {
			this.releaseComposite(retainedComposite, retainingComposite);
		}
	}

	/*
	 * Returns a real state of retainment.
	 *
	 * Composite considered really retained if:
	 * - It is used as a retainer's scope.
	 * - It is used by a retainer's type, import, member or observer.
	 */
	static compositeRetains(retainedComposite, retainingComposite) {
		if(
			retainedComposite?.addresses.self === retainingComposite?.addresses.self ||
			this.composites[retainingComposite?.addresses.self] == null
		) {
			return;
		}

		return (
			retainingComposite.addresses.scope === retainedComposite.addresses.self ||
			retainingComposite.type.some(v => v.reference === retainedComposite.addresses.self) ||
			this.importRetains(retainedComposite, retainingComposite) ||
			this.memberRetains(retainedComposite, retainingComposite) ||
			this.observerRetains(retainedComposite, retainingComposite)
		);
	}

	/*
	 * Returns a significant state of retainment.
	 *
	 * Composite considered significantly retained if it is reachable from
	 * the global namespace, a current scope namespace or a return value.
	 */
	static compositeRetained(composite) {
		return (
			this.compositeReachable(composite, this.getComposite(0)) ||
			this.compositeReachable(composite, this.getScope().namespace) ||
			this.compositeReachable(composite, this.getValueComposite(this.controlTransfer?.value))
		);
	}

	/*
	 * Returns true if the retainedComposite is reachable from the retainingComposite,
	 * recursively checking its retainers addresses.
	 *
	 * retainChain is used to exclude a retain cycles and meant to be set internally only.
	 */
	static compositeReachable(retainedComposite, retainingComposite, retainChain = []) {
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

			if(this.compositeReachable(this.getComposite(retainerAddress), retainingComposite, retainChain)) {
				return true;
			}
		}
	}

	static createClass(title, scope) {
		return this.createComposite(title, [{ predefined: 'Class' }], scope);
	}

	static createEnumeration(title, scope) {
		return this.createComposite(title, [{ predefined: 'Enumeration' }], scope);
	}

	static createFunction(title, statements, scope) {
		let function_ = this.createComposite(title, [{ predefined: 'Function' }], scope);

		this.setStatements(function_, statements);

		return function_;
	}

	static createNamespace(title, scope) {
		return this.createComposite(title, [{ predefined: 'Namespace' }], scope);
	}

	static createObject(scope) {
		let title = (scope.title ?? '#'+scope.addresses.self)+'()';

		return this.createComposite(title, [{ predefined: 'Object' }], scope);
	}

	static createProtocol(title, scope) {
		return this.createComposite(title, [{ predefined: 'Protocol' }], scope);
	}

	static createStructure(title, scope) {
		return this.createComposite(title, [{ predefined: 'Structure' }], scope);
	}

	/*
	 * Forwarding allows a function's statements to be evaluated in its scope directly. Although do not creating a
	 * temporary namespace also means that the scope will not be protected from destroy by release sequence while function is running.
	 *
	 * If no scope is specified, default function's scope is used.
	 */
	static callFunction(function_, arguments_, forwarded, scope) {
		scope ??= this.getComposite(function_.addresses.scope);

		let namespaceTitle = 'Call<'+(function_.title ?? '#'+function_.addresses.self)+'>',
			namespace = !forwarded ? this.createNamespace(namespaceTitle, scope) : scope,
			parameters = this.getFunctionTypeParameters(function_.type);

		for(let i = 0; i < (arguments_ ?? []).length; i++) {
			let argument = arguments_[i],
				parameterType = this.getSubtype(parameters, parameters[i]),
				identifier = parameterType[0]?.identifier ?? '$'+i;

			this.setMember(namespace, [], identifier, parameterType, argument.value, []);
		}

		this.addScope(namespace, function_);
		this.evaluateNodes(function_.statements, namespace);
		this.removeScope();

		let returnValue = this.controlTransfer?.value;

		this.resetControlTransfer();

		return returnValue;
	}

	static bindFunction(function_, scope) {}

	static getFunctionTypeParameters(functionType) {
		let parameters = this.getSubtype(functionType, functionType.find(v => v.parameters && v.super === 0));

		parameters.shift();

		return parameters;
	}

	static getScope(offset = 0) {
		return this.scopes[this.scopes.length-1+offset]
	}

	/*
	 * Should be used for function bodies before evaluating its contents,
	 * so ARC can correctly detect any references.
	 *
	 * Additionally function itself can be specified for debugging purposes.
	 */
	static addScope(namespace, function_) {
		this.scopes.push({
			namespace: namespace,
			function: function_
		});
	}

	/*
	 * Removes from the stack and automatically destroys its last scope.
	 */
	static removeScope() {
		let namespace = this.scopes.at(-1)?.namespace;

		this.scopes.pop();
		this.destroyReleasedComposite(namespace);
	}

	/*
	 * Should be used for statements inside of a bodies.
	 *
	 * Last statement in a body will be treated like a returning one even if it's not an explicit return.
	 * Manual control transfer is supported but should be also implemented by rules.
	 */
	static evaluateNodes(nodes, scope) {
		let globalScope = this.getScope(-1) == null,
			controlTransferTypes = [
				'breakStatement',
				'continueStatement',
				'returnStatement'
			]

		for(let node of nodes ?? []) {
			let start = this.composites.length,
				value = this.rules[node.type]?.(node, scope),
				end = this.composites.length,
				returned = this.controlTransfer != null && this.controlTransfer.value === value,
				returning = controlTransferTypes.includes(node.type) || node === nodes.at(-1);

			if(!returned && returning) {
				this.setControlTransfer(!globalScope ? value : undefined, node.type);
			}

			for(start; start < end; start++) {
				this.destroyReleasedComposite(this.getComposite(start));
			}

			if(controlTransferTypes.includes(this.controlTransfer?.type)) {
				break;
			}
		}
	}

	static setControlTransfer(value, type) {
		this.controlTransfer = {
			value: value,
			type: type
		}
	}

	static resetControlTransfer() {
		this.controlTransfer = undefined;
	}

	static getTypeParentAddress(type) {
		return type.find(v => v.super === 0)?.reference;
	}

	static createTypePart(type, superTypePart, flags) {
		let typePart = { ...flags }

		if(type != null) {
			if(superTypePart != null) {
				typePart.super = type.indexOf(superTypePart);
			}

			type.push(typePart);
		}

		return typePart;
	}

	static getSubtype(type, typePart, offset) {
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
				subtype.push(...this.getSubtype(type, typePart_, offset));
			}
		}

		return subtype;
	}

	static typeAccepts(acceptedType, acceptingType) {
		// TODO: Type equality check

		return true;
	}

	static createValue(primitiveType, primitiveValue) {
		return {
			primitiveType: primitiveType,	// 'boolean', 'dictionary', 'float', 'integer', 'node', 'pointer', 'reference', 'string'
			primitiveValue: primitiveValue	// boolean, integer, map (object), string, AST node
		}
	}

	static getValueType(value) {
		if(value == null) {
			return []
		}

		let typePart = this.createTypePart();

		if(!['pointer', 'reference'].includes(value.primitiveType)) {
			typePart.predefined = value.primitiveType;
		} else {
			if(value.primitiveType === 'pointer') {
				typePart.inout = true;
			}

			typePart.reference = value.primitiveValue;

			let composite = this.getValueComposite(value);

			while(composite != null) {
				if(composite.type[0]?.predefined !== 'Object') {
					if(composite.addresses.self === value.primitiveValue) {
						typePart.self = true;
					} else {
						typePart.reference = composite.addresses.self;
					}

					composite = undefined;
				} else {
					composite = this.getComposite(composite.addresses.scope);
				}
			}
		}

		return [typePart]
	}

	static getValueComposite(value) {
		if(!['pointer', 'reference'].includes(value?.primitiveType)) {
			return;
		}

		return this.getComposite(value.primitiveValue);
	}

	static getValueFunction(value, arguments_) {
		let function_ = this.getValueComposite(value);

		if(function_?.type[0]?.predefined !== 'Function') {
			return;
		}

		let parameters = this.getFunctionTypeParameters(function_.type);

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
			if(!this.typeAccepts(this.getValueType(argument.value), parameter)) {
				return;
			}
		}

		return function_;
	}

	static findValueFunction(value, arguments_) {
		let function_;

		if(value?.primitiveType === 'dictionary') {
			for(let key in value.primitiveValue) {
				if((function_ = this.getValueFunction(value.primitiveValue[key], arguments_)) != null) {
					break;
				}
			}
		} else {
			function_ = this.getValueFunction(value, arguments_);
		}

		return function_;
	}

	static retainOrReleaseValueComposites(retainingValue, retainingComposite) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				this.retainOrReleaseValueComposites(retainingValue.primitiveValue[key], retainingComposite);
			}
		} else {
			this.retainOrReleaseComposite(this.getValueComposite(retainingValue), retainingComposite);
		}
	}

	static valueRetains(retainedComposite, retainingValue) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				let retainingValue_ = retainingValue.primitiveValue[key]

				if(this.valueRetains(retainedComposite, retainingValue_)) {
					return true;
				}
			}
		} else {
			return this.getValueComposite(retainingValue) === retainedComposite;
		}
	}

	static setStatements(composite, statements) {
		if(composite.type[0]?.predefined !== 'Function') {
			return;
		}

		composite.statements = statements;
	}

	static findImport(composite, identifier) {
		while(composite != null) {
			let import_ = this.getImport(composite, identifier);

			if(import_ != null) {
				return import_;
			}

			composite = this.getComposite(composite.addresses.scope);
		}
	}

	static getImport(composite, identifier) {
		return composite.imports[identifier]
	}

	static setImport(composite, identifier, value) {
		if(composite.type[0]?.predefined !== 'Namespace') {
			return;
		}

		composite.imports[identifier] = value;
	}

	static deleteImport(composite, identifier) {
		let value = this.getComposite(this.getImport(composite, identifier));

		delete composite.imports[identifier]

		this.retainOrReleaseComposite(value, composite);
	}

	static importRetains(retainedComposite, retainingComposite) {
		for(let identifier in retainingComposite.imports) {
			if(retainingComposite.imports[identifier] === retainedComposite.addresses.self) {
				return true;
			}
		}
	}

	static findOperator(composite, identifier) {
		while(composite != null) {
			let operator = this.getOperator(composite, identifier);

			if(operator != null) {
				return operator;
			}

			composite = this.getComposite(composite.addresses.scope);
		}
	}

	static getOperator(composite, identifier) {
		return composite.operators[identifier]
	}

	static setOperator(composite, modifiers, identifier, associativity, precedence) {
		if(composite.type[0]?.predefined !== 'Namespace') {
			return;
		}

		let operator = this.getOperator(composite, identifier) ?? (composite.operators[identifier] = {});

		operator.modifiers = modifiers;
		operator.associativity = associativity;
		operator.precedence = precedence;
	}

	static deleteOperator(composite, identifier) {
		delete composite.operators[identifier]
	}

	/*
	 * Search order:
	 *
	 * 1. Current.(Lowest).Higher.Higher...  Object chain (parentheses - member is virtual)
	 * 2. Current.Parent.Parent...           Inheritance chain
	 * 3. Current.Scope.Scope...             Scope chain (search is not internal)
	 */
	static findMember(composite, identifier, internal) {
		return (
			this.findMemberInObjectChain(composite, identifier) ??
			this.findMemberInInheritanceChain(composite, identifier) ??
			!internal ? this.findMemberInScopeChain(composite, identifier) : undefined
		);
	}

	static findMemberInObject(object, identifier) {
		if(object.type[0]?.predefined !== 'Object') {
			return;
		}

		let member = this.getMember(object, identifier);

		if(member != null) {
			let member_ = this.findMemberInInheritanceChain(object, identifier);

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

	static findMemberInObjectChain(object, identifier) {
		let member,
			virtual;

		while(object != null) {  // Higher
			member = this.findMemberInObject(object, identifier);

			if(member != null) {
				if(!virtual && member.modifiers.includes('virtual')) {
					virtual = true;

					let object_;

					while((object_ = this.getComposite(object.addresses.sub)) != null) {  // Lowest
						object = object_;
					}

					continue;
				}

				break;
			}

			object = this.getComposite(object.addresses.super);
		}

		return member;
	}

	static findMemberInInheritanceChain(composite, identifier) {
		let member;

		while(composite != null) {
			member = this.getMember(composite, identifier);

			if(member != null) {
				break;
			}

			composite = this.getComposite(composite.addresses.Super);
		}

		return member;
	}

	static findMemberInScopeChain(composite, identifier) {
		let member;

		while(composite != null) {
			member = this.getMember(composite, identifier);

			if(member != null) {
				break;
			}

			// TODO: In-imports lookup

			composite = this.getComposite(composite.addresses.scope);
		}

		return member;
	}

	static getMember(composite, identifier) {
		return composite.members[identifier]
	}

	static setMember(composite, modifiers, identifier, type, value, observers) {
		let member = this.getMember(composite, identifier) ?? (composite.members[identifier] = {}),
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
				this.retainOrReleaseComposite(this.getComposite(address), composite);
			}
		}
		if(ov !== nv) {
			this.retainOrReleaseValueComposites(ov, composite);
			this.retainOrReleaseValueComposites(nv, composite);
		}
	}

	static deleteMember(composite, identifier) {
		let member = this.getMember(composite, identifier);

		if(member == null) {
			return;
		}

		delete composite.members[identifier]

		this.retainOrReleaseValueComposites(member.value, composite);
	}

	static memberRetains(retainedComposite, retainingComposite) {
		for(let identifier in retainingComposite.members) {
			let member = retainingComposite.members[identifier]

			if(
				member.type.some(v => v.reference === retainedComposite.addresses.self) ||
				this.valueRetains(retainedComposite, member.value) ||
				member.observers.some(v => v.value === retainedComposite.addresses.self)
			) {
				return true;
			}
		}
	}

	static findObserver() {}

	static getObserver() {}

	static setObserver(composite) {
		if(!['Class', 'Function', 'Namespace', 'Structure'].includes(composite.type[0]?.predefined)) {
			return;
		}
	}

	static deleteObserver(composite, observer) {
		composite.observers = composite.observers.filter(v => v !== observer);

		this.retainOrReleaseComposite(this.getComposite(observer.value), composite);
	}

	static observerRetains(retainedComposite, retainingComposite) {
		return retainingComposite.observers.some(v => v.value === retainedComposite.addresses.self);
	}

	static report(level, node, string) {
		let position = node?.range.start ?? 0,
			location = this.tokens[position]?.location ?? {
				line: 0,
				column: 0
			}

		if(this.reports.find(v =>
			v.location.line === location.line &&
			v.location.column === location.column &&
			v.string === string) != null
		) {
			return;
		}

		this.reports.push({
			level: level,
			position: position,
			location: location,
			string: (node?.type ?? '?')+' -> '+string
		});
	}

	static reset(composites, preferences) {
		this.tokens = []
		this.tree = undefined;
		this.composites = composites ?? []
		this.scopes = []
		this.controlTransfer = undefined;
		this.preferences = {
			allowedReportLevel: 2,
			metaprogrammingLevel: 3,
			arbitaryPrecisionArithmetics: true,
			...(preferences ?? {})
		}
		this.reports = []
	}

	static interpret(lexerResult, parserResult, composites = [], preferences = {}) {
		this.reset(composites, preferences);

		this.tokens = lexerResult.tokens;
		this.tree = structuredClone(parserResult.tree);

		this.rules.module();

		let result = {
			composites: this.composites,
			reports: this.reports
		}

		this.reset();

		return result;
	}
}