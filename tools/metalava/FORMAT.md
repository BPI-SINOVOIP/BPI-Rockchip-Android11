# Signature Formats

This document describes the signature file format created and used by metalava,
doclava, apicheck, etc.

There are currently 3 versions of this format:

1. The format emitted by doclava, and used for Android's signature files up
   through Android P. Note that this isn't actually a single format; it evolved
   over time, so older signature files vary a bit (many of these changes were
   due to bugs getting fixed, such as type parameters missing from classes
   and methods until they start appearing), and some were deliberate changes,
   such as dropping the "final" modifier in front of every member if the
   containing class is final.
   
2. The "new" format, which is described below, and is used in Android Q. This
   format adds new information, such as annotations, parameter names and default
   values, as well as cleans up a number of things (such as dropping
   java.lang. prefixes on types, etc)
   
3. This is format v2, but will all nullness annotations replaced by a
   Kotlin-syntax, e.g. "?" for nullable types, "!" for unknown/platform types,
   and no suffix for non-nullable types. The initial plan was to include this
   in format v2, but it was deferred since type-use annotations introduces
   somple complexities in the implementation.
    

## Motivation

Why did we change from the historical doclava signature format (v1)
to a new format?

In order to support Kotlin better (though this will also benefit Java
developers), we'd like to have nullness annotations (as well as some other
annotations) be a formal part of the SDK.

That means the annotations should be part of the signature files too -- such
that we can not just record explicitly what the API contract is, but also
enforce that changes are not only deliberate changes but also compatible
changes. (For example, you can change the return value of a final method from
nullable to non null, but not the other way around.)

And if we were going to change the signature format, we might as well make some
other changes too.


### Comments

