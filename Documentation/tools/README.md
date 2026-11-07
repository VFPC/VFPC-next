# Building the User Guide PDF

The Markdown files in ../docs are the shared source for the website and PDF.
Chapter order, target version, edition status, PDF filename and review date are in
../user-guide.json. The PDF cover, headers and footer use that metadata.

Run on Windows with Python, ReportLab and Pillow available:

    python Documentation/tools/build_user_guide_pdf.py

An alternative output location can be supplied with --output. The default is
the versioned PDF in Documentation/docs/downloads. The build uses Windows
Arial fonts and does not install dependencies or publish the website.

The overview is read from docs/index.md rather than maintained separately in
the builder. Only the PDF-download section is omitted in print. The five
following chapters are rendered from the same Markdown as the website.
Website styling and print pagination differ; content and examples are shared.

Figure 6 is defined in guide_figure6.py. The builder exports the SVG referenced
by validation-results.md and embeds that same drawing as vector PDF content.
The cover image was retained from the released User Guide artwork.

After a build, render and inspect every page, check internal links and examples,
and confirm the source version agrees with src/VersionInfo.h. The PDF build
does not rebuild or install the plugin. Historical ADR/test records retain the
versions of the binaries actually tested and must not be relabelled.

Set status to RELEASE for the release edition, or DRAFT while reviewing an upcoming edition. This changes only the guide labelling; it never publishes a release.
