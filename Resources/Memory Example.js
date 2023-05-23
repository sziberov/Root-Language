let memory = [
	{
		address: 0,
		inheritance: ['Namespace'],
		imports: [
			/*
			{
				identifier: 'Foundation',
				value: 0
			}
			*/
		],
		operators: [
			{
				modifiers: ['postfix'],
				identifier: '++',
				associativity: null,
				precedence: null
			},
			{
				modifiers: ['postfix'],
				identifier: '--',
				associativity: null,
				precedence: null
			},
			{
				modifiers: ['prefix'],
				identifier: '++',
				associativity: null,
				precedence: null
			},
			{
				modifiers: ['prefix'],
				identifier: '--',
				associativity: null,
				precedence: null
			},
			{
				modifiers: [/* 'private', */ 'infix'],
				identifier: '+',
				associativity: 'left',
				precedence: 64
			}
		],
		members: [
			{
				modifiers: ['public', 'static'],
				identifier: 'TestClass',
				typed: true,
				inheritance: [1],
				nillable: true,
				value: 1,
				observers: []
			},
			{
				modifiers: ['public', 'static', 'final'],
				identifier: 'testPi',
				typed: true,
				inheritance: ['float'],
				nillable: true,
				value: '3.14',
				observers: []
			},
			{
				modifiers: ['public', 'static'],
				identifier: 'testClassInstance',
				typed: true,
				inheritance: [2],
				nillable: true,
				value: 3,
				observers: []
			},
			{
				modifiers: ['public'],
				identifier: 'computedInstanceProperty',
				typed: false,
				inheritance: [],
				nillable: true,
				value: null,
				observers: [
					{
						identifier: 'willGet',  // 'get', 'didGet', 'willSet', 'set', 'didSet', 'willDelete', 'delete', 'didDelete'
						value: null  // Function address
					}
				]
			}
		],
		observers: [
			{
				type: 'chain',
				modifiers: [],  // 'static'
				identifier: 'willGet',
				value: null
			},
			{
				type: 'subscript',
				modifiers: [],  // 'private', 'protected', 'public', 'static'
				identifier: 'willGet',
				value: null
			}
		],
		scopeAddress: null,
		retainerAddresses: [1]
	},
	{
		address: 1,
		inheritance: ['Class'],
		genericParameters: [],
		members: [
			{
				modifiers: ['public', 'static', 'final'],
				identifier: 'init',
				typed: true,
				inheritance: [
					/*
					{
						type: 'Function',
						genericParameters: [],  // [null] - Unspecified list, [] - Empty list
						parameters: [
							{
								typed: false,
								inheritance: [],
								nillable: true,
								variadic: true
							},
							{
								typed: false,
								inheritance: [],
								nillable: true,
								variadic: true
							}
						],
						awaits: 0,  // -1 - Unspecified, 0 - False, 1 - True
						throws: 0,
						return: {
							typed: true,
							inheritance: ['bool'],
							nillable: false
						}
					}
					*/
					'Function'
				],
				nillable: false,
				value: 2,
				observers: []
			},
			{
				modifiers: ['public'],
				identifier: 'testVariable',
				typed: true,
				inheritance: ['int'],
				nillable: true,
				value: null,
				observers: []
			}
		],
		observers: {
			chain: [],
			subscript: []
		},
		scopeAddress: 0,
		retainers: [0]
	},
	{
		address: 2,
		inheritance: ['Function'],
		genericParameters: [
			/*
			{
				identifier: 'A',
				typed: false,
				inheritance: [],
				nillable: true
			},
			*/
		],
		parameters: [
			{
				label: null,
				identifier: 'firstParameter',
				type: {
					inout: false,
					fixed: true,
					inheritance: ['float'],
					nillable: false,
					variadic: false
				},
				value: null  // Value node from Parser's result
			},
			{
				label: null,
				identifier: 'secondParameter',
				type: {
					inout: false,
					fixed: true,
					inheritance: ['bool'],
					nillable: true,
					variadic: false
				},
				value: null
			}
		],
		awaits: false,
		throws: false,
		returnType: {
			fixed: true,
			inheritance: ['bool'],
			nillable: true
		},
		body: '\ttestVariable = 2 print(\'Test: \'+firstParameter)\n\n\treturn secondParameter',  // Body node from Parser's result
		scopeAddress: 1,
		retainers: [1]
	},
	{
		address: 3,
		inheritance: ['Object'],
		members: [
			{
				identifier: 'testVariable',
				value: '2'
			}
		],
		scopeAddress: 1,
		retainers: [0]
	}
]