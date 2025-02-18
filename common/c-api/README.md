# C API Usage

## SWSSResult and exceptions

All functions in the C API (except very basic ones defined in [util.h](util.h))
return `SWSSResult`. `SWSSResult` represents a possible exception that occurred
in a function, and it its defined in [result.h](result.h). A function's true
return value is given via one or more output pointers, specified as the final
parameter(s) of the function and prefixed with `out`.

If an exception occurs in a function that returns values via an output
pointer, no value is written to the output pointer. It is invalid to use the
returned value, including passing them to destructors (e.g. an `SWSSString`
to `SWSSString_free`).

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
validate the sanity of continuing after an exception.

Currently, `SWSSResult` only represents a catch-all of `std::exception`.
However, it can be augmented to differentiate different exceptions by adding a
new value to `SWSSException` and a corresponding catch block in `SWSSTry_` in
[result.h](result.h).

## Structures and memory management

Several functions take or produce nontrivial types, e.g. C++ strings
or associative arrays. 
