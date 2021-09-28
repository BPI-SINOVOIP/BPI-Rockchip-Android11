#!/usr/bin/python3
""" Generate NeuralNetworks.h or types.hal from a specification file and a template file.
    See README.md for more details.
"""

import argparse
import re

class Reader:
  """ Simple base class facilitates reading a file.
      Derived class must implement handle_line() and may implement finish().
  """
  def __init__(self, filename):
    self.filename = filename
    self.line = None # most recently read line
    self.lineno = -1 # zero-based
  def finish(self):
    """ Called after entire file has been read
    """
    pass
  def handle_line(self):
    """ Called after each line has been read
    """
    assert False
  def read(self):
    with open(self.filename) as f:
      lines = f.readlines()
    for self.lineno in range(len(lines)):
      self.line = lines[self.lineno]
      self.handle_line()
    self.finish()
  def context(self):
    """ Error-reporting aid: Return a string describing the location
        of the most recently read line
    """
    return "line " + str(self.lineno + 1) + " of " + self.filename

class Specification(Reader):
  """ Reader for specification file
  """

  # Describes %kind state
  UNCONDITIONAL = 0   # No %kind in effect
  CONDITIONAL_OFF = 1 # %kind in effect, lines are to be ignored
  CONDITIONAL_ON = 2  # %kind in effect, lines are to be processed

  def __init__(self, filename, kind):
    super(Specification, self).__init__(filename)
    self.sections = dict() # key is section name, value is array of strings (lines) in the section
    self.section = None # name of current %section
    self.defmacro = dict() # key is macro name, value is string (body of macro)
    self.deflines = dict() # key is definition name, value is array of strings (lines) in the definition
    self.deflines_key = None # name of current %define-lines
    self.kind = kind
    self.kinds = None # remember %define-kinds
    self.conditional = self.UNCONDITIONAL

  def finish(self):
    assert self.section is None, "\"%section " + self.section + \
      "\" not terminated by end of specification file"
    assert self.deflines_key is None, "\"%define-lines " + self.deflines_key + \
      "\" not terminated by end of specification file"
    assert self.conditional is self.UNCONDITIONAL, "%kind not terminated by end of specification file"

  def macro_substitution(self):
    """ Performs macro substitution on self.line, and returns the result
    """
    LINESEARCH = "(%\{)(\S+?)(?=[\s}])\s*(.*?)\s*(\})"
    BODYSEARCH = "(%\{)(\d+)(\})"

    orig = self.line
    out = ""
    match = re.search(LINESEARCH, orig)
    while match:
      # lookup macro
      key = match[2]
      assert key in self.defmacro, "Missing definition of macro %{" + key + "} at " + self.context()

      # handle macro arguments (read them and substitute for them in the macro body)
      body_orig = self.defmacro[key]
      body_out = ""
      args = []
      if match[3] != "":
        args = re.split("\s+", match[3])
      bodymatch = re.search(BODYSEARCH, body_orig)
      while bodymatch:
        argnum = int(bodymatch[2])
        assert argnum >= 0, "Macro argument number must be positive (at " + self.context() + ")"
        assert argnum <= len(args), "Macro argument number " + str(argnum) + " exceeds " + \
          str(len(args)) + " supplied arguments at " + self.context()
        body_out = body_out + body_orig[:bodymatch.start(1)] + args[int(bodymatch[2]) - 1]
        body_orig = body_orig[bodymatch.end(3):]
        bodymatch = re.search(BODYSEARCH, body_orig)
      body_out = body_out + body_orig

      # perform macro substitution
      out = out + orig[:match.start(1)] + body_out
      orig = orig[match.end(4):]
      match = re.search(LINESEARCH, orig)
    out = out + orig
    return out

  def match_kind(self, patterns_string):
    """ Utility routine for %kind directive: Is self.kind found within patterns_string?"""
    patterns = re.split("\s+", patterns_string.strip())
    for pattern in patterns:
      wildcard_match = re.search("^(.*)\*$", pattern)
      lowest_version_match = re.search("^(.*)\+$", pattern)
      if wildcard_match:
        # A wildcard pattern: Ends in *, so see if it's a prefix of self.kind.
        if re.search("^" + re.escape(wildcard_match[1]), self.kind):
          return True
      elif lowest_version_match:
        # A lowest version pattern: Ends in + and we check if self.kind is equal
        # to the kind in the pattern or to any kind which is to the right of the
        # kind in the pattern in self.kinds.
        assert lowest_version_match[1] in self.kinds, (
            "Kind \"" + pattern + "\" at " + self.context() +
            " wasn't defined in %define-kinds"
        )
        lowest_pos = self.kinds.index(pattern[:-1])
        return self.kind in self.kinds[lowest_pos:]
      else:
        # An ordinary pattern: See if it matches self.kind.
        if not self.kinds is None and not pattern in self.kinds:
          # TODO: Something similar for the wildcard case above
          print("WARNING: kind \"" + pattern + "\" at " + self.context() +
                " would have been rejected by %define-kinds")
        if pattern == self.kind:
          return True
    return False

  def handle_line(self):
    """ Most of the work occurs here.  Having read a line, we act on it immediately:
        skip a comment, process a directive, add a line to a section or a to a multiline
        definition, etc.
    """

    DIRECTIVES = ["%define", "%define-kinds", "%define-lines", "%/define-lines",
                  "%else", "%insert-lines", "%kind", "%/kind", "%section",
                  "%/section"]

    # Common typos: /%directive, \%directive
    matchbad = re.search("^[/\\\]%(\S*)", self.line)
    if matchbad and "%/" + matchbad[1] in DIRECTIVES:
      print("WARNING: Probable misspelled directive at " + self.context())

    # Directive?
    if re.search("^%", self.line) and not re.search("^%{", self.line):
      # Check for comment
      if re.search("^%%", self.line):
        return

      # Validate directive name
      match = re.search("^(%\S*)", self.line);
      directive = match[1]
      if not directive in DIRECTIVES:
        assert False, "Unknown directive \"" + directive + "\" on " + self.context()

      # Check for end of multiline macro
      match = re.search("^%/define-lines\s*(\S*)", self.line)
      if match:
        assert match[1] == "", "Malformed directive \"%/define-lines\" on " + self.context()
        assert not self.deflines_key is None, "%/define-lines with no matching %define-lines on " + \
          self.context()
        self.deflines_key = None
        return

      # Directives are forbidden within multiline macros
      assert self.deflines_key is None, "Directive is not permitted in definition of \"" + \
        self.deflines_key + "\" at " + self.context()

      # Check for define (multi line)
      match = re.search("^%define-lines\s+(\S+)\s*$", self.line)
      if match:
        key = match[1]
        if self.conditional is self.CONDITIONAL_OFF:
          self.deflines_key = ""
          return
        assert not key in self.deflines, "Duplicate definition of \"" + key + "\" on " + self.context()
        self.deflines[key] = []
        self.deflines_key = key
        # Non-directive lines will be added to self.deflines[key] as they are read
        # until we see %/define-lines
        return

      # Check for insert
      match = re.search("^%insert-lines\s+(\S+)\s*$", self.line)
      if match:
        assert not self.section is None, "%insert-lines outside %section at " + self.context()
        key = match[1]
        assert key in self.deflines, "Missing definition of lines \"" + key + "\" at " + self.context()
        if self.conditional is self.CONDITIONAL_OFF:
          return
        self.sections[self.section].extend(self.deflines[key]);
        return

      # Check for start of section
      match = re.search("^%section\s+(\S+)\s*$", self.line)
      if match:
        assert self.section is None, "Nested %section is forbidden at " + self.context()
        assert self.conditional is self.UNCONDITIONAL, "%section within %kind is forbidden at " + self.context()
        key = match[1]
        assert not key in self.sections, "Duplicate definition of \"" + key + "\" on " + self.context()
        self.sections[key] = []
        self.section = key
        # Non-directive lines will be added to self.sections[key] as they are read
        # until we see %/section
        return

      # Check for end of section
      if re.search("^%/section\s*$", self.line):
        assert not self.section is None, "%/section with no matching %section on " + self.context()
        assert self.conditional is self.UNCONDITIONAL # can't actually happen
        self.section = None
        return

      # Check for start of kind
      match = re.search("^%kind\s+((\S+)(\s+\S+)*)\s*$", self.line)
      if match:
        assert self.conditional is self.UNCONDITIONAL, "%kind is nested at " + self.context()
        patterns = match[1]
        if self.match_kind(patterns):
          self.conditional = self.CONDITIONAL_ON
        else:
          self.conditional = self.CONDITIONAL_OFF
        return

      # Check for complement of kind (else)
      if re.search("^%else\s*$", self.line):
        assert not self.conditional is self.UNCONDITIONAL, "%else without matching %kind on " + self.context()
        if self.conditional == self.CONDITIONAL_ON:
          self.conditional = self.CONDITIONAL_OFF
        else:
          assert self.conditional == self.CONDITIONAL_OFF
          self.conditional = self.CONDITIONAL_ON
        # Note that we permit
        #   %kind foo
        #   abc
        #   %else
        #   def
        #   %else
        #   ghi
        #   %/kind
        # which is equivalent to
        #   %kind foo
        #   abc
        #   ghi
        #   %else
        #   def
        #   %/kind
        # Probably not very useful, but easier to allow than to forbid.
        return

      # Check for end of kind
      if re.search("^%/kind\s*$", self.line):
        assert not self.conditional is self.UNCONDITIONAL, "%/kind without matching %kind on " + self.context()
        self.conditional = self.UNCONDITIONAL
        return

      # Check for kinds definition
      match = re.search("^%define-kinds\s+(\S.*?)\s*$", self.line)
      if match:
        assert self.conditional is self.UNCONDITIONAL, "%define-kinds within %kind is forbidden at " + \
          self.context()
        kinds = re.split("\s+", match[1])
        assert self.kind in kinds, "kind \"" + self.kind + "\" is not listed on " + self.context()
        assert self.kinds is None, "Second %define-kinds directive at " + self.context()
        self.kinds = kinds
        return

      # Check for define
      match = re.search("^%define\s+(\S+)(.*)$", self.line)
      if match:
        if self.conditional is self.CONDITIONAL_OFF:
          return
        key = match[1]
        assert not key in self.defmacro, "Duplicate definition of \"" + key + "\" on " + self.context()
        tail = match[2]
        match = re.search("\s(.*)$", tail)
        if match:
          self.defmacro[key] = match[1]
        else:
          self.defmacro[key] = ""
        return

      # Malformed directive -- the name matched, but the syntax didn't
      assert False, "Malformed directive \"" + directive + "\" on " + self.context()

    if self.conditional is self.CONDITIONAL_OFF:
      pass
    elif not self.deflines_key is None:
      self.deflines[self.deflines_key].append(self.macro_substitution())
    elif self.section is None:
      # Treat as comment
      pass
    else:
      self.sections[self.section].append(self.macro_substitution())

