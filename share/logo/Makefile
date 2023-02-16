HEIGHT=512
LOGOS=circle.png circle_fill.png full.png
all: $(LOGOS)

%.png: %.svg
	inkscape -C -o $@ -h $(HEIGHT) $<

clean:
	rm -f $(LOGOS)
.PHONY: clean
