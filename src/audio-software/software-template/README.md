# Per-Application Documentation Template

Copy this directory to src/audio-software/<software-id>/ only when the
application needs retained manual material or local compatibility notes. Do not
create empty application directories for every catalog row.

Set the profile placeholders, rename manuals.example.json to manuals.json once
a document exists, and follow [manual ingestion](../manual-ingestion.md).

~~~text
<software-id>/
  README.md
  profile.json
  reference/README.md
  manual/
    README.md
    manuals.example.json
    source/README.md
    extracted/README.md
~~~