class Template(Reader):
  """ Reader for template file
  """

  def __init__(self, filename, specification):
    super(Template, self).__init__(filename)
    self.lines = []
    self.specification = specification

  def handle_line(self):
    """ Most of the work occurs here.  Having read a line, we act on it immediately:
        skip a comment, process a directive, accumulate a line.
    """

    # Directive?
    if re.search("^%", self.line):
      # Check for comment
      if re.search("^%%", self.line):
        return

      # Check for insertion
      match = re.search("^%insert\s+(\S+)\s*$", self.line)
      if match:
        key = match[1]
        assert key in specification.sections, "Unknown section \"" + key + "\" on " + self.context()
        for line in specification.sections[key]:
          if re.search("TODO", line, re.IGNORECASE):
            print("WARNING: \"TODO\" at " + self.context())
          self.lines.append(line)
        return

      # Bad directive
      match = re.search("^(%\S*)", self.line)
      assert False, "Unknown directive \"" + match[1] + "\" on " + self.context()

    # Literal text
    if re.search("TODO", self.line, re.IGNORECASE):
      print("WARNING: \"TODO\" at " + self.context())
    self.lines.append(self.line)

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Create an output file by inserting sections "
                                   "from a specification file into a template file")
  parser.add_argument("-k", "--kind", required=True,
                      help="token identifying kind of file to generate (per \"kind\" directive)")
  parser.add_argument("-o", "--output", required=True,
                      help="path to generated output file")
  parser.add_argument("-s", "--specification", required=True,
                      help="path to input specification file")
  parser.add_argument("-t", "--template", required=True,
                      help="path to input template file")
  parser.add_argument("-v", "--verbose", action="store_true")
  args = parser.parse_args()
  if args.verbose:
    print(args)

  # Read the specification
  specification = Specification(args.specification, args.kind)
  specification.read()
  if (args.verbose):
    print(specification.defmacro)
    print(specification.deflines)

  # Read the template
  template = Template(args.template, specification)
  template.read()

  # Write the output
  with open(args.output, "w") as f:
    f.write("".join(["".join(line) for line in template.lines]))

# TODO: Write test cases for malformed specification and template files
# TODO: Find a cleaner way to handle conditionals (%kind) or nesting in general;
#       maybe add support for more nesting
# TODO: Unify section/define-lines, rather than having two kinds of text regions?
#       Could we take this further and do away with the distinction between a
#       specification file and a template file, and add a %include directive?