In v2, line comments (starting with //) are allowed. This allows us to leave
reminders and other issues with the signature source (though the update-api task
will generally blow these away, so use sparingly.)

### Header

New signature files (v2+) generally include a file header comment which states
the version number. This makes it possible for tools to more safely interpret
signature files. For example, in v3 the type "String" means "@NonNull String",
but in v2 "String" means "String with unknown nullness".

The header looks like this:

```
// Signature format: 2.0
```

Here "2" is the major format version; the .0 allows for compatible minor
variations of the format.

### Include Annotations

The new signature format now includes annotations; not all annotations (such as
@Override etc); only those which are significant for the API, such as nullness
annotations, etc.

Annotations are included on the same line as the class/field/method, right
before the modifiers.

Here's how this looks:


```
  method @Nullable public static Integer compute1(@Nullable java.util.List<java.lang.String>);
```


(Notice how the annotations are not using fully qualified name; that's discussed
below.)

The annotations to be included are annotations for annotation types that are not
hidden, and have class file or runtime retention.

The annotations should be sorted alphabetically by fully qualified name.


### Use Special Syntax or Nullness Annotations

(Note: Only in version format 3+)

As a special optimization, since we eventually want **all** APIs to have
explicit nullness, use Kotlin's syntax for nullness. That means that for
nullable elements, we add "?" after the type, for unknown nullness we add "!",
and otherwise there's no suffix. In other words:


<table>
  <tr>
   <td>
   </td>
   <td>Java Type
   </td>
   <td>Signature File Type
   </td>
  </tr>
  <tr>
   <td>Nullable
   </td>
   <td>@Nullable String
   </td>
   <td>String?
   </td>
  </tr>
  <tr>
   <td>Not nullable
   </td>
   <td>@NonNull String
   </td>
   <td>String
   </td>
  </tr>
  <tr>
   <td>Unknown nullability
   </td>
   <td>String
   </td>
   <td>String!
   </td>
  </tr>
</table>


The above signature line is turned into


```
 method public Integer? compute1(java.util.List<java.lang.String!>?);
```


### Clean Up Terminology 

Format v2 also cleans up some of the terminology used to describe the class
structure in the signature file. For example, in v1, an interface is called an
"abstract interface"; an interface extending another interface is said to
"implement" it instead of "extend"-ing it, etc; enums and annotations are just
referred to as classes that extend java.lang.Enum, or java.lang.Annotation etc.

With these changes, these lines from v1 signature files:


```
  public abstract interface List<E> implements java.util.Collection { ... }
  public class TimeUnit extends java.lang.Enum { ... }
  public abstract class SuppressLint implements java.lang.annotation.Annotation { ... }
```


are replaced by


```
    public interface List<E> extends java.util.Collection<E> { ... }
    public enum TimeUnit { ... }
    public @interface SuppressLint { ... }
```



### Use Generics Everywhere 

The v1 signature files uses raw types in some places but not others.  Note that
in the above it was missing from super interface Collection:


```
  public abstract interface List<E> implements java.util.Collection { ... }
```


 whereas in the v2 format it's included:


```
    public interface List<E> extends java.util.Collection<E> { ... }
```


Similarly, v1 used erasure in throws clauses. For example, for this method:


```
    public <X extends Throwable> T orElseThrow(Supplier<? extends X> exceptionSupplier) throws X
```

v1 used this signature:


```
 method public <X extends java.lang.Throwable> T orElseThrow(java.util.function.Supplier<? extends X>) throws java.lang.Throwable;
```

Note how that's "throws Throwable" instead of "throws X". This results in b/110302703.

In the v2 format we instead use the correct throws type:

```
 method public <X extends java.lang.Throwable> T orElseThrow(java.util.function.Supplier<? extends X>) throws X;
```


### Support Annotations

The old format was completely missing annotation type methods:

```
  public static abstract class ViewDebug.ExportedProperty implements java.lang.annotation.Annotation {
  }
```

We need to include annotation member methods, as well as their default values
since those are API-significant. Here's how this looks in the v2 file format
(also applying the @interface terminology change described above) :


```
  public static @interface ViewDebug.ExportedProperty {
    method public abstract String category() default "";
    method public abstract boolean deepExport() default false;
    method public abstract android.view.ViewDebug.FlagToString[] flagMapping() default {};
    method public abstract boolean formatToHexString() default false;
    method public abstract boolean hasAdjacentMapping() default false;
    method public abstract android.view.ViewDebug.IntToString[] indexMapping() default {};
    method public abstract android.view.ViewDebug.IntToString[] mapping() default {};
    method public abstract String prefix() default "";
    method public abstract boolean resolveId() default false;
  }
```


### Support Kotlin Modifiers 

This doesn't currently apply to the SDK, but the signature files are also used
in the support library, and some of these are written in Kotlin and exposes
Kotlin-specific APIs.

That means the v2 format can express API-significant aspects of Kotlin. This
includes special modifiers, such as sealed, inline, operator, infix, etc:

```
 method public static operator int get(android.graphics.Bitmap, int x, int y);
 method public static infix android.graphics.Rect and(android.graphics.Rect, android.graphics.Rect r);
```

### Support Kotlin Properties 

Kotlin's Java support means that it wil take a Kotlin property and compile it
into getters and setters which you can call from Java. But you cannot calls
these getters and setters from Kotlin; you **must** use the property
syntax. Therefore, we need to also capture properties in the signature files. If
you have this Kotlin code:


```
         var property2: String? = "initial"
```

it will get recorded in the signature files like this:

```
         property public java.lang.String? property2 = "initial";
         method public java.lang.String? getProperty2();
         method public void setProperty2(java.lang.String? p);
```

The last two elements are "redundant"; they could be computed from the property
name (and included if the property declaration uses special annotations to name
the getters and setters away from the defaults), but it's helpful to be explicit
(and this allows us to specify the default value).

### Support Named Parameters 

Kotlin supports default values for parameters, and these are a part of the API
contract, so we need to include them in the signature format.

Here's an example:

```
    method public static void edit(android.content.SharedPreferences, boolean commit);
```

In v1 files we only list type names, but in v2 we allow an optional parameter
name to be specified; "commit" in the above.

Note that this isn't just for Kotlin. Just like there are special nullness
annotations to mark up the null contract for an element, we will also have a
special annotation to explicitly name a Java parameter:
@android.annotation.ParameterName (which is hidden). This obviously isn't usable
from Java, but Kotlin client code can now reference the parameter.

Therefore, the following Java code (not signature code) will also produce
exactly the same signature as the above:

```
    public static void edit(SharedPreferences prefs, @ParameterName("commit") boolean ct) {…}
```

(Note how the implementation parameter doesn't have to match the public, API
name of the parameter.)

### Support Default Values 

In addition to named parameters, Kotlin also supports default values. These are
also be part of the v2 signature since (as an example) removing a default value
is a compile-incompatible change.

Therefore, the v2 format allows default values to be specified after the type
and/or parameter name:

```
    method public static void edit(SharedPreferences, boolean commit = false);
```

For Kotlin code, the default parameter values are extracted automatically, and
for Java, just as with parameter names, you can specify a special annotation to
record the default value for usage from languages that support default parameter
values:

```
    public static void edit(SharedPreferences prefs, @DefaultValue("false") boolean ct) {…}
```


### Include Inherited Methods

Consider a scenario where a public class extends a hidden class, and that hidden
class defines a public method.

Doclava did not include these methods in the signature files, but they **were**
present in the stub files (and therefore part of the API). In the v2 signature
file format, we include these.

An example of this is StringBuilder#setLength. According to the old signature
files, that method does not exist, but clearly it's there in the SDK. The reason
this happens is that StringBuilder is a public class which extends hidden class
AbstractStringBuilder, which defines the public method setLength.


### No Hardcoded Enum Methods

Doclava always inserted two special methods in the signature files for every
enum: values() and valueOf():

```
  public static final class CursorJoiner.Result extends java.lang.Enum {
    method public static android.database.CursorJoiner.Result valueOf(java.lang.String);
    method public static final android.database.CursorJoiner.Result[] values();
    enum_constant public static final android.database.CursorJoiner.Result BOTH;
    enum_constant public static final android.database.CursorJoiner.Result LEFT;
    enum_constant public static final android.database.CursorJoiner.Result RIGHT;
  }
```

It didn't do that in stubs, because you can't: those are special methods
generated by the compiler. There's no reason to list these in the signature
files since they're entirely implied by the enum, you can't change them, and
it's just extra noise.

In the new v2 format these are no longer present:

```
  public static enum CursorJoiner.Result {
    enum_constant public static final android.database.CursorJoiner.Result BOTH;
    enum_constant public static final android.database.CursorJoiner.Result LEFT;
    enum_constant public static final android.database.CursorJoiner.Result RIGHT;
  }
```

### Remove "deprecated" Modifier

The old signature file format used "deprecated" as if it was a modifier. In the
new format, we instead list these using annotations, @Deprecated.

### Standard Modifier Order

Doclava had a "random" (but stable) order of modifiers.

In the new signature format, we're using the standard modifier order for Java
and Kotlin, wihch more closely mirrors what is done in the source code.

Version format 1 order:

```
public/protected/private default static final abstract synchronized transient volatile
```

Version format 2 order:

```
public/protected/internal/private abstract default static final transient volatile synchronized
```

The above list doesn't include the Kotlin modifiers, which are inserted
according to the Kotlin language style guide:
https://kotlinlang.org/docs/reference/coding-conventions.html#modifiers

### Sort Classes By Fully Qualified Names

In "extends" lists, the signature file can list a comma separated list of
classes. The classes are listed by fully qualified name, but in v1 it was sorted
by simple name. In the v2 format, we sort by fully qualified name instead.

### Use Wildcards Consistently

Doclava (v1) would sometimes use the type bound <?> and other times use <?
extends Object>. These are equivalent. In the v2 format, <? extends Object> is
always written as <?>.

### Annotation Simple Names

We have a number of annotations which are significant for the API -- not just
the nullness as deprecation ones (which are specially supported in v3 via the
?/! Kotlin syntax and the deprecated "modifier"), but annotations for permission
requirements, range constraints, valid constant values for an integer, and so
on.

In the codebase, these are typically in the android.annotation. package,
referencing annotation classes that are generally **not** part of the API. When
we generate the SDK, we translate these into publicly known annotations,
androidx.annotation, such that Studio, lint, the Kotlin compiler and others can
recognize the metadata.

That begs the question: which fully qualified name should we put in the
signature file? The one that appeared in the source (which is hidden, or in the
case of Kotlin code, a special JetBrains nullness annotation), or the one that
it gets translated into?

In v2 we do neither: We use only the simple name of the annotations in the
signature file, for annotations that are in the well known packages. In other
words, instead of any of these alternative declarations:

```
   method public void setTitleTextColor(@android.annotation.ColorInt int);
   method public void setTitleTextColor(@android.support.annotation.ColorInt int);
   method public void setTitleTextColor(@androidx.annotation.ColorInt int);
```

in v2 we have simply 

```
   method public void setTitleTextColor(@ColorInt int);
```

### Simple Names in Java.lang

In Java files, you can implicitly reference classes in java.lang without
importing them. In v2 offer the same thing in signature files. There are several
classes from java.lang that are used in lots of places in the signature file
(java.lang.String alone is present in over 11,000 lines of the API file), and
other common occurrences are java.lang.Class, java.lang.Integer,
java.lang.Runtime, etc.

This basically builds on the same idea from having an implicit package for
annotations, and doing the same thing for java.lang: Omitting it when writing
signature files, and implicitly adding it back when reading in signature files.

This only applies to the java.lang package, not any subpackages, so for example
java.lang.reflect.Method will **not** be shortened to reflect.Method.

### Type Use Annotations

In v3, "type use annotations" are supported which means annotations can appear
within types.

### Skipping some signatures

If a method overrides another method, and the signatures are the same, the
overriding method is left out of the signature file. This basically compares the
modifiers, ignoring some that are not API significant (such as "native"). Note
also that some modifiers are implicit; for example, if a method is implementing
a method from an interface, the interface method is implicitly abstract, so the
implementation will be included in the signature file.

In v2, we take this one step further: If a method differs **only** from its
overridden method by "final", **and** if the containing class is final, then the
method is not included in the signature file. The same is the case for
deprecated.

### Miscellaneous

Some other minor tweaks in v2:

*   Fix formatting for package private elements. These had two spaces of
    indentation; this is probably just a bug. The new format aligns their
    indentation with all other elements.
*   Don't add spaces in type bounds lists (e.g. Map<X,Y>, not Map<X, Y>.)

## Historical API Files

Metalava can read and write these formats. To switch output formats, invoke it
with for example --format=v2.

The Android source tree also has checked in versions of the signatures for all
the previous API levels. Metalava can regenerate these for a new format.
For example, to update all the signature files to v3, run this command:

```
$ metalava --write-android-jar-signatures *<android source dir>* --format=v3
```
