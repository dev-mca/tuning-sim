# Protocol v1 examples (contract)

One file per message type: `server/` = simcore → UI, `client/` = UI → simcore. They are the
shared contract of docs/interface.md (plan.md §11, option A):

- `core/tests/protocol_contract_test.cpp` decodes every client example and encodes a sample of
  every server message, and compares the **shape** (field names, nesting and JSON types) with
  the example.
- `ui/src/protocol/contract.test.ts` does the same with typed TypeScript samples and
  `parseServerMessage`.

Changing a field on one side only turns a test red. Change a message in this order: example
here → C++ and TypeScript → docs/interface.md. `null` in an example means "not available"
(e.g. turbo fields of a naturally aspirated engine) and matches a number or null.
