[RFC][LLDB] Handling abi_tags in expression evaluator

This RFC seeks to discuss how the expression evaluator should handle
calls to C++ constructors/destructors that are annotated with the
`[[gnu::abi_tag]]` in the expression evaluator.

# Problem

Given this example:
```
struct Tagged {
  [[gnu::abi_tag("CtorTag")]] Tagged() {}
};

int main() {
	Tagged t;
	__builtin_debugtrap();
}
```
Calling the `Tagged()` constructor will fail in LLDB with following error:
```
(lldb) target create "a.out"                                                                       
(lldb) r                                                                                           
Process 961323 launched: 'a.out' (x86_64)  
Process 961323 stopped                                                                             
* thread #1, name = 'a.out', stop reason = breakpoint 1.1                                          
    frame #0: 0x0000000000401114 a.out`main at tags.cpp:7:2                                        
   4                                                                                               
   5    int main() {                                                                               
   6            Tagged t;                                                                          
-> 7            __builtin_debugtrap();                                                             
   8    }                                                                                          
(lldb) expr Tagged()                                                                               
error: Couldn't lookup symbols:                                                                    
  Tagged::Tagged()                                                                                 
```

If we look at the IR that Clang lowered the LLDB (DWARF) AST into,
the reason for the failed symbol lookup becomes more apparent:
```
lldb             Module as passed in to IRForTarget: 
"; ModuleID = '$__lldb_module'
source_filename = "$__lldb_module"

<-- snipped -->

define dso_local void @"_Z12$__lldb_exprPv"(ptr %"$__lldb_arg") #0 {
entry:
  <-- snipped -->

init.check:                                       ; preds = %entry
  call void @_ZN6TaggedC1Ev(ptr nonnull align 1 dereferenceable(1) @"_ZZ12$__lldb_exprPvE19$__lldb_expr_result") #2
```
We're trying to call a function symbol with mangled name `_ZN6TaggedC1Ev`.
However, no such symbol exists in the binary because the actual mangled
name for the constructors is `_ZN6TaggedC2B7CtorTagEv`:
```
$ llvm-dwarfdump a.out --name=Tagged
a.out:  file format elf64-x86-64

...

0x0000006e: DW_TAG_subprogram
              DW_AT_low_pc      (0x0000000000401130)
              DW_AT_high_pc     (0x000000000040113a)
              DW_AT_frame_base  (DW_OP_reg6 RBP)
              DW_AT_object_pointer      (0x00000089)
              DW_AT_linkage_name        ("_ZN6TaggedC2B7CtorTagEv")
              DW_AT_specification       (0x00000033 "Tagged")

```

I.e., *the ABI tags are missing from the mangled name*.

When LLDB creates the Clang AST from DWARF, it creates `clang::CXXConstructorDecl`s
from the `DW_TAG_subprogram` of a constructor's *declaration*. DWARF doesn't
tell us anything about the existence of abi-tags on the declaration, so we
never attach an `clang::AbiTagAttr` to said decl. Hence when Clang mangles the
`CXXConstructorDecl`, it doesn't know to include the tags in the mangling.

This may seem like a contrived example, but this manifests quite frequently. E.g.,
when dereferncing the result of a function returing a `std::shared_ptr` (who's structors
are abi-tagged). And there are no great workarounds to recommend to users for this currently.

## Related Solutions

We already solved this problem for non-structor `FunctionDecl`s in https://reviews.llvm.org/D40283.
The idea is to use the `DW_AT_linkage_name` on the function definition to
provide Clang with the mangled name of the function. We let Clang know about
the mangling using the `clang::AsmLabelAttr` (which Clang will note as the definitive
mangled it should use).

We then did the same for `CXXMethodDecl`s in https://reviews.llvm.org/D131974. This relies
on the fact that a `DW_AT_linkage_name` is attached to method *declarations* (since
LLDB creates the AST nodes for methods by parsing the parent `DW_TAG_structure_type`,
it only ever sees the method declaration, not definition).

## What's special about structors?

The declarations for structors don't have a `DW_AT_linkage_name`, for good reason. That's because
Clang (when using the Itanium ABI) will generate multiple variants for a constructor, each mangled
differently: https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling-special-ctor-dtor

So we can end up with following (simplified) DWARF:
```
0x00000057:   DW_TAG_structure_type
                DW_AT_containing_type   (0x00000057 "X")
                DW_AT_calling_convention        (DW_CC_pass_by_reference)
                DW_AT_name      ("X")

