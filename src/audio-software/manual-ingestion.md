# Manual Intake And Text Extraction

Use this only after choosing an application for active study. The catalog links
to official documentation but does not copy manuals into the repository.

## Create And Retain

Copy software-template to src/audio-software/<software-id>/ and set catalog_id
to the matching catalog.json ID.

Before downloading, confirm the official PDF may be kept for this repository
internal-study scope. Save it in manual/source/ with a stable name. Record its
URL, revision, retrieval date, page count, size, checksum, and extraction path
in manual/manuals.json. Do not save installers, third-party documents, or
material with unclear retention rights.

## Extract Text

Install Poppler pdftotext. Linux distributions commonly provide it through
poppler-utils; on Windows invoke the Poppler executable as pdftotext.exe.

Linux:

    pdftotext -layout source/<manual>.pdf extracted/<manual>.txt
    sha256sum source/<manual>.pdf

Windows PowerShell:

    & pdftotext.exe -layout .\source\<manual>.pdf .\extracted\<manual>.txt
    (Get-FileHash .\source\<manual>.pdf -Algorithm SHA256).Hash.ToLower()

The layout output is a local search layer. Use the original PDF for diagrams,
screenshots, page layout, and page numbering. Write source-linked conclusions
in reference/, and document actual tests with app version, OS, backend, device,
driver, plugin format, steps, and result.
