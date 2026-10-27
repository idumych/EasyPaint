# Test plan: EasyPaint

EasyPaint is a documents app: it opens, edits and saves images. Its
evidence is the files it writes.

## Fixtures

None beyond the app. Each check writes its image into the guest's home
folder (Haiku) or `$HOME` (Linux) and the tester copies the `file` output to
`dist/V/evidence/`.

## Fresh state

- Haiku: `rm -f ~/config/settings/"EasyPaint settings"`
- Linux: `rm -f ~/.config/cosmoe/"EasyPaint settings"`

With no settings, the first window is a white 400 × 300 image and the Pen
tool is selected.

## Feature checks

### draw-save-reopen

Steps:
1. Start EasyPaint. In the tool box, choose the **Rectangle** tool (its
   tooltip says "Rectangle"); in the palette, click the red square.
2. Drag a rectangle across the middle of the image. Choose the **Text** tool,
   drag a box below the rectangle and type `hello from {p}` in the **Text**
   window; click **OK**.
3. **File › Save as…**, type `easypaint-{p}.png`, press Enter.
4. **File › Close**, then **File › Open…** in another window (or start
   EasyPaint with `easypaint ~/easypaint-{p}.png`) and check the rectangle
   and the text are there.
5. On the guest: `file ~/easypaint-{p}.png > {p}-saved.txt` and copy it to
   `dist/V/evidence/`. Screenshot the reopened window as `{p}-reopened.png`.

PASS when: `{p}-saved.txt` says `PNG image data, 400 x 300`, and the
reopened window shows the red rectangle and `hello from {p}`.

### save-jpeg

Steps: with the image from the previous check open, **File › Save as…**,
type `easypaint-{p}.jpg`, press Enter. On the guest:
`file ~/easypaint-{p}.jpg > {p}-jpeg.txt`.

PASS when: `{p}-jpeg.txt` says `JPEG image data`, which shows the format
was chosen from the extension through the Translation Kit.
