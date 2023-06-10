// Rules for a type descriptor parts:
// - One TDP can have only one collection mask.
// - TDPs that have collection masks should also have children.
// - Non-first collection masks are attached as a separate child TDPs.
// - Masks that are not mutually exclusive can be combined.
// - Masks can not be ambigously combined (e.g. variadic array type vs array of variadic type).
// - TDPs should be minimalistic and can not be redundant.

//    inout A! | _? & Global<C>...
// 0: inout    |               ...
// 1:       A!
// 2:               &
// 3:            _?
// 4:                 Global< >
// 5:                        C

let type0 = [
	{
		super: undefined,
		mask: ['inout', 'union', 'variadic'],
		value: undefined
	},
	{
		super: 0,
		mask: ['reference', 'default'],
		value: 1
	},
	{
		super: 0,
		mask: ['intersection'],
		value: undefined
	},
	{
		super: 2,
		mask: ['predefined', 'nillable'],
		value: '_'
	},
	{
		super: 2,
		mask: ['reference', 'genericArguments'],
		value: 0
	},
	{
		super: 4,
		mask: ['reference'],
		value: 3
	}
]

//    (A | B) & C
// 0:         &
// 1: (  |  )
// 2:  A
// 3:      B
// 4:           C

let type1 = [
	{
		super: undefined,
		mask: ['intersection'],
		value: undefined
	},
	{
		super: 0,
		mask: ['union'],
		value: undefined
	},
	{
		super: 1,
		mask: ['reference'],
		value: 1
	},
	{
		super: 1,
		mask: ['reference'],
		value: 2
	},
	{
		super: 0,
		mask: ['reference'],
		value: 3
	}
]

//    () / Function
// 0: () / Function

let type2 = [
	{
		super: undefined,
		mask: ['predefined'],
		value: 'Function'
	}
]

//    <...>(...) awaits? throws? -> _?
// 0: <   >      awaits? throws?
// 1:  ...
// 2:      (   )
// 3:       ...
// 4:                            -> _?

let type3 = [
	{
		super: undefined,
		mask: ['function', 'genericParameters', 'awaits?', 'throws?'],
		value: undefined
	},
	{
		super: 0,
		mask: ['variadic'],
		value: undefined
	},
	{
		super: 0,
		mask: ['parameters'],
		value: undefined
	},
	{
		super: 2,
		mask: ['variadic'],
		value: undefined
	},
	{
		super: 0,
		mask: ['return', 'predefined', 'nillable'],
		value: '_'
	}
]

//    ([]..., ...) awaits throws -> _
// 0: (          ) awaits throws
// 1:  []...
// 2:       , ...
// 3:                            -> _

let type3 = [
	{
		super: undefined,
		mask: ['function', 'awaits', 'throws'],
		value: undefined
	},
	{
		super: 0,
		mask: ['array', 'variadic'],
		value: undefined
	},
	{
		super: 0,
		mask: ['variadic'],
		value: undefined
	},
	{
		super: 0,
		mask: ['return', 'predefined'],
		value: '_'
	}
]

//    (() -> _)?
// 0: (()     )?
// 1:     -> _

let type4 = [
	{
		super: undefined,
		mask: ['function', 'nillable'],
		value: undefined
	},
	{
		super: 0,
		mask: ['return', 'predefined'],
		value: '_'
	}
]

//    [_?]
// 0: [  ]
// 1:  _?

let type5 = [
	{
		super: undefined,
		mask: ['array'],
		value: undefined
	},
	{
		super: 0,
		mask: ['predefined', 'nillable'],
		value: '_'
	}
]

//    Array<_?>
// 0: Array<  >
// 1:       _?

let type6 = [
	{
		super: undefined,
		mask: ['reference', 'genericArguments'],
		value: 1 // Array's address
	},
	{
		super: 0,
		mask: ['predefined', 'nillable'],
		value: '_'
	}
]