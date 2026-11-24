# VT Ai

ViewTouch's small AI, on the store's own computer: it turns a menu typed or
pasted, a photo of one or a PDF into items, sizes and choices for Menu
Builder; answers a manager's questions about sales from ViewTouch's own
reports; explains menu problems and writes descriptions. It only proposes:
a manager looks and confirms. Nothing it reads or writes leaves the store.

- `vtmodern-ai.service` runs llama.cpp's `llama-server` on 127.0.0.1:7720,
  with the model `/var/lib/viewtouch-ai/model.gguf`, below the point of sale
  (3 cores at most, lowest priority, 4.5 GB of memory at most).
- `vtmodern-device ai-model` downloads the model ViewTouch's release page
  names (signed, like updates); `vtmodern-device ai-model-usb` takes one from
  a USB stick (with its .sig); `vtmodern-device ai on|off` starts or stops it.
- Text in photos and scanned PDFs is read with Tesseract; PDFs with Poppler.

The model's license comes with it (/var/lib/viewtouch-ai/MODEL-LICENSE).
llama.cpp is MIT-licensed (/usr/share/doc/vtmodern-ai/llama.cpp-LICENSE).
