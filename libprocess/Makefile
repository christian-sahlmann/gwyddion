
GTK_CFLAGS = $(shell pkg-config --cflags gtk+-2.0)
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-2.0)
CFLAGS = -Wall -W -Wno-unused -ggdb -DDEBUG -DGWY_LOG_DOMAIN=\"Gwyddion\" $(GTK_CFLAGS) -I../libgwyddion/
LDFLAGS = $(GTK_LDFLAGS) -lm 

test: test.o datafield.o dataline.o pixfield.o simplefft.o ../libgwyddion/gwyserializable.o ../libgwyddion/gwywatchable.o


