// Guest function stubs, from bo2-bisect/_probe_mp/src/stubs.cpp.
//
// bo2-bisect needed these because its SDK (0.10.0) left the imports unresolved. development
// implements or stubs every one of them in the kernel (xboxkrnl_usbcam.cpp, xam_stub.cpp), so for
// the P0 upstream baseline this file defines nothing. Not carried over: the `mode` cvar (zombies
// boot flag, needs the DefaultMpApp hook) and g_bo2_fast_fmas (the generated-code FMA patch).
//
// bo2-bisect returned 0x80004005 from XUsbcamDestroy / SetConfig / SetCaptureMode / ReadFrame;
// upstream's bare stubs differ. Revisit only if a camera path shows up in the logs.
