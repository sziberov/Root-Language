class Interpreter {
	static tokens;
	static tree;
	static composites;
	static scopes;
	static controlTransfer;
	static preferences;
	static reports;

	static rules = {
		argument: (node, scope) => {
			return {
				label: node.label?.value,
				value: this.rules[node.value?.type]?.(node.value, scope)
			}
		},
		arrayLiteral: (node, scope) => {
			let result,
				values = []

			for(let value of node.values) {
				value = this.rules[value.type]?.(value, scope);

				if(value != null) {
					values.push(value);
				}
			}

			let composite = this.getValueComposite(this.findMember(scope, 'Array')?.value);

			if(composite != null) {
				// TODO: Instantinate Array()
			} else {
				result = this.createValue('dictionary', values);
			}

			return result;
		},
		arrayType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'array');
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

			let value = this.rules[node.callee.type]?.(node.callee, scope);

			for(let i = 0; i < 2; i++) {
				if(i === 1) {  // Fallback to search of an initializer
					let composite = this.getValueComposite(value);

					if(
						composite == null ||
						!this.typeIsComposite(composite.type, 'Class') ||
						!this.typeIsComposite(composite.type, 'Structure')
					) {
						this.report(2, node, 'Composite is not callable or wasn\'t found.');

						return;
					}

					value = this.getMember(composite, 'init')?.value;
				}

				if(value == null) {
					this.report(2, node, i === 0 ? 'Callee wasn\'t found.' : 'Composite doesn\'t have an initializer.');

					continue;
				}

				let function_ = this.findValueFunction(value, arguments_);

				if(function_ == null) {
					if(i === 1) {
						this.report(2, node, 'Found "init" is not a function or is an initializer of a different type.');
					}

					continue;
				}

				this.report(0, node, 'ca: '+function_.title+', '+function_.addresses.ID);
				//if(i === 0) {
					value = this.callFunction(function_, arguments_);
				//}
				/*
				if(i === 1) {
					let objects = [],
						superObject = scope,
						subObject;

					while(superObject != null) {
						let object = this.createObject(scope);

						this.callFunction(function_, arguments_, false, object);

						objects.push(object);


					}

					value = objects.at(-1);
				}
				*/
				this.report(0, node, 'ke: '+JSON.stringify(value));

				return value;
			}
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
				identifier = node.identifier?.value;

			if(identifier == null) {
				return;
			}

			let inheritedTypes = node.inheritedTypes,
				composite = this.createClass(identifier, scope);

			if(node.inheritedTypes.length > 0) {
				composite.type[0].inheritedTypes = true;

				for(let node_ of node.inheritedTypes) {
					this.helpers.createTypePart(composite.type, composite.type[0], node_, scope, false);
				}
				for(let typePart of composite.type) {
					this.retainComposite(composite, this.getComposite(typePart.reference));
				}
			}

			let type = [this.createTypePart(undefined, undefined, { predefined: 'Class', self: true })],
				value = this.createValue('reference', composite.addresses.ID),
				observers = []

			this.setMember(scope, identifier, modifiers, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);

			// TODO: Protocol conformance checking (if not conforms, remove from list and report)
		},
		collectionType: (node, scope, type, typePart, title) => {
			let capitalizedTitle = title[0].toUpperCase()+title.slice(1),
				composite = this.getValueComposite(this.findMember(scope, capitalizedTitle)?.value);

			if(composite != null) {
				typePart.reference = composite.addresses.ID;
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
		dictionaryLiteral: (node, scope) => {
			let result,
				entries = new Map();

			for(let entry of node.entries) {
				entry = this.rules.entry(entry, scope);

				if(entry != null) {
					entries.set(entry.key, entry.value);
				}
			}

			let composite = this.getValueComposite(this.findMember(scope, 'Dictionary')?.value);

			if(composite != null) {
				// TODO: Instantinate Dictionary()
			} else {
				result = this.createValue('dictionary', entries);
			}

			return result;
		},
		dictionaryType: (n, s, t, tp) => {
			this.rules.collectionType(n, s, t, tp, 'dictionary');
		},
		entry: (node, scope) => {
			return {
				key: this.rules[node.key.type]?.(node.key, scope),
				value: this.rules[node.value?.type]?.(node.value, scope)
			}
		},
		enumerationDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				composite = this.createEnumeration(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Enumeration', self: true })],
				value = this.createValue('reference', composite.addresses.ID),
				observers = []

			this.setMember(scope, identifier, modifiers, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		expressionsSequence: (node, scope) => {
			if(node.values.length === 3 && node.values[1].type === 'infixOperator' && node.values[1].value === '=') {
				let lhs = node.values[0],
					lhsComposite,
					lhsMemberIdentifier,
					internal;

				if(lhs.type === 'identifier') {
					lhsComposite = scope;
					lhsMemberIdentifier = lhs.value;
					internal = false;

					lhs = this.findMember(lhsComposite, lhsMemberIdentifier);
				} else
				if(lhs.type === 'chainExpression') {
					lhsComposite = this.getValueComposite(this.rules[lhs.composite.type]?.(lhs.composite, scope));
					lhsMemberIdentifier = lhs.member.type === 'identifier' ? lhs.member.value : this.rules.stringLiteral(lhs.member, scope, true).primitiveValue;
					internal = true;

					if(lhsComposite == null || lhsMemberIdentifier == null) {
						return;
					}

					lhs = this.findMember(lhsComposite, lhsMemberIdentifier, true);
				}

				// TODO: Create member with default type if not exists

				if(lhs == null || lhsComposite == null || lhsMemberIdentifier == null) {
					this.report(1, node, 'Cannot assign to anything but a valid identifier or chain expression.');

					return;
				}

				let rhs = this.rules[node.values[2].type]?.(node.values[2], scope);

				this.setMember(lhsComposite, lhsMemberIdentifier, lhs.modifiers, lhs.type, rhs, lhs.observers, internal);

				return rhs;
			}
		},
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
				value = this.createValue('reference', function_.addresses.ID),
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

			this.setMember(scope, identifier, modifiers, type, value, observers);
		},
		functionExpression: (node, scope) => {
			let signature = this.rules.functionSignature(node.signature, scope),
				function_ = this.createFunction(undefined, node.body?.statements, scope);

			function_.type = signature;

			return this.createValue('reference', function_.addresses.ID);
		},
		functionSignature: (node, scope) => {
			let type = [],
				typePart = this.createTypePart(type, undefined, { predefined: 'Function' });

			typePart.awaits = node?.awaits ?? -1;
			typePart.throws = node?.throws ?? -1;

			for(let v of ['genericParameters', 'parameters']) {
				if(node?.[v].length > 0) {
					let typePart_ = this.createTypePart(type, typePart, { [v]: true });

					for(let node_ of node[v]) {
						this.rules[node_.type]?.(node_, scope, type, typePart_);
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

			let namespace = this.createNamespace('Local<'+(scope.title ?? '#'+scope.addresses.ID)+', If>', scope),
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

			let print = this.createFunction('print', (arguments_) => {
				this.report(0, undefined, JSON.stringify(arguments_[0].value));
			}, namespace);

			print.type = [
				{
	                "predefined": "Function",
	                "awaits": -1,
	                "throws": -1
	            },
	            {
	                "parameters": true,
	                "super": 0
	            },
	            {
	                "super": 1,
	                "predefined": "_",
	                "nillable": true,
	                "identifier": "value"
	            },
	            {
	                "super": 0,
	                "predefined": "_",
	                "nillable": true
	            }
            ]

            this.setMember(namespace, 'print', [], [{ predefined: 'Function' }], this.createValue('reference', print.addresses.ID), []);

			this.addScope(namespace);
			this.evaluateNodes(this.tree?.statements, namespace);
			this.removeScope();
			this.resetControlTransfer();
		},
		namespaceDeclaration: (node, scope) => {
			let modifiers = node.modifiers,
				identifier = node.identifier.value,
				composite = this.createNamespace(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Namespace', self: true })],
				value = this.createValue('reference', composite.addresses.ID),
				observers = []

			this.setMember(scope, identifier, modifiers, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		nillableType: (n, s, t, tp) => {
			this.rules.optionalType(n, s, t, tp, ['default', 'nillable'], 1);
		},
		nilLiteral: () => {},
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
				type = [this.createTypePart(undefined, undefined, { predefined: 'Protocol', self: true })],
				value = this.createValue('reference', composite.addresses.ID),
				observers = []

			this.setMember(scope, identifier, modifiers, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		protocolType: (node, scope, type, typePart) => {

		},
		returnStatement: (node, scope) => {
			return this.rules[node.value?.type]?.(node.value, scope);
		},
		stringLiteral: (node, scope, primitive) => {
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
				identifier = node.identifier?.value;

			if(identifier == null) {
				return;
			}

			let composite = this.createStructure(identifier, scope),
				type = [this.createTypePart(undefined, undefined, { predefined: 'Structure', self: true })],
				value = this.createValue('reference', composite.addresses.ID),
				observers = []

			this.setMember(scope, identifier, modifiers, type, value, observers);
			this.evaluateNodes(node.body?.statements, composite);
		},
		typeIdentifier: (node, scope, type, typePart) => {
			let composite = this.getValueComposite(this.rules.identifier(node.identifier, scope));

			if(composite == null || this.typeIsComposite(composite.type, 'Object')) {
				typePart.predefined = 'Any';

				this.report(2, node, 'Composite is an object or wasn\'t found.');
			} else {
				typePart.reference = composite?.addresses.ID;
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

				this.setMember(scope, identifier, modifiers, type, value, observers);
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

		return JSON.stringify(this.composites, (k, v) => {
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

	static deserializeMemory(serializedMemory) /*restoreSave()*/ {}

	static createComposite(title, type, scope) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		let composite = {
			title: title,
			addresses: {
				ID: this.composites.length,
				self: undefined,
				Self: undefined,
				super: undefined,
				Super: undefined,
				sub: undefined,
				Sub: undefined,
				scope: undefined,
				retainers: []
			},
			alive: true,
			type: type,
			statements: [],
			imports: {},
			operators: {},
			members: {},
			observers: []
		}

		this.composites.push(composite);
		this.setScopeAddress(composite, scope);
		this.report(0, undefined, 'cr: '+composite.title+', '+composite.addresses.ID);

		return composite;
	}

	static destroyComposite(composite) {
		if(!composite.alive) {
			return;
		}

		composite.alive = false;

		if(this.typeIsComposite(composite, 'Object')) {
			let value = this.findMember(composite, 'deinit', true)?.value;

			if(value != null) {
				let function_ = this.findValueFunction(value, []);

				if(function_ != null) {
					this.callFunction(function_);
				} else {
					this.report(1, undefined, 'Found "deinit" is not a function or is an incorrect deinitializer.');
				}
			}
		}

		for(let composite_ of this.composites) {
			if(composite_?.addresses.retainers.includes(composite.addresses.ID)) {
				this.releaseComposite(composite, composite_);
			}
		}

		delete this.composites[composite.addresses.ID]

		if(composite.addresses.retainers.length > 0) {
		// TODO: Notify all retainers about destroying

			this.report(1, undefined, 'Composite #'+composite.addresses.ID+' was destroyed with a non-empty retainer list.');
		}

		this.report(0, undefined, 'de: '+JSON.stringify(composite));
	}

	static destroyReleasedComposite(composite) {
		if(composite != null && !this.compositeRetained(composite)) {
			this.destroyComposite(composite);
		}
	}

	static getComposite(address) {
		return this.composites[address]
	}

	static retainComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null) {
			return;
		}

		if(!retainedComposite.addresses.retainers.includes(retainingComposite.addresses.ID)) {
			retainedComposite.addresses.retainers.push(retainingComposite.addresses.ID);
		}
	}

	static releaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null) {
			return;
		}

		retainedComposite.addresses.retainers = retainedComposite.addresses.retainers.filter(v => v !== retainingComposite.addresses.ID);

		this.destroyReleasedComposite(retainedComposite);
	}

	static retainOrReleaseComposite(retainingComposite, retainedComposite) {
		if(retainedComposite == null) {
			return;
		}

		if(this.compositeRetains(retainingComposite, retainedComposite)) {
			this.retainComposite(retainingComposite, retainedComposite);
		} else {
			this.releaseComposite(retainingComposite, retainedComposite);
		}
	}

	/*
	 * Returns a real state of retainment.
	 *
	 * Composite considered really retained if it is used by an at least one of
	 * retainer's addresses (excluding "self" and retainers list), type, import, member or observer.
	 */
	static compositeRetains(retainingComposite, retainedComposite) {
		if(
			this.composites[retainingComposite?.addresses.ID] == null ||
			retainedComposite?.addresses.ID === retainingComposite?.addresses.ID ||
			!retainingComposite?.alive
		) {
			return;
		}

		return (
			this.addressesRetain(retainingComposite, retainedComposite) ||
			this.typeRetains(retainingComposite.type, retainedComposite) ||
			this.importsRetain(retainingComposite, retainedComposite) ||
			this.membersRetain(retainingComposite, retainedComposite) ||
			this.observersRetain(retainingComposite, retainedComposite)
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
		if(retainedComposite.addresses.ID === retainingComposite.addresses.ID) {
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
		let class_ = this.createComposite(title, [{ predefined: 'Class' }], scope);

		this.setSelfAddress(class_, class_);

		return class_;
	}

	static createEnumeration(title, scope) {
		let enumeration = this.createComposite(title, [{ predefined: 'Enumeration' }], scope);

		this.setSelfAddress(enumeration, enumeration);

		return enumeration;
	}

	static createFunction(title, statements, scope) {
		let function_ = this.createComposite(title, [{ predefined: 'Function' }], scope);

		this.setInheritedSelfAddress(function_, scope);
		this.setStatements(function_, statements);

		return function_;
	}

	static createNamespace(title, scope) {
		let namespace = this.createComposite(title, [{ predefined: 'Namespace' }], scope);

		this.setSelfAddress(namespace, namespace);

		return namespace;
	}

	static createObject(superObject, subObject) {
		let title = (superObject.title ?? '#'+superObject.addresses.ID)+'()',
			object = this.createComposite(title, [{ predefined: 'Object' }]);

		this.setSelfAddress(object, object);
		this.setSuperSubAddresses(object, superObject, subObject);

		return object;
	}

	static createProtocol(title, scope) {
		let protocol = this.createComposite(title, [{ predefined: 'Protocol' }], scope);

		this.setSelfAddress(protocol, protocol);

		return protocol;
	}

	static createStructure(title, scope) {
		let structure = this.createComposite(title, [{ predefined: 'Structure' }], scope);

		this.setSelfAddress(structure, structure);

		return structure;
	}

	/*
	 * Forwarding allows a function's statements to be evaluated in its scope directly.
	 *
	 * If no scope is specified, default function's scope is used.
	 */
	static callFunction(function_, arguments_ = [], forwarded, scope) {
		if(typeof function_.statements === 'function') {
			return function_.statements(arguments_);
		}

		scope ??= this.getComposite(function_.addresses.scope);

		let namespaceTitle = 'Call<'+(function_.title ?? '#'+function_.addresses.ID)+'>',
			namespace = !forwarded ? this.createNamespace(namespaceTitle, scope) : scope,
			parameters = this.getTypeFunctionParameters(function_.type);

		this.setInheritedSelfAddress(namespace, scope);

		for(let i = 0; i < arguments_.length; i++) {
			let argument = arguments_[i],
				parameterType = this.getSubtype(parameters, parameters[i]),
				identifier = parameterType[0]?.identifier ?? '$'+i;

			this.setMember(namespace, identifier, [], parameterType, argument.value, []);
		}

		this.addScope(namespace, function_);
		this.evaluateNodes(function_.statements, namespace);
		this.removeScope();

		let returnValue = this.controlTransfer?.value;

		this.resetControlTransfer();

		return returnValue;
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

	static setCompositeType(composite, type) {
		if(!this.typeIsComposite(type)) {
			return;
		}

		composite.type = type;
	}

	static setAddress(composite, key, value) {
		if(!['Self', 'super', 'Super', 'sub', 'Sub', 'scope'].includes(key)) {
			return;
		}

		let ov, nv;  // Old/new value

		ov = composite.addresses[key]
		nv = composite.addresses[key] = value?.addresses.ID;

		if(ov !== nv) {
			this.retainOrReleaseComposite(composite, this.getComposite(ov));
			this.retainOrReleaseComposite(composite, value);
		}
	}

	static setSelfAddress(composite, composite_) {
		composite.addresses.self = composite_.addresses.ID;

		if(!this.typeIsComposite(composite_.type, 'Object')) {
			composite.addresses.Self = composite_.addresses.ID;
		} else {
			composite.addresses.Self = this.getTypeInheritedAddress(composite_.type);
		}
	}

	static setInheritedSelfAddress(composite, composite_) {
		composite.addresses.self = composite_?.addresses.self;
		composite.addresses.Self = composite_?.addresses.Self;
	}

	static setSuperSubAddresses(object, superObject, subObject) {
		if(this.typeIsComposite(superObject.type, 'Object')) {
			object.addresses.super = superObject.addresses.ID;
			object.addresses.Super = this.getTypeInheritedAddress(superObject.type);
		}

		if(subObject != null) {
			if(this.typeIsComposite(subObject.type, 'Object')) {
				object.addresses.sub = subObject.addresses.ID;
				object.addresses.Sub = this.getTypeInheritedAddress(subObject.type);
			}
		}
	}

	static setScopeAddress(composite, composite_) {
		this.setAddress(composite, 'scope', composite_);
	}

	static addressesRetain(retainingComposite, retainedComposite) {
		for(let key in retainingComposite.addresses) {
			if(
				key !== 'self' && key !== 'retainers' &&
				retainingComposite.addresses[key] === retainedComposite.addresses.ID
			) {
				return true;
			}
		}
	}

	static createTypePart(type, superPart, flags = {}) {
		let part = { ...flags }

		if(type != null) {
			if(superPart != null) {
				part.super = type.indexOf(superPart);
			}

			type.push(part);
		}

		return part;
	}

	static getTypeInheritedAddress(inheritingType) {
		return inheritingType.find(v => v.super === 0)?.reference;
	}

	static getTypeFunctionParameters(functionType) {
		let parameters = this.getSubtype(functionType, functionType.find(v => v.parameters && v.super === 0));

		parameters.shift();

		return parameters;
	}

	static getSubtype(type, part, offset) {
		offset ??= type.indexOf(part);

		if(offset < 0) {
			return []
		}
		if(offset === 0) {
			return structuredClone(type);
		}

		let type_ = [],
			part_ = structuredClone(part);

		part_.super -= offset;

		if(part_.super < 0) {
			delete part_.super;
		}

		type_.push(part_);

		for(let part_ of type) {
			if(part_.super === type.indexOf(part)) {
				type_.push(...this.getSubtype(type, part_, offset));
			}
		}

		return type_;
	}

	static typeIsComposite(type, wantedValue, any) {
		if(type == null) {
			return;
		}

		let values = [
				'Class',
				'Enumeration',
				'Function',
				'Namespace',
				'Object',
				'Protocol',
				'Structure'
			],
			currentValue = type[0]?.predefined;

		if(any) {
			values.push('Any');
		}

		return values.includes(currentValue) && (wantedValue == null || currentValue === wantedValue);
	}

	static typeAccepts(acceptingType, acceptedType) {
		// TODO: Type equality check

		return true;
	}

	static typeRetains(retainingType, retainedComposite) {
		return retainingType.some(v => v.reference === retainedComposite.addresses.ID);
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

		let type = [],
			part = this.createTypePart(type);

		if(!['pointer', 'reference'].includes(value.primitiveType)) {
			part.predefined = value.primitiveType;
		} else {
			if(value.primitiveType === 'pointer') {
				part.inout = true;
			}

			part.reference = value.primitiveValue;

			let composite = this.getValueComposite(value);

			while(composite != null) {
				if(!this.typeIsComposite(composite.type, 'Object')) {
					if(composite.addresses.ID === value.primitiveValue) {
						part.self = true;
					} else {
						part.reference = composite.addresses.ID;
					}

					composite = undefined;
				} else {
					composite = this.getComposite(composite.addresses.scope);
				}
			}
		}

		return type;
	}

	static getValueComposite(value) {
		if(!['pointer', 'reference'].includes(value?.primitiveType)) {
			return;
		}

		return this.getComposite(value.primitiveValue);
	}

	static getValueFunction(value, arguments_) {
		let function_ = this.getValueComposite(value);

		if(!this.typeIsComposite(function_?.type, 'Function')) {
			return;
		}

		let parameters = this.getTypeFunctionParameters(function_.type);

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
			if(!this.typeAccepts(parameter, this.getValueType(argument.value))) {
				return;
			}
		}

		return function_;
	}

	static findValueFunction(value, arguments_) {
		let function_;

		if(value?.primitiveType === 'dictionary') {
			for(let key in value.primitiveValue) {
				if((function_ = this./*find*/getValueFunction(value.primitiveValue[key], arguments_)) != null) {
					break;
				}
			}
		} else {
			function_ = this.getValueFunction(value, arguments_);
		}

		return function_;
	}

	static retainOrReleaseValueComposites(retainingComposite, retainingValue) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				this.retainOrReleaseValueComposites(retainingComposite, retainingValue.primitiveValue[key]);
			}
		} else {
			this.retainOrReleaseComposite(retainingComposite, this.getValueComposite(retainingValue));
		}
	}

	static valueRetains(retainingValue, retainedComposite) {
		if(retainingValue?.primitiveType === 'dictionary') {
			for(let key in retainingValue.primitiveValue) {
				let retainingValue_ = retainingValue.primitiveValue[key]

				if(this.valueRetains(retainingValue_, retainedComposite)) {
					return true;
				}
			}
		} else {
			return this.getValueComposite(retainingValue) === retainedComposite;
		}
	}

	static setStatements(function_, statements) {
		if(!this.typeIsComposite(function_.type, 'Function')) {
			return;
		}

		function_.statements = statements;
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

	static setImport(namespace, identifier, value) {
		if(!this.typeIsComposite(namespace.type, 'Namespace')) {
			return;
		}

		namespace.imports[identifier] = value;
	}

	static deleteImport(composite, identifier) {
		let value = this.getComposite(this.getImport(composite, identifier));

		delete composite.imports[identifier]

		this.retainOrReleaseComposite(composite, value);
	}

	static importsRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.imports) {
			if(retainingComposite.imports[identifier] === retainedComposite.addresses.ID) {
				return true;
			}
		}
	}

	static getOperator(composite, identifier) {
		return composite.operators[identifier]
	}

	static addOperator(namespace, identifier) {
		if(!this.typeIsComposite(namespace.type, 'Namespace')) {
			return;
		}

		if(this.getOperator(namespace, identifier) == null) {
			namespace.operators[identifier] = {}
		}
	}

	static removeOperator(composite, identifier) {
		delete composite.operators[identifier]
	}

	static findOperatorOverload(composite, identifier, matching) {
		while(composite != null) {
			let operator = this.getOperator(composite, identifier);

			if(operator != null) {
				for(let overload of operator) {
					if(matching(overload)) {
						return operator;
					}
				}
			}

			composite = this.getComposite(composite.addresses.scope);
		}
	}

	static setOperatorOverload(composite, identifier, matching, modifiers, associativity, precedence) {
		let operator = this.findOperatorOverload(namespace, identifier, matching);

		if(operator == null) {
			return;
		}

		operator.modifiers = modifiers;
		operator.associativity = associativity;
		operator.precedence = precedence;
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
		if(!this.typeIsComposite(object.type, 'Object')) {
			return;
		}

		let member = this.getMember(object, identifier);

		if(member != null) {
			member = this.getMemberProxy(member, object, this.findMemberInInheritanceChain(object, identifier));
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
		while(composite != null) {
			let member = this.getMember(composite, identifier);

			if(member != null) {
				return this.getMemberProxy(member, composite);
			}

			composite = this.getComposite(composite.addresses.Super);
		}
	}

	static findMemberInScopeChain(composite, identifier) {
		while(composite != null) {
			let member = this.getMember(composite, identifier);

			if(member != null) {
				return this.getMemberProxy(member, composite);
			}

			// TODO: In-imports lookup

			composite = this.getComposite(composite.addresses.scope);
		}
	}

	static getMember(composite, identifier) {
		return composite.members[identifier]
	}

	static getMemberProxy(member, owningComposite, underlayingDeclaration) {
		return new Proxy(member, {
			get(target, key, receiver) {
				if(key === 'owner') {
					return owningComposite;
				}

				if(underlayingDeclaration == null || key === 'value') {
					return member[key]
				} else {
					return underlayingDeclaration[key]
				}
			},
			set(target, key, value) {
				if(underlayingDeclaration == null || key === 'value') {
					member[key] = value;
				} else {
					underlayingDeclaration[key] = value;
				}

				return true;
			}
		});
	}

	/*
	 * Not specifying "internal" means use of direct declaration without search.
	 */
	static setMember(composite, identifier, modifiers, type, value, observers, internal) {
		let member = internal == null ? this.getMember(composite, identifier) : this.findMember(composite, identifier, internal);

		if(member == null) {
			/*
			if(internal == null) {
				return;
			}
			*/

			member = composite.members[identifier] = {}
		} else
		if(internal != null) {
			composite = member.owner;
		}

		let ot = member.type ?? [],  // Old/new type
			nt = type ?? [],
			ov = member.value,  // Old/new value
			nv = value;

		member.modifiers = modifiers;
		member.type = type;
		member.value = value;
		member.observers = observers;

		if(ot !== nt) {
			let addresses = new Set([...ot, ...nt].map(v => v.reference));

			for(let address of addresses) {
				this.retainOrReleaseComposite(composite, this.getComposite(address));
			}
		}
		if(ov !== nv) {
			this.retainOrReleaseValueComposites(composite, ov);
			this.retainOrReleaseValueComposites(composite, nv);
		}
	}

	static deleteMember(composite, identifier) {
		let member = this.getMember(composite, identifier);

		if(member == null) {
			return;
		}

		delete composite.members[identifier]

		this.retainOrReleaseValueComposites(composite, member.value);
	}

	static membersRetain(retainingComposite, retainedComposite) {
		for(let identifier in retainingComposite.members) {
			let member = retainingComposite.members[identifier]

			if(
				this.typeRetains(member.type, retainedComposite) ||
				this.valueRetains(member.value, retainedComposite) ||
				member.observers.some(v => v.value === retainedComposite.addresses.ID)
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

		this.retainOrReleaseComposite(composite, this.getComposite(observer.value));
	}

	static observersRetain(retainingComposite, retainedComposite) {
		return retainingComposite.observers.some(v => v.value === retainedComposite.addresses.ID);
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
			string: (node != null ? node.type+' -> ' : '')+string
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