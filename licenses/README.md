# Third-party licenses

| Component | License | File |
|---|---|---|
| Inter 4.1 (Regular, Medium, SemiBold) — https://github.com/rsms/inter | SIL Open Font License 1.1 | `OFL-Inter.txt` |
| JetBrains Mono 2.304 (Regular, Medium) — https://github.com/JetBrains/JetBrainsMono | SIL Open Font License 1.1 | `OFL-JetBrainsMono.txt` |
| Space Grotesk (Medium, SemiBold) — https://github.com/floriankarsten/space-grotesk | SIL Open Font License 1.1 | `OFL-SpaceGrotesk.txt` |
| JUCE 8 — https://juce.com | AGPLv3 (JUCE 8 open-source licence) | fetched at build time |

The fonts are embedded unmodified in the plugin binary, with one exception: upstream Space Grotesk
ships no static SemiBold file, so `assets/fonts/SpaceGrotesk-SemiBold.ttf` is an instance of the
upstream variable font at `wght=600`, made with fontTools `varLib.instancer`. The licence permits
this; no Reserved Font Name is declared.
