# swss-common C API

This directory contains the swss-common C API. The C API is an in-progress,
hand-written C wrapper around functions in swss-common. Functionality is
added as needed. It is designed to be understandable without incurring major
performance penalties. It was initially written to support Rust code using
swss-common ([here](https://github.com/sonic-net/sonic-dash-ha/tree/master/crates/swss-common)), 
but it can be used to create bindings to any language that can call C functions.

## SWSSResult and Exceptions

All functions in the C API (except some very basic ones) return `SWSSResult`.
`SWSSResult` represents a possible exception that occurred in a function, and it
is defined in [result.h](result.h). A function's true return value is given via
one or more output pointers, specified as the final parameter(s) of the function
and prefixed with `out`.

If an exception occurs in a function that returns values via an output
pointer, no value is written to the output pointer. It is invalid to use the
returned value in this case.

Example:

```c
SWSSDBConnector db_connector;
SWSSResult res = SWSSDBConnector_new_named("APPL_DB", 5000, 0, &db_connector);
// The new DBConnector is returned via this output pointer     ^^^^^^^^^^^^^

if (res.exception != SWSSException_None) {
  fprintf(stderr, "Exception occurred at '%s': %s\n", 
          SWSSStrRef_c_str((SWSSStrRef)res.location), 
          SWSSStrRef_c_str((SWSSStrRef)res.message));
  SWSSString_free(res.location);
  SWSSString_free(res.message);
  return ERROR;
}

// We know db_connector is valid now
```

No guarantees are made about the safety or sanity of continuing to run after
an exception occurs. If you wish to continue running after an exception, it
is expected that you read the underlying C++ code of the function yourself to
validate the sanity of doing this.

Currently, `SWSSResult` only represents a catch-all of `std::exception`.
However, it can be augmented to differentiate different exceptions by adding a
new value to `SWSSException` and a corresponding catch block in `SWSSTry_` in
[result.h](result.h).

## Complex Structures and Memory Management

Several functions take or produce nontrivial types, e.g. C++ `std::string`s
or associative arrays. All complex types and associated methods are defined in
[util.h](util.h). Each struct has a comment explaining how to properly free it.

In general, returned C strings (`char *`) must be freed with libc's `free`.
Other structs, like `SWSSString`, `SWSSFieldValueArray`, etc. must be freed
with corresponding free functions, like `SWSSString_free`, `SWSSFieldValueArray_free`, etc.

No function in the C API ever implicitly frees its arguments. It is required
that callers free all returned structures and structures passed as arguments to
C API functions. Similarly, free functions are not recursive, e.g. callers must
manually `free` each element in an `SWSSStringArray`, as `SWSSStringArray_free`
will not do this. This is done to give callers fine-grain control over data
lifetimes and the ability to reuse data to avoid memcpys.

However, C API functions do *take* data from their arguments using C++ move
semantics when possible. This means, for example, that an `SWSSFieldValueArray`
passed to `ZMQProducerStateTable_set` cannot be reused, as its strings have been
moved using `std::move`, but it still must be freed manually. This copies Rust's
ownership model, where all complex arguments passed are moved and considered invalidated.
The only exception to this is `SWSSStrRef`, which never moves data from the string.
