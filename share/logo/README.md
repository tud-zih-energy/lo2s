# `lo2s` Logo
This directory contains all data associated to the `lo2s` logo.

## Logo Variants
- `circle`, only circle, transparent background

  ![lo2s logo circle](./circle.png)

- `circle_fill`, only circle, white background inside circle (outside transparent)

  ![lo2s logo circle fill](./circle_fill.png)

- `full`, lo2s name, uses circle logo as o

  ![lo2s logo full](./full.png)
  
## Notes on Design
- font
  - The font is embedded as polygons in the files, no external font required.
  - The used font is [DejaVu Sans Mono](https://dejavu-fonts.github.io/).
- background fill in circle
  - The white background in the `circle_fill` variant is slightly smaller than the outer circle segments.
    Making both the same size leads to a white border looming behind the circle segments when rendering to a pixel image.
    This effect should only become clearly visible in huge resolutions, and if required you can adjust the sizes.

## Building the Logos from Source
Use inkscape to render the `.svg` files.
You can invoke that by calling `make`.

Always **commit the built `.png`s**.

To adjust the height call `make HEIGHT=1024` with your desired height in pixel,
the width is automatically selected to respect aspect ratio.
Default height is 512 px.

The files are linked, only `circle.svg` contains the circle logo.
All other files only refer to it.
This link requires setting the DPI for the embedded file, currently this value is 8192.
When printing to a high resolution (poster etc.) you might have to modify this DPI value.
(No data is lost in this process, this is purely an inkscape rendering issue.)

The text is embedded as paths,
but the original text object still exists in the files (but is set to invisble.)
