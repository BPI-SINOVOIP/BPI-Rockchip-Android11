# API File Generation

There are certain pieces of `NeuralNetworks.h` and of our various `*.hal` files
that ought to be kept in sync -- most notably the operand type and operation
type definitions and descriptions in our `NeuralNetworks.h` and `types.hal`
files.  To avoid having to do this manually, a tool `generate_api.py` is
employed to combine a single *specification file* with one *template file* per
API file (`NeuralNetworks.h` or `types.hal`) to produce that API file.  The
script `generate_api.sh` invokes `generate_api.py` once per API file, passing
appropriate arguments.

## `generate_api.sh`

The environment variable `ANDROID_BUILD_TOP` must be set.

Invoked with no arguments, this script regenerates the `NeuralNetworks.h` file
and every `types.hal` file in place, by invoking `generate_api.py` once per
generated file.

Invoked with the `--dryrun` argument, this script instead shows how it would
invoke `generate_api.py`.

## `generate_api.py`

This tool generates a single output file from an input specification file and an
input template file.  It takes the following mandatory arguments:

* `--output OUTPUT` path to generated output file (such as `NeuralNetworks.h`)
* `--specification SPECIFICATION` path to input specification file
* `--template TEMPLATE` path to input template file
* `--kind KIND` token identifying kind of file to generate

The "kind" is an arbitrary token that the specification file can reference with
the `%kind` directive to help generate different text in different situations.
It has no meaning to the tool itself.  Today, the following kinds are used:
`ndk` (when generating `NeuralNetworks.h`), `hal_1.0` (when generating
`1.0/types.hal`), `hal_1.1`, `hal_1.2` and `hal_1.3`.

## Template File Syntax

Every line of the template file is copied verbatim to the output file *unless*
that line begins with `%`.

A line that begins with `%%` is a comment, and is ignored.

A line that begins with `%` and is not a comment is a *directive*.

### Directives

#### `%insert *name*`

Copy the *section* with the specified *name* from the specification file to the
output file.  The section is defined by a `%section` directive in the
specification file.

## Specification File Syntax

The specification file consists of comments, *directives*, and other text.

A line that begins with `%%` is a comment, and is ignored.

A line that begins with `%` and is not a comment is a *directive*.

The meaning of a line that is neither a comment nor a directive depends on the
context -- the *region* in which that line appears.

### Regions

The specification file is divided into *regions*, which are sequences of lines
delimited by certain directives.

Certain regions can enclose certain other regions, but this is very limited:

* A conditional region can enclose a definition region.
* A section region can enclose a conditional region or a definition region.

Equivalently:

* A conditional region can be enclosed by a section region.
* A definition region can be enclosed by a conditional region or a section
  region.
  
#### null region

A *null region* is a sequence of lines that is not part of any other region.
For example, a specification file that contains no directives other than
`%define` and `%define-kinds` consists of a single null region.

Within a null region, all lines other than directives are treated as comments
and are ignored.

#### conditional region

A *conditional region* is a sequence of lines immediately preceded by the `%kind
*list*` directive and immediately followed by the `%/kind` directive.  The
`%kind` directive establishes a condition state **on** or **off** (see the
description of the directive for details).  When the condition is **on**, the
lines in the region are processed normally (i.e., directives have their usual
effect, and non-directive lines are added to the enclosing section region, if
any).  When the condition is **off**, lines in the region other than the `%else`
directive are ignored *except* that even ignored directives undergo some level
of syntactic and semantic checking.

#### definition region

A *definition region* is a sequence of lines immediately preceded by the
`%define-lines *name*` directive and immediately followed by the
`%/define-lines` directive.  Every non-comment line in the sequence undergoes
macro substitution, and the resulting lines are associated with the region name.
They can later be added to a section region with the `%insert-lines` directive.

This can be thought of as a multi-line macro facility.

#### section region

A *section region* is a sequence of lines immediately preceded by the `%section
*name*` directive and immediately followed by the `%/section` directive.  Every
non-comment line in the sequence undergoes macro substitution, and the resulting
lines are associated with the section name.  They can be inserted into the
generated output file as directed by the template file's `%insert` directive.

This is the mechanism by which a specification file contributes text to the
generated output file.

### Directives

#### `%define *name* *body*`

Defines a macro identified by the token *name*.  The *body* is separated from
the *name* by exactly one whitespace character, and extends to the end of the
line -- it may contain whitespace itself. For example,

  %define test  this body begins and ends with a space character 

Macro substitution occurs within a definition region or a section region: a
substring `%{*name*}` is replaced with the corresponding *body*.  Macro
substitution is *not* recursive: A substring `%{*name2*}` in *body* will not
undergo macro substitution, except as discussed for *macro arguments* below.

Permitted in regions: null, conditional, section

##### macro arguments

The more general form of a macro invocation is `%{*name* *arglist*}`, where
*arglist* is a list of whitespace-separated arguments.  Within the *body*, a
substring of the form `%{argnum}` will be replaced by the corresponding argument
from *arglist*.  For example, if the definition is

  %define test second is %{2}, first is %{1}
  
then the macro invocation

  %{test alpha beta}
  
is expanded to

  second is beta, first is alpha

The only check on the number of arguments supplied at macro invocation time is
that there must be at least as many arguments as the highest `%{argnum}`
reference in the macro body.  In the example above, `%{test alpha}` would be an
error, but `%{test alpha beta gamma}` would not.

#### `%define-lines *name*`, `%/define-lines`

`%define-lines *name*` creates a *definition region* terminated by
`%/define-lines`.

Permitted in regions: null, conditional, section

#### `%insert-lines *name*`

Adds all lines from the named definition region to the current section region.

Permitted in regions: section

#### `%kind *list*`, `%else`, `%/kind`

`%kind *list*` creates a *conditional region* terminated by `%/kind`.

The *list* consists of a space-delimited list of tokens, any of which may end in
`*` to indicate a *wildcard pattern* or `+` to indicate a *lowest version
pattern*. Any other pattern is a *simple pattern*. The condition is **on** in
three cases:
* One of the simple pattern tokens equals the "kind"
* One of the wildcard pattern tokens less the `*` is a prefix of the "kind"
* One of the lowest version pattern tokens less the `+` matches the "kind" or
  the "kind" matches any token to the right from the lowest version pattern in
  the list passed to %define-kinds

In all other cases, the condition is **off**.

Within the region, the condition is inverted every time the `%else` directive
appears.

Permitted in regions: null, section

#### `%define-kinds *list*`

This directive has two purposes:

* Sanity-checking. If the "kind" is not on the space-delimited *list* of tokens,
  `generate_api.py` terminates with an error.
* Ordering the possible kinds for the *lowest version pattern* (see the section
  above for the explanation of the pattern).

Only one such directive is allowed per specification file.

Permitted in regions: null, section

#### `%section *name*`, `%/section`

`%section *name*` creates a *section region* terminated by `%/section`.

Permitted in regions: null