0x0000007c:     DW_TAG_subprogram
                  DW_AT_name    ("X")
                  DW_AT_declaration     (true)
                  DW_AT_external        (true)

0x000000a9:   DW_TAG_subprogram
                DW_AT_low_pc    (0x0000000000000000)
                DW_AT_high_pc   (0x000000000000001c)
                DW_AT_linkage_name      ("_ZN1XC2Ev")
                DW_AT_specification     (0x0000007c "X")

0x000000de:   DW_TAG_subprogram
                DW_AT_low_pc    (0x0000000000000020)
                DW_AT_high_pc   (0x0000000000000054)
                DW_AT_linkage_name      ("_ZN1XC1Ev")
                DW_AT_specification     (0x0000007c "X")
```

Note how the constructor is just a declaration with a name, and we have two
constructor definitions (with different `DW_AT_linkage_name`s) pointing
back to the same declaration.

So we can't really pick an linkage name up-front that we can attach to
a `CXXConstructorDecl` (like we did for methods).

This RFC proposes solutions to exactly this problem.

## Using the `std` module

We have a `target.import-std-module` setting that would work around this problem
because we can load an accurate AST into LLDB without going via DWARF. Unfortunately
it has its own issues at the moment (doesn't support all STL types and is not stable
enough to be enabled by default). Also, it wouldn't help with users debugging with
`libstdc++`.

# Potential Solutions

TODO: https://discord.com/channels/@me/1007084556702199938/1297856232325124158

## Encode ABI tags in DWARF

The idea here would be to introduce a new attribute `DW_AT_abi_tag` whose value would be a
string (presumably a `DW_FORM_strp`) to the `abi_tag`s (there can be multiple tags on a declaration)
of a structure/function/namespace declaration. We can't get away with only attaching them to
constructors because abi-tags are part of a type's mangling. So they would also appear in
the constructor mangled names via parameters (or template arguments/return types in the case
of templated constructors).

This approach was attempted in [D144181](https://reviews.llvm.org/D144181) but stalled because of
the downsides described below.

Downsides:

* A lot of functions/types in the STL are abi-tagged. So we would need to be careful to mitigate
  size impact. The prototype in [D144181](https://reviews.llvm.org/D144181) showed a large increase
  in the `.debug_info` section (albeit we used `DW_TAG_llvm_annotation`s for this, so it wasn't the
  most space-efficient representation). I have yet to measure the size impact of encoding it with
  a dedicated attribute and a `DW_FORM_strp`.
* This deviates from how we handle this for other types of function calls. Which might be fine, but
  does raise the question: do we want to rely on the mangled name roundtripping given LLDB's
  reconstructed AST isn't/can't be fully accurate). Using the linkage name seems more robust
  (though in an offline discussion @labath didn't point out that even linkage names aren't the most
  robust here, since they need not uniquely identify a function. A more complete/robust solution would
  encode, in the mangled name or elsewhere, enough info to point LLDB directly to the function).
* It's unclear how useful this attribute would be for any other DWARF consumer.

## Attach *all* mangled names to structor AST node

Pavel suggestions in https://reviews.llvm.org/D143652 and https://reviews.llvm.org/D144181
Clang Attribute+ctor kind DWARF attribute

Pros:
* Now aligns with how we hanlde this for non-structor function calls
* Doesn't rely on manlged name round-tripping (i.e., future-proof)
* Likely more space efficient

Cons:
* Requires Clang attribute (though not terribly controversial as this would only be
  used programmatically; when I brought this up with Aaron he wasn't terribly opposed,
  though that was only a very brief mention)
* Requires DWARF attribute to differentiate structor types. Alternatively could try determing structor type from mangled name
