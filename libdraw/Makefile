
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-2.0)
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-2.0)
CFLAGS = -Wall -W -Wno-unused -ggdb -DDEBUG -DGWY_LOG_DOMAIN=\"Gwyddion\" $(GTK_CFLAGS) -I../libgwyddion/ -I../libprocess/
LDFLAGS = $(GTK_LDFLAGS) -lm 

test: test.o gwypalettedef.o gwypalette.o ../libgwyddion/gwyserializable.o ../libgwyddion/gwywatchable.o ../libprocess/interpolation.o


