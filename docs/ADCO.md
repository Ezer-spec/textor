# Adco - the language of Textor themes and addons

Themes and addons are written in **Adco** (`.adco` files). Textor highlights `.adco` files itself,
and shows mistakes (with line and column) in the terminal when it starts.

Where Textor looks for `.adco` files:

- `~/.config/textor/addons/` and `~/.config/textor/themes/`
- `addons/` and `themes/` next to the program

One file can hold any number of `theme`, `addon` and `language` blocks.

**Every addon lives in its own folder**, named after the addon. The `.adco` file is the addon,
and its icon is in the `svg/` folder next to it:

```
addons/
  lua/
    lua.adco          the addon
    svg/
      lua.svg         the file icon
```

A single `.adco` file lying directly in `addons/` or `themes/` still works (handy for themes).

## The basics

```adco
// a comment            /* a block comment /* can be nested */ */

import adco.std                 // optional, the standard functions

let brand = #3D9EE6             // a constant
var size  = 12                  // `var` works the same way (values are not changed later)

func soft(c: Color) -> Color {  // a function: parameters, then `return`
    return mix(c, #000000, 0.5)
}

theme "My Theme" {              // a block: name, then { ... }
    dark: true                  // a property: key: value  (or key = value)
    colors { accent: brand }    // blocks can be nested
}
```

Rules in short:

- Lines do not need `;`. You may use `;` or `,` to put several things on one line.
- A property defines a name for the rest of its block: after `accent: brand`, later lines
  in the same `colors { }` can write `selection: soft(accent)`.
- `if cond { ... } else { ... }` works in blocks and in functions.
- Strings use `"..."`. Inside you can write `"size \(12 + 2)"`. Escapes: `\n \t \" \\`.
- `@name` before a block marks it. `@hidden` makes Textor skip it.

## Values

| Kind | Examples |
|---|---|
| Number | `12`, `0.5`, `1_000` |
| String | `"JetBrains Mono"` |
| Bool | `true`, `false` |
| Color | `#3D9EE6`, `#fff`, `#3D9EE680` (with alpha), `rgb(0.2, 0.6, 0.9)` |
| Array | `["a", "b"]`, joined with `+` |
| nil | `nil` |

Operators: `+ - * / %`, `== != < > <= >=`, `&& || !`, and `cond ? a : b`.
`"a" + 1` makes a string. `color * 0.5` makes the colour darker.
`c.r`, `c.g`, `c.b`, `c.a`, `c.luma` read parts of a colour. `array.count` is its length.

## Functions

| Function | Result |
|---|---|
| `rgb(r, g, b [, a])` | colour, each part from 0 to 1 |
| `rgb8(r, g, b [, a])` | colour, each part from 0 to 255 |
| `gray(v [, a])` | grey, 0 is black and 1 is white |
| `hsl(h, s, l [, a])` | `h` in degrees (0-360), `s` and `l` from 0 to 1 |
| `hex("#3d9ee6")` | colour from a string |
| `mix(a, b, t)` | `t = 0` gives `a`, `t = 1` gives `b` |
| `lighten(c, t)` / `darken(c, t)` | move toward white / black |
| `alpha(c, a)` | same colour, new opacity |
| `min`, `max`, `clamp(v, lo, hi)`, `abs`, `round` | numbers |
| `count(x)`, `upper(s)`, `lower(s)` | length of array/string, letter case |

Your own `func` with the same name replaces a built-in one.

## Themes

```adco
theme "Name" extends "Other Theme" {     // extends is optional
    dark: true                           // or: light: true
    colors {
        bg, surface, panel, elevated     // grey levels: gray(0.07)
        text, muted, gutterBg, gutterText, lineHighlight
        accent, accent2, cursor, selection
        keyword, function, string, comment
        error, warning, success          // colours
    }
    font { family: "JetBrains Mono"; size: 13; lineHeight: 20; indent: 4 }
}
```

`bg`, `surface`, `panel`, `elevated`, `text`, `muted` and the gutter values are single
grey levels in Textor. If you give a colour for them, its average brightness is used.
Setting `bg` also moves `surface`, `panel` and `elevated` with it; set them after `bg`
to choose them yourself. A theme with the same name as an existing one replaces it.

See `themes/example.adco`.

## Addons

An addon adds one or more languages (and may bring themes):

```adco
addon "Lua Support" {
    version: "1.0"
    author: "Textor Community"
    description: "Lua highlighting"

    language "Lua" {
        extensions:  [".lua"]
        keywords:    ["function", "end", "local", "if", "then"]
        types:       ["table", "string"]
        builtins:    ["print", "pairs", "ipairs"]
        completions: ["table.insert", "string.format"]   // shown by autocomplete
        strings:     ["\"", "'"]                         // quote characters

        comment {
            line:  "--"
            block: ["--[[", "]]"]                        // [start, end]
        }
        colors      { keyword: #8CA6FF; type: #4DD9D9; builtin: #8CD9B3; operator: #AAAAAA }
        tokenColors { "self": #FFB86C }                  // colour for one exact word
        icon        { color: #7388F2; svg: "lua.svg" }   // file icon in the Explorer
    }

    theme "Lua Night" { ... }                            // optional
}
```

`icon` has three keys, all optional: `color` (the icon colour), `svg` (a file from the `svg/` folder
of the addon, drawn instead of the built-in icon of that language) and `symbol` (the name of a built-in
icon, e.g. `"cpp"`, to use when the addon has no SVG of its own). The SVG should be one colour: Textor
paints it with `color`. A wrong file name is listed in the terminal when Textor starts. A language with
no icon at all gets a plain file shape in its colour.

Colours that you leave out come from the active theme. Optional switches inside `language`:

- `hexColors: true` - `#rrggbb` is drawn in its own colour (used by Adco itself)
- `highlightKeys: true` - `name:` keys get the accent colour
- `attributePrefix: "@"` - `@word` is highlighted as an attribute

A bare `language "Name" { ... }` or `theme "Name" { ... }` at the top of a file works too.
Textor highlights C, C++, D, Rust, Python, Shell, Lua, Kotlin, Mekanism, Markdown (.md) and config files by itself.
**Adco** and any other language (Ruby, Go, JavaScript, ...) come from addons. A file whose extension
belongs to a built-in language uses the built-in highlighter; an addon only adds colours and completions there.
If two addons claim the same extension, the one loaded last wins.

## Mistakes

Problems are printed as `file.adco:LINE:COL: message` and listed in the terminal when
Textor starts. A file with mistakes still loads whatever it can: a wrong line is skipped,
the rest works.

## For developers

The whole language is one header, `base/adco.hxx`: `Lexer` -> `Parser` -> `Evaluator`.
`adco::evaluate(text)` returns a tree of `Node`s (kind, label, properties, children).
What a node means is decided by the callers: `theme_from_adco()` in `color/themes.hxx`
and `adco_sem::language_from_node()` in `base/addon_loader.hxx`. To add a new colour or
a new language option, change only those two places.
