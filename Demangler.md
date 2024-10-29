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

The Swift demangler is configurable via a [`swift::Demangle::DemangleOptions`](https://github.com/swiftlang/swift/blob/b4b99e9d28232b7ae2d37534b9fa2bf91162e490/include/swift/Demangling/Demangle.h#L44-L67) structure.

I was told the MSVC demangler can be configured to display fewer things, though haven't been able to find any official documentation on this.

# Implementation Considerations

This section describes some details of a possible implementation of this RFC (based on our experience developing the prototype).

## Should this be part of the demangler?

One alternative is to do the simplification of these names in a pre- or post-processing step. E.g., `llvm-cxxmap` is a tool that allows one to specify equivalences between elements of mangled name. But it doesn't currently do any sort of remangling. It's primary (and only?) use is for a tool to determine whether two mangled names represent the same C++ name. One could try to re-use that infrastructure to write some sort of "mangled name simplification" tool. So we would then feed the simplified mangled name into the demangler. This seems difficult to get this right for more complex option such as "hide all template parameters after certain depth".

Post-processing seems undesirable, since at that point we're implementing a C++ parser.

## How to pass options to ItaniumDemangle

The main entry point to printing a demangled name lives on the demangle tree itself (on [`Node::print`](https://github.com/llvm/llvm-project/blob/4b44639a4320f980b3c9fa3b96e911e0741f179c/llvm/include/llvm/Demangle/ItaniumDemangle.h#L283-L296)). This makes it tricky to pass any sort of state that needs to be tracked throughout the whole printing process because there's simply no way to store it apart from passing it as a function parameter. The `Node` class doesn't link back to the `Demangler` (despite its lifetime being tied to it), so storing the options on the `Demangler` is currently not possible.

This has been previously worked around by [storing printing related state inside the `OutputBuffer` class](https://github.com/llvm/llvm-project/blob/4b44639a4320f980b3c9fa3b96e911e0741f179c/llvm/include/llvm/Demangle/Utility.h#L86-L95). That's because it is the only structure we do pass around while printing.

To avoid polluting the `OutputBuffer` with something like a `PrintingPolicy` member there are two alternatives:
1. Pass `PrintingPolicy` to all the `Node::printLeft`/`Node::printRight` overrides
2. Add a link from `Node` to the owning `Demangler` and store options on the `Demangler`
3. Add `PrintingPolicy` as a member to `OutputBuffer`
4. Decouple printing from the `Node` class using a new `PrintVisitor` that holds state used during printing

We ended up implementing option (4) because it seemd like the most architecurally sane approach. Though it does introduce the most churn here (since we need to move all the `printLeft`/`printRight` overrides into a new class. So we'd be happy to consider the other options.

### PrintVisitor (option 4)

The idea here is to rip out the `Node::printLeft`/`Node::printRight` APIs into a new `itanium_demangle::PrintVisitor` class. This class would look something like:
```
struct PrintVisitor {
  OutputBuffer &OB;
  PrintingPolicy PP;
  // ... other printing related state

  void printLeft(const Node *Node) {
    Node->visit([this](auto *N) {
        this->operator()(N, PrintKind::Left);
    });
  }

  void printRight(const Node *Node) {
    Node->visit([this](auto *N) {
        this->operator()(N, PrintKind::Right);
    });
  }

  // Example operator() overload
  void operator()(const NameType *Node, PrintKind::Side S) {
    switch (S) {
    case PrintKind::Left
      OB += Node->Name;
    case PrintKind::Right:
      return;
    }
  }
};
```

This re-uses the `Node::visit` API to walk over the demangle tree and print as we did before. Only now we the `printLeft`/`printRight` are part of the same `operator()` overload for each `Node` type and we have access to printing state without passing it around.

This has the added benefit of being able to move the printing state out of `OutputBuffer`.

Users that previously called `Node::print` would now call something like `Node->visit(PrintVisitor(Buffer, Policy)` (which could further be encapsulated).

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
