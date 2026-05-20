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
