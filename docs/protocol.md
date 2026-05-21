# Protocol draft

Transport: TCP

Frame format:
- uint32 length (network byte order)
- payload bytes (UTF-8 text)

Payload format examples (key=value; pairs):
- type=login;user=alice
- type=msg;to=bob;text=hello
- type=info;text=...
- type=error;text=...

## Message types

### Client -> Server

- Login:
  - `type=login;user=<name>`

- Direct message:
  - `type=msg;to=<user>;text=<text>`

### Server -> Client

- Delivery to recipient:
  - `type=deliver;from=<user>;text=<text>`

- Info:
  - `type=info;text=<text>`

- Error:
  - `type=error;text=<text>`
