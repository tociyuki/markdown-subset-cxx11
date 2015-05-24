MD=../../mkdown
DIFF=/usr/bin/diff -u

test :
	for i in *.md; do\
	  $(MD) < $$i > $${i%.*}.out ;\
	  $(DIFF) $${i%.*}.xhtml $${i%.*}.out ;\
	done

clean :
	rm -f *.out
