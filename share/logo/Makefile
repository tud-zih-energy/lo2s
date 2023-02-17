HEIGHT=512
LOGOS=rendered/circle.png rendered/circle_fill.png rendered/full.png rendered/circle.svg rendered/circle_fill.svg rendered/full.svg rendered/circle.pdf rendered/circle_fill.pdf rendered/full.pdf

all: $(LOGOS)

rendered/%.png: src/%.svg
	inkscape -C -o $@ -h $(HEIGHT) $<
rendered/%.svg: src/%.svg
	inkscape -C -l -o $@ $<
rendered/%.pdf: src/%.svg
	inkscape -C -o $@ $<

clean:
	rm -f $(LOGOS)
.PHONY: clean
