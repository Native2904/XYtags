# xytags.wdx – XYplorer Tag File as TC Content Plugin

Displays XYplorer tags (Label, Tags, Comment) as columns in Total Commander.

---

## Files

```
xytags.ini      Configuration file (place next to the .wdx after building)
xytags.wdx      Compiled plugin (32-bit)
xytags.wdx64    Compiled plugin (64-bit)
```

---

## Installation

1. Copy `xytags.wdx` and `xytags.wdx64` to a plugin directory of your choice,
   e.g. `C:\Program Files\totalcmd\Plugins\WDX\xytags\`

2. Place `xytags.ini` in the **same directory** as the .wdx files.

3. Set the path to your `tag.dat` in the INI file:

   ```ini
   [XYTags]
   TagFile=C:\Users\Home\AppData\Roaming\XYplorer\tag.dat
   ```

4. In TC: Configuration → Plugins → Content Plugins (.WDX) → Add
   → select `xytags.wdx`.

---

## Adding Columns in TC

1. TC file list → right-click on the column header → "Customize Columns…"
2. "Add" → Plugin: xytags → choose a field:
   - **XY_Label**    – the label (e.g. "Tcmd", "XYplorer")
   - **XY_Tags**     – tags (e.g. "app, portable")
   - **XY_Comment**  – comment / description

---

## Fields

| Field Name | Content                    | Example         |
| ---------- | -------------------------- | --------------- |
| XY_Label   | Label name from XYplorer   | `Tcmd`          |
| XY_Tags    | Comma-separated tags       | `app, portable` |
| XY_Comment | Free-text comment          | `FTP Client`    |

Files without an entry in tag.dat will show empty fields (no error).

---

## Notes

- The tag.dat is loaded **once at startup** and cached.
  If the tag.dat changes: restart TC or reload plugins
  (Configuration → Plugins → Reload).

- Path comparisons are **case-insensitive** (standard on Windows).

- The format `XYplorer File Tags v5` (UTF-16LE) is supported.
  For older versions with a different column count, adjust the
  comment field index in `xytags.cpp` (line `f[19]`).
