PDFium runtime files are downloaded automatically during an x64 build.

The Visual Studio post-build step runs tools/get_pdfium.ps1, stores pdfium.dll
here, and copies it beside O2EM-NG.exe. The binary comes from the prebuilt
PDFium packages maintained by bblanchon/pdfium-binaries.

Pinned package: chromium/7961, win-x64.
