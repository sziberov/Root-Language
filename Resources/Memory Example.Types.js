//    inout A! | _? & Global<C>...
// 0: inout    |               ...
// 1:       A!
// 2:               &
// 3:            _?
// 4:                 Global< >
// 5:                        C

let type1 = [
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

let type2 = [
	{
		super: undefined,
		mask: ['intersection'],
		value: undefined
	},
	{
		super: 0,
		mask: ['parenthesized', 'union'],
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