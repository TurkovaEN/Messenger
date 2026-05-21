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

  ## Group chats (rooms)

### Client -> Server
- Create room:
  - `type=room_create;room=<name>`
- Join room:
  - `type=room_join;room=<name>`
- Leave room:
  - `type=room_leave;room=<name>`
- Send message to room:
  - `type=room_msg;room=<name>;text=<text>`

### Server -> Client
- Delivery to room members:
  - `type=room_deliver;room=<name>;from=<user>;text=<text>`
