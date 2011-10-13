# sfpng, a straightforward minimal PNG decoder

See manual.asciidoc for the what and the why.

I wrote this half a year ago with the aim of replacing libpng in
Chrome, but eventually realized that:

1. in the limit Chrome also be able to *encode* PNGs, something I
   wasn't especially interested in working on at the time,
2. on Linux, due to using GTK, we'd end up with both sfpng and
   libpng linked into the final binary, *growing* the resulting
   program rather than shrinking it.

Perhaps it'd still make sense.  Maybe I'll get inspired in the future
and finish it.  In any case I haven't touched the code in quite a
while so I may as well publish it rather than having it on my laptop.

  -- Evan Martin, 13 Oct 2011
