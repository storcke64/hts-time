# HTS Time System — Submission Notes for Reviewers

Although the application includes an optional CLI mode for scripting and automation, the primary interface is a full GTK desktop GUI featuring a screensaver engine, video player, deep-time calendar display, and QR code output. The CLI is a supplementary tool, not the application's primary function.

### Permissions & Sandbox Justifications

**`--share=network`**
The application embeds a `libmicrohttpd` HTTP server bound exclusively to 127.0.0.1 (loopback only, never external). This is required because WebKitGTK enforces autoplay restrictions on `file://` URIs — video content served from the local filesystem will not play or loop reliably. Serving assets over `http://127.0.0.1` is the only viable workaround within the WebKitGTK engine. No data ever leaves the machine.

**`--filesystem=xdg-videos:ro`**
Required for the Wayscreen screensaver engine to automatically scan, queue, and loop video assets from the user's Videos directory in the background without requiring manual interaction via the file chooser portal.

**`--device=dri`**
Required for hardware-accelerated video decoding and rendering via GStreamer and the ffmpeg-full extension.

### Dependencies

**`org.freedesktop.Platform.ffmpeg-full` version 24.08**
The ffmpeg-full extension follows Freedesktop SDK versioning, not GNOME versioning. 24.08 is the correct branch for the GNOME 49 runtime.
