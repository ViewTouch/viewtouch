# VT Ai

ViewTouch's small AI, on the store's own computer: it turns a menu typed or
pasted, a photo of one or a PDF into items, sizes and choices for Menu
Builder; answers a manager's questions about sales from ViewTouch's own
reports; explains menu problems and writes descriptions. It only proposes:
a manager looks and confirms. Nothing it reads or writes leaves the store.

- `vtmodern-ai.service` runs llama.cpp's `llama-server` on 127.0.0.1:7720,
  with the model `/var/lib/viewtouch-ai/model.gguf`, below the point of sale
  (2 cores, lowest priority, one question at a time in about 3 GB,
  and the first process the kernel stops if memory runs out: a Raspberry
  Pi's kernel has no per-service memory limits, so MemoryMax applies only
  elsewhere).
- `vtmodern-device ai-model` downloads the model ViewTouch's release page
  names (signed, like updates); `vtmodern-device ai-model-usb` takes one from
  a USB stick (with its .sig); `vtmodern-device ai on|off` starts or stops it.
- Text in photos and scanned PDFs is read with Tesseract; PDFs with Poppler.

The model: Qwen3-4B-Instruct-2507, 4-bit (Q4_K_M, 2.5 GB; Apache 2.0). On
ViewTouch's menu benchmark (tools/vtai_bench.py, tests/vtai-bench) it read
all 64 items and prices of the 10 menus right; on the AIO-CM5-156 that took
about 55 seconds a menu (2.7 words a second), the register working as usual.
The 3B models tried (Llama 3.2 3B, SmolLM3 3B) were quicker but got a
third to a half of the items wrong. Any GGUF model can be put in its place.

The model's license comes with it (/var/lib/viewtouch-ai/MODEL-LICENSE).
llama.cpp is MIT-licensed (/usr/share/doc/vtmodern-ai/llama.cpp-LICENSE).
