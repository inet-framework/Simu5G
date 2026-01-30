# Simu5G Development Rules

## Coding Guidelines

- EXPLICIT ERRORS PREFERRED: When the code meets an unexpected condition,
  report it as an error (assertion or throw) rather than silently ignore or
  sidestep it with making the affected code block conditional on the inputs
  being OK, or using early returns. For example, if a pointer argument is
  nullptr, throw an exception or assert, do not just skip dereferencing it by
  surrounding the code with `if (p != nullptr)` statements.

- ASSERT VS EXCEPTION: Assertions are for ensuring assumptions are valid,
  exceptions are for reporting invalid input or state.

- EXCEPTION TYPE: Prefer cRuntimeError which has a sprintf-like argument list.
  Include relevant values in the error message. (Usually no need to include
  the enclosing class name, as the path and type of the OMNeT++ module "in context"
  will be part of the error message).

- Use `ASSERT()` (of OMNeT++) instead of `assert()`.

## Compiling

- Use debug builds with `-j`: `make MODE=debug -j12`.

## Open eyes

- Bring up criticism without explicit request from the user! The user is INTERESTED
  in what is wrong in the code!

- BE CRITICAL. Do not assume the code in the original project is always correct!
  Report misnamed functions and variables (where the name does not much the
   functionality) and suspicious (potentially buggy) code fragments.

- INCORRECT MODEL. In case of a simulation model of a network protocol, report if
  the code obviously does NOT confirm to the standard (RFC, IEEE standard, etc).

- Report if you find code or functionality in a class that obviously does NOT
  belong there.
