[RFC][ItaniumDemangle] Compact C++ names

Demangled C++ names can quickly become hard to read due to their length (e.g., particularly when templates are involved). Users of tools that display demangled names like LLDB (for backtraces) may not care about all elements of the original C++ name (in the case of debugger backtraces a user most likely just wants to quickly see the function name for each frame). We've had similar requests in the past from users looking at backtraces in crash reports or profiling tools, where shorter demangled names would help user-experience and also potentially help with crash report sizes.

The proposal is to introduce a new set of options (akin to a "printing policy") to the LLVM ItaniumDemangler to control aspects of how the demangled name should be printed.

E.g.,:
```
$ llvm-cxxfilt _ZN4llvm2cl4listINSt3__112basic_stringIcNS2_11char_traitsIcEENS2_9allocatorIcEEEEbNS0_6parserIS8_EEEC1IJA43_cNS0_4descENS0_9MiscFlagsENS0_12OptionHiddenENS0_3catENS0_2cbIvRKS8_EEEEEDpRKT_

llvm::cl::list<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>, bool, llvm::cl::parser<std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>>> >::list<char [43], llvm::cl::desc, llvm::cl::MiscFlags, llvm::cl::OptionHidden, llvm::cl::cat, llvm::cl::cb<void, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&>>( char const (&) [43], llvm::cl::desc const&, llvm::cl::MiscFlags const&, llvm::cl::OptionHidden const&, llvm::cl::cat const&, llvm::cl::cb<void, std::__1::basic_string<char, std::__1::char_traits<char>, std::__1::allocator<char>> const&> const&)
```

Note:
1. `std::basic_string` isn't reduced to `std::string` (technically because the `Ss` substitution was inhibited by the inline namespace)
2. Default template parameters are expanded (the mangling doesn't tell us anything about whether a parameter is defaulted, so nothing the demangler could've known)
3. It's really hard to identify what the function name here actually is

A user might really just want to see the following:
```
llvm::cl::list<..>::list<..>(char const (&)[43], llvm::cl::desc const&, llvm::cl::MiscFlags const&, llvm::cl::OptionHidden const&, llvm::cl::cast const&, llvm::cl::cb<..> const&)
```

Now it's much more apparent that we're dealing with a constructor call.

Related discussion: https://discourse.llvm.org/t/llvm-cxxfilt-alternate-renderings-for-particularly-large-demanglings/71303

# Prior Art

On Windows the demangler is configurable.

`swift::Demangle::DemangleOptions`.

# Implementation Considerations

This section describes some details of a possible implementation of this RFC (based on our experience developing the prototype).

## How to pass options to ItaniumDemangle

* Main entry point to printing
* printing<-> AST coupling

Solution:
* Separate the `print` out of `itanium_demangle::Node` into a new `itanium_demangle::PrintVisitor` class which walks over the demangle tree. This visitor can now hold any printing-related state.
Instantiate PrintOptions at arg-parsing time; refactor
printLeft/printRight/etc methods on Node classes into operator()
overloads (one for each Node subclass) on PrintOptions PrintVisitor,
which traverses the AST with Node->visit. This lets us have the
PrintOptions (stored in the PrintVisitor) in scope while printing each
Node without storing it in the Node itself or the OutputBuffer.

demangler already has a visitor, but it's only used for dumping debug representation
  * This has the benefit of being able to move the printing state out of `OutputBuffer`
* New `itanium_demangle::PrintingPolicy` structure passed as an argument to new `PrintVisitor`

One thing I find nasty is that in order to print, you need to have the
Demangler object around, because all the node’s lifetimes are tied to
the Demangler. So PrintVisitor::print (and Node::print previously) are
kind of easy to mis-use by accident, because they seem to be
completely decoupled from the Demangler, which in reality they’re not.
I wonder if Demangler::print is the better place for the print entry
point. But haven’t thought much about the architecture implications of
this. This can actually be done in a completely separate commit prior
to our refactors:

Add a AbstractManglingParser::print(Node const*) method
Ensure nothing apart from AbstractManglingParser and Node calls the
print methods

ItaniumPartialDemangler makes this annoying....

Then our changes just add a PrintVisitor which only change the
implementation of AbstractManglingParser::print , but don’t need to
add a new entry point. So our final diff might be smaller since we
don’t need to patch all the Node->print callers.

## How to expose options beyond demangler library

This proposal is only concerned with adding these options to the demangler itself. Tools that get the demangled name via `llvm::Demangle` (such as `llvm-cxxfilt`) wouldn't immediately benefit from this. We would need agree on what the command-line options looked like and how those would be passed into the `ItaniumDemangle`.

We primarily looked at the LLDB use-case when prototyping this, which invokes the `ItaniumPartialDemangler` directly, so can construct the `PrintingPolicy` at the call-site.

# Potential PrintingPolicy Options

* Hide template parameters
  * Could have configurable depth
  * Can even have HideAllTemplateParams/HideClassTemplateParams/etc.
* Recognize `std::__1::` as `std::` (though this may not be the appropriate level to fix this particular issue at)
* Omit function return types
* Omit abi-tags
